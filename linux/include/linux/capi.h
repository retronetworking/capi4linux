/* $Id$
 * 
 * CAPI 2.0 Interface for Linux
 * 
 * Copyright 1997 by Carsten Paeth (calle@calle.in-berlin.de)
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef __LINUX_CAPI_H__
#define __LINUX_CAPI_H__

#include <asm/types.h>
#include <linux/ioctl.h>
#include <linux/compiler.h>

/*
 * CAPI_REGISTER
 */

/**
 *	struct capi_register_params - application parameters structure
 *	@level3cnt:	maximum number of logical connections the application
 *			can concurrently maintain
 *	@datablkcnt:	maximum number of received data blocks that can be
 *			reported to the application simultaneously for each
 *			logical connection
 *	@datablklen:	maximum size of the application data block to be
 *			transmitted and received
 */
typedef struct capi_register_params {
	__u32 level3cnt;
	__u32 datablkcnt;
	__u32 datablklen;
} capi_register_params;

#define	CAPI_REGISTER	_IOW('C',0x01,struct capi_register_params)

/*
 * CAPI_GET_MANUFACTURER
 */

#define CAPI_MANUFACTURER_LEN		64

#define	CAPI_GET_MANUFACTURER	_IOWR('C',0x06,int)	/* broken: wanted size 64 (CAPI_MANUFACTURER_LEN) */

/*
 * CAPI_GET_VERSION
 */

/**
 *	struct capi_version - version structure
 *	@majorversion:		major CAPI version
 *	@minorversion:		minor CAPI version
 *	@majormanuversion:	major manufacturer specific version
 *	@minormanuversion:	minor manufacturer specific version
 */
typedef struct capi_version {
	__u32 majorversion;
	__u32 minorversion;
	__u32 majormanuversion;
	__u32 minormanuversion;
} capi_version;

#define CAPI_GET_VERSION	_IOWR('C',0x07,struct capi_version)

/*
 * CAPI_GET_SERIAL
 */

#define CAPI_SERIAL_LEN		8
#define CAPI_GET_SERIAL		_IOWR('C',0x08,int)	/* broken: wanted size 8 (CAPI_SERIAL_LEN) */

/*
 * CAPI_GET_PROFILE
 */

/**
 *	struct capi_profile - device capabilities structure
 *	@ncontroller:	number of devices
 *	@nbchannel:	number of B-Channels
 *	@goptions:	global options
 *	@support1:	B1 protocols
 *	@support2:	B2 protocols
 *	@support3:	B3 protocols
 *	@reserved:	reserved
 *	@manu:		manufacturer specific information
 *
 *	For more information about the individual bit-fields,
 *	see the CAPI standard.
 */
typedef struct capi_profile {
	__u16 ncontroller;
	__u16 nbchannel;
	__u32 goptions;
	__u32 support1;
	__u32 support2;
	__u32 support3;
	__u32 reserved[6];
	__u32 manu[5];
} capi_profile;

#define CAPI_GET_PROFILE	_IOWR('C',0x09,struct capi_profile)

typedef struct capi_manufacturer_cmd {
	unsigned long cmd;
	void __user *data;
} capi_manufacturer_cmd;

/*
 * CAPI_MANUFACTURER_CMD
 */

#define CAPI_MANUFACTURER_CMD	_IOWR('C',0x20, struct capi_manufacturer_cmd)

/*
 * CAPI_GET_ERRCODE
 * capi errcode is set, * if read, write, or ioctl returns EIO,
 * ioctl returns errcode directly, and in arg, if != 0
 */

#define CAPI_GET_ERRCODE	_IOR('C',0x21, __u16)

/*
 * CAPI_INSTALLED
 */
#define CAPI_INSTALLED		_IOR('C',0x22, __u16)


/*
 * member contr is input for
 * CAPI_GET_MANUFACTURER, CAPI_VERSION, CAPI_GET_SERIAL
 * and CAPI_GET_PROFILE
 */
typedef union capi_ioctl_struct {
	__u32 contr;
	capi_register_params rparams;
	__u8 manufacturer[CAPI_MANUFACTURER_LEN];
	capi_version version;
	__u8 serial[CAPI_SERIAL_LEN];
	capi_profile profile;
	capi_manufacturer_cmd cmd;
	__u16 errcode;
} capi_ioctl_struct;

/*
 * Middleware extension
 */

#define CAPIFLAG_HIGHJACKING	0x0001

#define CAPI_GET_FLAGS		_IOR('C',0x23, unsigned)
#define CAPI_SET_FLAGS		_IOR('C',0x24, unsigned)
#define CAPI_CLR_FLAGS		_IOR('C',0x25, unsigned)

#define CAPI_NCCI_OPENCOUNT	_IOR('C',0x26, unsigned)

#define CAPI_NCCI_GETUNIT	_IOR('C',0x27, unsigned)

#endif				/* __LINUX_CAPI_H__ */
