/*
 *  $Id$
 *
 *  Copyright(C) 2004 Frank A. Uepping
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "core.h"
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/isdn/capiutil.h>
#include <linux/isdn/capicmd.h>


struct capi_device* capi_devices_table[CAPI_MAX_DEVS];
spinlock_t capi_devices_table_lock;
int __nr_capi_devices;


static inline void
__capi_device_get(struct capi_device* dev)
{
	kref_get(&dev->refs);
}


static struct capi_device*
capi_device_get(struct capi_device* dev)
{
	if (!dev)
		return NULL;

	spin_lock(&dev->lock);
	if (unlikely(dev->state == CAPI_DEVICE_STATE_ZOMBIE))
		dev = NULL;
	else
		__capi_device_get(dev);
	spin_unlock(&dev->lock);

	return dev;
}


static inline void
capi_device_put(struct capi_device* dev)
{
	kref_put(&dev->refs);
}


struct capi_device*
capi_device_alloc(void)
{
	struct capi_device* dev = kmalloc(sizeof *dev, GFP_KERNEL);
	if (unlikely(!dev))
		return NULL;

	memset(dev, 0, sizeof *dev);

	atomic_set(&dev->refs.refcount, 0);	/* Sanity check in capi_device_free(). */

	dev->class_dev.class = &capi_class;
	class_device_initialize(&dev->class_dev);

	return dev;
}


static void
capi_driver_release_device(struct kref* kref)
{
	struct capi_device* dev = container_of(kref, struct capi_device, refs);
	struct module* owner = dev->drv->owner;
	int appls;

	wmb();
	appl = atomic_read(&dev->appls);

	dev->drv->release(dev);

	while (appls--)
		module_put(owner);
}


void
release_capi_device(struct class_device* cd)
{
	struct capi_device* dev = to_capi_device(cd);

	if (likely(dev->id)) {
		spin_lock(&capi_devices_table_lock);
		capi_devices_table[dev->id - 1] = NULL;
		__nr_capi_devices--;
		spin_unlock(&capi_devices_table_lock);
	}

	kfree(dev);
}


static inline int
register_capi_device(struct capi_device* dev)
{
	int id;

	spin_lock(&capi_devices_table_lock);
	for (id = 0; id < CAPI_MAX_DEVS; id++)
		if (!capi_devices_table[id]) {
			capi_devices_table[id] = dev;
			__nr_capi_devices++;
			dev->id = id + 1;
			break;
		}
	spin_unlock(&capi_devices_table_lock);

	return dev->id;
}


int
capi_device_register(struct capi_device* dev)
{
	int err;

	if (unlikely(!dev))
		return -EINVAL;

	if (unlikely(!register_capi_device(dev)))
		return -EMFILE;

	err = capi_device_register_sysfs(dev);
	if (unlikely(err))
		return err;

	kref_init(&dev->refs, capi_driver_release_device);
	spin_lock_init(&dev->stats.lock);
	spin_lock_init(&dev->lock);
	atomic_set(&dev->appls, 0);

	dev->state = CAPI_DEVICE_STATE_RUNNING;

	return 0;
}


void
capi_device_unregister(struct capi_device* dev)
{
	class_device_del(&dev->class_dev);

	spin_lock(&dev->lock);
	dev->state = CAPI_DEVICE_STATE_ZOMBIE;
	spin_unlock(&dev->lock);

	BUG_ON(!module_is_live(dev->drv->owner) && atomic_read(&dev->refs.refcount) != 1);
	capi_device_put(dev);
}


void
capi_appl_signal_info(struct capi_appl* appl, capinfo_0x11 info)
{
	unsigned long flags;

	spin_lock_irqsave(&appl->info_lock, flags);
	if (unlikely(appl->info))
		spin_unlock_irqrestore(&appl->info_lock, flags);
	else {
		appl->info = info;
		spin_unlock_irqrestore(&appl->info_lock, flags);

		capi_appl_signal(appl);	
	}
}


static unsigned long __capi_appl_ids[BITS_TO_LONGS(CAPI_MAX_APPLS)];
static spinlock_t __capi_appl_ids_lock;


static inline int
alloc_capi_appl_id(struct capi_appl* appl)
{
	int id;

	spin_lock(__capi_appl_ids_lock);
	id = find_first_zero_bit(__capi_appl_ids, CAPI_MAX_APPLS);
	if (likely(id < CAPI_MAX_APPLS)) {
		__set_bit(id, __capi_appl_ids);
		appl->id = id + 1;
	}
	spin_unlock(__capi_appl_ids_lock);

	return appl->id;
}


