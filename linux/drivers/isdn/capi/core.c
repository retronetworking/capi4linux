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
#include <linux/isdn/capidevice.h>
#include <linux/isdn/capiutil.h>
#include <linux/isdn/capicmd.h>


static struct capi_device* capi_devs_table[CAPI_MAX_DEVS];
static spinlock_t capi_devs_table_lock = SPIN_LOCK_UNLOCKED;

LIST_HEAD(capi_appls_list);
DECLARE_MUTEX(capi_appls_list_sem);

static LIST_HEAD(capi_devs_list);
static DECLARE_RWSEM(capi_devs_list_sem);

atomic_t nr_capi_devs = ATOMIC_INIT(0);


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


void
free_capi_device(struct class_device* cd)
{
	struct capi_device* dev = to_capi_device(cd);

	if (likely(dev->id)) {
		spin_lock_bh(&capi_devs_table_lock);
		capi_devs_table[dev->id - 1] = NULL;
		spin_unlock_bh(&capi_devs_table_lock);
	}

	kfree(dev);
}


static inline int
add_capi_device(struct capi_device* dev)
{
	int i;

	for (i = 0; i < CAPI_MAX_DEVS; i++)
		if (!capi_devs_table[i]) {
			capi_devs_table[i] = dev;

			return dev->id = i + 1;
		}

	return dev->id = 0;
}


static void
register_capi_appl(struct capi_appl* appl, struct capi_device* dev)
{
	capinfo_0x11 info = dev->drv->capi_register(dev, appl);
	if (unlikely(info)) {
		printk(KERN_NOTICE "capicore: appl %d couldn't be registered with device %d (info: %#x).\n", appl->id, dev->id, info);
		return;
	}

	set_bit(dev->id - 1, appl->devs);
	capi_device_get(dev);
}


static inline int
bind_capi_device(struct capi_device* dev)
{
	struct capi_appl* appl;

	down_write(&capi_devs_list_sem);
	if (likely(add_capi_device(dev))) {
		list_for_each_entry(appl, &capi_appls_list, entry)
			register_capi_appl(appl, dev);

		up_write(&dev->sem);
		atomic_inc(&nr_capi_devs);

		list_add_tail(&dev->entry, &capi_devs_list);
	}
	up_write(&capi_devs_list_sem);

	return dev->id;
}


void
unregister_capi_device(struct capi_device* dev)
{
	down_write(&capi_devs_list_sem);
	list_del_init(&dev->entry);
	up_write(&capi_devs_list_sem);

	down_write(&dev->sem);
	atomic_dec(&nr_capi_devs);
}


int
capi_device_register(struct capi_device* dev)
{
	int	capi_device_register_sysfs	(struct capi_device* dev);

	int res;

	if (unlikely(!dev))
		return -EINVAL;

	init_rwsem(&dev->sem);
	down_write(&dev->sem);

	spin_lock_init(&dev->stats.lock);

	if (unlikely(!bind_capi_device(dev)))
		return -EMFILE;

	res = capi_device_register_sysfs(dev);
	if (unlikely(res))
		unregister_capi_device(dev);
	else
		pr_info("capicore: registered new device %d\n", dev->id);

	return res;
}


void
capi_device_unregister(struct capi_device* dev)
{
	unregister_capi_device(dev);
	class_device_del(&dev->class_dev);

	pr_info("capicore: unregistered device %d\n", dev->id);
}


static inline int
add_capi_appl(struct capi_appl* appl)
{
	struct capi_appl* a;
	u16 id = 1;

	down(&capi_appls_list_sem);
	list_for_each_entry(a, &capi_appls_list, entry) {
		if (a->id > id)
			break;
		id++;
	}

	if (unlikely(id > CAPI_MAX_APPLS))
		appl->id = 0;
	else {
		appl->id = id;
		list_add_tail(&appl->entry, &a->entry);
	}
	up(&capi_appls_list_sem);

	return appl->id;
}


static inline int
bind_capi_appl(struct capi_appl* appl)
{
	struct capi_device* dev;

	down_read(&capi_devs_list_sem);
	if (likely(add_capi_appl(appl)))
		list_for_each_entry(dev, &capi_devs_list, entry)
			register_capi_appl(appl, dev);
	up_read(&capi_devs_list_sem);

	return appl->id;
}


capinfo_0x10
capi_register(struct capi_appl* appl)
{
	if (unlikely(!appl))
		return CAPINFO_0X10_OSRESERR;

	if (unlikely(appl->params.datablklen < 128))
		return CAPINFO_0X10_LOGBLKSIZETOSMALL;

	skb_queue_head_init(&appl->msg_queue);
	appl->info = CAPINFO_0X11_NOERR;

	memset(&appl->stats, 0, sizeof appl->stats);
	spin_lock_init(&appl->stats.lock);

	memset(&appl->devs, 0, sizeof appl->devs);

	if (unlikely(!bind_capi_appl(appl)))
		return CAPINFO_0X10_TOOMANYAPPLS;

	return CAPINFO_0X10_NOERR;
}


static inline int
capi_device_listed(struct capi_device* dev)
{
	return !list_empty(&dev->entry);
}


