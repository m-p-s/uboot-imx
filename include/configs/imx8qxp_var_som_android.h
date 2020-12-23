/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2020 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 * Copyright 2019-2020 Variscite Ltd.
 */

#ifndef IMX8QXP_VAR_SOM_ANDROID_H
#define IMX8QXP_VAR_SOM_ANDROID_H

#define CONFIG_USBD_HS

#define CONFIG_CMD_READ
#define CONFIG_USB_GADGET_VBUS_DRAW	2

#define CONFIG_ANDROID_AB_SUPPORT
#ifdef CONFIG_ANDROID_AB_SUPPORT
#define CONFIG_SYSTEM_RAMDISK_SUPPORT
#endif
#define FSL_FASTBOOT_FB_DEV "mmc"


#define CONFIG_SERIAL_TAG

#undef CONFIG_EXTRA_ENV_SETTINGS
#undef CONFIG_BOOTCOMMAND

#define CONFIG_EXTRA_ENV_SETTINGS					\
	"splashpos=m,m\0"	  \
	"fdt_high=0xffffffffffffffff\0"	  \
	"initrd_high=0xffffffffffffffff\0" \
	"panel=NULL\0" \
	"bootargs=" \
		"console=ttyLP3,115200 " \
		"init=/init " \
		"androidboot.console=ttyLP3 " \
		"consoleblank=0 " \
		"androidboot.hardware=freescale " \
		"cma=1184M@0x960M-0xe00M " \
		"firmware_class.path=/vendor/firmware " \
		"loop.max_part=7 " \
		"androidboot.fbTileSupport=enable " \
		"androidboot.primary_display=imx-drm " \
		"androidboot.wificountrycode=US " \
		"transparent_hugepage=never\0"

#define AVB_AB_I_UNDERSTAND_LIBAVB_AB_IS_DEPRECATED

#endif /* IMX8QXP_VAR_SOM_ANDROID_H */
