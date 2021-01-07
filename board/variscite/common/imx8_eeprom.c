/*
 * Copyright (C) 2018-2019 Variscite Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <dm.h>
#include <i2c.h>
#include <asm/io.h>
#include <u-boot/crc.h>

#ifdef CONFIG_ARCH_IMX8M
#include <asm/arch/ddr.h>
#endif

#ifdef CONFIG_ARCH_IMX8
#include <asm/mach-imx/sci/sci.h>
#endif

#include "imx8_eeprom.h"

#ifdef CONFIG_ARCH_IMX8

DECLARE_GLOBAL_DATA_PTR;

#define CTL_CODE(function, method) ((4 << 16) | ((function) << 2) | (method))

#define METHOD_BUFFERED		0
#define METHOD_NEITHER		3

#define SOMINFO_READ_EEPROM	CTL_CODE(2100, METHOD_BUFFERED)
#define SOMINFO_WRITE_EEPROM	CTL_CODE(2101, METHOD_BUFFERED)

static int var_scu_eeprom_read(uint8_t *buf, uint32_t size)
{
	uint32_t command;
	sc_err_t ret = 0;
	sc_ipc_t ipc_handle;

	/* Open IPC channel */
	ret = sc_ipc_open(&ipc_handle, SC_IPC_CH);
	if (ret != SC_ERR_NONE) {
		printf("sc_ipc_open failed\n");
		return -EPERM;
	}

	command = SOMINFO_READ_EEPROM;

	/* Send command to SC firmware */
	memset(buf, 0, size);
	flush_dcache_all();
	invalidate_icache_all();
	ret = sc_misc_board_ioctl(ipc_handle, &command, (uint32_t *)&buf, &size);

	/* Close IPC channel */
	sc_ipc_close(ipc_handle);

	flush_dcache_all();
	invalidate_icache_all();

	return ret;
}

int var_scu_eeprom_read_header(struct var_eeprom *e)
{
	int ret;

	ret = var_scu_eeprom_read((uint8_t *)e, sizeof(struct var_eeprom));
	if (ret) {
		debug("SCU EEPROM read failed\n");
		return ret;
	}

	return 0;
}
#endif

#ifdef CONFIG_DM_I2C
static struct udevice *var_eeprom_init(void)
{
	int ret;
	struct udevice *bus, *dev;

	ret = uclass_get_device_by_seq(UCLASS_I2C, VAR_EEPROM_I2C_BUS, &bus);
	if (ret) {
		debug("%s: No bus %d\n", __func__, VAR_EEPROM_I2C_BUS);
		return NULL;
	}

	ret = dm_i2c_probe(bus, VAR_EEPROM_I2C_ADDR, 0, &dev);
	if (ret) {
		debug("%s: Can't find device id=0x%x, on bus %d\n",
			__func__, VAR_EEPROM_I2C_ADDR, VAR_EEPROM_I2C_BUS);
		return NULL;
	}

	return dev;
}

int var_eeprom_read_header(struct var_eeprom *e)
{
	int ret;
	struct udevice *edev;

	edev = var_eeprom_init();
	if (!edev) {
#ifdef CONFIG_ARCH_IMX8
		debug("var_eeprom_read_header: calling SCU to read EEPROM\n");
		return var_scu_eeprom_read_header(e);
#else
		debug("var_eeprom_read_header: I2C init failed\n");
		return -1;
#endif
	}

	/* Read EEPROM to memory */
	ret = dm_i2c_read(edev, 0, (void *)e, sizeof(*e));
	if (ret) {
		debug("EEPROM read failed, ret=%d\n", ret);
		return ret;
	}

	return 0;
}
#else
int var_eeprom_read_header(struct var_eeprom *e)
{
	int ret;

	/* Probe EEPROM */
	i2c_set_bus_num(VAR_EEPROM_I2C_BUS);
	ret = i2c_probe(VAR_EEPROM_I2C_ADDR);
	if (ret) {
#ifdef CONFIG_ARCH_IMX8
		debug("var_eeprom_read_header: calling SCU to read EEPROM\n");
		return var_scu_eeprom_read_header(e);
#else
		printf("EEPROM init failed\n");
		return -1;
#endif
	}

	/* Read EEPROM header to memory */
	ret = i2c_read(VAR_EEPROM_I2C_ADDR, 0, 1, (uint8_t *)e, sizeof(*e));
	if (ret) {
		printf("EEPROM read failed ret=%d\n", ret);
		return -1;
	}

	return 0;
}
#endif

int var_eeprom_get_mac(struct var_eeprom *e, u8 *buf)
{
	if (!var_eeprom_is_valid(e))
		return -1;

	memcpy(buf, e->mac, sizeof(e->mac));

	return 0;
}

int var_eeprom_get_dram_size(struct var_eeprom *e, u32 *size)
{
	u8 dramsize;

	if (!var_eeprom_is_valid(e))
		return -1;

	memcpy(&dramsize, (void *)&e->dramsize, sizeof(e->dramsize));

	if (e->version == 1)
		*size = dramsize * 1024;
	else
		*size = dramsize * 128;

	return 0;
}

