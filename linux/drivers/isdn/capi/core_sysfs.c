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


#include <linux/isdn/capidevice.h>
#include <linux/isdn/capiutil.h>


void	release_capi_device	(struct class_device* cd);


static ssize_t
show_manufacturer(struct class_device* cd, char* buf)
{
	return sprintf(buf, "%s", to_capi_device(cd)->manufacturer);
}
CLASS_DEVICE_ATTR(manufacturer, S_IRUGO, show_manufacturer, NULL);


static ssize_t
show_serial(struct class_device* cd, char* buf)
{
	return sprintf(buf, "%s", to_capi_device(cd)->serial);
}
CLASS_DEVICE_ATTR(serial, S_IRUGO, show_serial, NULL);


static ssize_t
show_version(struct class_device* cd, char* buf)
{
	struct capi_version* v = &to_capi_device(cd)->version;

	return sprintf(buf, "%d.%d.%d.%d", v->majorversion, v->minorversion, v->majormanuversion, v->minormanuversion);
}
CLASS_DEVICE_ATTR(version, S_IRUGO, show_version, NULL);


static struct class_device_attribute* attrs[] = {
	&class_device_attr_manufacturer,
	&class_device_attr_serial,
	&class_device_attr_version,
	NULL
};


static ssize_t
read_profile(struct kobject* kobj, char* buf, loff_t pos, size_t size)
{
	extern int nr_capi_devices;
	const int CAPI_PROFILE_LEN = 64;
	char b[CAPI_PROFILE_LEN];
	struct capi_profile* p;

	if (pos > CAPI_PROFILE_LEN)
		return 0;

	if (pos + size > CAPI_PROFILE_LEN)
		size = CAPI_PROFILE_LEN - pos;

	p = &to_capi_device(container_of(kobj, struct class_device, kobj))->profile;

	capimsg_setu16(b, 0, nr_capi_devices);
	capimsg_setu16(b, 2, p->nbchannel);
	memcpy(b + 4, &p->goptions, 4);
	memcpy(b + 8, &p->support1, 4);
	memcpy(b + 12, &p->support2, 4);
	memcpy(b + 16, &p->support3, 4);
	memset(b + 20, 0, 24);
	memcpy(b + 44, &p->manu, 20);

	memcpy(buf, b + pos, size);

	return size;
}


static struct bin_attribute profile_attr = {
	.attr	= { .name	= "profile",
		    .owner	= THIS_MODULE,
		    .mode	= S_IRUGO
	},
	.size	= 64,
	.read	= read_profile
};


#define STAT_ENTRY(name)						\
static ssize_t					       			\
show_##name(struct class_device* cd, char* buf)				\
{									\
	return sprintf(buf, "%lu", to_capi_device(cd)->stats.name);	\
}									\
static CLASS_DEVICE_ATTR(name, S_IRUGO, show_##name, NULL)


STAT_ENTRY(rx_bytes);
STAT_ENTRY(tx_bytes);
STAT_ENTRY(rx_packets);
STAT_ENTRY(tx_packets);
STAT_ENTRY(rx_data_bytes);
STAT_ENTRY(tx_data_bytes);
STAT_ENTRY(rx_data_packets);
STAT_ENTRY(tx_data_packets);


static struct attribute* stats_attrs[] = {
	&class_device_attr_rx_bytes.attr,
	&class_device_attr_tx_bytes.attr,
	&class_device_attr_rx_packets.attr,
	&class_device_attr_tx_packets.attr,
	&class_device_attr_rx_data_bytes.attr,
	&class_device_attr_tx_data_bytes.attr,
	&class_device_attr_rx_data_packets.attr,
	&class_device_attr_tx_data_packets.attr,
	NULL
};


static struct attribute_group stats_attrs_group = {
	.name	= "statistics",
	.attrs	= stats_attrs
};


struct class capi_class = {
	.name		= "capi",
	.release	= release_capi_device
};


int
capi_device_register_sysfs(struct capi_device* dev)
{
	struct class_device* cd = &dev->class_dev;
	int err, i;

	snprintf(cd->class_id, BUS_ID_SIZE, "%d", dev->id);

	err = class_device_add(cd);
	if (unlikely(err))
		return err;

	for (i = 0; attrs[i]; i++) {
		err = class_device_create_file(cd, attrs[i]);
		if (unlikely(err))
			goto out;
	}

	err = sysfs_create_bin_file(&cd->kobj, &profile_attr);
	if (unlikely(err))
		goto out;

	err = sysfs_create_group(&cd->kobj, &stats_attrs_group);
	if (unlikely(err))
		goto out;

	return 0;

 out:	class_device_del(cd);

	return err;
}


EXPORT_SYMBOL(capi_class);
