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


#ifndef CAPIDEVICE_H
#define CAPIDEVICE_H


#ifdef __KERNEL__
#include <linux/isdn/capiappl.h>
#include <linux/device.h>
#include <linux/kref.h>


struct capi_device;


struct capi_driver {
	struct module*	owner;

	capinfo_0x10	(*capi_register)	(struct capi_device* dev, struct capi_appl* appl);
	void		(*capi_release)		(struct capi_device* dev, struct capi_appl* appl);
	capinfo_0x11	(*capi_put_message)	(struct capi_device* dev, struct capi_appl* appl, struct sk_buff* msg);

	void 		(*release)		(struct capi_device* dev);
};


struct capi_device {
	int			id;

	u8			manufacturer[CAPI_MANUFACTURER_LEN];
	u8			serial[CAPI_SERIAL_LEN];
	struct capi_version	version;
	struct capi_profile	profile;

	struct capi_driver*	drv;
	struct kref		refs;

	struct capi_stats	stats;

	enum capi_device_state {
		CAPI_DEVICE_STATE_RUNNING,
		CAPI_DEVICE_STATE_ZOMBIE
	};
	enum capi_device_state	state;
	spinlock_t		lock;

	atomic_t		appls;

	struct class_device	class_dev;
};


struct capi_device*	capi_device_alloc	(void);


static inline void
capi_device_free(struct capi_device* dev)
{
	BUG_ON(atomic_read(&dev->refs.refcount));
	class_device_put(&dev->class_dev);
}


static inline void
capi_device_set_devdata(struct capi_device* dev, void* data)
{
	class_set_devdata(&dev->class_dev, data);
}


static inline void*
capi_device_get_devdata(struct capi_device* dev)
{
	return class_get_devdata(&dev->class_dev);
}


static inline void
capi_device_set_device(struct capi_device* capi_dev, struct device* dev)
{
	capi_dev->class_dev.dev = dev;
}


int	capi_device_register	(struct capi_device* dev);
void	capi_device_unregister	(struct capi_device* dev);


static inline struct capi_device*
to_capi_device(struct class_device* cd)
{
	return container_of(cd, struct capi_device, class_dev);
}
extern struct class capi_class;


static inline void
capi_appl_signal(struct capi_appl* appl)
{
	appl->sig(appl, appl->sig_param);		
}


static inline void
capi_appl_get_message(struct capi_appl* appl, struct sk_buff* msg)
{
	skb_queue_tail(&appl->msg_queue, msg);
}


void	capi_appl_signal_info	(struct capi_appl* appl, capinfo_0x11 info);
#endif	/* __KERNEL__ */


#endif	/* CAPIDEVICE_H */
