# Makefile for the CAPI subsystem.

# Each configuration option enables a list of files.

obj-$(CONFIG_ISDN_CAPI)			+= capicore.o
obj-$(CONFIG_ISDN_CAPI_KERNELCAPI)	+= kernelcapi.o
obj-$(CONFIG_ISDN_CAPI_CAPI20)		+= capi.o
obj-$(CONFIG_ISDN_CAPI_CAPIFS)		+= capifs.o
obj-$(CONFIG_ISDN_CAPI_CAPIDRV)         += capidrv.o

# Multipart objects.

capicore-y				:= core.o core_sysfs.o core_proc.o capiutil.o
