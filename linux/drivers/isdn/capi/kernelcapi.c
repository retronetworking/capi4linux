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
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/kernelcapi.h>
#include <linux/isdn/capidevice.h>
#include <linux/isdn/capicmd.h>
#include <linux/isdn/capiutil.h>


struct kernelcapi_appl {
	struct capi_appl	appl;

	void	(*signal)	(u16 appl, u32 param);
	u32			param;
};


static struct kernelcapi_appl* kernelcapi_appls[CAPI_MAX_APPLS];

static struct capi_interface_user* kernelcapi_iface_list;
static DECLARE_RWSEM(kernelcapi_iface_list_sem);


static u16
kernelcapi_isinstalled(void)
{
	return capi_isinstalled();
}


static u16
kernelcapi_register(capi_register_params* param, u16* applid)
{
	capinfo_0x10 info;

	struct kernelcapi_appl* a = kmalloc(sizeof *a, GFP_KERNEL);
	if (unlikely(!a))
		return CAPINFO_0X10_OSRESERR;

	a->appl.params = *param;
	capi_set_signal(&a->appl, NULL, 0);

	info = capi_register(&a->appl);
	if (unlikely(info))
		goto out;

	if (unlikely(a->appl.id - 1 >= CAPI_MAX_APPLS)) {
		info = CAPINFO_0X10_TOOMANYAPPLS;
		goto out;
	}

	kernelcapi_appls[a->appl.id - 1] = a;
	*applid = a->appl.id;

	return CAPINFO_0X10_NOERR;

 out:	kfree(a);

	return info;
}


static u16
kernelcapi_release(u16 applid)
{
	struct kernelcapi_appl* a;
	capinfo_0x11 info;

	if (unlikely(applid - 1 >= CAPI_MAX_APPLS))
		return CAPINFO_0X11_ILLAPPNR;

	a = kernelcapi_appls[applid - 1];

	info = capi_release(&a->appl);

	kernelcapi_appls[applid - 1] = NULL;
	kfree(a);

	return info;
}


static u16
kernelcapi_put_message(u16 applid, struct sk_buff* msg)
{
	struct kernelcapi_appl* a;
	capinfo_0x11 info;
	int n;

	if (unlikely(applid - 1 >= CAPI_MAX_APPLS))
		return CAPINFO_0X11_ILLAPPNR;

	n = CAPIMSG_LEN(msg->data);
	if (CAPIMSG_CMD(msg->data) == CAPI_DATA_B3_REQ)
		n += CAPIMSG_DATALEN(msg->data);

	a = kernelcapi_appls[applid - 1];

	info = capi_put_message(&a->appl, msg);
	if (!info) {
		spin_lock_bh(&a->appl.stats.lock);
		a->appl.stats.tx_packets++;
		a->appl.stats.tx_bytes += n;
		spin_unlock_bh(&a->appl.stats.lock);
	}

	return info;
}


static u16
kernelcapi_get_message(u16 applid, struct sk_buff** msg)
{
	struct kernelcapi_appl* a;
	capinfo_0x11 info;

	if (unlikely(applid - 1 >= CAPI_MAX_APPLS))
		return CAPINFO_0X11_ILLAPPNR;

	a = kernelcapi_appls[applid - 1];

	info = capi_get_message(&a->appl, msg);
	if (!info) {
		int n = CAPIMSG_LEN((*msg)->data);
		if (CAPIMSG_CMD((*msg)->data) == CAPI_DATA_B3_IND)
			n += CAPIMSG_DATALEN((*msg)->data);

		spin_lock_bh(&a->appl.stats.lock);
		a->appl.stats.rx_packets++;
		a->appl.stats.rx_bytes += n;
		spin_unlock_bh(&a->appl.stats.lock);
	}

	return info;
}


static void
kernelcapi_signal_handler(struct capi_appl* appl, unsigned long param)
{
	struct kernelcapi_appl* a = container_of(appl, struct kernelcapi_appl, appl);

	a->signal(appl->id, a->param);
}


static u16
kernelcapi_set_signal(u16 applid, void (*signal)(u16 applid, u32 param), u32 param)
{
	struct kernelcapi_appl* a;

	if (unlikely(applid - 1 >= CAPI_MAX_APPLS))
		return CAPINFO_0X11_ILLAPPNR;

	a = kernelcapi_appls[applid - 1];

	a->signal = signal;
	a->param = param;

	capi_set_signal(&a->appl, signal ? kernelcapi_signal_handler : NULL, 0);

	return CAPINFO_0X11_NOERR;
}


