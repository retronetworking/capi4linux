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


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/isdn/capidevice.h>
#include <linux/isdn/capiutil.h>
#include <linux/isdn/capicmd.h>


static struct capi_device* capi_devices_table[CAPI_MAX_DEVS];
static spinlock_t capi_devices_table_lock;
atomic_t nr_capi_devices = ATOMIC_INIT(0);


int	capi_device_register_sysfs	(struct capi_device* dev);


static inline void
get_capi_device(struct capi_device* dev)
{
	kref_get(&dev->refs);
}


static struct capi_device*
try_get_capi_device(struct capi_device* dev)
{
	if (!dev)
		return NULL;

	spin_lock(&dev->state_lock);
	if (unlikely(dev->state != CAPI_DEVICE_STATE_RUNNING))
		dev = NULL;
	else
		get_capi_device(dev);
	spin_unlock(&dev->state_lock);

	return dev;
}


static inline void
put_capi_device(struct capi_device* dev)
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

	dev->class_dev.class = &capi_class;
	class_device_initialize(&dev->class_dev);

	return dev;
}


static void
release_capi_device(struct kref* kref)
{
	struct capi_device* dev = container_of(kref, struct capi_device, refs);
	struct module* owner = dev->drv->owner;

	int appls = atomic_read(&dev->appls);
	while (appls--)
		module_put(owner);

	complete(&dev->done);
}


void
free_capi_device(struct class_device* cd)
{
	struct capi_device* dev = to_capi_device(cd);

	if (likely(dev->id)) {
		spin_lock(&capi_devices_table_lock);
		capi_devices_table[dev->id - 1] = NULL;
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
			spin_unlock(&capi_devices_table_lock);

			atomic_inc(&nr_capi_devices);

			return dev->id = id + 1;
		}
	spin_unlock(&capi_devices_table_lock);

	return 0;
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

	kref_init(&dev->refs, release_capi_device);
	init_completion(&dev->done);
	atomic_set(&dev->appls, 0);
	spin_lock_init(&dev->stats.lock);
	spin_lock_init(&dev->state_lock);
	dev->state = CAPI_DEVICE_STATE_RUNNING;

	return 0;
}


void
capi_device_unregister(struct capi_device* dev)
{
	class_device_del(&dev->class_dev);

	spin_lock(&dev->state_lock);
	dev->state = CAPI_DEVICE_STATE_ZOMBIE;
	spin_unlock(&dev->state_lock);

	atomic_dec(&nr_capi_devices);

	BUG_ON(!module_is_live(dev->drv->owner) && atomic_read(&dev->refs.refcount) != 1);
	put_capi_device(dev);
	wait_for_completion(&dev->done);
}


static unsigned long __capi_appl_ids[BITS_TO_LONGS(CAPI_MAX_APPLS)];
static spinlock_t __capi_appl_ids_lock;


