/*
 * Copyright 2018 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <env.h>
#include <dm.h>
#include <i2c.h>
#include <spl.h>
#include <dm/uclass.h>
#include <dm/device.h>
#include <dm/uclass-internal.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <bootm.h>
#include <asm/gpio.h>
#include <asm/arch/sci/sci.h>
#include <asm/arch/imx8-pins.h>
#include <asm/arch/iomux.h>

#include "../common/imx8_eeprom.h"
#include "imx8qm_var_som.h"

DECLARE_GLOBAL_DATA_PTR;

static struct var_eeprom eeprom = {0};

void spl_board_init(void)
{

	struct var_eeprom *ep = VAR_EEPROM_DATA;
	struct udevice *dev;

	uclass_find_first_device(UCLASS_MISC, &dev);

	for (; dev; uclass_find_next_device(&dev)) {
		if (device_probe(dev))
			continue;
	}

	board_early_init_f();

	timer_init();

	preloader_console_init();

	//if(var_eeprom_read_header(&eeprom)) {
	if(var_scu_eeprom_read_header(&eeprom)) {
		puts("Error: Reading SCU EEPROM\n");
		return;
	}

	/* Copy EEPROM contents to DRAM */
	memcpy(ep, &eeprom, sizeof(*ep));

	puts("Normal Boot\n");
}

void spl_board_prepare_for_boot(void)
{
	board_quiesce_devices();
}


int board_fit_config_name_match(const char *name)
{
	int board_id;

	if (eeprom.somrev & 0x40)
                board_id = SPEAR_MX8;
        else 
                board_id = VAR_SOM_MX8;

	if ((board_id == SPEAR_MX8) && !strcmp(name, "fsl-imx8qm-var-spear"))
		return 0;
	else if ((board_id == VAR_SOM_MX8) && !strcmp(name, "fsl-imx8qm-var-som"))
		return 0;
	else
		return -1;
}

void board_init_f(ulong dummy)
{
	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	arch_cpu_init();

	board_init_r(NULL, 0);
}
