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


#ifndef CAPIAPPL_H
#define CAPIAPPL_H


#ifdef __KERNEL__
#include <linux/capi.h>
#include <linux/kref.h>
#include <linux/wait.h>
#include <linux/skbuff.h>
#include <linux/config.h>


#define CAPI_MAX_DEVS		CONFIG_CAPI_MAX_DEVS
#define CAPI_MAX_APPLS		CONFIG_CAPI_MAX_APPLS


struct capi_appl;


typedef enum {
	CAPINFO_0X10_NOERR			= 0x0000,
	CAPINFO_0X10_TOOMANYAPPLS		= 0x1001,
	CAPINFO_0X10_LOGBLKSIZETOSMALL		= 0x1002,
	CAPINFO_0X10_BUFEXCEEDS64K		= 0x1003,
	CAPINFO_0X10_MSGBUFSIZETOOSMALL		= 0x1004,
	CAPINFO_0X10_MAXNUMOFLOGCONNNOTSUPP	= 0x1005,
	CAPINFO_0X10_RESERVED			= 0x1006,
	CAPINFO_0X10_BUSY			= 0x1007,
	CAPINFO_0X10_OSRESERR			= 0x1008,
	CAPINFO_0X10_NOTINSTALLED		= 0x1009,
	CAPINFO_0X10_CTRLDOESNOTSUPPEXTEQUIP	= 0x100a,
	CAPINFO_0X10_CTRLDOESONLYSUPPEXTEQUIP	= 0x100b
} capinfo_0x10;


typedef enum {
	CAPINFO_0X11_NOERR			= 0x0000,
	CAPINFO_0X11_ILLAPPNR			= 0x1101,
	CAPINFO_0X11_ILLCMDORMSGTOSMALL		= 0x1102,
	CAPINFO_0X11_QUEUEFULL			= 0x1103,
	CAPINFO_0X11_QUEUEEMPTY			= 0x1104,
	CAPINFO_0X11_QUEUEOVERFLOW		= 0x1105,
	CAPINFO_0X11_ILLNOTIFICATIONPARAM	= 0x1106,
	CAPINFO_0X11_BUSY			= 0x1107,
	CAPINFO_0X11_OSRESERR			= 0x1108,
	CAPINFO_0X11_NOTINSTALLED		= 0x1109,
	CAPINFO_0X11_CTRLDOESNOTSUPPEXTEQUIP	= 0x110a,
	CAPINFO_0X11_CTRLDOESONLYSUPPEXTEQUIP	= 0x110b
} capinfo_0x11;


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


#endif	/* CAPIAPPL_H */
