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


#ifndef CORE_H
#define CORE_H


#include <linux/isdn/capidevice.h>


#if 1
#define CAPICORE_DEBUG(format, arg...) do { printk(KERN_DEBUG "capicore: %s(): " format "\n", __FUNCTION__ , ## arg); } while (0)
#else
#define CAPICORE_DEBUG(format, arg...)
#endif


int	capi_device_register_sysfs	(struct capi_device* dev);
void	release_capi_device		(struct class_device* cd);


static inline int nr_capi_devices(void)
{
	extern int __nr_capi_devices;
	return __nr_capi_devices;
}


#endif  /* CORE_H */