capinfo_0x11
capi_release(struct capi_appl* appl)
{
	struct capi_device* dev;
	int i;

	down_read(&capi_devs_list_sem);
	for (i = 0; (i = find_next_bit(appl->devs, CAPI_MAX_DEVS, i)) < CAPI_MAX_DEVS; i++) {
		dev = capi_devs_table[i];
		BUG_ON(!dev);

		if (likely(capi_device_listed(dev)))
			dev->drv->capi_release(dev, appl);

		capi_device_put(dev);
	}

	down(&capi_appls_list_sem);
	list_del(&appl->entry);
	up(&capi_appls_list_sem);

	skb_queue_purge(&appl->msg_queue);
	up_read(&capi_devs_list_sem);

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

	if (unlikely(!msg || msg->len < CAPIMSG_BASELEN + 4))
		return CAPINFO_0X11_ILLCMDORMSGTOSMALL;

	id = CAPIMSG_CONTROLLER(msg->data);
	if (unlikely(!(id && id <= CAPI_MAX_DEVS && test_bit(id - 1, appl->devs))))
		return CAPINFO_0X11_OSRESERR;

	dev = capi_devs_table[id - 1];
	BUG_ON(!dev);

	if (unlikely(!down_read_trylock(&dev->sem)))
		return CAPINFO_0X11_OSRESERR;
	info = dev->drv->capi_put_message(dev, appl, msg);
	up_read(&dev->sem);

	return info;
}


capinfo_0x11
capi_get_message(struct capi_appl* appl, struct sk_buff** msg)
{
	if (unlikely(appl->info))
		return appl->info;

	return (*msg = skb_dequeue(&appl->msg_queue)) ?
		CAPINFO_0X11_NOERR :
		CAPINFO_0X11_QUEUEEMPTY;
}


capinfo_0x11
capi_peek_message(struct capi_appl* appl)
{
	if (unlikely(appl->info))
		return appl->info;

	return skb_queue_empty(&appl->msg_queue) ?
		CAPINFO_0X11_QUEUEEMPTY :
		CAPINFO_0X11_NOERR;
}


capinfo_0x11
capi_isinstalled(void)
{
	return atomic_read(&nr_capi_devs) ?
		CAPINFO_0X11_NOERR :
		CAPINFO_0X11_NOTINSTALLED;
}


static struct capi_device*
try_get_capi_device_by_id(int id)
{
	struct capi_device* dev;

	if (id < 1 || id > CAPI_MAX_DEVS)
		return NULL;

	spin_lock_bh(&capi_devs_table_lock);
	dev = capi_devs_table[id - 1];
	if (dev && !down_read_trylock(&dev->sem))
		dev = NULL;
	spin_unlock_bh(&capi_devs_table_lock);

	return dev;
}


u8*
capi_get_manufacturer(int id, u8 manufacturer[CAPI_MANUFACTURER_LEN])
{
	if (id) {
		struct capi_device* dev = try_get_capi_device_by_id(id);
		if (!dev)
			return NULL;

		memcpy(manufacturer, dev->manufacturer, CAPI_MANUFACTURER_LEN);
		up_read(&dev->sem);
	} else
		strlcpy(manufacturer, "NGC4Linux", CAPI_MANUFACTURER_LEN);

	return manufacturer;
}


u8*
capi_get_serial_number(int id, u8 serial[CAPI_SERIAL_LEN])
{
	if (id) {
		struct capi_device* dev = try_get_capi_device_by_id(id);
		if (!dev)
			return NULL;

		memcpy(serial, dev->serial, CAPI_SERIAL_LEN);
		up_read(&dev->sem);
	} else
		strlcpy(serial, "0", CAPI_SERIAL_LEN);

	return serial;
}


struct capi_version*
capi_get_version(int id, struct capi_version* version)
{
	static struct capi_version capicore_version = { 2, 0, 0, 0 };

	if (id) {
		struct capi_device* dev = try_get_capi_device_by_id(id);
		if (!dev)
			return NULL;

		*version = dev->version;
		up_read(&dev->sem);
	} else
		*version = capicore_version;

	return version;
}


capinfo_0x11
capi_get_profile(int id, struct capi_profile* profile)
{
	if (id) {
		struct capi_device* dev = try_get_capi_device_by_id(id);
		if (!dev)
			return CAPINFO_0X11_OSRESERR;

		*profile = dev->profile;
		profile->ncontroller = atomic_read(&nr_capi_devs);
		up_read(&dev->sem);
	} else
		profile->ncontroller = atomic_read(&nr_capi_devs);

	return CAPINFO_0X11_NOERR;
}


u8*
capi_get_product(int id, u8 product[CAPI_PRODUCT_LEN])
{
	if (id) {
		struct capi_device* dev = try_get_capi_device_by_id(id);
		if (!dev)
			return NULL;

		memcpy(product, dev->product, CAPI_PRODUCT_LEN);
		up_read(&dev->sem);
	} else
		return NULL;

	return product;
}


static int __init
capicore_init(void)
{
	int	capi_register_proc	(void);

	int res = capi_register_proc();
	if (unlikely(res))
		return res;

	res = class_register(&capi_class);
	if (unlikely(!res))
		pr_info("capicore: $Revision$\n");

	return res;
}


static void __exit
capicore_exit(void)
{
	void	capi_unregister_proc	(void);

	class_unregister(&capi_class);
	capi_unregister_proc();

	pr_info("capicore: unloaded\n");
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
EXPORT_SYMBOL(capi_get_serial_number);
EXPORT_SYMBOL(capi_get_version);
EXPORT_SYMBOL(capi_get_profile);
EXPORT_SYMBOL(capi_get_product);
