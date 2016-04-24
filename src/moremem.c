 /****************************************************************/
/*			Apple IIgs emulator			*/
/*			Copyright 1996 Kent Dickey		*/
/*								*/
/*	This code may not be used in a commercial product	*/
/*	without prior written permission of the author.		*/
/*								*/
/*	You may freely distribute this code.			*/ 
/*								*/
/*	You can contact the author at kentd@cup.hp.com.		*/
/*	HP has nothing to do with this software.		*/
/****************************************************************/

const char rcsid_moremem_c[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/moremem.c,v 1.3 2005/09/23 12:37:09 fredyd Exp $";

#include "sim65816.h"
#include "paddles.h"
#include "dis.h"
#include "video.h"
#include "sound.h"
#include "adb.h"
#include "engine.h"
#include "clock.h"
#include "scc.h"
#include "iwm.h"


int	statereg;
int	halt_on_c02a = 0;
int	g_shadow_all_banks = 0;
int	g_num_shadow_all_banks = 0;

#define IOR(val) ( (val) ? 0x80 : 0x00 )

int	linear_vid = 1;
int	bank1latch = 0;

int	wrdefram = 0;
int	int_crom[8] = { 0, 0, 0, 0,  0, 0, 0, 0 };

int	annunc_0 = 0;
int	annunc_1 = 0;
int	annunc_2 = 0;

int	shadow_reg = 0x08;

int	stop_on_c03x = 0;

int	g_border_color = 0;

int	speed_fast = 1;
word32	g_slot_motor_detect = 0;
int	power_on_clear = 0;


int	g_c023_val = 0;
int	c023_scan_int_irq_pending = 0;
int	c023_1sec_int_irq_pending = 0;

int	c02b_val = 0x08;

int	c039_write_val = 0;

int	c041_en_25sec_ints = 0;
int	c041_en_vbl_ints = 0;
int	c041_en_switch_ints = 0;
int	c041_en_move_ints = 0;
int	c041_en_mouse = 0;

int	g_c046_val = 0;

int	c046_25sec_irq_pend = 0;
int	c046_vbl_irq_pending = 0;


#define UNIMPL_READ	\
	halt_printf("UNIMP READ to addr %08x\n", loc);	\
	return 0;

#define UNIMPL_WRITE	\
	halt_printf("UNIMP WRITE to addr %08x, val: %04x\n", loc, val);	\
	return;

static void fixup_hires_on(void);
static void fixup_bank0_2000_4000(void);
static void fixup_bank0_0400_0800(void);
static void fixup_any_bank_any_page(int start_page, int num_pages, byte *mem0rd, byte *mem0wr);
static void fixup_intcx(void);
static void fixup_wrdefram(int new_wrdefram);
static void fixup_st80col(double dcycs);
static void fixup_altzp(void);
static void fixup_page2(double dcycs);
static void fixup_ramrd(void);
static void fixup_ramwrt(void);
static void fixup_lcbank2(void);
static void fixup_rdrom(void);
static void set_statereg(double dcycs, int val);
static void fixup_shadow_txt1(void);
static void fixup_shadow_txt2(void);
static void fixup_shadow_hires1(void);
static void fixup_shadow_hires2(void);
static void fixup_shadow_shr(void);
static void fixup_shadow_iolc(void);
static void update_shadow_reg(int val);
static void fixup_shadow_all_banks(void);
static void show_bankptrs(int bnk);
static void show_addr(byte *ptr);
static int in_vblank(double dcycs);
static int read_vid_counters(int loc, double dcycs);

/* ZIP/TRANSWARP EMULATION ! codes */
int	g_zipgs_unlock = 0;
int	g_zipgs_disabled = 0;
int	g_zipgs_reg_c059 = 0x5f;
int	g_zipgs_reg_c05a = 0x0f;		// 100%
int	g_zipgs_reg_c05b = 0x40;
int	g_zipgs_reg_c05c = 0x00;

#define	ZIP_SPEED	8400	// 3 times 2.8MHz Speed

int	g_twgs_speed = ZIP_SPEED;
int	g_twgs_speed_index = 2;		// 0=slow, 1=normal, 2=warp
extern	double	g_speed_zip_mult;
extern	Fplus	g_recip_projected_pmhz_zip;


void recalcaccelarationspeed(int _noacc,int _twindex,int _zippercentage,int _twspeed)
{
	double newspeed = 2.5;

	if (_noacc !=-1)
	{
		if (_noacc)
			newspeed = 2.6;
		else
			newspeed = ZIP_SPEED / 1000;
	}
	if (_twindex!=-1)
	{
		switch(_twindex)
		{
		case 0:
			newspeed= 1.0;
			break;
		case 1:
			newspeed= 2.6;
			break;
		case 2:
			newspeed= ZIP_SPEED / 1000.0;
			break;
		}
	}
	else
	if (_zippercentage != -1)
	{
		newspeed = ZIP_SPEED / 1000.0 * (_zippercentage / 100.0 );
	}
	else
	if (_twspeed != -1)
	{
		newspeed = _twspeed / 1000.0;
	}

	ki_printf("Setting accelerator speed to : %e\n", newspeed);

	g_recip_projected_pmhz_zip.plus_1 = (1.0 / newspeed);
	g_recip_projected_pmhz_zip.plus_2 = (2.0 / newspeed);
	g_recip_projected_pmhz_zip.plus_3 = (3.0 / newspeed);
	g_recip_projected_pmhz_zip.plus_x_minus_1 = (1.98 - (1.0 /newspeed));

	g_speed_zip_mult = newspeed;
}

 void initzipspeed()
{
	g_zipgs_reg_c05a = 0x0f;		// 100%
	g_twgs_speed = ZIP_SPEED;
	g_twgs_speed_index = 2;
	recalcaccelarationspeed(0,-1,-1,-1);
}

unsigned char* transwarpptr = NULL;

unsigned char transwarpcode[][32]={
{
	/*0xBCFF00*/ 'T','W','G','S',0,0,0,0,0,0,0,0,0,0,0,0,
	/*0xBCFF10*/ 0x6C,0x40,0xFF,0xBC,	//	JMP GetMaxSpeed   
	/*0xBCFF14*/ 0x6C,0x60,0xFF,0xBC,	//	JMP	GetNumISpeed   
	/*0xBCFF18*/ 0x6B,0x00,0x00,0x00,	//	???
	/*0xBCFF1C*/ 0x6B,0x00,0x00,0x00	//	???
},
{
	/*0xBCFF20*/ 0x5C,0x80,0xFF,0xBC,	//	JMP	GetCurSpeed
	/*0xBCFF24*/ 0x5C,0xA0,0xFF,0xBC,	//	JMP	SetCurSpeed
	/*0xBCFF28*/ 0x5C,0xC0,0xFF,0xBC,	//	JMP GetCurISpeed
	/*0xBCFF2C*/ 0x5C,0xE0,0xFF,0xBC,	//	JMP SetCurISpeed
	/*0xBCFF30*/ 0x6B,0x00,0x00,0x00,	//	???
	/*0xBCFF34*/ 0x6B,0x00,0x00,0x00,	//	???
	/*0xBCFF38*/ 0x6B,0x00,0x00,0x00,	//	???
	/*0xBCFF3C*/ 0x6B,0x00,0x00,0x00	//	GetTWConfig 
},
{
	/* 0xBCFF40*/	// GetMaxSpeed
	0xA9, ZIP_SPEED & 0xFF, (ZIP_SPEED >> 8) &0xFF,	// LDA 0x1F40		// Max Speed = 8.0Mhz
	0x6B,			// RTL
	0x00,0x00,0x00,0x00,		//4
	0x00,0x00,0x00,0x00,		//8
	0x6B,0x00,0x00,0x00,		//C		Space Shark calls this address ???
},
{
	/* 0xBCFF60*/	//GetNumISpeed
	0xA9,0x02,0x00,	// LDA 0x0002		// 0=slow, 1=normal, 2=warp
	0x6B,			// RTL
},
{
	/* 0xBCFF80*/ //GetCurSpeed
	0xAF, 0x6A, 0xC0, 0x00,		// LDA 0xC06A (/6B)
	0x6B,		// RTL
},
{
	/* 0xBCFFA0*/ //SetCurSpeed
	0x8F, 0x6A, 0xC0, 0x00,		// STA 0xC06A (/6B)
	0x6B,		// RTL
},
{
	/* 0xBCFFC0*/ //GetCurISpeed
	0xAF, 0x6A, 0xC0, 0x00,		// LDA 0xC06C (/6D)
	0x6B,		// RTL
},
{
	/* 0xBCFFE0*/ //SetCurISpeed
	0x8F, 0x6A, 0xC0, 0x00,		// STA 0xC06C (/6D)
	0x6B,		// RTL
}
};

/*
	0x08,			// PHP
	0xE2, 0x20,	// SEP $20
	0xA9, 0xFF, 0xFF,	// LDA -1
	0x6B,				// RTL
*/


void
fixup_brks()
{
	word32	page;
	word32	tmp, tmp2;
	Pg_info	val;
	int	i, num;

	num = g_num_breakpoints;
	for(i = 0; i < num; i++) {
		page = (g_breakpts[i] >> 8) & 0xffff;
		val = GET_PAGE_INFO_RD(page);
		tmp = PTR2WORD(val) & 0xff;
		tmp2 = tmp | BANK_IO_TMP | BANK_BREAK;
		SET_PAGE_INFO_RD(page, val - tmp + tmp2);
		val = GET_PAGE_INFO_WR(page);
		tmp = PTR2WORD(val) & 0xff;
		tmp2 = tmp | BANK_IO_TMP | BANK_BREAK;
		SET_PAGE_INFO_WR(page, val - tmp + tmp2);
	}
}

void
fixup_hires_on()
{
	if((g_cur_a2_stat & ALL_STAT_ST80) == 0) {
		return;
	}

	fixup_bank0_2000_4000();
	fixup_brks();
}

void
fixup_bank0_2000_4000()
{
	byte	*mem0rd;
	byte	*mem0wr;

	mem0rd = &(g_memory_ptr[0x2000]);
	mem0wr = mem0rd;
	if((g_cur_a2_stat & ALL_STAT_ST80) && (g_cur_a2_stat & ALL_STAT_HIRES)){
		if(PAGE2) {
			mem0rd += 0x10000;
			mem0wr += 0x10000;
			if((shadow_reg & 0x12) == 0 || (shadow_reg & 0x8) == 0){
				mem0wr += BANK_SHADOW2;
			}
		} else if((shadow_reg & 0x02) == 0) {
			mem0wr += BANK_SHADOW;
		}
		
	} else {
		if(RAMRD) {
			mem0rd += 0x10000;
		}
		if(RAMWRT) {
			mem0wr += 0x10000;
			if((shadow_reg & 0x12) == 0 || (shadow_reg & 0x8) == 0){
				mem0wr += BANK_SHADOW2;
			}
		} else if((shadow_reg & 0x02) == 0) {
			mem0wr += BANK_SHADOW;
		}
	}

	fixup_any_bank_any_page(0x20, 0x20, mem0rd, mem0wr);
}

void
fixup_bank0_0400_0800()
{
	byte	*mem0rd;
	byte	*mem0wr;
	int	shadow;

	mem0rd = &(g_memory_ptr[0x400]);
	mem0wr = mem0rd;
	shadow = BANK_SHADOW;
	if(g_cur_a2_stat & ALL_STAT_ST80) {
		if(PAGE2) {
			shadow = BANK_SHADOW2;
			mem0rd += 0x10000;
			mem0wr += 0x10000;
		}
	} else {
		if(RAMWRT) {
			shadow = BANK_SHADOW2;
			mem0wr += 0x10000;
		}
		if(RAMRD) {
			mem0rd += 0x10000;
		}
	}
	if((shadow_reg & 0x01) == 0) {
		mem0wr += shadow;
	}

	fixup_any_bank_any_page(0x4, 4, mem0rd, mem0wr);
}

void
fixup_any_bank_any_page(int start_page, int num_pages, byte *mem0rd,
		byte *mem0wr)
{
	int	i;

	for(i = 0; i < num_pages; i++) {
		SET_PAGE_INFO_RD(i + start_page, mem0rd);
		mem0rd += 0x100;
	}

	for(i = 0; i < num_pages; i++) {
		SET_PAGE_INFO_WR(i + start_page, mem0wr);
		mem0wr += 0x100;
	}

}

void
fixup_intcx()
{
	byte	*rom10000;
	byte	*rom_inc;
	int	no_io_shadow;
	int	off;
	int	start_k;
	int	indx;
	int	j, k;

	rom10000 = &(g_rom_fc_ff_ptr[0x30000]);

	no_io_shadow = (shadow_reg & 0x40);

	start_k = 0;
	if(no_io_shadow) {
		/* if not shadowing, banks 0 and 1 are not affected by intcx */
		start_k = 2;
	}

	for(k = start_k; k < 4; k++) {
		off = k;
		if(k >= 2) {
			off += (0xe0 - 2);
		}
		/* step off through 0x00, 0x01, 0xe0, 0xe1 */

		off = off << 8;
		SET_PAGE_INFO_RD(0xc0 + off, SET_BANK_IO);

		for(j = 0xc1; j < 0xc8; j++) {
			indx = j & 0xf;
			if(j < 0xc8) {
				rom_inc = SET_BANK_IO;
				if((int_crom[indx] == 0) || INTCX) {
					rom_inc = rom10000 + (j << 8);
				} else {
					// User-slot rom
					rom_inc = &(g_rom_cards_ptr[0]) +
						((j - 0xc0) << 8);
				}
				SET_PAGE_INFO_RD(j + off, rom_inc);
			}
		}
		for(j = 0xc8; j < 0xd0; j++) {
			/* c800 - cfff */
			if(int_crom[3] == 0 || INTCX) {
				rom_inc = rom10000 + (j << 8);
			} else {
				/* c800 space not necessarily mapped */
				/*   just map in ROM */
				rom_inc = rom10000 + (j << 8);
			}
			SET_PAGE_INFO_RD(j + off, rom_inc);
		}
		for(j = 0xc0; j < 0xd0; j++) {
			SET_PAGE_INFO_WR(j + off, SET_BANK_IO);
		}
	}

	if(!no_io_shadow) {
		SET_PAGE_INFO_RD(0xc7, SET_BANK_IO);		/* smartport */
	}

	fixup_brks();

	// Rajoute la transwarp
	if (!transwarpptr)
	{
	    transwarpptr = (unsigned char*)memalloc_align(sizeof(transwarpcode),0,0);
		memcpy(transwarpptr,transwarpcode,sizeof(transwarpcode));
	}
	SET_PAGE_INFO_RD(0xBCFF,transwarpptr);

}

void
fixup_wrdefram(int new_wrdefram)
{
	byte	*mem0wr;
	byte	*wrptr;
	int	j;
	
	wrdefram = new_wrdefram;

	if(shadow_reg & 0x40) {
		/* do nothing */
		return;
	}

	/* if shadowing, banks 0 and 1 are affected by wrdefram */
	mem0wr = &(g_memory_ptr[0]);
	if(!new_wrdefram) {
		mem0wr += (BANK_IO_TMP | BANK_IO2_TMP);
	}

	wrptr = mem0wr + 0x1e000;
	for(j = 0x1e0; j < 0x200; j++) {
		SET_PAGE_INFO_WR(j, wrptr);
		wrptr += 0x100;
	}

	wrptr = mem0wr + 0x0e000;
	if(ALTZP) {
		wrptr += 0x10000;
	}
	for(j = 0xe0; j < 0x100; j++) {
		SET_PAGE_INFO_WR(j, wrptr);
		wrptr += 0x100;
	}

	wrptr = mem0wr + 0x1d000;
	if(! LCBANK2) {
		wrptr -= 0x1000;
	}
	for(j = 0x1d0; j < 0x1e0; j++) {
		SET_PAGE_INFO_WR(j, wrptr);
		wrptr += 0x100;
	}

	wrptr = mem0wr + 0xd000;
	if(! LCBANK2) {
		wrptr -= 0x1000;
	}
	if(ALTZP) {
		wrptr += 0x10000;
	}
	for(j = 0xd0; j < 0xe0; j++) {
		SET_PAGE_INFO_WR(j, wrptr);
		wrptr += 0x100;
	}

	fixup_brks();
}

void
fixup_st80col(double dcycs)
{
	int	cur_a2_stat;

	cur_a2_stat = g_cur_a2_stat;

	fixup_bank0_0400_0800();

	if(cur_a2_stat & ALL_STAT_HIRES) {
		/* fixup no matter what PAGE2 since PAGE2 and RAMRD/WR */
		/*  can work against each other */
		fixup_bank0_2000_4000();
	}

	if(PAGE2) {
		change_display_mode(dcycs);
	}

	fixup_brks();
}

void
fixup_altzp()
{
	byte	*mem0rd, *mem0wr;
	int	altzp;

	altzp = ALTZP;
	mem0rd = &(g_memory_ptr[0]);
	if(altzp) {
		mem0rd += 0x10000;
	}
	SET_PAGE_INFO_RD(0, mem0rd);
	SET_PAGE_INFO_RD(1, mem0rd + 0x100);
	SET_PAGE_INFO_WR(0, mem0rd);
	SET_PAGE_INFO_WR(1, mem0rd + 0x100);

	mem0rd = &(g_memory_ptr[0xd000]);
	mem0wr = mem0rd;

	if(shadow_reg & 0x40) {
		if(ALTZP) {
			mem0rd += 0x10000;
		}
		fixup_any_bank_any_page(0xd0, 0x10, mem0rd - 0x1000,
						mem0rd - 0x1000);
	} else {
		if(!wrdefram) {
			mem0wr += (BANK_IO_TMP | BANK_IO2_TMP);
		}
		if(ALTZP) {
			mem0rd += 0x10000;
			mem0wr += 0x10000;
		}
		if(! LCBANK2) {
			mem0rd -= 0x1000;
			mem0wr -= 0x1000;
		}
		if(RDROM) {
			mem0rd = &(g_rom_fc_ff_ptr[0x3d000]);
		}
		fixup_any_bank_any_page(0xd0, 0x10, mem0rd, mem0wr);
	}

	mem0rd = &(g_memory_ptr[0xe000]);
	mem0wr = mem0rd;
	if(!wrdefram) {
		mem0wr += (BANK_IO_TMP | BANK_IO2_TMP);
	}
	if(ALTZP) {
		mem0rd += 0x10000;
		mem0wr += 0x10000;
	}
	if(RDROM) {
		mem0rd = &(g_rom_fc_ff_ptr[0x3e000]);
	}
	fixup_any_bank_any_page(0xe0, 0x20, mem0rd, mem0wr);
}

void
fixup_page2(double dcycs)
{
	if((g_cur_a2_stat & ALL_STAT_ST80)) {
		fixup_bank0_0400_0800();
		if((g_cur_a2_stat & ALL_STAT_HIRES)) {
			fixup_bank0_2000_4000();
		}
	} else {
		change_display_mode(dcycs);
	}
}

void
fixup_ramrd()
{
	byte	*mem0rd;
	int	cur_a2_stat;
	int	j;

	cur_a2_stat = g_cur_a2_stat;

	if((cur_a2_stat & ALL_STAT_ST80) == 0) {
		fixup_bank0_0400_0800();
	}
	if( ((cur_a2_stat & ALL_STAT_ST80) == 0) ||
				((cur_a2_stat & ALL_STAT_HIRES) == 0) ) {
		fixup_bank0_2000_4000();
	}

	mem0rd = &(g_memory_ptr[0x0000]);
	if(RAMRD) {
		mem0rd += 0x10000;
	}

	SET_PAGE_INFO_RD(2, mem0rd + 0x200);
	SET_PAGE_INFO_RD(3, mem0rd + 0x300);

	for(j = 8; j < 0x20; j++) {
		SET_PAGE_INFO_RD(j, mem0rd + j*0x100);
	}

	for(j = 0x40; j < 0xc0; j++) {
		SET_PAGE_INFO_RD(j, mem0rd + j*0x100);
	}
}

void
fixup_ramwrt()
{
	byte	*mem0wr;
	int	cur_a2_stat;
	int	shadow;
	int	ramwrt;
	int	j;

	cur_a2_stat = g_cur_a2_stat;

	if((cur_a2_stat & ALL_STAT_ST80) == 0) {
		fixup_bank0_0400_0800();
	}
	if( ((cur_a2_stat & ALL_STAT_ST80) == 0) ||
				((cur_a2_stat & ALL_STAT_HIRES) == 0) ) {
		fixup_bank0_2000_4000();
	}

	mem0wr = &(g_memory_ptr[0x0000]);
	ramwrt = RAMWRT;
	if(ramwrt) {
		mem0wr += 0x10000;
	}

	SET_PAGE_INFO_WR(2, mem0wr + 0x200);
	SET_PAGE_INFO_WR(3, mem0wr + 0x300);

	shadow = BANK_SHADOW;
	if(ramwrt) {
		shadow = BANK_SHADOW2;
	}
	if(((shadow_reg & 0x20) != 0) || g_rom_version < 3) {
		shadow = 0;
	}
	for(j = 8; j < 0x0c; j++) {
		SET_PAGE_INFO_WR(j, mem0wr + j*0x100 + shadow);
	}

	for(j = 0xc; j < 0x20; j++) {
		SET_PAGE_INFO_WR(j, mem0wr + j*0x100);
	}

	shadow = 0;
	if(ramwrt) {
		if((shadow_reg & 0x14) == 0 || (shadow_reg & 0x08) == 0) {
			shadow = BANK_SHADOW2;
		}
	} else if((shadow_reg & 0x04) == 0) {
		shadow = BANK_SHADOW;
	}
	for(j = 0x40; j < 0x60; j++) {
		SET_PAGE_INFO_WR(j, mem0wr + j*0x100 + shadow);
	}

	shadow = 0;
	if(ramwrt && (shadow_reg & 0x08) == 0) {
		/* shr shadowing */
		shadow = BANK_SHADOW2;
	}
	for(j = 0x60; j < 0xa0; j++) {
		SET_PAGE_INFO_WR(j, mem0wr + j*0x100 + shadow);
	}

	for(j = 0xa0; j < 0xc0; j++) {
		SET_PAGE_INFO_WR(j, mem0wr + j*0x100);
	}
}

void
fixup_lcbank2()
{
	byte	*mem0rd, *mem0wr;
	int	off;
	int	k;

	for(k = 0; k < 4; k++) {
		off = k;
		if(k >= 2) {
			off += (0xe0 - 2);
		}
		/* step off through 0x00, 0x01, 0xe0, 0xe1 */

		if(k < 2) {
			mem0rd = &(g_memory_ptr[k << 16]);
		} else {
			mem0rd = &(g_slow_memory_ptr[(k & 1) << 16]);
		}
		if((k == 0) && ALTZP) {
			mem0rd += 0x10000;
		}
		if(! LCBANK2) {
			mem0rd -= 0x1000;	/* lcbank1, use 0xc000-cfff */
		}
		mem0wr = mem0rd;
		if((k < 2) && !wrdefram) {
			mem0wr += (BANK_IO_TMP | BANK_IO2_TMP);
		}
		if((k < 2) && RDROM) {
			mem0rd = &(g_rom_fc_ff_ptr[0x30000]);
		}
		fixup_any_bank_any_page(off*0x100 + 0xd0, 0x10,
					mem0rd + 0xd000, mem0wr + 0xd000);
	}
}

void
fixup_rdrom()
{
	byte	*mem0rd;
	int	j, k;

	/* fixup_lcbank2 handles 0xd000-dfff for rd & wr*/
	fixup_lcbank2();

	for(k = 0; k < 2; k++) {
		/* k is the bank */
		mem0rd = &(g_memory_ptr[k << 16]);
		if((k == 0) && ALTZP) {
			mem0rd += 0x10000;
		}
		if((shadow_reg & 0x40) == 0) {
			if(RDROM) {
				mem0rd = &(g_rom_fc_ff_ptr[0x30000]);
			}
		}
		for(j = 0xe0; j < 0x100; j++) {
			SET_PAGE_INFO_RD(j + k*0x100, mem0rd + j*0x100);
		}
	}

}

void
set_statereg(double dcycs, int val)
{
	int	xor;

	xor = val ^ statereg;
	statereg = val;
	if(xor == 0) {
		return;
	}

	if(xor & 0x80) {
		/* altzp */
		fixup_altzp();
	}
	if(xor & 0x40) {
		/* page2 */
		fixup_page2(dcycs);
	}

	if(xor & 0x20) {
		/* RAMRD */
		fixup_ramrd();
	}

	if(xor & 0x10) {
		/* RAMWRT */
		fixup_ramwrt();
	}

	if(xor & 0x08) {
		/* RDROM */
		fixup_rdrom();
	}

	if(xor & 0x04) {
		/* LCBANK2 */
		fixup_lcbank2();
	}

	if(xor & 0x02) {
		/* ROMBANK */
		halt_printf("Just set rombank = %d\n", ROMB);
	}

	if(xor & 0x01) {
		fixup_intcx();
	}

	if(xor) {
		fixup_brks();
	}
}

void
fixup_shadow_txt1()
{
	byte	*mem0wr;
	int	j;

	fixup_bank0_0400_0800();

	mem0wr = &(g_memory_ptr[0x10000]);
	if((shadow_reg & 0x01) == 0) {
		mem0wr += BANK_SHADOW2;
	}
	for(j = 4; j < 8; j++) {
		SET_PAGE_INFO_WR(0x100 + j, mem0wr + j*0x100);
	}
}

void
fixup_shadow_txt2()
{
	byte	*mem0wr;
	int	shadow;
	int	j;

	/* bank 0 */
	mem0wr = &(g_memory_ptr[0x00000]);
	shadow = BANK_SHADOW;
	if(RAMWRT) {
		mem0wr += 0x10000;
		shadow = BANK_SHADOW2;
	}
	if(((shadow_reg & 0x20) == 0) && (g_rom_version >= 3)) {
		mem0wr += shadow;
	}
	for(j = 8; j < 0xc; j++) {
		SET_PAGE_INFO_WR(j, mem0wr + j*0x100);
	}

	/* and bank 1 */
	mem0wr = &(g_memory_ptr[0x10000]);
	if(((shadow_reg & 0x20) == 0) && (g_rom_version >= 3)) {
		mem0wr += BANK_SHADOW2;
	}
	for(j = 8; j < 0xc; j++) {
		SET_PAGE_INFO_WR(0x100 + j, mem0wr + j*0x100);
	}
}

void
fixup_shadow_hires1()
{
	byte	*mem0wr;
	int	j;

	fixup_bank0_2000_4000();

	/* and bank 1 */
	mem0wr = &(g_memory_ptr[0x10000]);
	if((shadow_reg & 0x12) == 0 || (shadow_reg & 0x8) == 0) {
		mem0wr += BANK_SHADOW2;
	}
	for(j = 0x20; j < 0x40; j++) {
		SET_PAGE_INFO_WR(0x100 + j, mem0wr + j*0x100);
	}
}

void
fixup_shadow_hires2()
{
	byte	*mem0wr;
	int	j;

	/* bank 0 */
	mem0wr = &(g_memory_ptr[0x00000]);
	if(RAMWRT) {
		mem0wr += 0x10000;
		if((shadow_reg & 0x14) == 0 || (shadow_reg & 0x8) == 0) {
			mem0wr += BANK_SHADOW2;
		}
	} else if((shadow_reg & 0x04) == 0) {
		mem0wr += BANK_SHADOW;
	}
	for(j = 0x40; j < 0x60; j++) {
		SET_PAGE_INFO_WR(j, mem0wr + j*0x100);
	}

	/* and bank 1 */
	mem0wr = &(g_memory_ptr[0x10000]);
	if((shadow_reg & 0x14) == 0 || (shadow_reg & 0x8) == 0) {
		mem0wr += BANK_SHADOW2;
	}
	for(j = 0x40; j < 0x60; j++) {
		SET_PAGE_INFO_WR(0x100 + j, mem0wr + j*0x100);
	}
}

void
fixup_shadow_shr()
{
	byte	*mem0wr;
	int	j;

	/* bank 0, only pages 0x60 - 0xa0 */
	mem0wr = &(g_memory_ptr[0x00000]);
	if(RAMWRT) {
		mem0wr += 0x10000;
		if((shadow_reg & 0x8) == 0) {
			mem0wr += BANK_SHADOW2;
		}
	}
	for(j = 0x60; j < 0xa0; j++) {
		SET_PAGE_INFO_WR(j, mem0wr + j*0x100);
	}

	/* and bank 1, only pages 0x60 - 0xa0 */
	mem0wr = &(g_memory_ptr[0x10000]);
	if((shadow_reg & 0x8) == 0) {
		mem0wr += BANK_SHADOW2;
	}
	for(j = 0x60; j < 0xa0; j++) {
		SET_PAGE_INFO_WR(0x100 + j, mem0wr + j*0x100);
	}
}

void
fixup_shadow_iolc()
{
	byte	*mem0rd, *mem0wr;
	int	k;

	for(k = 0; k < 2; k++) {
		mem0rd = &(g_memory_ptr[k << 16]);
		if(shadow_reg & 0x40) {
			fixup_any_bank_any_page((k << 8) + 0xc0, 0x10,
				mem0rd + 0xd000, mem0rd + 0xd000);
			if(k == 0 && ALTZP) {
				mem0rd += 0x10000;
			}
			fixup_any_bank_any_page((k << 8) + 0xd0, 0x10,
				mem0rd + 0xc000, mem0rd + 0xc000);
			fixup_any_bank_any_page((k << 8) + 0xe0, 0x20,
				mem0rd + 0xe000, mem0rd + 0xe000);
		} else {
			/* 0xc000 area */
			fixup_intcx();

			/* 0xd000 area */
			fixup_lcbank2();

			if(k == 0 && ALTZP) {
				mem0rd += 0x10000;
			}
			mem0wr = mem0rd;
			if(!wrdefram) {
				mem0wr += (BANK_IO_TMP | BANK_IO2_TMP);
			}
			if(RDROM) {
				mem0rd = &(g_rom_fc_ff_ptr[0x30000]);
			}
			fixup_any_bank_any_page((k << 8) + 0xe0, 0x20,
				mem0rd + 0xe000, mem0wr + 0xe000);
		}
	}
}

void
update_shadow_reg(int val)
{
	int	xor;

	if(shadow_reg == val) {
		return;
	}

	xor = shadow_reg ^ val;
	shadow_reg = val;

	if(xor & 8) {
		fixup_shadow_hires1();
		fixup_shadow_hires2();
		fixup_shadow_shr();
		xor = xor & (~0x16);
	}
	if(xor & 0x10) {
		fixup_shadow_hires1();
		fixup_shadow_hires2();
		xor = xor & (~0x6);
	}
	if(xor & 2) {
		fixup_shadow_hires1();
	}
	if(xor & 4) {
		fixup_shadow_hires2();
	}
	if(xor & 1) {
		fixup_shadow_txt1();
	}
	if((xor & 0x20) && g_rom_version >= 3) {
		fixup_shadow_txt2();
	}
	if(xor & 0x40) {
		fixup_shadow_iolc();
	}
}

void
fixup_shadow_all_banks()
{
	byte	*mem0rd;
	int	shadow;
	int	num_banks;
	int	j, k;

	/* Assume Ninja Force Megademo */
	/* only do banks 3 - num_banks by 2, shadowing into e1 */

	shadow = 0;
	if(g_shadow_all_banks && ((shadow_reg & 0x08) == 0)) {
		shadow = BANK_SHADOW2;
	}
	num_banks = g_mem_size_exp >> 16;	/* short a few, but it's ok */
	for(k = 3; k < num_banks; k += 2) {
		mem0rd = &(g_memory_ptr[k*0x10000 + 0x2000]) + shadow;
		for(j = 0x20; j < 0xa0; j++) {
			SET_PAGE_INFO_WR(k*0x100 + j, mem0rd);
			mem0rd += 0x100;
		}
	}
}

void
setup_pageinfo()
{
	byte	*mem0rd;
	word32	mem_size_pages;

	/* first, set all of memory to point to itself */
	mem_size_pages = (g_mem_size_base + g_mem_size_exp)/256;
	mem0rd = &(g_memory_ptr[0]);
	fixup_any_bank_any_page(0, mem_size_pages, mem0rd, mem0rd);

	/* mark unused memory as BAD_MEM */
	fixup_any_bank_any_page(mem_size_pages, 0xfc00-mem_size_pages,
			BANK_BAD_MEM, BANK_BAD_MEM);

	fixup_shadow_all_banks();

	/* ROM */
	mem0rd = &(g_rom_fc_ff_ptr[0]);
	fixup_any_bank_any_page(0xfc00, 0x400, mem0rd,
				mem0rd + (BANK_IO_TMP | BANK_IO2_TMP));

	/* banks e0, e1 */
	mem0rd = &(g_slow_memory_ptr[0]);
	fixup_any_bank_any_page(0xe000, 0x04, mem0rd + 0x0000, mem0rd + 0x0000);
	fixup_any_bank_any_page(0xe004, 0x08, mem0rd + 0x0400,
						mem0rd + 0x0400 + BANK_SHADOW);
	fixup_any_bank_any_page(0xe00c, 0x14, mem0rd + 0x0c00, mem0rd + 0x0c00);
	fixup_any_bank_any_page(0xe020, 0x40, mem0rd + 0x2000,
						mem0rd + 0x2000 + BANK_SHADOW);
	fixup_any_bank_any_page(0xe060, 0xa0, mem0rd + 0x6000, mem0rd + 0x6000);

	mem0rd = &(g_slow_memory_ptr[0x10000]);
	fixup_any_bank_any_page(0xe100, 0x04, mem0rd + 0x0000, mem0rd + 0x0000);
	fixup_any_bank_any_page(0xe104, 0x08, mem0rd + 0x0400,
						mem0rd + 0x0400 + BANK_SHADOW2);
	fixup_any_bank_any_page(0xe10c, 0x14, mem0rd + 0x0c00, mem0rd + 0x0c00);
	fixup_any_bank_any_page(0xe120, 0x80, mem0rd + 0x2000,
						mem0rd + 0x2000 + BANK_SHADOW2);
	fixup_any_bank_any_page(0xe1a0, 0x60, mem0rd + 0xa000, mem0rd + 0xa000);

	fixup_intcx();	/* correct banks 0xe0,0xe1, 0xc000-0xcfff area */
	fixup_lcbank2(); /* correct 0xd000-0xdfff area */

	fixup_bank0_2000_4000();
	fixup_bank0_0400_0800();
	fixup_wrdefram(wrdefram);
	fixup_altzp();
	fixup_ramrd();
	fixup_ramwrt();
	fixup_rdrom();
	fixup_shadow_txt1();
	fixup_shadow_txt2();
	fixup_shadow_hires1();
	fixup_shadow_hires2();
	fixup_shadow_shr();
	fixup_shadow_iolc();
	fixup_brks();
}

void
show_bankptrs_bank0rdwr()
{
	show_bankptrs(0);
	show_bankptrs(1);
	show_bankptrs(0xe0);
	show_bankptrs(0xe1);
	ki_printf("statereg: %02x\n", statereg);
}

void
show_bankptrs(int bnk)
{
	int i;
	Pg_info rd, wr;
	byte *ptr_rd, *ptr_wr;

	ki_printf("g_memory_ptr: %p, dummy_mem: %p, slow_mem_ptr: %p\n",
		g_memory_ptr, g_dummy_memory1_ptr, g_slow_memory_ptr);
	ki_printf("g_rom_fc_ff_ptr: %p\n", g_rom_fc_ff_ptr);

	ki_printf("Showing bank_info array for %02x\n", bnk);
	for(i = 0; i < 256; i++) {
		rd = GET_PAGE_INFO_RD(bnk*0x100 + i);
		wr = GET_PAGE_INFO_WR(bnk*0x100 + i);
		ptr_rd = (byte *)rd;
		ptr_wr = (byte *)wr;
		ki_printf("%04x rd: ", bnk*256 + i);
		show_addr(ptr_rd);
		ki_printf(" wr: ");
		show_addr(ptr_wr);
		ki_printf("\n");
	}
}

void
show_addr(byte *ptr)
{
	word32	mem_size;

	mem_size = g_mem_size_base + g_mem_size_exp;
	if(ptr >= g_memory_ptr && ptr < &g_memory_ptr[mem_size]) {
		ki_printf("%p--memory[%06x]", ptr,
					(word32)(ptr - g_memory_ptr));
	} else if(ptr >= g_rom_fc_ff_ptr && ptr < &g_rom_fc_ff_ptr[256*1024]) {
		ki_printf("%p--rom_fc_ff[%06x]", ptr,
					(word32)(ptr - g_rom_fc_ff_ptr));
	} else if(ptr >= g_slow_memory_ptr && ptr<&g_slow_memory_ptr[128*1024]){
		ki_printf("%p--slow_memory[%06x]", ptr,
					(word32)(ptr - g_slow_memory_ptr));
	} else if(ptr >=g_dummy_memory1_ptr && ptr < &g_dummy_memory1_ptr[256]){
		ki_printf("%p--dummy_memory[%06x]", ptr,
					(word32)(ptr - g_dummy_memory1_ptr));
	} else {
		ki_printf("%p--unknown", ptr);
	}
}


#if 0
#define CALC_DCYCS_FROM_CYC_PTR(dcycs, cyc_ptr, fcyc, new_fcyc)	\
	fcyc = *cyc_ptr;					\
	new_fcyc = (int)(fcyc + g_cur_fplus_ptr->plus_x_minus_1); \
	*cyc_ptr = new_fcyc;					\
	dcycs = g_last_vbl_dcycs + new_fcyc;
#endif

#define CALC_DCYCS_FROM_CYC_PTR(dcycs, cyc_ptr, fcyc, new_fcyc)	\
	dcycs = g_last_vbl_dcycs + *cyc_ptr;


int dummy = 0;

int
io_read(word32 loc, double *cyc_ptr)
{
	double	dcycs;
	word64	word64_tmp;
#if 0
	double	fcyc, new_fcyc;
#endif
	int new_lcbank2;
	int new_wrdefram;
	int tmp;
	int slot;
	int i;

	CALC_DCYCS_FROM_CYC_PTR(dcycs, cyc_ptr, fcyc, new_fcyc);

/* IO space */
	switch((loc >> 8) & 0xf) {
	case 0: /* 0xc000 - 0xc0ff */
		switch(loc & 0xff) {
		/* 0xc000 - 0xc00f */
		case 0x00: case 0x01: case 0x02: case 0x03:
		case 0x04: case 0x05: case 0x06: case 0x07:
		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: case 0x0f:
			return(adb_read_c000());

		/* 0xc010 - 0xc01f */
		case 0x10: /* c010 */
			return(adb_access_c010());
		case 0x11: /* c011 = RDLCBANK2 */
			return IOR(LCBANK2);
		case 0x12: /* c012= RDLCRAM */
			return IOR(!RDROM);
		case 0x13: /* c013=rdramd */
			return IOR(RAMRD);
		case 0x14: /* c014=rdramwrt */
			return IOR(RAMWRT);
		case 0x15: /* c015 = INTCX */
			return IOR(INTCX);
		case 0x16: /* c016: ALTZP */
			return IOR(ALTZP);
		case 0x17: /* c017: rdc3rom */
			return IOR(int_crom[3]);
		case 0x18: /* c018: rd80c0l */
			return IOR((g_cur_a2_stat & ALL_STAT_ST80));
		case 0x19: /* c019: rdvblbar */
			tmp = in_vblank(dcycs);
			return IOR(tmp);
		case 0x1a: /* c01a: rdtext */
			return IOR(g_cur_a2_stat & ALL_STAT_TEXT);
		case 0x1b: /* c01b: rdmix */
			return IOR(g_cur_a2_stat & ALL_STAT_MIX_T_GR);
		case 0x1c: /* c01c: rdpage2 */
			return IOR(PAGE2);
		case 0x1d: /* c01d: rdhires */
			return IOR(g_cur_a2_stat & ALL_STAT_HIRES);
		case 0x1e: /* c01e: altcharset on? */
			return IOR(g_cur_a2_stat & ALL_STAT_ALTCHARSET);
		case 0x1f: /* c01f: rd80vid */
			return IOR(g_cur_a2_stat & ALL_STAT_VID80);

		/* 0xc020 - 0xc02f */
		case 0x20: /* 0xc020 */
			/* Click cassette port */
			return 0x00;
		case 0x21: /* 0xc021 */
			UNIMPL_READ;
		case 0x22: /* 0xc022 */
			return (g_cur_a2_stat >> BIT_ALL_STAT_BG_COLOR) & 0xff;
		case 0x23: /* 0xc023 */
			return g_c023_val;
		case 0x24: /* 0xc024 */
			return mouse_read_c024();
		case 0x25: /* 0xc025 */
			return adb_read_c025();
		case 0x26: /* 0xc026 */
			return adb_read_c026();
		case 0x27: /* 0xc027 */
			return adb_read_c027();
		case 0x28: /* 0xc028 */
			UNIMPL_READ;
		case 0x29: /* 0xc029 */
			return((g_cur_a2_stat & 0xa0) | (linear_vid<<6) |
				(bank1latch));
		case 0x2a: /* 0xc02a */
#if 0
			ki_printf("Reading c02a...returning 0\n");
#endif
			return 0;
		case 0x2b: /* 0xc02b */
			return c02b_val;
		case 0x2c: /* 0xc02c */
			/* ki_printf("reading c02c, returning 0\n"); */
			return 0;
		case 0x2d: /* 0xc02d */
			tmp = 0;
			for(i = 0; i < 8; i++) {
				tmp = tmp | (int_crom[i] << i);
			}
			return tmp;
		case 0x2e: /* 0xc02e */
		case 0x2f: /* 0xc02f */
			return read_vid_counters(loc, dcycs);

		/* 0xc030 - 0xc03f */
		case 0x30: /* 0xc030 */
			/* click speaker */
			return doc_read_c030(dcycs);
		case 0x31: /* 0xc031 */
			/* 3.5" control */
			return (head_35 << 7) | (g_apple35_sel << 6);
		case 0x32: /* 0xc032 */
			/* scan int */
			return 0;
		case 0x33: /* 0xc033 = CLOCKDATA*/
			return clock_read_c033();
		case 0x34: /* 0xc034 = CLOCKCTL */
			return clock_read_c034();
		case 0x35: /* 0xc035 */
			return shadow_reg;
		case 0x36: /* 0xc036 = CYAREG */
			tmp = (speed_fast << 7) + (power_on_clear << 6) +
				(g_shadow_all_banks << 4) + g_slot_motor_detect;
			return tmp;
		case 0x37: /* 0xc037 */
			return 0;
		case 0x38: /* 0xc038 */
			return scc_read_reg(1, dcycs);
		case 0x39: /* 0xc039 */
			return scc_read_reg(0, dcycs);
		case 0x3a: /* 0xc03a */
			return scc_read_data(1, dcycs);
		case 0x3b: /* 0xc03b */
			return scc_read_data(0, dcycs);
		case 0x3c: /* 0xc03c */
			/* doc control */
			return doc_read_c03c(dcycs);
		case 0x3d: /* 0xc03d */
			return doc_read_c03d(dcycs);
		case 0x3e: /* 0xc03e */
			return (doc_ptr & 0xff);
		case 0x3f: /* 0xc03f */
			return (doc_ptr >> 8);

		/* 0xc040 - 0xc04f */
		case 0x40: /* 0xc040 */
			/* cassette */
			return 0;
		case 0x41: /* 0xc041 */
			tmp = ((c041_en_25sec_ints << 4) +
				(c041_en_vbl_ints << 3) +
				(c041_en_switch_ints << 2) +
				(c041_en_move_ints << 1) + (c041_en_mouse) );
			return tmp;
		case 0x45: /* 0xc045 */
			halt_printf("Mega II mouse read: c045\n");
			return 0;
		case 0x46: /* 0xc046 */
			tmp = g_c046_val;
			g_c046_val = (tmp & 0xbf) + ((tmp & 0x80) >> 1);
			return tmp;
		case 0x47: /* 0xc047 */
			if(c046_vbl_irq_pending) {
				remove_irq();
				c046_vbl_irq_pending = 0;
			}
			if(c046_25sec_irq_pend) {
				remove_irq();
				c046_25sec_irq_pend = 0;
			}
			g_c046_val &= 0xe7;	/* clear vbl_int, 1/4sec int*/
			return 0;
		case 0x42: /* 0xc042 */
		case 0x43: /* 0xc043 */
			return 0;
		case 0x44: /* 0xc044 */
		case 0x48: /* 0xc048 */
		case 0x49: /* 0xc049 */
		case 0x4a: /* 0xc04a */
		case 0x4b: /* 0xc04b */
		case 0x4c: /* 0xc04c */
		case 0x4d: /* 0xc04d */
		case 0x4e: /* 0xc04e */
		case 0x4f: /* 0xc04f */
			UNIMPL_READ;

		/* 0xc050 - 0xc05f */
		case 0x50: /* 0xc050 */
			if(g_cur_a2_stat & ALL_STAT_TEXT) {
				g_cur_a2_stat &= (~ALL_STAT_TEXT);
				change_display_mode(dcycs);
			}
			return 0;
		case 0x51: /* 0xc051 */
			if((g_cur_a2_stat & ALL_STAT_TEXT) == 0) {
				g_cur_a2_stat |= (ALL_STAT_TEXT);
				change_display_mode(dcycs);
			}
			return 0;
		case 0x52: /* 0xc052 */
			if(g_cur_a2_stat & ALL_STAT_MIX_T_GR) {
				g_cur_a2_stat &= (~ALL_STAT_MIX_T_GR);
				change_display_mode(dcycs);
			}
			return 0;
		case 0x53: /* 0xc053 */
			if((g_cur_a2_stat & ALL_STAT_MIX_T_GR) == 0) {
				g_cur_a2_stat |= (ALL_STAT_MIX_T_GR);
				change_display_mode(dcycs);
			}
			return 0;
		case 0x54: /* 0xc054 */
			set_statereg(dcycs, statereg & (~0x40));
			return 0;
		case 0x55: /* 0xc055 */
			set_statereg(dcycs, statereg | 0x40);
			return 0;
		case 0x56: /* 0xc056 */
			if(g_cur_a2_stat & ALL_STAT_HIRES) {
				g_cur_a2_stat &= (~ALL_STAT_HIRES);
				fixup_hires_on();
				change_display_mode(dcycs);
			}
			return 0;
		case 0x57: /* 0xc057 */
			if((g_cur_a2_stat & ALL_STAT_HIRES) == 0) {
				g_cur_a2_stat |= (ALL_STAT_HIRES);
				fixup_hires_on();
				change_display_mode(dcycs);
			}
			return 0;
		case 0x58: /* 0xc058 */
			if(g_zipgs_unlock < 4) {
				annunc_0 = 0;
			}
			return 0;
		case 0x59: /* 0xc059 */
			if(g_zipgs_unlock >= 4) {
				return g_zipgs_reg_c059;
			} else {
				annunc_0 = 1;
			}
			return 0;
		case 0x5a: /* 0xc05a */
			if(g_zipgs_unlock >= 4) {
				return g_zipgs_reg_c05a;
			} else {
				annunc_1 = 0;
			}
			return 0;
		case 0x5b: /* 0xc05b */
			if(g_zipgs_unlock >= 4) {
				word64_tmp = (word64)dcycs;
				tmp = (int) ((word64_tmp >> 9) & 1);
				return (tmp << 7) + (g_zipgs_reg_c05b & 0x6f) +
					(g_zipgs_disabled << 4);;
			} else {
				annunc_1 = 1;
			}
			return 0;
		case 0x5c: /* 0xc05c */
			if(g_zipgs_unlock >= 4) {
				return g_zipgs_reg_c05c;
			} else {
				annunc_2 = 0;
			}
			return 0;
		case 0x5d: /* 0xc05d */
			if(g_zipgs_unlock >= 4) {
				halt_printf("Reading ZipGS $c05d!\n");
			} else {
				annunc_2 = 1;
			}
			return 0;
		case 0x5e: /* 0xc05e */
			if(g_zipgs_unlock >= 4) {
				halt_printf("Reading ZipGS $c05e!\n");
			} else if(g_cur_a2_stat & ALL_STAT_ANNUNC3) {
				g_cur_a2_stat &= (~ALL_STAT_ANNUNC3);
				change_display_mode(dcycs);
			}
			return 0;
		case 0x5f: /* 0xc05f */
			if(g_zipgs_unlock >= 4) {
				halt_printf("Reading ZipGS $c05f!\n");
			} else if((g_cur_a2_stat & ALL_STAT_ANNUNC3) == 0) {
				g_cur_a2_stat |= (ALL_STAT_ANNUNC3);
				change_display_mode(dcycs);
			}
			return 0;


		/* 0xc060 - 0xc06f */
		case 0x60: /* 0xc060 */
			return IOR(g_paddle_button[3]);
		case 0x61: /* 0xc061 */
			return IOR(adb_is_cmd_key_down() || g_paddle_button[0]);
		case 0x62: /* 0xc062 */
			return IOR(adb_is_option_key_down() ||
							g_paddle_button[1]);
		case 0x63: /* 0xc063 */
			return IOR(g_paddle_button[2]);
		case 0x64: /* 0xc064 */
			return read_paddles(0, dcycs);
		case 0x65: /* 0xc065 */
			return read_paddles(1, dcycs);
		case 0x66: /* 0xc066 */
			return read_paddles(2, dcycs);
		case 0x67: /* 0xc067 */
			return read_paddles(3, dcycs);
		case 0x68: /* 0xc068 = STATEREG */
			return statereg;
		case 0x69: /* 0xc069 */
			/* Reserved reg, return 0 */
			return 0;
		// OG
		// Use for TWGS control
		case 0x6a: /* 0xc06a */
			return g_twgs_speed & 0xFF;
		case 0x6b: /* 0xc06b */
			return (g_twgs_speed>>8) & 0xFF;
		case 0x6c: /* 0xc06c */
			return g_twgs_speed_index & 0xFF;
		case 0x6d: /* 0xc06d */
			return (g_twgs_speed_index>>8) & 0xFF;
		//~OG
		case 0x6e: /* 0xc06e */
		case 0x6f: /* 0xc06f */
			UNIMPL_READ;

		/* 0xc070 - 0xc07f */
		case 0x70: /* c070 */
			paddle_trigger(dcycs);
			return 0;
		case 0x71:	/* 0xc071 */
		case 0x72: case 0x73:
		case 0x74: case 0x75: case 0x76: case 0x77:
		case 0x78: case 0x79: case 0x7a: case 0x7b:
		case 0x7c: case 0x7d: case 0x7e: case 0x7f:
			return g_rom_fc_ff_ptr[3*65536 + 0xc000 + (loc & 0xff)];

		/* 0xc080 - 0xc08f */
		case 0x80: case 0x81: case 0x82: case 0x83:
		case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8a: case 0x8b:
		case 0x8c: case 0x8d: case 0x8e: case 0x8f:
			new_lcbank2 = ((loc & 0x8) >> 1) ^ 0x4;
			new_wrdefram = (loc & 1);
			if(new_wrdefram != wrdefram) {
				fixup_wrdefram(new_wrdefram);
			}
			switch(loc & 0x3) {
			case 0x1: /* 0xc081 */
			case 0x2: /* 0xc082 */
				/* Read rom, set lcbank2 */
				set_statereg(dcycs, (statereg & ~(0x04)) |
					(new_lcbank2 | 0x08));
				break;
			case 0x0: /* 0xc080 */
			case 0x3: /* 0xc083 */
				/* Read ram (clear RDROM), set lcbank2 */
				set_statereg(dcycs, (statereg & ~(0x0c)) |
					(new_lcbank2));
				break;
			}
			return 0xa0;
		/* 0xc090 - 0xc09f */
		case 0x90: case 0x91: case 0x92: case 0x93:
		case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b:
		case 0x9c: case 0x9d: case 0x9e: case 0x9f:
			/* UNIMPL_READ; */
			return 0;
		/* 0xc0a0 - 0xc0af */
		case 0xa0: case 0xa1: case 0xa2: case 0xa3:
		case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xaa: case 0xab:
		case 0xac: case 0xad: case 0xae: case 0xaf:
			return 0;
			/* UNIMPL_READ; */

		/* 0xc0b0 - 0xc0bf */
		case 0xb0:
			/* c0b0: female voice tool033 look at this */
			return 0;
		case 0xb1: case 0xb2: case 0xb3:
		case 0xb4: case 0xb5: case 0xb6: case 0xb7:
		case 0xb9: case 0xba: case 0xbb:
		case 0xbc: case 0xbd: case 0xbe: case 0xbf:
			/* UNIMPL_READ; */
			return 0;
		/* c0b8: Second Sight card stuff: return 0 */
		case 0xb8:
			return 0;
			break;

		/* 0xc0c0 - 0xc0cf */
		case 0xc0: case 0xc1: case 0xc2: case 0xc3:
		case 0xc4: case 0xc5: case 0xc6: case 0xc7:
		case 0xc8: case 0xc9: case 0xca: case 0xcb:
		case 0xcc: case 0xcd: case 0xce: case 0xcf:
			return 0;
		/* 0xc0d0 - 0xc0df */
		case 0xd0: case 0xd1: case 0xd2: case 0xd3:
		case 0xd4: case 0xd5: case 0xd6: case 0xd7:
		case 0xd8: case 0xd9: case 0xda: case 0xdb:
		case 0xdc: case 0xdd: case 0xde: case 0xdf:
			return 0;
		/* 0xc0e0 - 0xc0ef */
		case 0xe0: case 0xe1: case 0xe2: case 0xe3:
		case 0xe4: case 0xe5: case 0xe6: case 0xe7:
		case 0xe8: case 0xe9: case 0xea: case 0xeb:
		          case 0xed: case 0xee: case 0xef:
			return read_iwm(loc, dcycs);
		case 0xec:
			return iwm_read_c0ec(dcycs);
		/* 0xc0f0 - 0xc0ff */
		case 0xf0: case 0xf1: case 0xf2: case 0xf3:
		case 0xf4: case 0xf5: case 0xf6: case 0xf7:
		case 0xf8: case 0xf9: case 0xfa: case 0xfb:
		case 0xfc: case 0xfd: case 0xfe: case 0xff:
			return 0;

		default:
			ki_printf("loc: %04x bad\n", loc);
			UNIMPL_READ;
		}
	case 1: case 2: case 3: case 4: case 5: case 6:
		/* c100 - c6ff */
		slot = ((loc >> 8) & 7);
		if(INTCX || (int_crom[slot] == 0)) {
			return(g_rom_fc_ff_ptr[0x3c000 + (loc & 0xfff)]);
		}
		return (dummy++) & 0xff;
		UNIMPL_READ;
	case 7:
		/* c700 */
		if(INTCX || (int_crom[7] == 0)) {
			return(g_rom_fc_ff_ptr[0x3c000 + (loc & 0xfff)]);
		}
		return g_rom_fc_ff_ptr[0x3c500 + (loc & 0xff)];
	case 8: case 9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xe:
		if(INTCX || (int_crom[3] == 0)) {
			return(g_rom_fc_ff_ptr[0x3c000 + (loc & 0xfff)]);
		}
		UNIMPL_READ;
	case 0xf:
		if(INTCX || (int_crom[3] == 0)) {
			return(g_rom_fc_ff_ptr[0x3c000 + (loc & 0xfff)]);
		}
		UNIMPL_READ;
	}

	halt_printf("io_read: hit end, loc: %06x\n", loc);

	return 0xff;
}

void
io_write(word32 loc, int val, double *cyc_ptr)
{
	double	dcycs;
#if 0
	double	fcyc, new_fcyc;
#endif
	int	new_tmp;
	int	new_lcbank2;
	int	new_wrdefram;
	int	i;
	int	tmp, tmp2;
	int	fixup;

	CALC_DCYCS_FROM_CYC_PTR(dcycs, cyc_ptr, fcyc, new_fcyc);

	val = val & 0xff;
	switch((loc >> 8) & 0xf) {
	case 0: /* 0xc000 - 0xc0ff */
		switch(loc & 0xff) {
		/* 0xc000 - 0xc00f */
		case 0x00: /* 0xc000 */
			if(g_cur_a2_stat & ALL_STAT_ST80) {
				g_cur_a2_stat &= (~ALL_STAT_ST80);
				fixup_st80col(dcycs);
			}
			return;
		case 0x01: /* 0xc001 */
			if((g_cur_a2_stat & ALL_STAT_ST80) == 0) {
				g_cur_a2_stat |= (ALL_STAT_ST80);
				fixup_st80col(dcycs);
			}
			return;
		case 0x02: /* 0xc002 */
			set_statereg(dcycs, statereg & ~0x20);
			return;
		case 0x03: /* 0xc003 */
			set_statereg(dcycs, statereg | 0x20);
			return;
		case 0x04: /* 0xc004 */
			set_statereg(dcycs, statereg & ~0x10);
			return;
		case 0x05: /* 0xc005 */
			set_statereg(dcycs, statereg | 0x10);
			return;
		case 0x06: /* 0xc006 */
			set_statereg(dcycs, statereg & ~0x01);
			return;
		case 0x07: /* 0xc007 */
			set_statereg(dcycs, statereg | 0x01);
			return;
		case 0x08: /* 0xc008 */
			set_statereg(dcycs, statereg & ~0x80);
			return;
		case 0x09: /* 0xc009 */
			set_statereg(dcycs, statereg | 0x80);
			return;
		case 0x0a: /* 0xc00a */
			if(int_crom[3] != 0) {
				int_crom[3] = 0;
				fixup_intcx();
			}
			return;
		case 0x0b: /* 0xc00b */
			if(int_crom[3] == 0) {
				int_crom[3] = 1;
				fixup_intcx();
			}
			return;
		case 0x0c: /* 0xc00c */
			if(g_cur_a2_stat & ALL_STAT_VID80) {
				g_cur_a2_stat &= (~ALL_STAT_VID80);
				change_display_mode(dcycs);
			}
			return;
		case 0x0d: /* 0xc00d */
			if((g_cur_a2_stat & ALL_STAT_VID80) == 0) {
				g_cur_a2_stat |= (ALL_STAT_VID80);
				change_display_mode(dcycs);
			}
			return;
		case 0x0e: /* 0xc00e */
			if(g_cur_a2_stat & ALL_STAT_ALTCHARSET) {
				g_cur_a2_stat &= (~ALL_STAT_ALTCHARSET);
				change_display_mode(dcycs);
			}
			return;
		case 0x0f: /* 0xc00f */
			if((g_cur_a2_stat & ALL_STAT_ALTCHARSET) == 0) {
				g_cur_a2_stat |= (ALL_STAT_ALTCHARSET);
				change_display_mode(dcycs);
			}
			return;
		/* 0xc010 - 0xc01f */
		case 0x10: case 0x11: case 0x12: case 0x13:
		case 0x14: case 0x15: case 0x16: case 0x17:
		case 0x18: case 0x19: case 0x1a: case 0x1b:
		case 0x1c: case 0x1d: case 0x1e: case 0x1f:
			adb_access_c010();
			return;
		/* 0xc020 - 0xc02f */
		case 0x20: /* 0xc020 */
			/* WRITE CASSETTE?? */
			return;
		case 0x21: /* 0xc021 */
			new_tmp = ((val >> 7) & 1) <<
						(31 - BIT_ALL_STAT_COLOR_C021);
			if((g_cur_a2_stat & ALL_STAT_COLOR_C021) != new_tmp) {
				g_cur_a2_stat ^= new_tmp;
				change_display_mode(dcycs);
			}
			return;
		case 0x22: /* 0xc022 */
			/* change text color */
			tmp = (g_cur_a2_stat >> BIT_ALL_STAT_BG_COLOR) & 0xff;
			if(val != tmp) {
				/* change text/bg color! */
				g_cur_a2_stat &= ~(ALL_STAT_TEXT_COLOR |
							ALL_STAT_BG_COLOR);
				g_cur_a2_stat += (val << BIT_ALL_STAT_BG_COLOR);
				change_display_mode(dcycs);
			}
			return;
		case 0x23: /* 0xc023 */
			if((val & 0x19) != 0) {
				halt_printf("c023 write of %02x!!!\n",val);
			}
			tmp = (g_c023_val & 0x70) | (val & 0x0f);
			if(((tmp & 0x22)==0x22) && !c023_scan_int_irq_pending){
				c023_scan_int_irq_pending = 1;
				add_irq();
			}
			if(!(tmp & 2) && c023_scan_int_irq_pending) {
				c023_scan_int_irq_pending = 0;
				remove_irq();
			}
			if(((tmp & 0x44)==0x44)&& !c023_1sec_int_irq_pending){
				c023_1sec_int_irq_pending = 1;
				add_irq();
			}
			if(!(tmp & 0x4) && c023_1sec_int_irq_pending) {
				c023_1sec_int_irq_pending = 0;
				remove_irq();
			}

			if(c023_1sec_int_irq_pending ||
					c023_scan_int_irq_pending) {
				tmp |= 0x80;
			}
			g_c023_val = tmp;
			return;
		case 0x24: /* 0xc024 */
			/* Write to mouse reg: Throw it away */
			return;
		case 0x26: /* 0xc026 */
			adb_write_c026(val);
			return;
		case 0x27: /* 0xc027 */
			adb_write_c027(val);
			return;
		case 0x29: /* 0xc029 */
			bank1latch = val & 1;
			linear_vid = (val >> 6) & 1;
			new_tmp = val & 0xa0;
			if(bank1latch == 0) {
				halt_printf("c029: %02x\n", val);
			}
			if(new_tmp != (g_cur_a2_stat & 0xa0)) {
				g_cur_a2_stat = (g_cur_a2_stat & (~0xa0)) +
					new_tmp;
				change_display_mode(dcycs);
			}
			return;
		case 0x2a: /* 0xc02a */
#if 0
			ki_printf("Writing c02a with %02x\n", val);
#endif
			return;
		case 0x2b: /* 0xc02b */
			c02b_val = val;
			if(val != 0x08 && val != 0x00) {
				ki_printf("Writing c02b with %02x (change display language)\n", val);
			}
			return;
		case 0x2d: /* 0xc02d */
			if((val & 0x9) != 0) {
				halt_printf("Illegal c02d write: %02x!\n", val);
			}
			fixup = 0;
			for(i = 0; i < 8; i++) {
				tmp = ((val & (1 << i)) != 0);
				if(int_crom[i] != tmp) {
					fixup = 1;
					int_crom[i] = tmp;
				}
			}
			if(fixup) {
				vid_printf("Write c02d of %02x\n", val);
				fixup_intcx();
			}
			return;
		case 0x25: /* 0xc025 */
			/* The Gog's patch : Ignore C025 write to run Space Shark */
			return;
		case 0x28: /* 0xc028 */
		case 0x2c: /* 0xc02c */
			UNIMPL_WRITE;
		case 0x2e: /* 0xc02e */
		case 0x2f: /* 0xc02f */
			/* Modulae writes to this--just ignore them */
			return;
			break;

		/* 0xc030 - 0xc03f */
		case 0x30: /* 0xc030 */
#if 0
			ki_printf("Write speaker?\n");
#endif
			(void)doc_read_c030(dcycs);
			return;
		case 0x31: /* 0xc031 */
			tmp = ((val & 0x80) != 0);
			tmp2 = ((val & 0x40) != 0);
			head_35 = tmp;
			iwm_set_apple35_sel(tmp2);
			iwm_printf("write c031: %02x, h: %d, 35: %d\n",
				val, head_35, g_apple35_sel);
			return;
		case 0x32: /* 0xc032 */
			tmp = g_c023_val & 0x7f;
			if(((val & 0x40) == 0) && (tmp & 0x40)) {
				/* clear 1 sec int */
				irq_printf("Clear 1sec int\n");
				if(c023_1sec_int_irq_pending) {
					remove_irq();
				}
				tmp &= 0xbf;
				g_c023_val = tmp;
				c023_1sec_int_irq_pending = 0;
			}
			if(((val & 0x20) == 0) && (tmp & 0x20)) {
				/* clear scan line int */
				irq_printf("Clear scn int1\n");
				if(c023_scan_int_irq_pending) {
					remove_irq();
				}
				c023_scan_int_irq_pending = 0;
				g_c023_val = tmp & 0xdf;
				check_for_new_scan_int(dcycs);
			}
			if(c023_1sec_int_irq_pending ||
					c023_scan_int_irq_pending) {
				g_c023_val |= 0x80;
			}
			if((val & 0x9f) != 0x9f) {
				irq_printf("c032: wrote %02x!\n", val);
			}
			return;
		case 0x33: /* 0xc033 = CLOCKDATA*/
			clock_write_c033(val);
			return;
		case 0x34: /* 0xc034 = CLOCKCTL */
			clock_write_c034(val);
			if((val & 0xf) != g_border_color) {
				g_border_color = val & 0xf;
				change_border_color(dcycs, val & 0xf);
			}
			return;
		case 0x35: /* 0xc035 */
			update_shadow_reg(val);
			return;
		case 0x36: /* 0xc036 = CYAREG */
			tmp = (val>>7) & 1;
			if(speed_fast != tmp) {
				speed_changed++;

				/* to recalculate times */
				set_halt(HALT_EVENT);
			}
			speed_fast = tmp;
			if((val & 0xf) != g_slot_motor_detect) {
				set_halt(HALT_EVENT);
			}
			g_slot_motor_detect = (val & 0xf);

			power_on_clear = (val >> 6) & 1;
			if((val & 0x60) != 0) {
				halt_printf("c036: %2x\n", val);
			}
			tmp = (val >> 4) & 1;
			if(tmp != g_shadow_all_banks) {
				if(g_num_shadow_all_banks++ == 0) {
					ki_printf("Shadowing all banks...This "
						"must be the NFC Megademo\n");
				}
				g_shadow_all_banks = tmp;
				fixup_shadow_all_banks();
			}
			return;
		case 0x37: /* 0xc037 */
			if(val != 0) {
				UNIMPL_WRITE;
			}
			return;
		case 0x38: /* 0xc038 */
			scc_write_reg(1, val, dcycs);
			return;
		case 0x39: /* 0xc039 */
			scc_write_reg(0, val, dcycs);
			return;
		case 0x3a: /* 0xc03a */
			scc_write_data(1, val, dcycs);
			return;
		case 0x3b: /* 0xc03b */
			scc_write_data(0, val, dcycs);
			return;
		case 0x3c: /* 0xc03c */
			/* doc ctl */
			doc_write_c03c(val, dcycs);
			return;
		case 0x3d: /* 0xc03d */
			/* doc data reg */
			doc_write_c03d(val, dcycs);
			return;
		case 0x3e: /* 0xc03e */
			doc_write_c03e(val);
			return;
		case 0x3f: /* 0xc03f */
			doc_write_c03f(val);
			return;

		/* 0xc040 - 0xc04f */
		case 0x41: /* c041 */
			c041_en_25sec_ints = ((val & 0x10) != 0);
			c041_en_vbl_ints = ((val & 0x8) != 0);
			c041_en_switch_ints = ((val & 0x4) != 0);
			c041_en_move_ints = ((val & 0x2) != 0);
			c041_en_mouse = ((val & 0x1) != 0);
			if((val & 0xe7) != 0) {
				halt_printf("write c041: %02x\n", val);
			}

			if(!c041_en_vbl_ints && c046_vbl_irq_pending) {
				/* there was an interrupt, but no more*/
				remove_irq();
				c046_vbl_irq_pending = 0;
			}
			if(!c041_en_25sec_ints && c046_25sec_irq_pend) {
				/* there was an interrupt, but no more*/
				remove_irq();
				c046_25sec_irq_pend = 0;
			}
			return;
		case 0x46: /* c046 */
			/* ignore writes to c046 */
			return;
		case 0x47: /* c047 */
			if(c046_vbl_irq_pending) {
				remove_irq();
				c046_vbl_irq_pending = 0;
			}
			if(c046_25sec_irq_pend) {
				remove_irq();
				c046_25sec_irq_pend = 0;
			}
			g_c046_val &= 0xe7;	/* clear vblint, 1/4sec int*/
			return;
		case 0x48: /* c048 */
			/* diversitune writes this--ignore it */
			return;
		case 0x42: /* c042 */
		case 0x43: /* c043 */
			return;
		case 0x40: /* c040 */
		case 0x44: /* c044 */
		case 0x45: /* c045 */
		case 0x49: /* c049 */
		case 0x4a: /* c04a */
		case 0x4b: /* c04b */
		case 0x4c: /* c04c */
		case 0x4d: /* c04d */
		case 0x4e: /* c04e */
		case 0x4f: /* c04f */
			UNIMPL_WRITE;

		/* 0xc050 - 0xc05f */
		case 0x50: /* 0xc050 */
			if(g_cur_a2_stat & ALL_STAT_TEXT) {
				g_cur_a2_stat &= (~ALL_STAT_TEXT);
				change_display_mode(dcycs);
			}
			return;
		case 0x51: /* 0xc051 */
			if((g_cur_a2_stat & ALL_STAT_TEXT) == 0) {
				g_cur_a2_stat |= (ALL_STAT_TEXT);
				change_display_mode(dcycs);
			}
			return;
		case 0x52: /* 0xc052 */
			if(g_cur_a2_stat & ALL_STAT_MIX_T_GR) {
				g_cur_a2_stat &= (~ALL_STAT_MIX_T_GR);
				change_display_mode(dcycs);
			}
			return;
		case 0x53: /* 0xc053 */
			if((g_cur_a2_stat & ALL_STAT_MIX_T_GR) == 0) {
				g_cur_a2_stat |= (ALL_STAT_MIX_T_GR);
				change_display_mode(dcycs);
			}
			return;
		case 0x54: /* 0xc054 */
			set_statereg(dcycs, statereg & (~0x40));
			return;
		case 0x55: /* 0xc055 */
			set_statereg(dcycs, statereg | 0x40);
			return;
		case 0x56: /* 0xc056 */
			if(g_cur_a2_stat & ALL_STAT_HIRES) {
				g_cur_a2_stat &= (~ALL_STAT_HIRES);
				fixup_hires_on();
				change_display_mode(dcycs);
			}
			return;
		case 0x57: /* 0xc057 */
			if((g_cur_a2_stat & ALL_STAT_HIRES) == 0) {
				g_cur_a2_stat |= (ALL_STAT_HIRES);
				fixup_hires_on();
				change_display_mode(dcycs);
			}
			return;
		case 0x58: /* 0xc058 */
			if(g_zipgs_unlock >= 4) {
				g_zipgs_reg_c059 &= 0x4;  /* last reset cold */
			} else {
				annunc_0 = 0;
			}
			return;
		case 0x59: /* 0xc059 */
			if(g_zipgs_unlock >= 4) {
				g_zipgs_reg_c059 = (val & 0xf8) |
						(g_zipgs_reg_c059 & 0x7);
			} else {
				annunc_0 = 1;
			}
			return;
		case 0x5a: /* 0xc05a */
			annunc_1 = 0;
			if((val & 0xf0) == 0x50) {
				g_zipgs_unlock++;
			} else if((val & 0xf0) == 0xa0) {
				g_zipgs_unlock = 0;
			} else if(g_zipgs_unlock >= 4) {
				g_zipgs_disabled = 1;
				recalcaccelarationspeed(1,-1,-1,-1);
				ki_printf("disabling zipgs!\n");
			}
			return;
		case 0x5b: /* 0xc05b */
			if(g_zipgs_unlock >= 4) {
				int v = (g_zipgs_reg_c05a >> 4)&0x0F;
				int s = (16-v)*100/16;
				g_zipgs_disabled = 0;
				ki_printf("restoring zipgs!\n");
				recalcaccelarationspeed(-1,-1,s,-1);
			} else {
				annunc_1 = 1;
			}
			return;
		case 0x5c: /* 0xc05c */
			if(g_zipgs_unlock >= 4) {
				g_zipgs_reg_c05c = val;
			} else {
				annunc_2 = 0;
			}
			return;
		case 0x5d: /* 0xc05d */
			if(g_zipgs_unlock >= 4) {
				int s = (16-(val>>4))*100/16;
				g_zipgs_reg_c05a = val | 0xf;
				ki_printf("set ZIP Speed to (%d) =%2d%%\n",val,s);
				recalcaccelarationspeed(-1,-1,s,-1);
			} else {
				annunc_2 = 1;
			}
			return;
		case 0x5e: /* 0xc05e */
			if(g_zipgs_unlock >= 4) {
				/* Zippy writes 0x80 and 0x00 here... */
			} else if(g_cur_a2_stat & ALL_STAT_ANNUNC3) {
				g_cur_a2_stat &= (~ALL_STAT_ANNUNC3);
				change_display_mode(dcycs);
			}
			return;
		case 0x5f: /* 0xc05f */
			if(g_zipgs_unlock >= 4) {
				halt_printf("Wrote ZipGS $c05f: %02x\n", val);
			} else if((g_cur_a2_stat & ALL_STAT_ANNUNC3) == 0) {
				g_cur_a2_stat |= (ALL_STAT_ANNUNC3);
				change_display_mode(dcycs);
			}
			return;


		/* 0xc060 - 0xc06f */
		case 0x60: /* 0xc060 */
		case 0x61: /* 0xc061 */
		case 0x62: /* 0xc062 */
		case 0x63: /* 0xc063 */
		case 0x64: /* 0xc064 */
		case 0x65: /* 0xc065 */
		case 0x66: /* 0xc066 */
		case 0x67: /* 0xc067 */
			/* all the above do nothing--return */
			return;
		case 0x68: /* 0xc068 = STATEREG */
			set_statereg(dcycs, val);
			return;
		case 0x69: /* 0xc069 */
			if(val != 0) {
				halt_printf("Writing c069 with %02x\n",val);
			}
			return;
		// OG
		// Use for TWGS communication!
		case 0x6a: /* 0xc06a */
			
			g_twgs_speed = (g_twgs_speed & 0xFF00 ) | val;
			return ;
		case 0x6b: /* 0xc06b */
			g_twgs_speed = (g_twgs_speed & 0x00FF ) | (val<<8);
			ki_printf("setting twgs speed to : %d\n",g_twgs_speed);
			recalcaccelarationspeed(-1,-1,-1,g_twgs_speed);
			return;
		case 0x6c: /* 0xc06c */
			g_twgs_speed_index = (g_twgs_speed_index & 0xFF00 ) | val;
			return ;
		case 0x6d: /* 0xc06d */
			g_twgs_speed_index = (g_twgs_speed_index & 0x00FF ) | (val<<8);
			ki_printf("setting twgs speed index to : %d\n",g_twgs_speed_index);
			recalcaccelarationspeed(-1,g_twgs_speed_index,-1,-1);
			set_halt(HALT_WANTTOBRK);
		// ~OG
		case 0x6e: /* 0xc06e */
		case 0x6f: /* 0xc06f */
			UNIMPL_WRITE;

		/* 0xc070 - 0xc07f */
		case 0x70: /* 0xc070 = Trigger paddles */
			paddle_trigger(dcycs);
			return;
		case 0x73: /* 0xc073 = slinky ram card bank addr? */
			return;
		case 0x71: /* 0xc071 = another slinky card enable? */
		case 0x7e: /* 0xc07e */
		case 0x7f: /* 0xc07f */
			return;
		case 0x72: /* 0xc072 */
		case 0x74: /* 0xc074 */
		case 0x75: /* 0xc075 */
		case 0x76: /* 0xc076 */
		case 0x77: /* 0xc077 */
		case 0x78: /* 0xc078 */
		case 0x79: /* 0xc079 */
		case 0x7a: /* 0xc07a */
		case 0x7b: /* 0xc07b */
		case 0x7c: /* 0xc07c */
		case 0x7d: /* 0xc07d */
			UNIMPL_WRITE;

		/* 0xc080 - 0xc08f */
		case 0x80: case 0x81: case 0x82: case 0x83:
		case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8a: case 0x8b:
		case 0x8c: case 0x8d: case 0x8e: case 0x8f:
			new_lcbank2 = ((loc & 0x8) >> 1) ^ 0x4;
			new_wrdefram = (loc & 1);
			if(new_wrdefram != wrdefram) {
				fixup_wrdefram(new_wrdefram);
			}
			switch(loc & 0xf) {
			case 0x1: /* 0xc081 */
			case 0x2: /* 0xc082 */
			case 0x5: /* 0xc085 */
			case 0x6: /* 0xc086 */
			case 0x9: /* 0xc089 */
			case 0xa: /* 0xc08a */
			case 0xd: /* 0xc08d */
			case 0xe: /* 0xc08e */
				/* Read rom, set lcbank2 */
				set_statereg(dcycs, (statereg & ~(0x04)) |
					(new_lcbank2 | 0x08));
				break;
			case 0x0: /* 0xc080 */
			case 0x3: /* 0xc083 */
			case 0x4: /* 0xc084 */
			case 0x7: /* 0xc087 */
			case 0x8: /* 0xc088 */
			case 0xb: /* 0xc08b */
			case 0xc: /* 0xc08c */
			case 0xf: /* 0xc08f */
				/* Read ram (clear RDROM), set lcbank2 */
				set_statereg(dcycs, (statereg & ~(0x0c)) |
					(new_lcbank2));
				break;
			}
			return;

		/* 0xc090 - 0xc09f */
		case 0x90: case 0x91: case 0x92: case 0x93:
		case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b:
		case 0x9c: case 0x9d: case 0x9e: case 0x9f:
			UNIMPL_WRITE;

		/* 0xc0a0 - 0xc0af */
		case 0xa0: case 0xa1: case 0xa2: case 0xa3:
		case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xaa: case 0xab:
		case 0xac: case 0xad: case 0xae: case 0xaf:
			UNIMPL_WRITE;

		/* 0xc0b0 - 0xc0bf */
		case 0xb0:
			/* Second sight stuff--ignore it */
			return;
		case 0xb1: case 0xb2: case 0xb3:
		case 0xb4: case 0xb5: case 0xb6: case 0xb7:
		case 0xb8: case 0xb9: case 0xba: case 0xbb:
		case 0xbc: case 0xbd: case 0xbe: case 0xbf:
			UNIMPL_WRITE;

		/* 0xc0c0 - 0xc0cf */
		case 0xc0: case 0xc1: case 0xc2: case 0xc3:
		case 0xc4: case 0xc5: case 0xc6: case 0xc7:
		case 0xc8: case 0xc9: case 0xca: case 0xcb:
		case 0xcc: case 0xcd: case 0xce: case 0xcf:
			UNIMPL_WRITE;

		/* 0xc0d0 - 0xc0df */
		case 0xd0: case 0xd1: case 0xd2: case 0xd3:
		case 0xd4: case 0xd5: case 0xd6: case 0xd7:
		case 0xd8: case 0xd9: case 0xda: case 0xdb:
		case 0xdc: case 0xdd: case 0xde: case 0xdf:
			UNIMPL_WRITE;

		/* 0xc0e0 - 0xc0ef */
		case 0xe0: case 0xe1: case 0xe2: case 0xe3:
		case 0xe4: case 0xe5: case 0xe6: case 0xe7:
		case 0xe8: case 0xe9: case 0xea: case 0xeb:
		case 0xec: case 0xed: case 0xee: case 0xef:
			write_iwm(loc, val, dcycs);
			return;

		/* 0xc0f0 - 0xc0ff */
		case 0xf0: case 0xf1: case 0xf2: case 0xf3:
		case 0xf4: case 0xf5: case 0xf6: case 0xf7:
		case 0xf8: case 0xf9: case 0xfa: case 0xfb:
		case 0xfc: case 0xfd: case 0xfe: case 0xff:
			UNIMPL_WRITE;
		default:
			ki_printf("WRite loc: %x\n",loc);
			my_exit(-300);
		}
		break;
	case 1: case 2: case 3: case 4: case 5: case 6: case 7:
		/* c1000 - c7ff */
			UNIMPL_WRITE;
	case 8: case 9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xe:
			UNIMPL_WRITE;
	case 0xf:
			UNIMPL_WRITE;
	}
	ki_printf("Huh2? Write loc: %x\n", loc);
	my_exit(-290);
}



#if 0
int
get_slow_mem(word32 loc, int duff_cycles)
{
	int val;

	loc = loc & 0x1ffff;
	
	if((loc &0xf000) == 0xc000) {
		return(io_read(loc &0xfff, duff_cycles));
	}
	if((loc & 0xf000) >= 0xd000) {
		if((loc & 0xf000) == 0xd000) {
			if(!LCBANK2) {
				/* Not LCBANK2 == be 0xc000 - 0xd000 */
				loc = loc - 0x1000;
			}
		}
	}

	val = g_slow_memory_ptr[loc];

	halt_printf("get_slow_mem: %06x = %02x\n", loc, val);

	return val;
}

int
set_slow_mem(word32 loc, int val, int duff_cycles)
{
	int	or_pos;
	word32	or_val;

	loc = loc & 0x1ffff;
	if((loc & 0xf000) == 0xc000) {
		return(io_write(loc & 0xfff, val, duff_cycles));
	}

	if((loc & 0xf000) == 0xd000) {
		if(!LCBANK2) {
			/* Not LCBANK2 == be 0xc000 - 0xd000 */
			loc = loc - 0x1000;
		}
	}

	if(g_slow_memory_ptr[loc] != val) {
		or_pos = (loc >> SHIFT_PER_CHANGE) & 0x1f;
		or_val = DEP1(1, or_pos, 0);
		if((loc >> CHANGE_SHIFT) >= SLOW_MEM_CH_SIZE || loc < 0) {
			ki_printf("loc: %08x!!\n", loc);
			my_exit(11);
		}
		slow_mem_changed[(loc & 0xffff) >> CHANGE_SHIFT] |= or_val;
	}

/* doesn't shadow text/hires graphics properly! */
	g_slow_memory_ptr[loc] = val;

	return val;
}
#endif

/* IIgs vertical line counters */
/* 0x7d - 0x7f: in vbl, top of screen? */
/* 0x80 - 0xdf: not in vbl, drawing screen */
/* 0xe0 - 0xff: in vbl, bottom of screen */

/* Note: lines are then 0-0x60 effectively, for 192 lines */
/* vertical blanking engages on line 192, even if in super hires mode */
/* (Last 8 lines in SHR are drawn with vbl_active set */

word32
get_lines_since_vbl(double dcycs)
{
	double	dcycs_since_last_vbl;
	double	dlines_since_vbl;
	double	dcyc_line_start;
	word32	lines_since_vbl;
	int	offset;

	dcycs_since_last_vbl = dcycs - g_last_vbl_dcycs;

	dlines_since_vbl = (262.0/DCYCS_IN_16MS) * dcycs_since_last_vbl;
	lines_since_vbl = (int)dlines_since_vbl;
	dcyc_line_start = (double)lines_since_vbl * (DCYCS_IN_16MS/262.0);

	offset = ((int)(dcycs_since_last_vbl - dcyc_line_start)) & 0xff;

	lines_since_vbl = (lines_since_vbl << 8) + offset;

	if(lines_since_vbl < 0x10680) {
		return lines_since_vbl;
	} else {
		halt_printf("lines_since_vbl: %08x!\n", lines_since_vbl);
		ki_printf("dc_s_l_v: %f, dcycs: %f, last_vbl_cycs: %f\n",
			dcycs_since_last_vbl, dcycs, g_last_vbl_dcycs);
		show_dtime_array();
		show_all_events();
		/* U_STACK_TRACE(); */
	}

	return lines_since_vbl;
}


int
in_vblank(double dcycs)
{
	int	lines_since_vbl;

	lines_since_vbl = get_lines_since_vbl(dcycs);

	if(lines_since_vbl >= 0xc000) {
		return 1;
	}

	return 0;
}

/* horizontal video counter goes from 0x00,0x40 - 0x7f, then 0x80,0xc0-0xff */
/* over 2*65 cycles.  The last visible screen pos is 0x7f and 0xff */
/* This matches KEGS starting line 0 at the border for line -1 */
int
read_vid_counters(int loc, double dcycs)
{
	word32	mask;
	int	lines_since_vbl;

	loc = loc & 0xf;

	lines_since_vbl = get_lines_since_vbl(dcycs);

	lines_since_vbl += 0x10000;
	if(lines_since_vbl >= 0x20000) {
		lines_since_vbl = lines_since_vbl - 0x20000 + 0xfa00;
	}

	if(lines_since_vbl > 0x1ffff) {
		halt_printf("lines_since_vbl: %04x, dcycs: %f, last_vbl: %f\n",
			lines_since_vbl, dcycs, g_last_vbl_dcycs);
	}

	if(loc == 0xe) {
		/* Vertical count */
		return (lines_since_vbl >> 9) & 0xff;
	}

	mask = (lines_since_vbl >> 1) & 0x80;

	lines_since_vbl = (lines_since_vbl & 0xff);
	if(lines_since_vbl >= 0x01) {
		lines_since_vbl = (lines_since_vbl + 0x3f) & 0x7f;
	}
	return (mask | (lines_since_vbl & 0xff));
}

void moremem_shut(void)
{

	dummy=0;

	halt_on_c02a = 0;
	g_shadow_all_banks = 0;
	g_num_shadow_all_banks = 0;


	linear_vid = 1;
	bank1latch = 0;

	wrdefram = 0;
	memset(int_crom,0,sizeof(int_crom));


	annunc_0 = 0;
	annunc_1 = 0;
	annunc_2 = 0;

	shadow_reg = 0x08;

	stop_on_c03x = 0;

	g_border_color = 0;

	speed_fast = 1;
	g_slot_motor_detect = 0;
	power_on_clear = 0;


	g_c023_val = 0;
	c023_scan_int_irq_pending = 0;
	c023_1sec_int_irq_pending = 0;

	c02b_val = 0x08;

	c039_write_val = 0;

	c041_en_25sec_ints = 0;
	c041_en_vbl_ints = 0;
	c041_en_switch_ints = 0;
	c041_en_move_ints = 0;
	c041_en_mouse = 0;

	g_c046_val = 0;

	c046_25sec_irq_pend = 0;
	c046_vbl_irq_pending = 0;

	if (transwarpptr)
	{
		//memfree_align(transwarpptr);
		transwarpptr = NULL;
	}
}
