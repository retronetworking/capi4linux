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


#ifndef _CAPIDEVICE_H
#define _CAPIDEVICE_H


#ifdef __KERNEL__
#include <linux/isdn/capiappl.h>
#include <linux/device.h>
#include <linux/kref.h>


#if 1
#define CAPICOREDBG(format, arg...) do { printk(KERN_DEBUG "capicore: %s(): " format "\n", __FUNCTION__ , ## arg); } while (0)
#else
#define CAPICOREDBG(format, arg...)
#endif


struct capi_device;


/**
 *	struct capi_driver - device operations structure
 *	@capi_register:		register an application
 *	@capi_release:		remove an application
 *	@capi_put_message:	transfer a message
 *
 *	The device-driver must provide three functions for calling by the
 *	capicore.  This enables the capicore to register and remove
 *	applications with devices, and transferring messages to them.
 *
 *	The device-driver must ensure that by the time @capi_release returns
 *	for an application, no thread will be executing in a call from the
 *	device-driver to any of the functions capi_appl_enqueue_message(),
 *	capi_appl_signal(), or capi_appl_signal_error() for that application.
 *
 *	If @capi_put_message can not accept messages due to flow-control or
 *	busy reasons, it has the option of rejecting the message by either
 *	returning %CAPINFO_0X11_QUEUEFULL or %CAPINFO_0X11_BUSY, respectively.
 *	In this case, the device-driver should call capi_appl_signal() at a
 *	later time, when it can accept messages again.
 *
 *	While @capi_register and @capi_release are called from process context
 *	and may block, @capi_put_message is called from bottom-half context and
 *	may not block.  All three functions must be reentrant.
 */
struct capi_driver {
	capinfo_0x10_t	(*capi_register)	(struct capi_device* dev, struct capi_appl* appl);
	void		(*capi_release)		(struct capi_device* dev, struct capi_appl* appl);
	capinfo_0x11_t	(*capi_put_message)	(struct capi_device* dev, struct capi_appl* appl, struct sk_buff* msg);
};


/**
 *	struct capi_device - structure representing a device instance
 *	@id:		number
 *	@product:	name
 *	@manufacturer:	manufacturer
 *	@serial:	serial number
 *	@version:	version
 *	@profile:	capabilities
 *	@drv:		operations
 *	@stats:		I/O statistics
 *	@class_dev:	class device
 *
 *	The device-driver is responsible for updating the device
 *	I/O statistics, and may freely use the @stats.lock.
 */
struct capi_device {
	unsigned short		id;

	u8			product[CAPI_PRODUCT_LEN];
	u8			manufacturer[CAPI_MANUFACTURER_LEN];
	u8			serial[CAPI_SERIAL_LEN];
	struct capi_version	version;
	struct capi_profile	profile;

	struct capi_driver*	drv;
	struct rw_semaphore	sem;  /* If readable, the device is registered. */

	struct capi_stats	stats;

	struct class_device	class_dev;

	struct list_head	entry;
};


struct capi_device*	capi_device_alloc	(void);
int			capi_device_register	(struct capi_device* dev);
void			capi_device_unregister	(struct capi_device* dev);


/**
 *	capi_device_get - increment the reference counter for a device
 *	@dev:		device
 *
 *	Context: !in_irq()
 */
static inline struct capi_device*
capi_device_get(struct capi_device* dev)
{
	if (unlikely(!dev))
		return NULL;

	class_device_get(&dev->class_dev);

	return dev;
}


/**
 *	capi_device_put - decrement the reference counter for a device
 *	@dev:		device
 *
 *	Context: !in_irq()
 *
 *	Decrement the reference counter for device, and if zero, free the
 *	device structure.
 */
static inline void
capi_device_put(struct capi_device* dev)
{
	class_device_put(&dev->class_dev);
}


/**
 *	capi_device_set_devdata - set the private data pointer
 *	@dev:		device
 *	@data:		private data
 */
static inline void
capi_device_set_devdata(struct capi_device* dev, void* data)
{
	class_set_devdata(&dev->class_dev, data);
}


/**
 *	capi_device_get_devdata - return the private data pointer
 *	@dev:		device
 */
static inline void*
capi_device_get_devdata(struct capi_device* dev)
{
	return class_get_devdata(&dev->class_dev);
}


/**
 *	capi_device_set_dev - set the device pointer
 *	@capi_dev:	device
 *	@dev:		device
 */
static inline void
capi_device_set_dev(struct capi_device* capi_dev, struct device* dev)
{
	capi_dev->class_dev.dev = dev;
}


/**
 *	capi_device_get_dev - return the device pointer
 *	@capi_dev:	device
 */
static inline struct device*
capi_device_get_dev(const struct capi_device* capi_dev)
{
	return capi_dev->class_dev.dev;
}


/**
 *	to_capi_device - cast to device structure
 *	@cd:		class device
 */
static inline struct capi_device*
to_capi_device(struct class_device* cd)
{
	return container_of(cd, struct capi_device, class_dev);
}
extern struct class capi_class;


/**
 *	capi_appl_signal - wakeup an application
 *	@appl:		application
 *
 *	Context: in_irq()
 *
 *	The device-driver should wakeup the application after either
 *	enqueuing messages, or clearing a queue-fulf/busy condition.
 *	The device-driver may batch messages.
 */
static inline void
capi_appl_signal(struct capi_appl* appl)
{
	appl->sig(appl, appl->sig_param);
}


/**
 *	capi_appl_enqueue_message - add a message to the application queue
 *	@appl:		application
 *	@msg:		message
 *
 *	Context: in_irq()
 *
 *	The application queue has unbounded capacity, and the device-driver
 *	must adhere to the CAPI data window protocol in order to prevent the
 *	queue from growing immensely.  If the device-driver needs to drop a
 *	messages, it should signal this event to the corresponding application.
 *
 *	In the case of a data transfer message (DATA_B3_IND), the data must be
 *	appended to the message (this is contrary to the CAPI standard which
 *	intends a shared buffer scheme) and the Data field will be ignored.
 */
static inline void
capi_appl_enqueue_message(struct capi_appl* appl, struct sk_buff* msg)
{
	skb_queue_tail(&appl->msg_queue, msg);
}


/**
 *	capi_appl_signal_error - inform an application about an error
 *	@appl:		application
 *	@info:		error
 *
 *	Context: in_irq()
 *
 *	Signal a non-recoverable message exchange error to the application.
 *	This usually implies that messages have been lost, and the only recovery
 *	for the application is to do a release.
 */
static inline void
capi_appl_signal_error(struct capi_appl* appl, capinfo_0x11_t info)
{
	if (likely(!appl->info))
		appl->info = info;

	capi_appl_signal(appl);
}
#endif	/* __KERNEL__ */


#endif	/* _CAPIDEVICE_H */
