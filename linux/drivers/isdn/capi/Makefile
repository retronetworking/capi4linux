# Makefile for the CAPI subsystem.

# Each configuration option enables a list of files.

obj-$(CONFIG_ISDN_CAPI)			+= capicore.o
obj-$(CONFIG_ISDN_CAPI_KERNELCAPI)	+= kernelcapi.o

# Multipart objects.

capicore-y				:= core.o core_sysfs.o core_proc.o
