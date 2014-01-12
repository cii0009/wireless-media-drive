/*
 * (C) Copyright 2001-2004
 * Stefan Roese, esd gmbh germany, stefan.roese@esd-electronics.com
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <command.h>
#include <malloc.h>

/* ------------------------------------------------------------------------- */

#if 0
#define FPGA_DEBUG
#endif

extern int do_reset (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern void lxt971_no_sleep(void);

/* fpga configuration data - gzip compressed and generated by bin2c */
const unsigned char fpgadata[] =
{
#include "fpgadata.c"
};

/*
 * include common fpga code (for esd boards)
 */
#include "../common/fpga.c"


/* Prototypes */
int gunzip(void *, int, unsigned char *, unsigned long *);


/* logo bitmap data - gzip compressed and generated by bin2c */
unsigned char logo_bmp_320[] =
{
#include "logo_320_240_4bpp.c"
};

unsigned char logo_bmp_640[] =
{
#include "logo_640_480_24bpp.c"
};


/*
 * include common lcd code (for esd boards)
 */
#include "../common/lcd.c"

#include "../common/s1d13704_320_240_4bpp.h"
#include "../common/s1d13806_320_240_4bpp.h"
#include "../common/s1d13806_640_480_16bpp.h"


int board_early_init_f (void)
{
	/*
	 * IRQ 0-15  405GP internally generated; active high; level sensitive
	 * IRQ 16    405GP internally generated; active low; level sensitive
	 * IRQ 17-24 RESERVED
	 * IRQ 25 (EXT IRQ 0) CAN0; active low; level sensitive
	 * IRQ 26 (EXT IRQ 1) SER0 ; active low; level sensitive
	 * IRQ 27 (EXT IRQ 2) SER1; active low; level sensitive
	 * IRQ 28 (EXT IRQ 3) FPGA 0; active low; level sensitive
	 * IRQ 29 (EXT IRQ 4) FPGA 1; active low; level sensitive
	 * IRQ 30 (EXT IRQ 5) PCI INTA; active low; level sensitive
	 * IRQ 31 (EXT IRQ 6) COMPACT FLASH; active high; level sensitive
	 */
	mtdcr(uicsr, 0xFFFFFFFF);       /* clear all ints */
	mtdcr(uicer, 0x00000000);       /* disable all ints */
	mtdcr(uiccr, 0x00000000);       /* set all to be non-critical*/
	mtdcr(uicpr, 0xFFFFFFB5);       /* set int polarities */
	mtdcr(uictr, 0x10000000);       /* set int trigger levels */
	mtdcr(uicvcr, 0x00000001);      /* set vect base=0,INT0 highest priority*/
	mtdcr(uicsr, 0xFFFFFFFF);       /* clear all ints */

	/*
	 * EBC Configuration Register: set ready timeout to 512 ebc-clks -> ca. 15 us
	 */
	mtebc (epcr, 0xa8400000); /* ebc always driven */

	return 0;
}

int misc_init_r (void)
{
	unsigned char *duart0_mcr = (unsigned char *)((ulong)DUART0_BA + 4);
	unsigned char *duart1_mcr = (unsigned char *)((ulong)DUART1_BA + 4);
	unsigned short *lcd_contrast =
		(unsigned short *)((ulong)CONFIG_SYS_FPGA_BASE_ADDR + CONFIG_SYS_FPGA_CTRL + 4);
	unsigned short *lcd_backlight =
		(unsigned short *)((ulong)CONFIG_SYS_FPGA_BASE_ADDR + CONFIG_SYS_FPGA_CTRL + 6);
	unsigned char *dst;
	ulong len = sizeof(fpgadata);
	int status;
	int index;
	int i;
	char *str;

	dst = malloc(CONFIG_SYS_FPGA_MAX_SIZE);
	if (gunzip (dst, CONFIG_SYS_FPGA_MAX_SIZE, (uchar *)fpgadata, &len) != 0) {
		printf ("GUNZIP ERROR - must RESET board to recover\n");
		do_reset (NULL, 0, 0, NULL);
	}

	status = fpga_boot(dst, len);
	if (status != 0) {
		printf("\nFPGA: Booting failed ");
		switch (status) {
		case ERROR_FPGA_PRG_INIT_LOW:
			printf("(Timeout: INIT not low after asserting PROGRAM*)\n ");
			break;
		case ERROR_FPGA_PRG_INIT_HIGH:
			printf("(Timeout: INIT not high after deasserting PROGRAM*)\n ");
			break;
		case ERROR_FPGA_PRG_DONE:
			printf("(Timeout: DONE not high after programming FPGA)\n ");
			break;
		}

		/* display infos on fpgaimage */
		index = 15;
		for (i=0; i<4; i++) {
			len = dst[index];
			printf("FPGA: %s\n", &(dst[index+1]));
			index += len+3;
		}
		putc ('\n');
		/* delayed reboot */
		for (i=20; i>0; i--) {
			printf("Rebooting in %2d seconds \r",i);
			for (index=0;index<1000;index++)
				udelay(1000);
		}
		putc ('\n');
		do_reset(NULL, 0, 0, NULL);
	}

	puts("FPGA:  ");

	/* display infos on fpgaimage */
	index = 15;
	for (i=0; i<4; i++) {
		len = dst[index];
		printf("%s ", &(dst[index+1]));
		index += len+3;
	}
	putc ('\n');

	free(dst);

	/*
	 * Reset FPGA via FPGA_INIT pin
	 */
	out_be32((void*)GPIO0_TCR, in_be32((void*)GPIO0_TCR) | FPGA_INIT); /* setup FPGA_INIT as output */
	out_be32((void*)GPIO0_OR, in_be32((void*)GPIO0_OR) & ~FPGA_INIT);  /* reset low */
	udelay(1000); /* wait 1ms */
	out_be32((void*)GPIO0_OR, in_be32((void*)GPIO0_OR) | FPGA_INIT);   /* reset high */
	udelay(1000); /* wait 1ms */

	/*
	 * Reset external DUARTs
	 */
	out_be32((void*)GPIO0_OR, in_be32((void*)GPIO0_OR) | CONFIG_SYS_DUART_RST); /* set reset to high */
	udelay(10); /* wait 10us */
	out_be32((void*)GPIO0_OR, in_be32((void*)GPIO0_OR) & ~CONFIG_SYS_DUART_RST); /* set reset to low */
	udelay(1000); /* wait 1ms */

	/*
	 * Set NAND-FLASH GPIO signals to default
	 */
	out_be32((void*)GPIO0_OR, in_be32((void*)GPIO0_OR) & ~(CONFIG_SYS_NAND_CLE | CONFIG_SYS_NAND_ALE));
	out_be32((void*)GPIO0_OR, in_be32((void*)GPIO0_OR) | CONFIG_SYS_NAND_CE);

	/*
	 * Setup EEPROM write protection
	 */
	out_be32((void*)GPIO0_OR, in_be32((void*)GPIO0_OR) | CONFIG_SYS_EEPROM_WP);
	out_be32((void*)GPIO0_TCR, in_be32((void*)GPIO0_TCR) | CONFIG_SYS_EEPROM_WP);

	/*
	 * Enable interrupts in exar duart mcr[3]
	 */
	out_8(duart0_mcr, 0x08);
	out_8(duart1_mcr, 0x08);

	/*
	 * Init lcd interface and display logo
	 */
	str = getenv("bd_type");
	if (strcmp(str, "voh405_bw") == 0) {
		lcd_setup(0, 1);
		lcd_init((uchar *)CONFIG_SYS_LCD_SMALL_REG, (uchar *)CONFIG_SYS_LCD_SMALL_MEM,
			 regs_13704_320_240_4bpp,
			 sizeof(regs_13704_320_240_4bpp)/sizeof(regs_13704_320_240_4bpp[0]),
			 logo_bmp_320, sizeof(logo_bmp_320));
	} else if (strcmp(str, "voh405_bwbw") == 0) {
		lcd_setup(0, 1);
		lcd_init((uchar *)CONFIG_SYS_LCD_SMALL_REG, (uchar *)CONFIG_SYS_LCD_SMALL_MEM,
			 regs_13704_320_240_4bpp,
			 sizeof(regs_13704_320_240_4bpp)/sizeof(regs_13704_320_240_4bpp[0]),
			 logo_bmp_320, sizeof(logo_bmp_320));
		lcd_setup(1, 1);
		lcd_init((uchar *)CONFIG_SYS_LCD_BIG_REG, (uchar *)CONFIG_SYS_LCD_BIG_MEM,
			 regs_13806_320_240_4bpp,
			 sizeof(regs_13806_320_240_4bpp)/sizeof(regs_13806_320_240_4bpp[0]),
			 logo_bmp_320, sizeof(logo_bmp_320));
	} else if (strcmp(str, "voh405_bwc") == 0) {
		lcd_setup(0, 1);
		lcd_init((uchar *)CONFIG_SYS_LCD_SMALL_REG, (uchar *)CONFIG_SYS_LCD_SMALL_MEM,
			 regs_13704_320_240_4bpp,
			 sizeof(regs_13704_320_240_4bpp)/sizeof(regs_13704_320_240_4bpp[0]),
			 logo_bmp_320, sizeof(logo_bmp_320));
		lcd_setup(1, 0);
		lcd_init((uchar *)CONFIG_SYS_LCD_BIG_REG, (uchar *)CONFIG_SYS_LCD_BIG_MEM,
			 regs_13806_640_480_16bpp,
			 sizeof(regs_13806_640_480_16bpp)/sizeof(regs_13806_640_480_16bpp[0]),
			 logo_bmp_640, sizeof(logo_bmp_640));
	} else {
		printf("Unsupported bd_type defined (%s) -> No display configured!\n", str);
		return 0;
	}

	/*
	 * Set invert bit in small lcd controller
	 */
	out_8((unsigned char *)(CONFIG_SYS_LCD_SMALL_REG + 2),
	      in_8((unsigned char *)(CONFIG_SYS_LCD_SMALL_REG + 2)) | 0x01);

	/*
	 * Set default contrast voltage on epson vga controller
	 */
	out_be16(lcd_contrast, 0x4646);

	/*
	 * Enable backlight
	 */
	out_be16(lcd_backlight, 0xffff);

	/*
	 * Enable external I2C bus
	 */
	out_be32((void*)GPIO0_TCR, in_be32((void*)GPIO0_TCR) | CONFIG_SYS_IIC_ON);

	return (0);
}


/*
 * Check Board Identity:
 */

int checkboard (void)
{
	char str[64];
	int i = getenv_r ("serial#", str, sizeof(str));

	puts ("Board: ");

	if (i == -1) {
		puts ("### No HW ID - assuming VOH405");
	} else {
		puts(str);
	}

	if (getenv_r("bd_type", str, sizeof(str)) != -1) {
		printf(" (%s)", str);
	} else {
		puts(" (Missing bd_type!)");
	}

	putc ('\n');

	return 0;
}

#ifdef CONFIG_IDE_RESET
#define FPGA_MODE (CONFIG_SYS_FPGA_BASE_ADDR + CONFIG_SYS_FPGA_CTRL)
void ide_set_reset(int on)
{
	/*
	 * Assert or deassert CompactFlash Reset Pin
	 */
	if (on) {		/* assert RESET */
		out_be16((void *)FPGA_MODE,
			 in_be16((void *)FPGA_MODE) & ~CONFIG_SYS_FPGA_CTRL_CF_RESET);
	} else {		/* release RESET */
		out_be16((void *)FPGA_MODE,
			 in_be16((void *)FPGA_MODE) | CONFIG_SYS_FPGA_CTRL_CF_RESET);
	}
}
#endif /* CONFIG_IDE_RESET */

#if defined(CONFIG_RESET_PHY_R)
void reset_phy(void)
{
#ifdef CONFIG_LXT971_NO_SLEEP

	/*
	 * Disable sleep mode in LXT971
	 */
	lxt971_no_sleep();
#endif
}
#endif

#if defined(CONFIG_SYS_EEPROM_WREN)
/* Input: <dev_addr>  I2C address of EEPROM device to enable.
 *         <state>     -1: deliver current state
 *	               0: disable write
 *		       1: enable write
 *  Returns:           -1: wrong device address
 *                      0: dis-/en- able done
 *		     0/1: current state if <state> was -1.
 */
int eeprom_write_enable (unsigned dev_addr, int state)
{
	if (CONFIG_SYS_I2C_EEPROM_ADDR != dev_addr) {
		return -1;
	} else {
		switch (state) {
		case 1:
			/* Enable write access, clear bit GPIO0. */
			out_be32((void*)GPIO0_OR, in_be32((void*)GPIO0_OR) & ~CONFIG_SYS_EEPROM_WP);
			state = 0;
			break;
		case 0:
			/* Disable write access, set bit GPIO0. */
			out_be32((void*)GPIO0_OR, in_be32((void*)GPIO0_OR) | CONFIG_SYS_EEPROM_WP);
			state = 0;
			break;
		default:
			/* Read current status back. */
			state = (0 == (in_be32((void*)GPIO0_OR) & CONFIG_SYS_EEPROM_WP));
			break;
		}
	}
	return state;
}

int do_eep_wren (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int query = argc == 1;
	int state = 0;

	if (query) {
		/* Query write access state. */
		state = eeprom_write_enable (CONFIG_SYS_I2C_EEPROM_ADDR, -1);
		if (state < 0) {
			puts ("Query of write access state failed.\n");
		} else {
			printf ("Write access for device 0x%0x is %sabled.\n",
				CONFIG_SYS_I2C_EEPROM_ADDR, state ? "en" : "dis");
			state = 0;
		}
	} else {
		if ('0' == argv[1][0]) {
			/* Disable write access. */
			state = eeprom_write_enable (CONFIG_SYS_I2C_EEPROM_ADDR, 0);
		} else {
			/* Enable write access. */
			state = eeprom_write_enable (CONFIG_SYS_I2C_EEPROM_ADDR, 1);
		}
		if (state < 0) {
			puts ("Setup of write access state failed.\n");
		}
	}

	return state;
}

U_BOOT_CMD(eepwren,	2,	0,	do_eep_wren,
	"Enable / disable / query EEPROM write access",
	""
);
#endif /* #if defined(CONFIG_SYS_EEPROM_WREN) */
