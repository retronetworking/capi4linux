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


struct capi_appl;


struct capi_stats {
	spinlock_t		lock;

	/* Total number of received/transmitted bytes. */
	unsigned long		rx_bytes;
	unsigned long		tx_bytes;

	/* Total number of received/transmitted packets. */
	unsigned long		rx_packets;
	unsigned long		tx_packets;

	/* Number of received/transmitted payload bytes. */
	unsigned long		rx_data_bytes;
	unsigned long		tx_data_bytes;

	/* Number of received/transmitted DATA_B3 packets. */
	unsigned long		rx_data_packets;
	unsigned long		tx_data_packets;
};


typedef void	(*capi_signal_handler)	(struct capi_appl* appl, unsigned long param);


struct capi_appl {
	u16			id;

	capi_signal_handler	sig;
	unsigned long		sig_param;

	struct sk_buff_head	msg_queue;
	capinfo_0x11		info;

	unsigned long		devs[BITS_TO_LONGS(CAPI_MAX_DEVS)];

	struct capi_stats	stats;

	capi_register_params	params;

	struct list_head	entry;
};


static inline void
capi_set_signal(struct capi_appl* appl, capi_signal_handler signal, unsigned long param)
{
	if (!appl)
		return;

	appl->sig = signal;
	appl->sig_param = param;
}


static inline void
capi_unget_message(struct capi_appl* appl, struct sk_buff* msg)
{
	skb_queue_head(&appl->msg_queue, msg);
}


capinfo_0x10	capi_register		(struct capi_appl* appl);
capinfo_0x11	capi_release		(struct capi_appl* appl);
capinfo_0x11	capi_put_message	(struct capi_appl* appl, struct sk_buff* msg);
capinfo_0x11	capi_get_message	(struct capi_appl* appl, struct sk_buff** msg);
capinfo_0x11	capi_peek_message	(struct capi_appl* appl);
capinfo_0x11	capi_isinstalled	(void);

struct capi_version*	capi_get_version	(int dev, struct capi_version* version);
u8*			capi_get_serial_number	(int dev, u8 serial[CAPI_SERIAL_LEN]);
capinfo_0x11		capi_get_profile	(int dev, struct capi_profile* profile);
u8*			capi_get_manufacturer	(int dev, u8 manufacturer[CAPI_MANUFACTURER_LEN]);
#endif	/* __KERNEL__ */


#endif	/* _CAPIAPPL_H */