#ifndef CONFIG_SPL_BUILD
void var_eeprom_print_prod_info(struct var_eeprom *e)
{
	u8 partnum[8] = {0};

	if (!var_eeprom_is_valid(e))
		return;

	memcpy(partnum, e->partnum, sizeof(e->partnum));

	/* Read second part of P/N  */
	if (e->version >= 3)
		memcpy(partnum + sizeof(e->partnum), e->partnum2, sizeof(e->partnum2));

#ifdef CONFIG_TARGET_IMX8MQ_VAR_DART
	printf("\nPart number: VSM-DT8M-%.*s\n", (int)sizeof(partnum), partnum);
#elif CONFIG_TARGET_IMX8MM_VAR_DART
	if (of_machine_is_compatible("variscite,imx8mm-var-dart"))
		printf("\nPart number: VSM-DT8MM-%.*s\n", (int)sizeof(partnum), partnum);
	else
		printf("\nPart number: VSM-VS8MM-%.*s\n", (int)sizeof(partnum), partnum);
#elif CONFIG_TARGET_IMX8QXP_VAR_SOM
	printf("\nPart number: VSM-MX8X-%.*s\n", (int)sizeof(partnum), partnum);
#elif CONFIG_TARGET_IMX8QM_VAR_SOM
	if (of_machine_is_compatible("variscite,imx8qm-var-spear"))
		printf("\nPart number: VSM-SP8-%.*s\n", (int)sizeof(partnum), partnum);
	else
		printf("\nPart number: VSM-MX8-%.*s\n", (int)sizeof(partnum), partnum);
#endif

	printf("Assembly: AS%.*s\n", (int)sizeof(e->assembly), (char *)e->assembly);

	printf("Production date: %.*s %.*s %.*s\n",
			4, /* YYYY */
			(char *)e->date,
			3, /* MMM */
			((char *)e->date) + 4,
			2, /* DD */
			((char *)e->date) + 4 + 3);

	printf("Serial Number: %02x:%02x:%02x:%02x:%02x:%02x\n",
		e->mac[0], e->mac[1], e->mac[2], e->mac[3], e->mac[4], e->mac[5]);

	debug("EEPROM version: 0x%x\n", e->version);
	debug("SOM features: 0x%x\n", e->features);
	if (e->version == 1)
		debug("DRAM size: %d GiB\n\n", e->dramsize);
	else
		debug("DRAM size: %d GiB\n\n", (e->dramsize * 128) / 1024);
}
#endif

#if defined(CONFIG_ARCH_IMX8M) && defined(CONFIG_SPL_BUILD)
/*
 * Modify DRAM table based on adjustment table in EEPROM
 *
 * Assumption: register addresses in the adjustment table
 * follow the order of register addresses in the original table
 *
 * @adj_table_offset - offset of adjustment table from start of EEPROM
 * @adj_table_size   - number of rows in adjustment table
 * @table            - pointer to DDR table
 * @table_size       - number of rows in DDR table
 */
static void adjust_dram_table(u8 adj_table_offset, u8 adj_table_size,
				struct dram_cfg_param *table, u8 table_size)
{
	int i, j = 0;
	u8 off = adj_table_offset;
	struct dram_cfg_param adj_table_row;

	/* Iterate over adjustment table */
	for (i = 0; i < adj_table_size; i++) {

		/* Read next entry from adjustment table */
		i2c_read(VAR_EEPROM_I2C_ADDR, off, 1,
			(uint8_t *)&adj_table_row, sizeof(adj_table_row));

		/* Iterate over DDR table and adjust it */
		for (; j < table_size; j++) {
			if (table[j].reg == adj_table_row.reg) {
				debug("Adjusting reg=0x%x val=0x%x\n",
					adj_table_row.reg, adj_table_row.val);
				table[j].val = adj_table_row.val;
				break;
			}
		}

		off += sizeof(adj_table_row);
	}
}

/*
 * Modify DRAM tables based on adjustment tables in EEPROM
 *
 * @e - pointer to EEPROM header structure
 * @d - pointer to DRAM configuration structure
  */