static u16
kernelcapi_get_version(u32 devid, struct capi_version* version)
{
	return capi_get_version(devid, version) ?
		CAPINFO_0X11_NOERR :
		CAPINFO_0X11_OSRESERR;
}


static u16
kernelcapi_get_serial(u32 devid, u8 serial[CAPI_SERIAL_LEN])
{
	return capi_get_serial_number(devid, serial) ?
		CAPINFO_0X11_NOERR :
		CAPINFO_0X11_OSRESERR;
}


static u16
kernelcapi_get_profile(u32 devid, struct capi_profile* profile)
{
	return capi_get_profile(devid, profile);
}


static u16
kernelcapi_get_manufacturer(u32 devid, u8 manufacturer[CAPI_MANUFACTURER_LEN])
{
	return capi_get_manufacturer(devid, manufacturer) ?
		CAPINFO_0X11_NOERR :
		CAPINFO_0X11_OSRESERR;
}


static int
kernelcapi_manufacturer(unsigned int cmd, void* data)
{
	return 0;
}


static struct capi_interface kernelcapi_interface = {
	capi_isinstalled:	kernelcapi_isinstalled,
	capi_register:		kernelcapi_register,
	capi_release:		kernelcapi_release,
	capi_put_message:	kernelcapi_put_message,
	capi_get_message:	kernelcapi_get_message,
	capi_set_signal:	kernelcapi_set_signal,
	capi_get_version:	kernelcapi_get_version,
	capi_get_serial:	kernelcapi_get_serial,
	capi_get_profile:	kernelcapi_get_profile,
	capi_get_manufacturer:	kernelcapi_get_manufacturer,
	capi_manufacturer:	kernelcapi_manufacturer
};


struct capi_interface*
attach_capi_interface(struct capi_interface_user* iface)
{
	if (iface->callback) {
		down_write(&kernelcapi_iface_list_sem);
		iface->next = kernelcapi_iface_list;
		kernelcapi_iface_list = iface;
		up_write(&kernelcapi_iface_list_sem);
	}

	return &kernelcapi_interface;
}


int
detach_capi_interface(struct capi_interface_user* iface)
{
	struct capi_interface_user** i;

	down_write(&kernelcapi_iface_list_sem);
	i = &kernelcapi_iface_list;
	while (*i) {
		if (*i == iface) {
			*i = iface->next;
			break;
		}

		*i = (*i)->next;
	}
	up_write(&kernelcapi_iface_list_sem);

	return 0;
}


static int
kernelcapi_devadd(struct class_device* cd)
{
	struct capi_device* dev = to_capi_device(cd);
	struct capi_interface_user* i = kernelcapi_iface_list;

	down_read(&kernelcapi_iface_list_sem);
	while (i) {
		i->callback(CAPI_INTERFACE_USER_DEVADD, dev->id, &dev->profile);
		i = i->next;
	}
	up_read(&kernelcapi_iface_list_sem);

	return 0;
}


static void
kernelcapi_devremove(struct class_device* cd)
{
	struct capi_device* dev = to_capi_device(cd);
	struct capi_interface_user* i = kernelcapi_iface_list;

	down_read(&kernelcapi_iface_list_sem);
	while (i) {
		i->callback(CAPI_INTERFACE_USER_DEVREMOVE, dev->id, NULL);
		i = i->next;
	}
	up_read(&kernelcapi_iface_list_sem);
}


static struct class_interface kernelcapi_iface = {
	.class	= &capi_class,
	.add	= kernelcapi_devadd,
	.remove	= kernelcapi_devremove
};


static int __init
kernelcapi_init(void)
{
	int res = class_interface_register(&kernelcapi_iface);
	if (unlikely(!res))
		pr_info("kernelcapi: $Revision$\n");

	return res;
}


static void __exit
kernelcapi_exit(void)
{
	class_interface_unregister(&kernelcapi_iface);

	pr_info("kernelcapi: unload\n");
}


module_init(kernelcapi_init);
module_exit(kernelcapi_exit);


MODULE_AUTHOR("Frank A. Uepping <Frank.Uepping@web.de>");
MODULE_LICENSE("GPL");


EXPORT_SYMBOL(attach_capi_interface);
EXPORT_SYMBOL(detach_capi_interface);
