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
 *	@capi_register:		callback function registering an application
 *	@capi_release:		callback function removing an application
 *	@capi_put_message:	callback function transferring a message
 *
 *	The device driver must provide three functions for calling by the
 *	capicore via this structure, enabling the registration and removal of
 *	applications with the device driver's devices and the transfer of
 *	messages to them.
 *
 *	The device driver must ensure that by the time a call to the callback
 *	function @capi_release returns, no thread is and will be executing,
 *	in the context of @dev, in a call to any of these functions
 *	capi_appl_signal_error(), capi_appl_enqueue_message(), or
 *	capi_appl_signal() for @appl.
 *
 *	The device driver has the option of rejecting @msg by either returning
 *	%CAPINFO_0X11_QUEUEFULL or %CAPINFO_0X11_BUSY from the callback function
 *	@capi_put_message.  In this case, @appl could block waiting for the
 *	clearance of the pending condition, and the device driver must call the
 *	function capi_appl_signal() for @appl either when the device driver can
 *	accept messages again for @dev or when the device driver has enqueued
 *	any message for @appl, whichever happens first.
 *
 *	While the callback functions @capi_register and @capi_release are called
 *	from process context and may block (but mustn't be slow, i.e., blocking
 *	indefinitely), @capi_put_message is called from bottom half context and
 *	must not block.  All three callback functions must be reentrant.
 */
struct capi_driver {
	capinfo_0x10_t	(*capi_register)	(struct capi_device* dev, struct capi_appl* appl);
	void		(*capi_release)		(struct capi_device* dev, struct capi_appl* appl);
	capinfo_0x11_t	(*capi_put_message)	(struct capi_device* dev, struct capi_appl* appl, struct sk_buff* msg);
};


/**
 *	struct capi_device - device control structure
 *	@id:		device number
 *	@product:	device name
 *	@manufacturer:	manufacturer
 *	@serial:	serial number
 *	@version:	version
 *	@profile:	capabilities
 *	@drv:		operations
 *	@stats:		I/O statistics
 *	@class_dev:	class device
 *
 *	The device driver is responsible for updating the device's
 *	I/O statistics, and may freely use the @stats.lock.
 *
 *	More fields are present, but not documented, since they are
 *	not part of the public interface.
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
 *	capi_device_get - get another reference to a device
 *	@dev:		device
 *
 *	Context: !in_irq()
 *
 *	Get another reference to @dev by incrementing its reference counter
 *	by one.  Return @dev.
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
 *	capi_device_put - release a reference to a device
 *	@dev:		device
 *
 *	Context: !in_irq()
 *
 *	Release a reference to @dev by decrementing its reference counter
 *	by one and, if zero, free @dev.
 */
static inline void
capi_device_put(struct capi_device* dev)
{
	class_device_put(&dev->class_dev);
}


/**
 *	capi_device_set_devdata - set private data pointer
 *	@dev:		device
 *	@data:		private data
 */
static inline void
capi_device_set_devdata(struct capi_device* dev, void* data)
{
	class_set_devdata(&dev->class_dev, data);
}


/**
 *	capi_device_get_devdata - return private data pointer
 *	@dev:		device
 */
static inline void*
capi_device_get_devdata(struct capi_device* dev)
{
	return class_get_devdata(&dev->class_dev);
}


/**
 *	capi_device_set_dev - set generic device pointer
 *	@capi_dev:	device
 *	@dev:		device
 */
static inline void
capi_device_set_dev(struct capi_device* capi_dev, struct device* dev)
{
	capi_dev->class_dev.dev = dev;
}


/**
 *	capi_device_get_dev - return generic device pointer
 *	@capi_dev:	device
 */
static inline struct device*
capi_device_get_dev(const struct capi_device* capi_dev)
{
	return capi_dev->class_dev.dev;
}


/**
 *	to_capi_device - downcast to generic CAPI device control structure
 *	@cd:		class device
 */
static inline struct capi_device*
to_capi_device(struct class_device* cd)
{
	return container_of(cd, struct capi_device, class_dev);
}
extern struct class capi_class;


/**
 *	capi_appl_enqueue_message - add a message to an application queue
 *	@appl:		application
 *	@msg:		message
 *
 *	Context: in_irq()
 *
 *	The message queue of @appl has unbounded capacity; hence the device
 *	driver must adhere to the CAPI data window protocol to prevent that
 *	queue from growing immensely.  A full data window should cause the
 *	device driver to trigger flow control on the line, if supported by
 *	the line protocol; otherwise, the device driver should drop @msg and
 *	notify @appl about that condition.
 *
 *	In the case of a data transfer message (DATA_B3_IND), the data must be
 *	appended to the message (this is contrary to the CAPI standard which
 *	intends a shared buffer scheme), and the Data field will be ignored.
 */
static inline void
capi_appl_enqueue_message(struct capi_appl* appl, struct sk_buff* msg)
{
	skb_queue_tail(&appl->msg_queue, msg);
}


/**
 *	capi_appl_signal - wakeup an application
 *	@appl:		application
 *
 *	Context: in_irq()
 *
 *	@appl should be woken up either after enqueuing messages or clearing a
 *	queue-full/busy condition on @appl, respectively.
 */
static inline void
capi_appl_signal(struct capi_appl* appl)
{
	appl->sig(appl, appl->sig_param);
}


/**
 *	capi_appl_signal_error - signal an error to an application
 *	@appl:		application
 *	@info:		error
 *
 *	Context: in_irq()
 *
 *	The only recovery for @appl, from this condition, is to do a release.
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