void var_eeprom_adjust_dram(struct var_eeprom *e, struct dram_timing_info *d)
{
	int i;
	int *idx_map;
	u8 adj_table_size[DRAM_TABLE_NUM];

	/* Indices of fsp tables in the offset table */
	int b0_idx_map[] = {3, 4, 6};
	int b1_idx_map[] = {3, 4, 5, 6};

	/* Check EEPROM validity */
	if (!var_eeprom_is_valid(e))
		return;

	/* Check EEPROM version - only version 2+ has DDR adjustment tables */
	if (e->version < 2) {
		debug("EEPROM version is %d\n", e->version);
		return;
	}

	debug("EEPROM offset table\n");
	for (i = 0; i < DRAM_TABLE_NUM + 1; i++)
		debug("off[%d]=%d\n", i, e->off[i]);

	/* Calculate DRAM adjustment table sizes */
	for (i = 0; i < DRAM_TABLE_NUM; i++)
		adj_table_size[i] = (e->off[i + 1] - e->off[i]) /
				(sizeof(struct dram_cfg_param));

	debug("\nSizes table\n");
	for (i = 0; i < DRAM_TABLE_NUM; i++)
		debug("sizes[%d]=%d\n", i, adj_table_size[i]);

	/* Adjust DRAM controller configuration table */
	debug("\nAdjusting DDRC table: offset=%d, count=%d\n",
		e->off[0], adj_table_size[0]);
	adjust_dram_table(e->off[0], adj_table_size[0],
				d->ddrc_cfg, d->ddrc_cfg_num);

	/* Adjust DDR PHY configuration table */
	debug("\nAdjusting DDR PHY CFG table: offset=%d, count=%d\n",
		e->off[1], adj_table_size[1]);
	adjust_dram_table(e->off[1], adj_table_size[1],
				d->ddrphy_cfg, d->ddrphy_cfg_num);

	/* Adjust DDR PHY PIE table */
	debug("\nAdjusting DDR PHY PIE table: offset=%d, count=%d\n",
		e->off[2], adj_table_size[2]);
	adjust_dram_table(e->off[2], adj_table_size[2],
				d->ddrphy_pie, d->ddrphy_pie_num);

	/* Adjust FSP configuration tables
	 * i.MX8M B0 has 3 tables, i.MX8M B1 and i.MX8M Mini have 4 tables
	 */
	idx_map = (d->fsp_msg_num == 4) ? b1_idx_map : b0_idx_map;
	for (i = 0; i < d->fsp_msg_num; i++) {
		int j = idx_map[i];
		debug("\nAdjusting FSP table %d: offset=%d, count=%d\n",
			i, e->off[j], adj_table_size[j]);
		adjust_dram_table(e->off[j], adj_table_size[j],
			d->fsp_msg[i].fsp_cfg, d->fsp_msg[i].fsp_cfg_num);
	}
}
#endif

#ifdef CONFIG_DM_I2C
int var_carrier_eeprom_read(int bus_no, int addr, struct var_carrier_eeprom *ep)
{
	int ret;
	struct udevice *bus;
	struct udevice *dev;

	ret = uclass_get_device_by_seq(UCLASS_I2C, bus_no, &bus);
	if (ret) {
		debug("%s: No bus %d\n", __func__, bus_no);
		return ret;
	}

	ret = dm_i2c_probe(bus, addr, 0, &dev);
	if (ret) {
		debug("%s: Carrier EEPROM I2C probe failed\n", __func__);
		return ret;
	}

	/* Read EEPROM to memory */
	ret = dm_i2c_read(dev, 0, (void *)ep, sizeof(*ep));
	if (ret) {
		debug("%s: Carrier EEPROM read failed, ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}
#else
int var_carrier_eeprom_read(int bus_no, int addr, struct var_carrier_eeprom *ep)
{
	int ret;

	/* Probe EEPROM */
	i2c_set_bus_num(bus_no);
	ret = i2c_probe(addr);
	if (ret) {
		debug("%s: Carrier EEPROM probe failed\n", __func__);
		return ret;
	}

	/* Read EEPROM contents */
	ret = i2c_read(addr, 0, 1, (uint8_t *)ep, sizeof(*ep));
	if (ret) {
		debug("%s: Carrier EEPROM read failed ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}
#endif

int var_carrier_eeprom_is_valid(struct var_carrier_eeprom *ep)
{
	u32 crc, crc_offset = offsetof(struct var_carrier_eeprom, crc);
	u32 *crcp; /* Pointer to the CRC in the data read from the EEPROM */

	if (htons(ep->magic) != VAR_CARRIER_EEPROM_MAGIC) {
		debug("Invalid carrier EEPROM magic 0x%hx, expected 0x%hx\n",
			htons(ep->magic), VAR_CARRIER_EEPROM_MAGIC);
		return 0;
	}

	if (ep->struct_ver < 1) {
		printf("Invalid carrier EEPROM version 0x%hx\n", ep->struct_ver);
		return 0;
	}

	if (ep->struct_ver == 1)
		return 1;

	/* Only EEPROM structure above version 1 has CRC field */
	crc = crc32(0, (void *)ep, crc_offset);
	crcp = (void *)ep + crc_offset;
	if (crc != (*crcp)) {
		printf("Carrier EEPROM CRC mismatch (%08x != %08x)\n",
			crc, be32_to_cpu(ep->crc));
		return 0;
	}

	return 1;
}

/* Returns carrier board revision string via 'rev' argument.
 * For legacy carrier board revisions the "legacy" string is returned.
 * For new carrier board revisions the actual carrier revision is returned.
 * Symphony-Board 1.4 and below are legacy, 1.4a and above are new.
 * DT8MCustomBoard 1.4 and below are legacy, 2.0 and above are new.
 */
void var_carrier_eeprom_get_revision(struct var_carrier_eeprom *ep, char *rev, size_t size)
{
	if (var_carrier_eeprom_is_valid(ep))
		strncpy(rev, (const char *)ep->carrier_rev, size);
	else
		strncpy(rev, "legacy", size);
}