static inline int
alloc_capi_appl_id(struct capi_appl* appl)
{
	int id;

	appl->id = 0;

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


capinfo_0x10
capi_register(struct capi_appl* appl)
{
	struct capi_device* dev;
	capinfo_0x10 info;
	int id;

	if (unlikely(!appl))
		return CAPINFO_0X10_OSRESERR;

	if (unlikely(appl->params.datablklen < 128))
		return CAPINFO_0X10_LOGBLKSIZETOSMALL;

	if (unlikely(!alloc_capi_appl_id(appl)))
		return CAPINFO_0X10_TOOMANYAPPLS;

	skb_queue_head_init(&appl->msg_queue);
	appl->info = CAPINFO_0X11_NOERR;
	spin_lock_init(&appl->stats.lock);
	memset(&appl->stats, 0, sizeof appl->stats);
	memset(&appl->devs, 0, sizeof appl->devs);

	spin_lock(&capi_devices_table_lock);
	for (id = 0; id < CAPI_MAX_DEVS; id++) {
		dev = capi_devices_table[id];
		if (!dev)
			continue;

		spin_lock(&dev->state_lock);
		if (unlikely(!(dev->state == CAPI_DEVICE_STATE_RUNNING && try_module_get(dev->drv->owner)))) {
			spin_unlock(&dev->state_lock);
			continue;
		}
		get_capi_device(dev);
		spin_unlock(&dev->state_lock);
		spin_unlock(&capi_devices_table_lock);

		info = dev->drv->capi_register(dev, appl);
		if (unlikely(info)) {
			struct module* owner = dev->drv->owner;

			printk(KERN_NOTICE "capi: CAPI_REGISTER failed on device %d (info: %#x)\n", id + 1, info);

			put_capi_device(dev);
			module_put(owner);
		} else {
			__set_bit(id, appl->devs);
			atomic_inc(&dev->appls);
			smp_mb__after_atomic_inc();
			capi_device_get(dev);
			put_capi_device(dev);
		}

		spin_lock(&capi_devices_table_lock);
	}
	spin_unlock(&capi_devices_table_lock);

	return CAPINFO_0X10_NOERR;
}


capinfo_0x11
capi_release(struct capi_appl* appl)
{
	struct capi_device* dev;
	int id;

	for (id = 0; (id = find_next_bit(appl->devs, CAPI_MAX_DEVS, id)) < CAPI_MAX_DEVS; id++) {
		dev = capi_devices_table[id];
		BUG_ON(!dev);
		if (likely(try_get_capi_device(dev))) {
			dev->drv->capi_release(dev, appl);

			atomic_dec(&dev->appls);
			smp_mb__after_atomic_dec();
			module_put(dev->drv->owner);

			put_capi_device(dev);
		}
		capi_device_put(dev);
	}

	skb_queue_purge(&appl->msg_queue);
	release_capi_appl_id(appl);

	return appl->info;
}


capinfo_0x11
capi_put_message(struct capi_appl* appl, struct sk_buff* msg)
{
	struct capi_device* dev;
	int id;

	capinfo_0x11 info = appl->info;
	if (unlikely(info))
		return info;

	if (!msg || msg->len < CAPIMSG_BASELEN + 4)
		return CAPINFO_0X11_ILLCMDORMSGTOSMALL;

	id = CAPIMSG_CONTROLLER(msg->data);
	if (unlikely(!(id && id <= CAPI_MAX_DEVS && test_bit(id - 1, appl->devs))))
		return CAPINFO_0X11_OSRESERR;

	dev = try_get_capi_device(capi_devices_table[id - 1]);
	if (unlikely(!dev))
		return CAPINFO_0X11_OSRESERR;
	info = dev->drv->capi_put_message(dev, appl, msg);
	put_capi_device(dev);

	return info;
}


capinfo_0x11
capi_get_message(struct capi_appl* appl, struct sk_buff** msg)
{
	if (unlikely(appl->info))
		return appl->info;

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
	if (atomic_read(&nr_capi_devices))
		return CAPINFO_0X11_NOERR;
	else
		return CAPINFO_0X11_NOTINSTALLED;
}


static struct capi_device*
try_get_capi_device_by_id(int id)
{
	struct capi_device* dev;

	if (id < 1 || id > CAPI_MAX_DEVS)
		return NULL;

	spin_lock(&capi_devices_table_lock);
	dev = try_get_capi_device(capi_devices_table[id - 1]);
	spin_unlock(&capi_devices_table_lock);

	return dev;
}


u8*
capi_get_manufacturer(int id, u8 manufacturer[CAPI_MANUFACTURER_LEN])
{
	u8* res = manufacturer;

	if (id) {
		struct capi_device* dev = try_get_capi_device_by_id(id);
		if (dev)
			memcpy(manufacturer, dev->manufacturer, CAPI_MANUFACTURER_LEN);
		else
			res = NULL;
		put_capi_device(dev);
	} else
		strlcpy(manufacturer, "NGC4Linux", CAPI_MANUFACTURER_LEN);

	return res;
}


u8*
capi_get_serial(int id, u8 serial[CAPI_SERIAL_LEN])
{
	u8* res = serial;

	if (id) {
		struct capi_device* dev = try_get_capi_device_by_id(id);
		if (dev)
			memcpy(serial, dev->serial, CAPI_SERIAL_LEN);
		else
			res = NULL;
		put_capi_device(dev);
	} else
		strlcpy(serial, "0", CAPI_SERIAL_LEN);

	return res;
}


struct capi_version*
capi_get_version(int id, struct capi_version* version)
{
	static struct capi_version capicore_version = { 2, 0, 0, 1 };

	if (id) {
		struct capi_device* dev = try_get_capi_device_by_id(id);
		if (dev)
			*version = dev->version;
		else
			version = NULL;
		put_capi_device(dev);
	} else
		*version = capicore_version;

	return version;
}


capinfo_0x11
capi_get_profile(int id, struct capi_profile* profile)
{
	capinfo_0x11 info = CAPINFO_0X11_NOERR;

	if (id) {
		struct capi_device* dev = try_get_capi_device_by_id(id);
		if (dev) {
			*profile = dev->profile;
			profile->ncontroller = atomic_read(&nr_capi_devices);
		} else
			info = CAPINFO_0X11_OSRESERR;
		put_capi_device(dev);
	} else
		profile->ncontroller = atomic_read(&nr_capi_devices);

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
