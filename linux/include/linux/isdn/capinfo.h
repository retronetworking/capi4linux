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


#ifndef _CAPINFO_H
#define _CAPINFO_H


#include <linux/config.h>


#define CAPI_MAX_DEVS		CONFIG_CAPI_MAX_DEVS
#define CAPI_MAX_APPLS		CONFIG_CAPI_MAX_APPLS


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


#endif	/* _CAPINFO_H */
