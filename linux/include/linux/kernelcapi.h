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


#ifndef _KERNELCAPI_H
#define _KERNELCAPI_H


#ifdef __KERNEL__
#include <linux/capi.h>
#include <linux/skbuff.h>
#include <linux/isdn/capinfo.h>


struct capi_interface {
	u16	(*capi_isinstalled)	(void);
	u16	(*capi_register)	(capi_register_params* param, u16* applid);
	u16	(*capi_release)		(u16 applid);
	u16	(*capi_put_message)	(u16 applid, struct sk_buff* msg);
	u16	(*capi_get_message)	(u16 applid, struct sk_buff** msg);
	u16	(*capi_set_signal)	(u16 applid,

					 /* Called from interrupt context. */
					 void (*signal)(u16 applid, u32 param),

					 u32 param);
	u16	(*capi_get_version)	(u32 devid, struct capi_version* version);
	u16	(*capi_get_serial)	(u32 devid, u8 serial[CAPI_SERIAL_LEN]);
	u16	(*capi_get_profile)	(u32 devid, struct capi_profile* profile);
	u16	(*capi_get_manufacturer)(u32 devid, u8 manufacturer[CAPI_MANUFACTURER_LEN]);
	int	(*capi_manufacturer)	(unsigned int cmd, void* data);
};


struct capi_interface_user {
	char				name[20];

#define CAPI_INTERFACE_USER_DEVADD	0
#define CAPI_INTERFACE_USER_DEVREMOVE	1
	void	(*callback)		(unsigned cmd, u32 devid, void* data);

	struct capi_interface_user*	next;
};


struct capi_interface*	attach_capi_interface	(struct capi_interface_user* iface);
int			detach_capi_interface	(struct capi_interface_user* iface);
#endif  /* __KERNEL__ */

#endif  /* _KERNELCAPI_H */