static inline void
release_capi_appl_id(struct capi_appl* appl)
{
	if (likely(appl->id))
		__clear_bit(appl->id - 1, __capi_appl_ids);
}


static void
release_capi_appl(struct kref* kref)
{
	struct capi_appl* appl = container_of(kref, struct capi_appl, refs);

	skb_queue_purge(&appl->msg_queue);
	release_capi_appl_id(appl);

	kfree(appl);
}


struct capi_appl*
capi_appl_alloc(void)
{
	struct capi_appl* appl = kmalloc(sizeof *appl, GFP_KERNEL);
	if (unlikely(!appl))
		return NULL;

	memset(appl, 0, sizeof *appl);

	skb_queue_head_init(&appl->msg_queue);
	kref_init(&appl->refs, release_capi_appl);

	return appl;
}


capinfo_0x10
capi_register(struct capi_appl* appl)
{
	struct capi_device* dev;
	capinfo_0x10 info;
	int id;

	if (unlikely(!appl))
		return CAPINFO_0X10_OSRESERR;

	spin_lock_init(&appl->info_lock);
	spin_lock_init(&appl->stats.lock);
	appl->info = CAPINFO_0X11_NOERR;

	if (unlikely(appl->params.datablklen < 128))
		return CAPINFO_0X10_LOGBLKSIZETOSMALL;

	if (unlikely(!alloc_capi_appl_id(appl)))
		return CAPINFO_0X10_TOOMANYAPPLS;

	spin_lock(&capi_devices_table_lock);
	for (id = 0; id < CAPI_MAX_DEVS; id++) {
		dev = capi_devices_table[id];
		if (!dev)
			continue;

		spin_lock(&dev->lock);
		if (unlikely(!(dev->state == CAPI_DEVICE_STATE_RUNNING && try_module_get(dev->drv->owner)))) {
			spin_unlock(&dev->lock);
			continue;
		}
		__capi_device_get(dev);
		spin_unlock(&dev->lock);
		spin_unlock(&capi_devices_table_lock);

		info = dev->drv->capi_register(dev, appl);
		if (unlikely(info)) {
			struct module* owner = dev->drv->owner;

			printk(KERN_NOTICE "capi: CAPI_REGISTER failed on device %d (info: %#x)\n", dev->id, info);
			capi_device_put(dev);
			module_put(owner);
		} else {
			__set_bit(id, appl->devs);
			atomic_inc(&dev->appls);
			class_device_get(&dev->class_dev);
			capi_device_put(dev);
		}

		spin_lock(&capi_devices_table_lock);
	}
	spin_unlock(&capi_devices_table_lock);

	return CAPINFO_0X10_NOERR;
}


static inline capinfo_0x11
get_capi_appl_info(struct capi_appl* appl)
{
	capinfo_0x11 info;
	unsigned long flags;

	spin_lock_irqsave(&appl->info_lock, flags);
	info = appl->info;
	if (unlikely(info))
		appl->info = CAPINFO_0X11_NOERR;
	spin_unlock_irqrestore(&appl->info_lock, flags);

	return info;
}


capinfo_0x11
capi_release(struct capi_appl* appl)
{
	struct capi_device* dev;
	int id;

	for (id = 0; (id = find_next_bit(appl->devs, CAPI_MAX_DEVS, id)) < CAPI_MAX_DEVS; id++) {
		dev = capi_devices_table[id];
		BUG_ON(!dev);
		if (likely(capi_device_get(dev))) {
			dev->drv->capi_release(dev, appl);

			atomic_dec(&dev->appls);
			capi_device_put(dev);
			module_put(dev->drv->owner);
		}
		class_device_put(&dev->class_dev);
	}

	return get_capi_appl_info(appl);
}


capinfo_0x11
capi_put_message(struct capi_appl* appl, struct sk_buff* msg)
{
	struct capi_device* dev;
	capinfo_0x11 info;
	int id;

	if (!msg || msg->len < CAPIMSG_BASELEN + 4)
		return CAPINFO_0X11_ILLCMDORMSGTOSMALL;

	id = CAPIMSG_CONTROLLER(msg->data);
	if (unlikely(!(id && id < CAPI_MAX_DEVS + 1 && test_bit(id - 1, appl->devs))))
		return CAPINFO_0X11_OSRESERR;

	info = get_capi_appl_info(appl);
	if (unlikely(info))
		return info;

	dev = capi_device_get(capi_devices_table[id - 1]);
	if (unlikely(!dev))
		return CAPINFO_0X11_OSRESERR;

	info = dev->drv->capi_put_message(dev, appl, msg);

	capi_device_put(dev);

	return info;
}


