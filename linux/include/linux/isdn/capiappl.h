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


#ifndef _CAPIAPPL_H
#define _CAPIAPPL_H


#ifdef __KERNEL__
#include <linux/capi.h>
#include <linux/wait.h>
#include <linux/skbuff.h>
#include <linux/isdn/capinfo.h>


#define CAPI_PRODUCT_LEN	KOBJ_NAME_LEN


struct capi_appl;


/**
 *	struct capi_stats - I/O statistics structure
 *	@lock:		spinlock
 *	@rx_bytes:	received bytes
 *	@tx_bytes:	transmitted bytes
 *	@rx_packets:	received messages
 *	@tx_packets:	transmitted messages
 */
struct capi_stats {
	spinlock_t		lock;

	unsigned long		rx_bytes;
	unsigned long		tx_bytes;

	unsigned long		rx_packets;
	unsigned long		tx_packets;
};


typedef void	(*capi_signal_handler_t)	(struct capi_appl* appl, unsigned long param);


/**
 *	struct capi_appl - application control structure
 *	@id:		application number
 *	@stats:		I/O statistics
 *	@params:	parameters
 *	@data:		private data
 *
 *	The application is responsible for updating the application's
 *	I/O statistics, and may freely use the @stats.lock.
 *
 *	More fields are present, but not documented, since they are
 *	not part of the public interface.
 */
struct capi_appl {
	u16				id;

	capi_signal_handler_t		sig;
	unsigned long			sig_param;

	struct sk_buff_head		msg_queue;
	capinfo_0x11_t			info;

	unsigned long			devs[BITS_TO_LONGS(CAPI_MAX_DEVS)];

	struct capi_stats		stats;

	struct capi_register_params	params;
	void*				data;

	struct list_head		entry;
};


/**
 *	capi_set_signal - install a signal handler
 *	@appl:		application
 *	@signal:	signal handler (in_irq() context)
 *	@param:		parameter to signal handler
 *
 *	The capicore ensures that by the time a call to the function
 *	capi_release() returns for @appl, no thread is and will be executing
 *	in a call from the capicore to @signal for @appl.
 *
 *	The signal handler informs the application of @appl about new messages,
 *	errors, or cleared queue-full/busy conditions.  The signal handler is
 *	not meant to be a workhorse, but just a mechanism for waking up and
 *	scheduling applications.
 *
 *	The signal handler must be reentrant and will be called from in_irq()
 *	context; consequently, it should be as simple and fast as possible.
 *
 *	The application must install a signal handler for @appl, and must not
 *	reset it once registered.
 */
static inline void
capi_set_signal(struct capi_appl* appl, capi_signal_handler_t signal, unsigned long param)
{
	if (!appl)
		return;

	appl->sig = signal;
	appl->sig_param = param;
}


/**
 *	capi_get_message - fetch a message from an application queue
 *	@appl:		application
 *	@msg:		message
 *
 *	Context: !in_irq()
 *
 *	Fetch a message from the message queue of @appl.  Upon successfully
 *	fetching a message, %CAPINFO_0X11_NOERR is returned and a pointer to
 *	the message is passed via @msg.  If there wasn't a message pending,
 *	return immediately passing %CAPINFO_0X11_QUEUEEMPTY as return value.
 *	Otherwise, a value indicating an error is returned.
 *
 *	In the case of a data transfer message (DATA_B3_IND), the data is
 *	appended to the message (this is contrary to the CAPI standard which
 *	intends a shared buffer scheme), and the Data field is undefined.
 */
static inline capinfo_0x11_t
capi_get_message(struct capi_appl* appl, struct sk_buff** msg)
{
	if (unlikely(appl->info))
		return appl->info;

	return (*msg = skb_dequeue(&appl->msg_queue)) ?
		CAPINFO_0X11_NOERR :
		CAPINFO_0X11_QUEUEEMPTY;
}


/**
 *	capi_unget_message - reinsert a message to an application queue
 *	@appl:		application
 *	@msg:		message
 *
 *	Context: !in_irq()
 *
 *	@msg is placed at the head of the message queue of @appl, so that
 *	it will be fetched next.
 */
static inline void
capi_unget_message(struct capi_appl* appl, struct sk_buff* msg)
{
	skb_queue_head(&appl->msg_queue, msg);
}


/**
 *	capi_peek_message - check whether a message is pending
 *	@appl:		application
 *
 *	Context: !in_irq()
 *
 *	If a message is pending on the message queue of @appl,
 *	%CAPINFO_0X11_NOERR is returned.  If not, %CAPINFO_0X11_QUEUEEMPTY
 *	is returned.  Otherwise, a value indicating an error is returned.
 */
static inline capinfo_0x11_t
capi_peek_message(struct capi_appl* appl)
{
	if (unlikely(appl->info))
		return appl->info;

	return skb_queue_empty(&appl->msg_queue) ?
		CAPINFO_0X11_QUEUEEMPTY :
		CAPINFO_0X11_NOERR;
}


capinfo_0x10_t	capi_register		(struct capi_appl* appl);
capinfo_0x11_t	capi_release		(struct capi_appl* appl);
capinfo_0x11_t	capi_put_message	(struct capi_appl* appl, struct sk_buff* msg);
capinfo_0x11_t	capi_isinstalled	(void);

u8*			capi_get_manufacturer	(int id, u8 manufacturer[CAPI_MANUFACTURER_LEN]);
u8*			capi_get_serial_number	(int id, u8 serial[CAPI_SERIAL_LEN]);
struct capi_version*	capi_get_version	(int id, struct capi_version* version);
capinfo_0x11_t		capi_get_profile	(int id, struct capi_profile* profile);
u8*			capi_get_product	(int id, u8 product[CAPI_PRODUCT_LEN]);
#endif	/* __KERNEL__ */


#endif	/* _CAPIAPPL_H */