capinfo_0x11
capi_get_message(struct capi_appl* appl, struct sk_buff** msg)
{
	capinfo_0x11 info = get_capi_appl_info(appl);
	if (unlikely(info))
		return info;

	*msg = skb_dequeue(&appl->msg_queue);
	if (unlikely(!*msg))
		return CAPINFO_0X11_QUEUEEMPTY;

	return CAPINFO_0X11_NOERR;
}


capinfo_0x11
capi_peek_message(struct capi_appl* appl)
{
	if (appl->info || !skb_queue_empty(&appl->msg_queue))
		return CAPINFO_0X11_QUEUEFULL;

	return CAPINFO_0X11_QUEUEEMPTY;
}


capinfo_0x11
capi_isinstalled(void)
{
	if (nr_capi_devices())
		return CAPINFO_0X11_NOERR;
	else
		return CAPINFO_0X11_NOTINSTALLED;
}


static struct capi_device*
capi_device_by_id_get(int id)
{
	struct capi_device* dev;

	if (id < 1 || id > CAPI_MAX_DEVS)
		return NULL;

	spin_lock(&capi_devices_table_lock);
	dev = capi_device_get(capi_devices_table[id - 1]);
	spin_unlock(&capi_devices_table_lock);

	return dev;
}


u8*
capi_get_manufacturer(int id, u8 manufacturer[CAPI_MANUFACTURER_LEN])
{
	u8* res = manufacturer;

	if (id) {
		struct capi_device* dev = capi_device_by_id_get(id);
		if (dev)
			memcpy(manufacturer, dev->manufacturer, CAPI_MANUFACTURER_LEN);
		else
			res = NULL;
		capi_device_put(dev);
	} else
		strlcpy(manufacturer, "NGC4Linux", CAPI_MANUFACTURER_LEN);

	return res;
}


u8*
capi_get_serial(int id, u8 serial[CAPI_SERIAL_LEN])
{
	u8* res = serial;

	if (id) {
		struct capi_device* dev = capi_device_by_id_get(id);
		if (dev)
			memcpy(serial, dev->serial, CAPI_SERIAL_LEN);
		else
			res = NULL;
		capi_device_put(dev);
	} else
		strlcpy(serial, "0", CAPI_SERIAL_LEN);

	return res;
}


struct capi_version*
capi_get_version(int id, struct capi_version* version)
{
	static struct capi_version capicore_version = { 2, 0, 0, 1 };

	if (id) {
		struct capi_device* dev = capi_device_by_id_get(id);
		if (dev)
			*version = dev->version;
		else
			version = NULL;
		capi_device_put(dev);
	} else
		*version = capicore_version;

	return version;
}


capinfo_0x11
capi_get_profile(int id, struct capi_profile* profile)
{
	capinfo_0x11 info = CAPINFO_0X11_NOERR;

	if (id) {
		struct capi_device* dev = capi_device_by_id_get(id);
		if (dev) {
			*profile = dev->profile;
			profile->ncontroller = nr_capi_devices();
		} else
			info = CAPINFO_0X11_OSRESERR;
		capi_device_put(dev);
	} else
		profile->ncontroller = nr_capi_devices();

	return info;
}


static int __init
capicore_init(void)
{
	return class_register(&capi_class);
}


static void __exit
capicore_exit(void)
{
	class_unregister(&capi_class);
}


module_init(capicore_init);
module_exit(capicore_exit);


MODULE_AUTHOR("Frank A. Uepping <Frank.Uepping@web.de>");
MODULE_LICENSE("GPL");


EXPORT_SYMBOL(capi_device_alloc);
EXPORT_SYMBOL(capi_device_register);
EXPORT_SYMBOL(capi_device_unregister);
EXPORT_SYMBOL(capi_appl_alloc);
EXPORT_SYMBOL(capi_register);
EXPORT_SYMBOL(capi_release);
EXPORT_SYMBOL(capi_put_message);
EXPORT_SYMBOL(capi_get_message);
EXPORT_SYMBOL(capi_peek_message);
EXPORT_SYMBOL(capi_isinstalled);
EXPORT_SYMBOL(capi_get_manufacturer);
EXPORT_SYMBOL(capi_get_serial);
EXPORT_SYMBOL(capi_get_version);
EXPORT_SYMBOL(capi_get_profile);
