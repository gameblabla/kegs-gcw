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

const char rcsid_video_c[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/video.c,v 1.3 2005/09/23 12:37:09 fredyd Exp $";

#include <time.h>

#include "sim65816.h"
#include "moremem.h"
#include "video.h"
#include "videodriver.h"
#include "dis.h"

static int get_line_stat(int line, int new_all_stat);
static void update_a2_ptrs(int line, int new_stat);
static void check_a2vid_palette(void);
static void update_a2_line_info(void);
static void update_border_info(void);
static void update_border_line(int line_in, int color);
static void redraw_changed_text_40(int start_offset, int start_line, int reparse, byte *screen_data, int altcharset, int bg_val, int fg_val);
static void redraw_changed_text_80(int start_offset, int start_line, int reparse, byte *screen_data, int altcharset, int bg_val, int fg_val);
static void redraw_changed_gr(int start_offset, int start_line, int reparse, byte *screen_data);
static void redraw_changed_dbl_gr(int start_offset, int start_line, int reparse, byte *screen_data);
static void redraw_changed_hires(int start_offset, int start_line, int color, int reparse, byte *screen_data);
static void redraw_changed_hires_bw(int start_offset, int start_line, int reparse, byte *screen_data);
static void redraw_changed_hires_color(int start_offset, int start_line, int reparse, byte *screen_data);
static void redraw_changed_dbl_hires(int start_offset, int start_line, int color, int reparse, byte *screen_data);
static void redraw_changed_dbl_hires_bw(int start_offset, int start_line, int reparse, byte *screen_data);
static void redraw_changed_dbl_hires_color(int start_offset, int start_line, int reparse, byte *screen_data);
static void check_super_hires_palette_changes(int reparse);
static void redraw_changed_super_hires_oneline_norm_320(byte *screen_data, int y, int scan, word32 ch_mask);
static void redraw_changed_super_hires_oneline_norm_640(byte *screen_data, int y, int scan, word32 ch_mask);
static void redraw_changed_super_hires_oneline_a2vid_320(byte *screen_data, int y, int scan, word32 ch_mask);
static void redraw_changed_super_hires_oneline_a2vid_640(byte *screen_data, int y, int scan, word32 ch_mask);
static void redraw_changed_super_hires_oneline_fill_320(byte *screen_data, int y, int scan, word32 ch_mask);
static void redraw_changed_super_hires_oneline_a2vid_fill_320(byte *screen_data, int y, int scan, word32 ch_mask);
static void redraw_changed_super_hires(int start_offset, int start_line, int in_reparse, byte *screen_data);
static void display_screen(void);
static void refresh_line(int line);
static void read_a2_font(void);

int a2_line_stat[25];
static int a2_line_must_reparse[25];
int a2_line_left_edge[25];
int a2_line_right_edge[25];
int a2_line_full_left_edge[25];
int a2_line_full_right_edge[25];
static byte *a2_line_ptr[25];
void *a2_line_xim[25];

static int mode_text[2][25];
static int mode_hires[2][25];
static int mode_superhires[25];
static int mode_border[25];

static byte cur_border_colors[270];

word32	a2_screen_buffer_changed = -1;
word32	g_full_refresh_needed = -1;

word32 g_cycs_in_40col = 0;

word32 slow_mem_changed[SLOW_MEM_CH_SIZE];

static word32 font40_even_bits[0x100][8][16/4];
static word32 font40_odd_bits[0x100][8][16/4];
static word32 font80_off0_bits[0x100][8][12/4];
static word32 font80_off1_bits[0x100][8][12/4];
static word32 font80_off2_bits[0x100][8][12/4];
static word32 font80_off3_bits[0x100][8][12/4];

extern byte font_array[256][8];

static byte superhires_scan_save[256];


static int	need_redraw = 1;
static int	g_palette_changed = 1;
int	g_border_sides_refresh_needed = 1;
int	g_border_special_refresh_needed = 1;
static int	g_border_line24_refresh_needed = 1;
int	g_status_refresh_needed = 1;

static int	g_vbl_border_color = 0;
static int	g_border_last_vbl_changes = 0;

int	g_use_dhr140 = 1;		/* HACK */

#define A2_MAX_ALL_STAT		34

static int	a2_new_all_stat[A2_MAX_ALL_STAT];
static int	a2_cur_all_stat[A2_MAX_ALL_STAT];
static int	g_new_a2_stat_cur_line = 0;

static int	g_expanded_col_0[16];
static int	g_expanded_col_1[16];
static int	g_expanded_col_2[16];


int g_cur_a2_stat = ALL_STAT_TEXT | ALL_STAT_ANNUNC3 |
		(0xf << BIT_ALL_STAT_TEXT_COLOR);

int	g_a2vid_palette = 0xe;
int	g_installed_full_superhires_colormap = 0;

static const int screen_index[24] = {
		0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380,
		0x028, 0x0a8, 0x128, 0x1a8, 0x228, 0x2a8, 0x328, 0x3a8,
		0x050, 0x0d0, 0x150, 0x1d0, 0x250, 0x2d0, 0x350, 0x3d0 };


static const int dbhires_colors[] = {
		/* rgb */
		0x000,		/* 0x0 black */
		0xd03,		/* 0x1 deep red */
		0x852,		/* 0x2 brown */
		0xf60,		/* 0x3 orange */
		0x070,		/* 0x4 dark green */
		0x555,		/* 0x5 dark gray */
		0x0d0,		/* 0x6 green */
		0xff0,		/* 0x7 yellow */
		0x009,		/* 0x8 dark blue */
		0xd0d,		/* 0x9 purple */
		0xaaa,		/* 0xa light gray */
		0xf98,		/* 0xb pink */
		0x22f,		/* 0xc medium blue */
		0x6af,		/* 0xd light blue */
		0x0f9,		/* 0xe aquamarine */
		0xfff		/* 0xf white */
};

static word32 g_dhires_convert[4096];	/* look up table of 7 bits (concat): */
				/* { 4 bits, |3 prev bits| } */

static const byte g_dhires_colors_16[] = {
		0x00,	/* 0x0 black */
		0x02,	/* 0x1 dark blue */
		0x04,	/* 0x2 dark green */
		0x06,	/* 0x3 medium blue */
		0x08,	/* 0x4 brown */
		0x0a,	/* 0x5 light gray */
		0x0c,	/* 0x6 green */
		0x0e,	/* 0x7 aquamarine */
		0x01,	/* 0x8 deep red */
		0x03,	/* 0x9 purple */
		0x05,	/* 0xa dark gray */
		0x07,	/* 0xb light blue */
		0x09,	/* 0xc orange */
		0x0b,	/* 0xd pink */
		0x0d,	/* 0xe yellow */
		0x0f/* 0xf white */
};

const int lores_colors[16] = {
		/* rgb */
		0x000,		/* 0x0 black */
		0xd03,		/* 0x1 deep red */
		0x009,		/* 0x2 dark blue */
		0xd0d,		/* 0x3 purple */
		0x070,		/* 0x4 dark green */
		0x555,		/* 0x5 dark gray */
		0x22f,		/* 0x6 medium blue */
		0x6af,		/* 0x7 light blue */
		0x852,		/* 0x8 brown */
		0xf60,		/* 0x9 orange */
		0xaaa,		/* 0xa light gray */
		0xf98,		/* 0xb pink */
		0x0d0,		/* 0xc green */
		0xff0,		/* 0xd yellow */
		0x0f9,		/* 0xe aquamarine */
		0xfff		/* 0xf white */
};

#if 0
static const int hires_colors[] = {
		/* rgb */
		0x000,		/* 0x0 black */
		0x0d0,		/* 0x1 green */
		0xd0d,		/* 0x2 purple */
		0xfff,		/* 0x3 white */
		0x000,		/* 0x4 black */
		0xf60,		/* 0x5 orange */
		0x22f,		/* 0x6 medium blue */
		0xfff		/* 0x7 white */
};
#endif

static const word32 bw_hires_convert[4] = {
	BIGEND(0x00000000),
	BIGEND(0x0f0f0000),
	BIGEND(0x00000f0f),
	BIGEND(0x0f0f0f0f)
};

static const word32 bw_dhires_convert[16] = {
	BIGEND(0x00000000),
	BIGEND(0x0f000000),
	BIGEND(0x000f0000),
	BIGEND(0x0f0f0000),

	BIGEND(0x00000f00),
	BIGEND(0x0f000f00),
	BIGEND(0x000f0f00),
	BIGEND(0x0f0f0f00),

	BIGEND(0x0000000f),
	BIGEND(0x0f00000f),
	BIGEND(0x000f000f),
	BIGEND(0x0f0f000f),

	BIGEND(0x00000f0f),
	BIGEND(0x0f000f0f),
	BIGEND(0x000f0f0f),
	BIGEND(0x0f0f0f0f),
};

const word32 hires_convert[64] = {
	BIGEND(0x00000000),	/* 00,0000 = black, black, black, black */
	BIGEND(0x00000000),	/* 00,0001 = black, black, black, black */
	BIGEND(0x03030000),	/* 00,0010 = purp , purp , black, black */
	BIGEND(0x0f0f0000),	/* 00,0011 = white, white, black, black */
	BIGEND(0x00000c0c),	/* 00,0100 = black, black, green, green */
	BIGEND(0x0c0c0c0c),	/* 00,0101 = green, green, green, green */
	BIGEND(0x0f0f0f0f),	/* 00,0110 = white, white, white, white */
	BIGEND(0x0f0f0f0f),	/* 00,0111 = white, white, white, white */
	BIGEND(0x00000000),	/* 00,1000 = black, black, black, black */
	BIGEND(0x00000000),	/* 00,1001 = black, black, black, black */
	BIGEND(0x03030303),	/* 00,1010 = purp , purp , purp , purp */
	BIGEND(0x0f0f0303),	/* 00,1011 = white ,white, purp , purp */
	BIGEND(0x00000f0f),	/* 00,1100 = black ,black, white, white */
	BIGEND(0x0c0c0f0f),	/* 00,1101 = green ,green, white, white */
	BIGEND(0x0f0f0f0f),	/* 00,1110 = white ,white, white, white */
	BIGEND(0x0f0f0f0f),	/* 00,1111 = white ,white, white, white */

	BIGEND(0x00000000),	/* 01,0000 = black, black, black, black */
	BIGEND(0x00000000),	/* 01,0001 = black, black, black, black */
	BIGEND(0x06060000),	/* 01,0010 = blue , blue , black, black */
	BIGEND(0x0f0f0000),	/* 01,0011 = white, white, black, black */
	BIGEND(0x00000c0c),	/* 01,0100 = black, black, green, green */
	BIGEND(0x09090c0c),	/* 01,0101 = orang, orang, green, green */
	BIGEND(0x0f0f0f0f),	/* 01,0110 = white, white, white, white */
	BIGEND(0x0f0f0f0f),	/* 01,0111 = white, white, white, white */
	BIGEND(0x00000000),	/* 01,1000 = black, black, black, black */
	BIGEND(0x00000000),	/* 01,1001 = black, black, black, black */
	BIGEND(0x06060303),	/* 01,1010 = blue , blue , purp , purp */
	BIGEND(0x0f0f0303),	/* 01,1011 = white ,white, purp , purp */
	BIGEND(0x00000f0f),	/* 01,1100 = black ,black, white, white */
	BIGEND(0x09090f0f),	/* 01,1101 = orang ,orang, white, white */
	BIGEND(0x0f0f0f0f),	/* 01,1110 = white ,white, white, white */
	BIGEND(0x0f0f0f0f),	/* 01,1111 = white ,white, white, white */

	BIGEND(0x00000000),	/* 10,0000 = black, black, black, black */
	BIGEND(0x00000000),	/* 10,0001 = black, black, black, black */
	BIGEND(0x03030000),	/* 10,0010 = purp , purp , black, black */
	BIGEND(0x0f0f0000),	/* 10,0011 = white, white, black, black */
	BIGEND(0x00000909),	/* 10,0100 = black, black, orang, orang */
	BIGEND(0x0c0c0909),	/* 10,0101 = green, green, orang, orang */
	BIGEND(0x0f0f0f0f),	/* 10,0110 = white, white, white, white */
	BIGEND(0x0f0f0f0f),	/* 10,0111 = white, white, white, white */
	BIGEND(0x00000000),	/* 10,1000 = black, black, black, black */
	BIGEND(0x00000000),	/* 10,1001 = black, black, black, black */
	BIGEND(0x03030606),	/* 10,1010 = purp , purp , blue , blue */
	BIGEND(0x0f0f0606),	/* 10,1011 = white ,white, blue , blue */
	BIGEND(0x00000f0f),	/* 10,1100 = black ,black, white, white */
	BIGEND(0x0c0c0f0f),	/* 10,1101 = green ,green, white, white */
	BIGEND(0x0f0f0f0f),	/* 10,1110 = white ,white, white, white */
	BIGEND(0x0f0f0f0f),	/* 10,1111 = white ,white, white, white */

	BIGEND(0x00000000),	/* 11,0000 = black, black, black, black */
	BIGEND(0x00000000),	/* 11,0001 = black, black, black, black */
	BIGEND(0x06060000),	/* 11,0010 = blue , blue , black, black */
	BIGEND(0x0f0f0000),	/* 11,0011 = white, white, black, black */
	BIGEND(0x00000909),	/* 11,0100 = black, black, orang, orang */
	BIGEND(0x09090909),	/* 11,0101 = orang, orang, orang, orang */
	BIGEND(0x0f0f0f0f),	/* 11,0110 = white, white, white, white */
	BIGEND(0x0f0f0f0f),	/* 11,0111 = white, white, white, white */
	BIGEND(0x00000000),	/* 11,1000 = black, black, black, black */
	BIGEND(0x00000000),	/* 11,1001 = black, black, black, black */
	BIGEND(0x06060606),	/* 11,1010 = blue , blue , blue , blue */
	BIGEND(0x0f0f0606),	/* 11,1011 = white ,white, blue , blue */
	BIGEND(0x00000f0f),	/* 11,1100 = black ,black, white, white */
	BIGEND(0x09090f0f),	/* 11,1101 = orang ,orang, white, white */
	BIGEND(0x0f0f0f0f),	/* 11,1110 = white ,white, white, white */
	BIGEND(0x0f0f0f0f),	/* 11,1111 = white ,white, white, white */
};

void
video_init_stat()
{
    int i;

	for(i = 0; i < 25; i++) {
		a2_line_ptr[i] = (byte *)0;
		a2_line_xim[i] = (void *)0;
		a2_line_stat[i] = -1;
		a2_line_must_reparse[i] = 1;
		a2_line_left_edge[i] = -1;
		a2_line_right_edge[i] = -1;
	}
	for(i = 0; i < A2_MAX_ALL_STAT; i++) {
		a2_new_all_stat[i] = 0;
		a2_cur_all_stat[i] = 1;
	}

	g_new_a2_stat_cur_line = 0;
}

void
video_init(int devtype)
{
	word32	col[4];
	word32	*ptr;
	word32	val0, val1, val2, val3;
	word32	prev_col, match_col;
	word32	val;
	int	total;
	int	i, j;
/* Initialize video system */

	video_init_stat();

	if(!video_init_device(devtype)) {
        vid_printf("Video device type %d not available, disabling video\n",devtype);
        if(!video_init_device(VIDEO_NONE)) {
            vid_printf("Cannot initialize video\n");
            my_exit(1);
        }
    }
            

	read_a2_font();

	vid_printf("Zeroing out video memory\n");

	for(i = 0; i < 5; i++) {
		switch(i) {
		case 0:
			ptr = (word32 *)&(video_data_text[0][0]);
			total = (A2_WINDOW_HEIGHT)*(A2_WINDOW_WIDTH);
			break;
		case 1:
			ptr = (word32 *)&(video_data_text[1][0]);
			total = (A2_WINDOW_HEIGHT)*(A2_WINDOW_WIDTH);
			break;
		case 2:
			ptr = (word32 *)&(video_data_hires[0][0]);
			total = (A2_WINDOW_HEIGHT)*(A2_WINDOW_WIDTH);
			break;
		case 3:
			ptr = (word32 *)&(video_data_hires[1][0]);
			total = (A2_WINDOW_HEIGHT)*(A2_WINDOW_WIDTH);
			break;
		case 4:
			ptr = (word32 *)&(video_data_superhires[0]);
			total = (A2_WINDOW_HEIGHT)*(A2_WINDOW_WIDTH);
			break;
#if 0
		case 5:
			ptr = (word32 *)&(video_data_border_sides[0]);
			total = (A2_WINDOW_HEIGHT)*(EFF_BORDER_WIDTH);
			break;
		case 6:
			ptr = (word32 *)&(video_data_border_special[0]);
			total = (X_A2_WINDOW_HEIGHT - A2_WINDOW_HEIGHT + 2*8) *
					(X_A2_WINDOW_WIDTH);
			break;
#endif
		default:
			ki_printf("i: %d, unknown\n", i);
			my_exit(3);
		}

		for(j = 0; j < total >> 2; j++) {
			*ptr++ = 0;
		}
	}

	for(i = 0; i < SLOW_MEM_CH_SIZE; i++) {
		slow_mem_changed[i] = (word32)-1;
	}

	/* create g_expanded_col_* */
	for(i = 0; i < 16; i++) {
		val = (lores_colors[i] >> 0) & 0xf;
		g_expanded_col_0[i] = val;

		val = (lores_colors[i] >> 4) & 0xf;
		g_expanded_col_1[i] = val;

		val = (lores_colors[i] >> 8) & 0xf;
		g_expanded_col_2[i] = val;
	}

	/* create g_dhires_convert[] array */
	for(i = 0; i < 4096; i++) {
		match_col = i & 0xf;
		prev_col = i & 0xf;
		for(j = 0; j < 4; j++) {
			val0 = match_col & 1;
			val1 = (i >> (4 + j)) & 1;
			if(val0 == val1) {
				col[j] = prev_col;
				match_col = match_col >> 1;
			} else {
				col[j] = (i >> (4 + j)) & 0xf;
				prev_col = col[j];
				match_col = col[j] >> 1;
			}
		}
		if(g_use_dhr140) {
			for(j = 0; j < 4; j++) {
				col[j] = (i >> 4) & 0xf;
			}
		}
		val0 = g_dhires_colors_16[col[0] & 0xf];
		val1 = g_dhires_colors_16[col[1] & 0xf];
		val2 = g_dhires_colors_16[col[2] & 0xf];
		val3 = g_dhires_colors_16[col[3] & 0xf];
#ifdef KEGS_LITTLE_ENDIAN
		val = (val3 << 24) + (val2 << 16) + (val1 << 8) + val0;
#else
		val = (val0 << 24) + (val1 << 16) + (val2 << 8) + val3;
#endif
		g_dhires_convert[i] = val;
	}

	change_display_mode(g_cur_dcycs);
	video_reset();
	display_screen();

	vid_printf("Done with display_screen\n");

	fflush(stdout);
}

void
show_a2_line_stuff()
{
	int	i;

	for(i = 0; i < 25; i++) {
		ki_printf("line: %d: stat: %04x, ptr: %p, reparse: %d, "
			"left_edge:%d, right_edge:%d\n",
			i, a2_line_stat[i], a2_line_xim[i],
			a2_line_must_reparse[i], a2_line_left_edge[i],
			a2_line_right_edge[i]);
	}

	ki_printf("new_a2_stat_cur_line: %d\n", g_new_a2_stat_cur_line);
	for(i = 0; i < A2_MAX_ALL_STAT; i++) {
		ki_printf("cur_all[%d]: %03x new_all: %03x\n", i,
			a2_cur_all_stat[i], a2_new_all_stat[i]);
	}

}

int	g_flash_count = 0;

void
video_reset()
{

	g_installed_full_superhires_colormap = (g_screen_depth != 8);
	g_cur_a2_stat = ALL_STAT_TEXT | ALL_STAT_ANNUNC3 |
		(0xf << BIT_ALL_STAT_TEXT_COLOR);

	update_a2_line_info();
	/* install_a2vid_colormap(); */
	video_update_physical_colormap();
	g_palette_changed = 0;
}

int	g_screen_redraw_skip_count = 0;
int	g_screen_redraw_skip_amt = -1;
int	g_show_screen = 1;

word32	g_cycs_in_check_input = 0;

void
video_update()
{
	register word32 start_time;
	register word32 end_time;

	update_a2_line_info();

	update_border_info();

	GET_ITIMER(start_time);
	video_check_input_events();
	GET_ITIMER(end_time);

	g_cycs_in_check_input += (end_time - start_time);

	if(g_show_screen) {
		g_screen_redraw_skip_count--;
		if(g_screen_redraw_skip_count < 0) {
			refresh_screen();
			g_screen_redraw_skip_count = g_screen_redraw_skip_amt;
		}
	}

	/* update flash */
	g_flash_count++;
	if(g_flash_count >= 30) {
		g_flash_count = 0;
		g_cur_a2_stat ^= ALL_STAT_FLASH_STATE;
		change_display_mode(g_cur_dcycs);
	}

	check_a2vid_palette();
}



void
change_display_mode(double dcycs)
{
	int	new_a2_stat;
	int	start_line;
	int	prev_stat;
	int	line;
	int	i;

	line = ((get_lines_since_vbl(dcycs) + 0xff) >> 8);
	if(line < 0) {
		line = 0;
		halt_printf("Line < 0!\n");
	}
	line = line >> 3;
	if(line > 24) {
		line = 0;
	}

	new_a2_stat = (g_cur_a2_stat & (~ALL_STAT_PAGE2)) + PAGE2;

	start_line = g_new_a2_stat_cur_line;
	prev_stat = a2_new_all_stat[start_line];

	for(i = start_line + 1; i < line; i++) {
		a2_new_all_stat[i] = prev_stat;
	}
	a2_new_all_stat[line] = new_a2_stat;

	g_new_a2_stat_cur_line = line;
}

int
get_line_stat(int line, int new_all_stat)
{
	int	page, color, dbl;
	int	st80, hires, annunc3, mix_t_gr;
	int	altchar, text_color, bg_color, flash_state;
	int	mode;

	st80 = new_all_stat & ALL_STAT_ST80;
	hires = new_all_stat & ALL_STAT_HIRES;
	annunc3 = new_all_stat & ALL_STAT_ANNUNC3;
	mix_t_gr = new_all_stat & ALL_STAT_MIX_T_GR;

	page = EXTRU(new_all_stat, 31 - BIT_ALL_STAT_PAGE2, 1) && !st80;
	color = EXTRU(new_all_stat, 31 - BIT_ALL_STAT_COLOR_C021, 1);
	dbl = EXTRU(new_all_stat, 31 - BIT_ALL_STAT_VID80, 1);

	altchar = 0; text_color = 0; bg_color = 0; flash_state = 0;

	if(new_all_stat & ALL_STAT_SUPER_HIRES) {
		mode = MODE_SUPER_HIRES;
		page = 0; dbl = 0; color = 0;
	} else {
		if(line >= 24) {
			mode = MODE_BORDER;
			page = 0; dbl = 0; color = 0;
		} else if((new_all_stat & ALL_STAT_TEXT) ||
						(line >= 20 && mix_t_gr)) {
			mode = MODE_TEXT;
			color = 0;
			altchar = EXTRU(new_all_stat,
					31 - BIT_ALL_STAT_ALTCHARSET, 1);
			text_color = EXTRU(new_all_stat,
					31 - BIT_ALL_STAT_TEXT_COLOR, 4);
			bg_color = EXTRU(new_all_stat,
					31 - BIT_ALL_STAT_BG_COLOR, 4);
			flash_state = EXTRU(new_all_stat,
					31 - BIT_ALL_STAT_FLASH_STATE, 1);
			if(altchar) {
				/* don't bother flashing if altchar on */
				flash_state = 0;
			}
		} else {
			/* obey the graphics mode */
			dbl = dbl && !annunc3;
			if(hires) {
				color = color | EXTRU(new_all_stat,
					31 - BIT_ALL_STAT_DIS_COLOR_DHIRES, 1);
				mode = MODE_HGR;
			} else {
				mode = MODE_GR;
			}
		}
	}

	return((text_color << 12) + (bg_color << 8) + (altchar << 7) +
		(mode << 4) + (flash_state << 3) + (page << 2) +
		(color << 1) + dbl);
}

void
update_a2_ptrs(int line, int new_stat)
{
	byte	*ptr;
	void	*xim;
	int	*mode_ptr;
	int	page;
	int	mode;

	page = (new_stat >> 2) & 1;

	mode = (new_stat >> 4) & 7;

	switch(mode) {
	case MODE_TEXT:
	case MODE_GR:
		ptr = video_data_text[page];
		xim = video_image_text[page];
		mode_ptr = &(mode_text[page][0]);
		break;
	case MODE_HGR:
		ptr = video_data_hires[page];
		xim = video_image_hires[page];
		mode_ptr = &(mode_hires[page][0]);
		/*  arrange to force superhires reparse since we use the */
		/*    same memory */
		mode_superhires[line] = -1;
		break;
	case MODE_SUPER_HIRES:
		ptr = video_data_superhires;
		xim = video_image_superhires;
		mode_ptr = &(mode_superhires[0]);
		/*  arrange to force hires reparse since we use the */
		/*    same memory */
		mode_hires[0][line] = -1;
		mode_hires[1][line] = -1;
		break;
	case MODE_BORDER:
		/* Hack: reuse text page last line as the special border */
		ptr = video_data_text[0];
		xim = video_image_text[0];
		mode_ptr = &(mode_border[0]);
		break;
	default:
		halt_printf("update_a2_ptrs: mode: %d unknown!\n", mode);
		return;
	}

	a2_line_xim[line] = xim;
	a2_line_ptr[line] = ptr;
	if(mode_ptr[line] != new_stat) {
		a2_line_must_reparse[line] = 1;
		mode_ptr[line] = new_stat;
	}

	g_full_refresh_needed |= (1 << line);
	a2_screen_buffer_changed |= (1 << line);
}

void
change_a2vid_palette(int new_palette)
{
	int	i;

	for(i = 0; i < 25; i++) {
		mode_text[0][i] = -1;
		mode_text[1][i] = -1;
		mode_hires[0][i] = -1;
		mode_hires[1][i] = -1;
		mode_superhires[i] = -1;
		mode_border[i] = -1;
		a2_line_must_reparse[i] = 1;
	}

	ki_printf("Changed a2vid_palette to %x\n", new_palette);

	g_a2vid_palette = new_palette;
	g_cur_a2_stat = (g_cur_a2_stat & (~ALL_STAT_A2VID_PALETTE)) +
			(new_palette << BIT_ALL_STAT_A2VID_PALETTE);
	change_display_mode(g_cur_dcycs);

	g_border_sides_refresh_needed = 1;
	g_border_special_refresh_needed = 1;
	g_status_refresh_needed = 1;
	g_palette_changed = 1;
	g_border_last_vbl_changes = 1;
	for(i = 0; i < 262; i++) {
		cur_border_colors[i] ^= 1;;
	}
}

int g_num_a2vid_palette_checks = 1;
int g_shr_palette_used[16];

void
check_a2vid_palette()
{
	int	sum;
	int	min;
	int	val;
	int	min_pos;
	int	count_cur;
	int	i;

	/* determine if g_a2vid_palette should change */

	g_num_a2vid_palette_checks--;
	if(g_num_a2vid_palette_checks || g_installed_full_superhires_colormap){
		return;
	}

	g_num_a2vid_palette_checks = 60;

	sum = 0;
	min = 0x100000;
	min_pos = -1;
	count_cur = g_shr_palette_used[g_a2vid_palette];

	for(i = 0; i < 16; i++) {
		val = g_shr_palette_used[i];
		g_shr_palette_used[i] = 0;
		if(val < min) {
			min = val;
			min_pos = i;
		}
		sum += val;
	}

	if(g_a2vid_palette != min_pos && (count_cur > min)) {
		change_a2vid_palette(min_pos);
	}
}

void
update_a2_line_info()
{
	int	end_line;
	int	cur_stat;
	int	new_all_stat;
	int	new_stat;
	int	i;

	/* called each VBL to find out what display changes happened */

	end_line = g_new_a2_stat_cur_line;
	cur_stat = a2_new_all_stat[end_line];

#if 0
	ki_printf("g_cur_a2_stat:%03x, new_all_stat[%d]: %03x\n", g_cur_a2_stat,
		end_line, cur_stat);
#endif

	for(i = end_line + 1; i < A2_MAX_ALL_STAT; i++) {
		a2_new_all_stat[i] = cur_stat;	
	}

	for(i = 0; i < 25; i++) {
		new_all_stat = a2_new_all_stat[i];
		if(new_all_stat != a2_cur_all_stat[i]) {
			a2_cur_all_stat[i] = new_all_stat;
			new_stat = get_line_stat(i, new_all_stat);
			if(new_stat != a2_line_stat[i]) {
				a2_line_stat[i] = new_stat;
				update_a2_ptrs(i, new_stat);
			}
		}
	}


	g_new_a2_stat_cur_line = 0;
	a2_new_all_stat[0] = cur_stat;
}

#define MAX_BORDER_CHANGES	65536

STRUCT(Border_changes) {
	float	fcycs;
	int	val;
};

static Border_changes border_changes[MAX_BORDER_CHANGES];
static int	g_num_border_changes = 0;

void
change_border_color(double dcycs, int val)
{
	int	pos;

	pos = g_num_border_changes;
	border_changes[pos].fcycs = (float)(dcycs - g_last_vbl_dcycs);
	border_changes[pos].val = val;

	pos++;
	g_num_border_changes = pos;

	if(pos >= MAX_BORDER_CHANGES) {
		halt_printf("num border changes: %d\n", pos);
		g_num_border_changes = 0;
	}
}

void
update_border_info()
{
	float	flines_per_fcyc;
	int	new_line;
	int	new_val;
	int	last_line;
	int	limit;
	int	color_now;
	int	i, j;

	/* to get this routine to redraw the border, change */
	/*  g_vbl_border_color,  set g_border_last_vbl_changes = 1 */
	/*  and change the cur_border_colors[] array */

	color_now = g_vbl_border_color;

	flines_per_fcyc = (float)(262.0 / DCYCS_IN_16MS);
	limit = g_num_border_changes;
	last_line = 0;
	for(i = 0; i < limit; i++) {
		new_line = (int)(border_changes[i].fcycs * flines_per_fcyc);
		new_val = border_changes[i].val;
		if(new_line < 0 || new_line > 262) {
			ki_printf("new_line: %d\n", new_line);
			new_line = last_line;
		}
		for(j = last_line; j < new_line; j++) {
			if(cur_border_colors[j] != color_now) {
				update_border_line(j, color_now);
				cur_border_colors[j] = color_now;
			}
		}
		last_line = new_line;
		color_now = new_val;
	}

	/* check remaining lines */
	if(g_border_last_vbl_changes || limit) {
		for(j = last_line; j < 262; j++) {
			if(cur_border_colors[j] != color_now) {
				update_border_line(j, color_now);
				cur_border_colors[j] = color_now;
			}
		}
	}

#if 0
	if(g_num_border_changes) {
		ki_printf("Border changes: %d\n", g_num_border_changes);
	}
#endif

	if(limit) {
		g_border_last_vbl_changes = 1;
	} else {
		g_border_last_vbl_changes = 0;
	}

	g_num_border_changes = 0;
	g_vbl_border_color = g_border_color;
}

void
update_border_line(int line_in, int color)
{
	word32	*ptr;
	word32	val;
	int	line;
	int	limit;
	int	i;

	val = (color + (g_a2vid_palette << 4));
	val = (val << 24) + (val << 16) + (val << 8) + val;

	if(line_in >= 200) {
		if(line_in >= 262) {
			halt_printf("line_in: %d out of range!\n", line_in);
			line_in = 200;
		}
		line = (line_in - 200) >> 1;

		if(2*line >= (X_A2_WINDOW_HEIGHT - A2_WINDOW_HEIGHT + 2*8) ||
				(line < 0)) {
			ki_printf("Line out of range: %d\n", line);
			line = 0;
		}

		ptr =(word32 *)&(video_data_border_special[2*line*X_A2_WINDOW_WIDTH]);
		limit = (2*X_A2_WINDOW_WIDTH) >> 2;

		g_border_special_refresh_needed = 1;
		for(i = 0; i < limit; i++) {
			*ptr++ = val;
		}
	} else if(line_in >= 192) {
		line = line_in - 192;

		if(line >= 8) {
			ki_printf("Line out of range2: %d\n", line);
			line = 0;
		}

		ptr =(word32 *)&(video_data_text[0][2*(192 + line)*A2_WINDOW_WIDTH]);
		limit = (2*A2_WINDOW_WIDTH) >> 2;

		g_border_line24_refresh_needed = 1;
		for(i = 0; i < limit; i++) {
			*ptr++ = val;
		}

	}
	if(line_in < 200) {
		line = line_in;
		ptr = (word32 *)&(video_data_border_sides[2*line * EFF_BORDER_WIDTH]);
		limit = (2 * EFF_BORDER_WIDTH) >> 2;
		g_border_sides_refresh_needed = 1;
		for(i = 0; i < limit; i++) {
			*ptr++ = val;
		}
	}

}


#define CH_SETUP_A2_VID(mem_ptr, ch_mask, reparse)			\
	ch_ptr = &(slow_mem_changed[mem_ptr >> CHANGE_SHIFT]);		\
	ch_bitpos = 0;							\
	bits_per_line = 40 >> SHIFT_PER_CHANGE;				\
	ch_shift_amount = (mem_ptr >> SHIFT_PER_CHANGE) & 0x1f;		\
	mask_per_line = (-(1 << (32 - bits_per_line)));			\
	mask_per_line = mask_per_line >> ch_shift_amount;		\
	ch_mask = *ch_ptr & mask_per_line;				\
	*ch_ptr = *ch_ptr & (~ch_mask);					\
	ch_mask = ch_mask << ch_shift_amount;				\
									\
	if(reparse) {							\
		ch_mask = - (1 << (32 - bits_per_line));		\
	}

#define CH_LOOP_A2_VID(ch_mask, ch_tmp)					\
		ch_tmp = ch_mask & 0x80000000;				\
		ch_mask = ch_mask << 1;					\
									\
		if(!ch_tmp) {						\
			continue;					\
		}

void
redraw_changed_text_40(int start_offset, int start_line, int reparse,
	byte *screen_data, int altcharset, int bg_val, int fg_val)
{
	word32 mem_ptr;
	byte	val0, val1;
	byte	*b_ptr;
	word32	*img_ptr, *img_ptr2;
	const word32 *font_ptr1;
	const word32 *font_ptr2;
	word32	tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;
	word32	palette_add;
	word32	diff_val;
	word32	and_val;
	word32	add_val;
	word32	ff_val;
	int	flash_state;
	int	y;
	int	x1, x2;
	int	i;
	word32	line_mask;
	word32	*ch_ptr;
	word32	mask_per_line;
	int	ch_bitpos;
	int	bits_per_line;
	int	ch_shift_amount;
	word32	ch_mask;
	word32	ch_tmp;
	byte	*slow_mem_ptr;
	int	shift_per;
	int	left, right;
	register word32 start_time, end_time;


	a2_line_full_left_edge[start_line] = 0 + BORDER_CENTER_X;
	a2_line_full_right_edge[start_line] = 560 + BORDER_CENTER_X;

	y = start_line;
	line_mask = 1 << (y);
	mem_ptr = 0x400 + screen_index[y] + start_offset;
	if(mem_ptr < 0x400 || mem_ptr >= 0xc00) {
		halt_printf("redraw_changed_text: mem_ptr: %08x\n", mem_ptr);
	}

	CH_SETUP_A2_VID(mem_ptr, ch_mask, reparse);

	if(ch_mask == 0) {
		return;
	}

	GET_ITIMER(start_time);

	shift_per = (1 << SHIFT_PER_CHANGE);

	a2_screen_buffer_changed |= line_mask;

	palette_add = (g_a2vid_palette << 4);
	palette_add = palette_add + (palette_add << 8) + (palette_add << 16) +
		(palette_add << 24);

	left = 40;
	right = 0;

	diff_val = (fg_val - bg_val) & 0xf;
	and_val = diff_val + (diff_val << 8) + (diff_val << 16) +(diff_val<<24);
	add_val = bg_val + (bg_val << 8) + (bg_val << 16) + (bg_val << 24);
	ff_val = 0x0f0f0f0f;


	flash_state = (g_cur_a2_stat & ALL_STAT_FLASH_STATE);

	for(x1 = 0; x1 < 40; x1 += shift_per) {

		CH_LOOP_A2_VID(ch_mask, ch_tmp);

		left = MIN(x1, left);
		right = MAX(x1 + shift_per, right);
		slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr + x1]);
		b_ptr = &screen_data[(y*16)*A2_WINDOW_WIDTH + x1*14 + BORDER_CENTER_X];
		img_ptr = (word32 *)b_ptr;
		img_ptr2 = (word32 *)(b_ptr + A2_WINDOW_WIDTH);


		for(x2 = 0; x2 < shift_per; x2 += 2) {
			val0 = *slow_mem_ptr++;
			val1 = *slow_mem_ptr++;

			if(!altcharset) {
				if(val0 >= 0x40 && val0 < 0x80) {
					if(flash_state) {
						val0 += 0x40;
					} else {
						val0 -= 0x40;
					}
				}
				if(val1 >= 0x40 && val1 < 0x80) {
					if(flash_state) {
						val1 += 0x40;
					} else {
						val1 -= 0x40;
					}
				}
			}

			for(i = 0; i < 8; i++) {
				font_ptr1 = &(font40_even_bits[val0][i][0]);
				tmp0 = (font_ptr1[0] & and_val) + add_val;
				tmp1 = (font_ptr1[1] & and_val) + add_val;
				tmp2 = (font_ptr1[2] & and_val) + add_val;

				font_ptr2 = &(font40_odd_bits[val1][i][0]);
				tmp3 = ((font_ptr1[3]+font_ptr2[0]) & and_val)+
						add_val;

				tmp4 = (font_ptr2[1] & and_val) + add_val;
				tmp5 = (font_ptr2[2] & and_val) + add_val;
				tmp6 = (font_ptr2[3] & and_val) + add_val;

				tmp0 = (tmp0 & ff_val) + palette_add;
				tmp1 = (tmp1 & ff_val) + palette_add;
				tmp2 = (tmp2 & ff_val) + palette_add;
				tmp3 = (tmp3 & ff_val) + palette_add;
				tmp4 = (tmp4 & ff_val) + palette_add;
				tmp5 = (tmp5 & ff_val) + palette_add;
				tmp6 = (tmp6 & ff_val) + palette_add;

				img_ptr[0] = tmp0;
				img_ptr[1] = tmp1;
				img_ptr[2] = tmp2;
				img_ptr[3] = tmp3;
				img_ptr[4] = tmp4;
				img_ptr[5] = tmp5;
				img_ptr[6] = tmp6;

				img_ptr2[0] = tmp0;
				img_ptr2[1] = tmp1;
				img_ptr2[2] = tmp2;
				img_ptr2[3] = tmp3;
				img_ptr2[4] = tmp4;
				img_ptr2[5] = tmp5;
				img_ptr2[6] = tmp6;

				img_ptr += (2*A2_WINDOW_WIDTH)/4;
				img_ptr2 += (2*A2_WINDOW_WIDTH)/4;
			}

			img_ptr -= (8*2*A2_WINDOW_WIDTH)/4;
			img_ptr2 -= (8*2*A2_WINDOW_WIDTH)/4;
			img_ptr += 7;
			img_ptr2 += 7;
		}
	}
	GET_ITIMER(end_time);

	a2_line_left_edge[start_line] = (left*14) + BORDER_CENTER_X;
	a2_line_right_edge[start_line] = (right*14) + BORDER_CENTER_X;

	if(left >= right || left < 0 || right < 0) {
		ki_printf("line %d, 40: left >= right: %d >= %d\n",
			start_line, left, right);
	}

	g_cycs_in_40col += (end_time - start_time);

	need_redraw = 0;
}

void
redraw_changed_text_80(int start_offset, int start_line, int reparse,
	byte *screen_data, int altcharset, int bg_val, int fg_val)
{
	word32 mem_ptr;
	byte	val0, val1, val2, val3;
	word32	tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;
	word32	diff_val;
	word32	add_val, and_val, ff_val;
	byte	*b_ptr;
	word32	*img_ptr, *img_ptr2;
	const word32 *font_ptr0, *font_ptr1, *font_ptr2, *font_ptr3;
	word32	palette_add;
	int	flash_state;
	int	y;
	int	x1, x2;
	int	i;
	word32	line_mask;

	word32	*ch_ptr;
	word32	mask_per_line;
	int	ch_bitpos;
	int	bits_per_line;
	int	ch_shift_amount;
	word32	ch_mask;
	word32	ch_tmp;
	byte	*slow_mem_ptr;
	int	shift_per;
	int	left, right;

	a2_line_full_left_edge[start_line] = 0 + BORDER_CENTER_X;
	a2_line_full_right_edge[start_line] = 560 + BORDER_CENTER_X;

	y = start_line;
	line_mask = 1 << y;
	mem_ptr = 0x400 + screen_index[y] + start_offset;
	if(mem_ptr < 0x400 || mem_ptr >= 0xc00) {
		halt_printf("redraw_changed_text: mem_ptr: %08x\n", mem_ptr);
	}

	CH_SETUP_A2_VID(mem_ptr, ch_mask, reparse);

	if(ch_mask == 0) {
		return;
	}

	shift_per = (1 << SHIFT_PER_CHANGE);

	a2_screen_buffer_changed |= line_mask;

	palette_add = (g_a2vid_palette << 4);
	palette_add = palette_add + (palette_add << 8) + (palette_add << 16) +
		(palette_add << 24);

	left = 40;
	right = 0;

	diff_val = (fg_val - bg_val) & 0xf;
	add_val = bg_val + (bg_val << 8) + (bg_val << 16) + (bg_val << 24);
	and_val = diff_val + (diff_val << 8) + (diff_val << 16) +(diff_val<<24);
	ff_val = 0x0f0f0f0f;

	flash_state = (g_cur_a2_stat & ALL_STAT_FLASH_STATE);

	for(x1 = 0; x1 < 40; x1 += shift_per) {
		CH_LOOP_A2_VID(ch_mask, ch_tmp);

		left = MIN(x1, left);
		right = MAX(x1 + shift_per, right);

		slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr + x1]);
		b_ptr = &screen_data[(y*16)*A2_WINDOW_WIDTH + x1*14 + BORDER_CENTER_X];
		img_ptr = (word32 *)b_ptr;
		img_ptr2 = (word32 *)(b_ptr + A2_WINDOW_WIDTH);

		for(x2 = 0; x2 < shift_per; x2 += 2) {
			/*  do 4 chars at once! */

			val1 = slow_mem_ptr[0];
			val3 = slow_mem_ptr[1];
			val0 = slow_mem_ptr[0x10000];
			val2 = slow_mem_ptr[0x10001];
			slow_mem_ptr += 2;

			if(!altcharset) {
				if(val0 >= 0x40 && val0 < 0x80) {
					if(flash_state) {
						val0 += 0x40;
					} else {
						val0 -= 0x40;
					}
				}
				if(val1 >= 0x40 && val1 < 0x80) {
					if(flash_state) {
						val1 += 0x40;
					} else {
						val1 -= 0x40;
					}
				}
				if(val2 >= 0x40 && val2 < 0x80) {
					if(flash_state) {
						val2 += 0x40;
					} else {
						val2 -= 0x40;
					}
				}
				if(val3 >= 0x40 && val3 < 0x80) {
					if(flash_state) {
						val3 += 0x40;
					} else {
						val3 -= 0x40;
					}
				}
			}

			for(i = 0; i < 8; i++) {
				font_ptr0 = &(font80_off0_bits[val0][i][0]);
				tmp0 = (font_ptr0[0] & and_val) + add_val;

				font_ptr3 = &(font80_off3_bits[val1][i][0]);
				tmp1 = ((font_ptr0[1]+font_ptr3[0]) & and_val)+
						add_val;
					/* 3 bytes from ptr0, 1 from ptr3 */
				tmp2 = (font_ptr3[1] & and_val) + add_val;

				font_ptr2 = &(font80_off2_bits[val2][i][0]);
				tmp3 = ((font_ptr3[2]+font_ptr2[0]) & and_val)+
						add_val;
					/* 2 bytes from ptr3, 2 from ptr2*/
				tmp4 = (font_ptr2[1] & and_val) + add_val;

				font_ptr1 = &(font80_off1_bits[val3][i][0]);
				tmp5 = ((font_ptr2[2]+font_ptr1[0]) & and_val)+
						add_val;
					/* 1 byte from ptr2, 3 from ptr1 */
				tmp6 = (font_ptr1[1] & and_val) + add_val;

				tmp0 = (tmp0 & ff_val) + palette_add;
				tmp1 = (tmp1 & ff_val) + palette_add;
				tmp2 = (tmp2 & ff_val) + palette_add;
				tmp3 = (tmp3 & ff_val) + palette_add;
				tmp4 = (tmp4 & ff_val) + palette_add;
				tmp5 = (tmp5 & ff_val) + palette_add;
				tmp6 = (tmp6 & ff_val) + palette_add;

				img_ptr[0] = tmp0;
				img_ptr[1] = tmp1;
				img_ptr[2] = tmp2;
				img_ptr[3] = tmp3;
				img_ptr[4] = tmp4;
				img_ptr[5] = tmp5;
				img_ptr[6] = tmp6;

				img_ptr2[0] = tmp0;
				img_ptr2[1] = tmp1;
				img_ptr2[2] = tmp2;
				img_ptr2[3] = tmp3;
				img_ptr2[4] = tmp4;
				img_ptr2[5] = tmp5;
				img_ptr2[6] = tmp6;

				img_ptr += (2*A2_WINDOW_WIDTH)/4;
				img_ptr2 += (2*A2_WINDOW_WIDTH)/4;
			}

			img_ptr -= (8*2*A2_WINDOW_WIDTH)/4;
			img_ptr2 -= (8*2*A2_WINDOW_WIDTH)/4;
			img_ptr += 7;
			img_ptr2 += 7;

		}
	}

	a2_line_left_edge[start_line] = (left*14)+BORDER_CENTER_X;
	a2_line_right_edge[start_line] = (right*14)+BORDER_CENTER_X;

	if(left >= right || left < 0 || right < 0) {
		ki_printf("line %d, 80: left >= right: %d >= %d\n",
			start_line, left, right);
	}

	need_redraw = 0;
}

void
redraw_changed_gr(int start_offset, int start_line, int reparse,
	byte *screen_data)
{
	word32	*img_ptr;
	byte	*b_ptr;
	word32	mem_ptr;
	word32	line_mask;
	word32	val0, val1;
	word32	val0_wd, val1_wd;
	word32	val01_wd;
	word32	val_even, val_odd;
	word32	palette_add;
	int	half;
	int	x1, x2;
	int	y;
	int	y2;

	word32	*ch_ptr;
	word32	mask_per_line;
	int	ch_bitpos;
	int	bits_per_line;
	int	ch_shift_amount;
	word32	ch_mask;
	word32	ch_tmp;
	byte	*slow_mem_ptr;
	int	shift_per;
	int	left, right;

	a2_line_full_left_edge[start_line] = 0 + BORDER_CENTER_X;
	a2_line_full_right_edge[start_line] = 560 + BORDER_CENTER_X;

	y = start_line;
	line_mask = 1 << y;
	mem_ptr = 0x400 + screen_index[y] + start_offset;
	if(mem_ptr < 0x400 || mem_ptr >= 0xc00) {
		ki_printf("redraw_changed_gr: mem_ptr: %08x\n", mem_ptr);
	}

	CH_SETUP_A2_VID(mem_ptr, ch_mask, reparse);

	if(ch_mask == 0) {
		return;
	}

	shift_per = (1 << SHIFT_PER_CHANGE);

	a2_screen_buffer_changed |= line_mask;

	palette_add = (g_a2vid_palette << 4);
	palette_add = palette_add + (palette_add << 8) + (palette_add << 16) +
		(palette_add << 24);

	left = 40;
	right = 0;

	for(x1 = 0; x1 < 40; x1 += shift_per) {
		CH_LOOP_A2_VID(ch_mask, ch_tmp);

		left = MIN(x1, left);
		right = MAX(x1 + shift_per, right);

		slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr + x1]);
		b_ptr = &screen_data[(y*16)*A2_WINDOW_WIDTH + x1*14 + BORDER_CENTER_X];
		img_ptr = (word32 *)b_ptr;

		for(x2 = 0; x2 < shift_per; x2 += 2) {
			val_even = *slow_mem_ptr++;
			val_odd = *slow_mem_ptr++;

			for(half = 0; half < 2; half++) {
				val0 = val_even & 0xf;
				val1 = val_odd & 0xf;
				val0_wd = (val0 << 24) + (val0 << 16) +
						(val0 << 8) + val0;
				val1_wd = (val1 << 24) + (val1 << 16) +
						(val1 << 8) + val1;
#ifdef KEGS_LITTLE_ENDIAN
				val01_wd = (val1_wd << 16) + (val0_wd & 0xffff);
#else
				val01_wd = (val0_wd << 16) + (val1_wd & 0xffff);
#endif

				for(y2 = 0; y2 < 8; y2++) {
					img_ptr[0] = val0_wd + palette_add;
					img_ptr[1] = val0_wd + palette_add;
					img_ptr[2] = val0_wd + palette_add;
					img_ptr[3] = val01_wd + palette_add;
					img_ptr[4] = val1_wd + palette_add;
					img_ptr[5] = val1_wd + palette_add;
					img_ptr[6] = val1_wd + palette_add;
					img_ptr += (A2_WINDOW_WIDTH)/4;
				}


				val_even = val_even >> 4;
				val_odd = val_odd >> 4;
			}

			img_ptr -= (16*A2_WINDOW_WIDTH)/4;
			img_ptr += 7;
		}
	}

	a2_line_left_edge[start_line] = (left*14) + BORDER_CENTER_X;
	a2_line_right_edge[start_line] = (right*14) + BORDER_CENTER_X;

	need_redraw = 0;
}

void
redraw_changed_dbl_gr(int start_offset, int start_line, int reparse,
	byte *screen_data)
{
	word32	*img_ptr;
	byte	*b_ptr;
	word32	mem_ptr;
	word32	line_mask;
	word32	val0, val1, val2, val3;
	word32	val0_wd, val1_wd, val2_wd, val3_wd;
	word32	val01_wd, val12_wd, val23_wd;
	word32	val_even_main, val_odd_main;
	word32	val_even_aux, val_odd_aux;
	word32	palette_add;
	int	half;
	int	x1, x2;
	int	y;
	int	y2;

	word32	*ch_ptr;
	word32	mask_per_line;
	int	ch_bitpos;
	int	bits_per_line;
	int	ch_shift_amount;
	word32	ch_mask;
	word32	ch_tmp;
	byte	*slow_mem_ptr;
	int	shift_per;
	int	left, right;

	a2_line_full_left_edge[start_line] = 0 + BORDER_CENTER_X;
	a2_line_full_right_edge[start_line] = 560 + BORDER_CENTER_X;

	y = start_line;
	line_mask = 1 << y;
	mem_ptr = 0x400 + screen_index[y] + start_offset;
	if(mem_ptr < 0x400 || mem_ptr >= 0xc00) {
		ki_printf("redraw_changed_gr: mem_ptr: %08x\n", mem_ptr);
	}

	CH_SETUP_A2_VID(mem_ptr, ch_mask, reparse);

	if(ch_mask == 0) {
		return;
	}

	shift_per = (1 << SHIFT_PER_CHANGE);

	a2_screen_buffer_changed |= line_mask;

	palette_add = (g_a2vid_palette << 4);
	palette_add = palette_add + (palette_add << 8) + (palette_add << 16) +
		(palette_add << 24);

	left = 40;
	right = 0;

	for(x1 = 0; x1 < 40; x1 += shift_per) {
		CH_LOOP_A2_VID(ch_mask, ch_tmp);

		left = MIN(x1, left);
		right = MAX(x1 + shift_per, right);

		slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr + x1]);
		b_ptr = &screen_data[(y*16)*A2_WINDOW_WIDTH + x1*14 + BORDER_CENTER_X];
		img_ptr = (word32 *)b_ptr;

		for(x2 = 0; x2 < shift_per; x2 += 2) {
			val_even_main = slow_mem_ptr[0];
			val_odd_main = slow_mem_ptr[1];
			val_even_aux = slow_mem_ptr[0x10000];
			val_odd_aux = slow_mem_ptr[0x10001];
			slow_mem_ptr += 2;

			for(half = 0; half < 2; half++) {
				val0 = val_even_aux & 0xf;
				val1 = val_even_main & 0xf;
				val2 = val_odd_aux & 0xf;
				val3 = val_odd_main & 0xf;

				/* Handle funny pattern of dbl gr aux mem */
				val0 = ((val0 << 1) & 0xf) + (val0 >> 3);
				val2 = ((val2 << 1) & 0xf) + (val2 >> 3);

				val0_wd = (val0 << 24) + (val0 << 16) +
						(val0 << 8) + val0;
				val1_wd = (val1 << 24) + (val1 << 16) +
						(val1 << 8) + val1;
				val2_wd = (val2 << 24) + (val2 << 16) +
						(val2 << 8) + val2;
				val3_wd = (val3 << 24) + (val3 << 16) +
						(val3 << 8) + val3;
#ifdef KEGS_LITTLE_ENDIAN
				val01_wd = (val1_wd << 24) + (val0_wd&0xffffff);
				val12_wd = (val2_wd << 16) + (val1_wd & 0xffff);
				val23_wd = (val3_wd << 8) + (val2_wd & 0xff);
#else
				val01_wd = (val0_wd << 8) + (val1_wd & 0xff);
				val12_wd = (val1_wd << 16) + (val2_wd & 0xffff);
				val23_wd = (val2_wd << 24) + (val3_wd&0xffffff);
#endif

				for(y2 = 0; y2 < 8; y2++) {
					img_ptr[0] = val0_wd + palette_add;
					img_ptr[1] = val01_wd + palette_add;
					img_ptr[2] = val1_wd + palette_add;
					img_ptr[3] = val12_wd + palette_add;
					img_ptr[4] = val2_wd + palette_add;
					img_ptr[5] = val23_wd + palette_add;
					img_ptr[6] = val3_wd + palette_add;
					img_ptr += (A2_WINDOW_WIDTH)/4;
				}

				val_even_aux = val_even_aux >> 4;
				val_even_main = val_even_main >> 4;
				val_odd_aux = val_odd_aux >> 4;
				val_odd_main = val_odd_main >> 4;
			}

			img_ptr -= (16*A2_WINDOW_WIDTH)/4;
			img_ptr += 7;
		}
	}

	a2_line_left_edge[start_line] = (left*14) + BORDER_CENTER_X;
	a2_line_right_edge[start_line] = (right*14) + BORDER_CENTER_X;

	need_redraw = 0;
}

void
redraw_changed_hires(int start_offset, int start_line, int color, int reparse,
	byte *screen_data)
{
	if(!color) {
		redraw_changed_hires_color(start_offset, start_line, reparse,
			screen_data);
	} else {
		redraw_changed_hires_bw(start_offset, start_line, reparse,
			screen_data);
	}
}

void
redraw_changed_hires_bw(int start_offset, int start_line, int reparse,
	byte *screen_data)
{
	word32	*img_ptr, *img_ptr2;
	byte	*b_ptr;
	word32	tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;
	word32	mem_ptr;
	word32	val0, val1;
	word32	val_whole;
	word32	line_mask;
	word32	palette_add;
	int	y;
	int	x1, x2;

	word32	*ch_ptr;
	word32	mask_per_line;
	int	ch_bitpos;
	int	bits_per_line;
	int	ch_shift_amount;
	word32	ch_mask;
	word32	ch_tmp;
	byte	*slow_mem_ptr;
	int	shift_per;
	int	left, right;

	a2_line_full_left_edge[start_line] = 0 + BORDER_CENTER_X;
	a2_line_full_right_edge[start_line] = 560 + BORDER_CENTER_X;

	palette_add = (g_a2vid_palette << 4);
	palette_add = palette_add + (palette_add << 8) + (palette_add << 16) +
		(palette_add << 24);

	left = 40;
	right = 0;

	for(y = 8*start_line; y < 8*(start_line + 1); y++) {
		line_mask = 1 << (y >> 3);
		mem_ptr = 0x2000 + (((y & 7) * 0x400) + screen_index[y >> 3]) +
			start_offset;

		CH_SETUP_A2_VID(mem_ptr, ch_mask, reparse);
	
		if(ch_mask == 0) {
			continue;
		}

	
		/* Hires depends on adjacent bits, so also reparse adjacent */
		/*  regions so that if bits on the edge change, redrawing is */
		/*  correct */
		ch_mask = ch_mask | (ch_mask >> 1) | (ch_mask << 1);

		shift_per = (1 << SHIFT_PER_CHANGE);
	
		a2_screen_buffer_changed |= line_mask;
	
		for(x1 = 0; x1 < 40; x1 += shift_per) {
			CH_LOOP_A2_VID(ch_mask, ch_tmp);
	
			left = MIN(x1, left);
			right = MAX(x1 + shift_per, right);

			slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr + x1]);
            b_ptr = &screen_data[(y*2)*A2_WINDOW_WIDTH + x1*14 + BORDER_CENTER_X];
			img_ptr = (word32 *)b_ptr;
			img_ptr2 = (word32 *)(b_ptr + A2_WINDOW_WIDTH);
	
			for(x2 = 0; x2 < shift_per; x2 += 2) {
				val0 = *slow_mem_ptr++;
				val1 = *slow_mem_ptr++;

				val_whole = ((val1 & 0x7f) << 7) +
								(val0 & 0x7f);

				tmp0 = bw_hires_convert[val_whole & 3];
				val_whole = val_whole >> 2;
				tmp1 = bw_hires_convert[val_whole & 3];
				val_whole = val_whole >> 2;
				tmp2 = bw_hires_convert[val_whole & 3];
				val_whole = val_whole >> 2;
				tmp3 = bw_hires_convert[val_whole & 3];
				val_whole = val_whole >> 2;
				tmp4 = bw_hires_convert[val_whole & 3];
				val_whole = val_whole >> 2;
				tmp5 = bw_hires_convert[val_whole & 3];
				val_whole = val_whole >> 2;
				tmp6 = bw_hires_convert[val_whole & 3];

				img_ptr[0] = tmp0 + palette_add;
				img_ptr[1] = tmp1 + palette_add;
				img_ptr[2] = tmp2 + palette_add;
				img_ptr[3] = tmp3 + palette_add;
				img_ptr[4] = tmp4 + palette_add;
				img_ptr[5] = tmp5 + palette_add;
				img_ptr[6] = tmp6 + palette_add;

				img_ptr2[0] = tmp0 + palette_add;
				img_ptr2[1] = tmp1 + palette_add;
				img_ptr2[2] = tmp2 + palette_add;
				img_ptr2[3] = tmp3 + palette_add;
				img_ptr2[4] = tmp4 + palette_add;
				img_ptr2[5] = tmp5 + palette_add;
				img_ptr2[6] = tmp6 + palette_add;

				img_ptr += 7;
				img_ptr2 += 7;
			}
		}
	}

	a2_line_left_edge[start_line] = (left*14) + BORDER_CENTER_X;
	a2_line_right_edge[start_line] = (right*14) + BORDER_CENTER_X;

	need_redraw = 0;
}

void
redraw_changed_hires_color(int start_offset, int start_line, int reparse,
	byte *screen_data)
{
	word32	*img_ptr, *img_ptr2;
	byte	*b_ptr;
	word32	mem_ptr;
	word32	val0, val1;
	word32	val_whole;
	word32	pix_val;
	word32	line_mask;
	word32	prev_pixel;
	word32	prev_hi;
	word32	loc_hi;
	word32	val_hi;
	word32	tmp_val;
	word32	palette_add;
	int	j;
	int	y;
	int	x1, x2;

	word32	*ch_ptr;
	word32	mask_per_line;
	int	ch_bitpos;
	int	bits_per_line;
	int	ch_shift_amount;
	word32	ch_mask;
	word32	ch_tmp;
	byte	*slow_mem_ptr;
	int	shift_per;
	int	left, right;

	a2_line_full_left_edge[start_line] = 0 + BORDER_CENTER_X;
	a2_line_full_right_edge[start_line] = 560 + BORDER_CENTER_X;

	palette_add = (g_a2vid_palette << 4);
	palette_add = palette_add + (palette_add << 8) + (palette_add << 16) +
		(palette_add << 24);

	left = 40;
	right = 0;

	for(y = 8*start_line; y < 8*(start_line + 1); y++) {
		line_mask = 1 << (y >> 3);
		mem_ptr = 0x2000 + (((y & 7) * 0x400) + screen_index[y >> 3]) +
			start_offset;

		CH_SETUP_A2_VID(mem_ptr, ch_mask, reparse);
	
		if(ch_mask == 0) {
			continue;
		}
	
		/* Hires depends on adjacent bits, so also reparse adjacent */
		/*  regions so that if bits on the edge change, redrawing is */
		/*  correct */
		ch_mask = ch_mask | (ch_mask >> 1) | (ch_mask << 1);

		shift_per = (1 << SHIFT_PER_CHANGE);
	
		a2_screen_buffer_changed |= line_mask;
	
		for(x1 = 0; x1 < 40; x1 += shift_per) {

			CH_LOOP_A2_VID(ch_mask, ch_tmp);
	
			left = MIN(x1, left);
			right = MAX(x1 + shift_per, right);

			slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr + x1]);
            b_ptr = &screen_data[(y*2)*A2_WINDOW_WIDTH + x1*14 + BORDER_CENTER_X];
			img_ptr = (word32 *)b_ptr;
			img_ptr2 = (word32 *)(b_ptr + A2_WINDOW_WIDTH);

			prev_pixel = 0;
			prev_hi = 0;

			if(x1 > 0) {
				tmp_val = slow_mem_ptr[-1];
				prev_pixel = (tmp_val >> 6) & 1;
				prev_hi = (tmp_val >> 7) & 0x1;
			}

			for(x2 = 0; x2 < shift_per; x2 += 2) {
				val0 = *slow_mem_ptr++;
				val1 = *slow_mem_ptr++;

				val_whole = ((val1 & 0x7f) << 8) +
							((val0 & 0x7f) << 1) +
						prev_pixel;

				loc_hi = prev_hi;
				if(((val1 >> 7) & 1) != 0) {
					loc_hi += 0x7f00;
				}
				if(((val0 >> 7) & 1) != 0) {
					loc_hi += 0xfe;
				}

				prev_pixel = (val1 >> 6) & 1;
				prev_hi = (val1 >> 7) & 1;
				if((x1 + x2 + 2) < 40) {
					tmp_val = slow_mem_ptr[0];
					if(tmp_val & 1) {
						val_whole |= 0x8000;
					}
					if(tmp_val & 0x80) {
						loc_hi |= 0x8000;
					}
				}

				loc_hi = loc_hi >> 1;

				for(j = 0; j < 7; j++) {
					tmp_val = val_whole & 0xf;
					val_hi = loc_hi & 0x3;

					pix_val = hires_convert[(val_hi << 4) +
								tmp_val];
					*img_ptr++ = pix_val + palette_add;
					*img_ptr2++ = pix_val + palette_add;
					val_whole = val_whole >> 2;
					loc_hi = loc_hi >> 2;
				}
			}
		}
	}

	a2_line_left_edge[start_line] = (left*14) + BORDER_CENTER_X;
	a2_line_right_edge[start_line] = (right*14) + BORDER_CENTER_X;

	need_redraw = 0;
}


void
redraw_changed_dbl_hires(int start_offset, int start_line, int color,
	int reparse, byte *screen_data)
{
	if(!color) {
		redraw_changed_dbl_hires_color(start_offset, start_line,
				reparse, screen_data);
	} else {
		redraw_changed_dbl_hires_bw(start_offset, start_line, reparse,
				screen_data);
	}
}


void
redraw_changed_dbl_hires_bw(int start_offset, int start_line, int reparse,
	byte *screen_data)
{
	byte	*b_ptr;
	word32	*img_ptr, *img_ptr2;
	word32	mem_ptr;
	word32	val0, val1, val2, val3;
	word32	tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;
	word32	val_whole;
	word32	line_mask;
	word32	palette_add;
	int	y;
	int	x1, x2;

	word32	*ch_ptr;
	word32	mask_per_line;
	int	ch_bitpos;
	int	bits_per_line;
	int	ch_shift_amount;
	word32	ch_mask;
	word32	ch_tmp;
	byte	*slow_mem_ptr;
	int	shift_per;
	int	left, right;

	a2_line_full_left_edge[start_line] = 0 + BORDER_CENTER_X;
	a2_line_full_right_edge[start_line] = 560 + BORDER_CENTER_X;

	palette_add = (g_a2vid_palette << 4);
	palette_add = palette_add + (palette_add << 8) + (palette_add << 16) +
		(palette_add << 24);

	left = 40;
	right = 0;

	for(y = 8*start_line; y < 8*(start_line + 1); y++) {
		line_mask = 1 << (y >> 3);
		mem_ptr = 0x2000 + (((y & 7) * 0x400) + screen_index[y >> 3] +
			start_offset);

		CH_SETUP_A2_VID(mem_ptr, ch_mask, reparse);
	
		if(ch_mask == 0) {
			continue;
		}
	
		shift_per = (1 << SHIFT_PER_CHANGE);
	
		a2_screen_buffer_changed |= line_mask;
	
		for(x1 = 0; x1 < 40; x1 += shift_per) {

			CH_LOOP_A2_VID(ch_mask, ch_tmp);

			left = MIN(x1, left);
			right = MAX(x1 + shift_per, right);
	
			slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr + x1]);
            b_ptr = &screen_data[(y*2)*A2_WINDOW_WIDTH + x1*14 + BORDER_CENTER_X];
			img_ptr = (word32 *)b_ptr;
			img_ptr2 = (word32 *)(b_ptr + A2_WINDOW_WIDTH);

			for(x2 = 0; x2 < shift_per; x2 += 2) {
				val0 = slow_mem_ptr[0x10000];
				val1 = slow_mem_ptr[0];
				val2 = slow_mem_ptr[0x10001];
				val3 = slow_mem_ptr[1];
				slow_mem_ptr += 2;

				val_whole = ((val3 & 0x7f) << 21) +
						((val2 & 0x7f) << 14) +
						((val1 & 0x7f) << 7) +
						(val0 & 0x7f);

				tmp0 = bw_dhires_convert[val_whole & 0xf];
				val_whole = val_whole >> 4;
				tmp1 = bw_dhires_convert[val_whole & 0xf];
				val_whole = val_whole >> 4;
				tmp2 = bw_dhires_convert[val_whole & 0xf];
				val_whole = val_whole >> 4;
				tmp3 = bw_dhires_convert[val_whole & 0xf];
				val_whole = val_whole >> 4;
				tmp4 = bw_dhires_convert[val_whole & 0xf];
				val_whole = val_whole >> 4;
				tmp5 = bw_dhires_convert[val_whole & 0xf];
				val_whole = val_whole >> 4;
				tmp6 = bw_dhires_convert[val_whole & 0xf];

				img_ptr[0] = tmp0 + palette_add;
				img_ptr[1] = tmp1 + palette_add;
				img_ptr[2] = tmp2 + palette_add;
				img_ptr[3] = tmp3 + palette_add;
				img_ptr[4] = tmp4 + palette_add;
				img_ptr[5] = tmp5 + palette_add;
				img_ptr[6] = tmp6 + palette_add;

				img_ptr2[0] = tmp0 + palette_add;
				img_ptr2[1] = tmp1 + palette_add;
				img_ptr2[2] = tmp2 + palette_add;
				img_ptr2[3] = tmp3 + palette_add;
				img_ptr2[4] = tmp4 + palette_add;
				img_ptr2[5] = tmp5 + palette_add;
				img_ptr2[6] = tmp6 + palette_add;

				img_ptr += 7;
				img_ptr2 += 7;
			}
		}
	}

	a2_line_left_edge[start_line] = (left*14) + BORDER_CENTER_X;
	a2_line_right_edge[start_line] = (right*14) + BORDER_CENTER_X;

	need_redraw = 0;
}

void
redraw_changed_dbl_hires_color(int start_offset, int start_line, int reparse,
	byte *screen_data)
{
	byte	*b_ptr;
	word32	*img_ptr, *img_ptr2;
	word32	mem_ptr;
	word32	val0, val1, val2, val3;
	word32	tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;
	word32	val_whole;
	word32	prev_val;
	word32	line_mask;
	word32	palette_add;
	int	y;
	int	x1, x2;

	word32	*ch_ptr;
	word32	mask_per_line;
	int	ch_bitpos;
	int	bits_per_line;
	int	ch_shift_amount;
	word32	ch_mask;
	word32	ch_tmp;
	byte	*slow_mem_ptr;
	int	shift_per;
	int	left, right;

	a2_line_full_left_edge[start_line] = 0 + BORDER_CENTER_X;
	a2_line_full_right_edge[start_line] = 560 + BORDER_CENTER_X;

	palette_add = (g_a2vid_palette << 4);
	palette_add = palette_add + (palette_add << 8) + (palette_add << 16) +
		(palette_add << 24);

	left = 40;
	right = 0;

	for(y = 8*start_line; y < 8*(start_line + 1); y++) {
		line_mask = 1 << (y >> 3);
		mem_ptr = 0x2000 + (((y & 7) * 0x400) + screen_index[y >> 3] +
			start_offset);

		CH_SETUP_A2_VID(mem_ptr, ch_mask, reparse);
	
		if(ch_mask == 0) {
			continue;
		}

		/* dbl-hires also depends on adjacent bits, so reparse */
		/*  adjacent regions so that if bits on the edge change, */
		/*  redrawing is correct */
		ch_mask = ch_mask | (ch_mask >> 1) | (ch_mask << 1);
		ch_mask = -1;
	
		shift_per = (1 << SHIFT_PER_CHANGE);
	
		a2_screen_buffer_changed |= line_mask;
	
		for(x1 = 0; x1 < 40; x1 += shift_per) {

			CH_LOOP_A2_VID(ch_mask, ch_tmp);

			left = MIN(x1, left);
			right = MAX(x1 + shift_per, right);
	
			slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr + x1]);
            b_ptr = &screen_data[(y*2)*A2_WINDOW_WIDTH + x1*14 + BORDER_CENTER_X];
			img_ptr = (word32 *)b_ptr;
			img_ptr2 = (word32 *)(b_ptr + A2_WINDOW_WIDTH);

			for(x2 = 0; x2 < shift_per; x2 += 2) {
				val0 = slow_mem_ptr[0x10000];
				val1 = slow_mem_ptr[0];
				val2 = slow_mem_ptr[0x10001];
				val3 = slow_mem_ptr[1];

				prev_val = 0;
				if((x1 + x2) > 0) {
					prev_val = (slow_mem_ptr[-1] >> 3) &0xf;
				}
				val_whole = ((val3 & 0x7f) << 25) +
						((val2 & 0x7f) << 18) +
						((val1 & 0x7f) << 11) +
						((val0 & 0x7f) << 4) + prev_val;

				tmp0 = g_dhires_convert[val_whole & 0xfff];
				val_whole = val_whole >> 4;
				tmp1 = g_dhires_convert[val_whole & 0xfff];
				val_whole = val_whole >> 4;
				tmp2 = g_dhires_convert[val_whole & 0xfff];
				val_whole = val_whole >> 4;
				tmp3 = g_dhires_convert[val_whole & 0xfff];
				val_whole = val_whole >> 4;
				tmp4 = g_dhires_convert[val_whole & 0xfff];
				val_whole = val_whole >> 4;
				tmp5 = g_dhires_convert[val_whole & 0xfff];
				val_whole = val_whole >> 4;
				if((x1 + x2 + 2) < 40) {
					val_whole += (slow_mem_ptr[0x10002]<<8);
				}
				tmp6 = g_dhires_convert[val_whole & 0xfff];

				img_ptr[0] = tmp0 + palette_add;
				img_ptr[1] = tmp1 + palette_add;
				img_ptr[2] = tmp2 + palette_add;
				img_ptr[3] = tmp3 + palette_add;
				img_ptr[4] = tmp4 + palette_add;
				img_ptr[5] = tmp5 + palette_add;
				img_ptr[6] = tmp6 + palette_add;

				img_ptr2[0] = tmp0 + palette_add;
				img_ptr2[1] = tmp1 + palette_add;
				img_ptr2[2] = tmp2 + palette_add;
				img_ptr2[3] = tmp3 + palette_add;
				img_ptr2[4] = tmp4 + palette_add;
				img_ptr2[5] = tmp5 + palette_add;
				img_ptr2[6] = tmp6 + palette_add;

				slow_mem_ptr += 2;
				img_ptr += 7;
				img_ptr2 += 7;
			}
		}
	}

	a2_line_left_edge[start_line] = (left*14) + BORDER_CENTER_X;
	a2_line_right_edge[start_line] = (right*14) + BORDER_CENTER_X;

	need_redraw = 0;
}

word32	g_saved_palettes[16][8];
int	g_saved_a2vid_palette = -1;
int	g_a2vid_palette_remap[16];
int	g_superhires_palette_checked = 0;

void
check_super_hires_palette_changes(int reparse)
{
	word32	*word_ptr;
	byte	*byte_ptr;
	word32	*ch_ptr;
	word32	ch_mask;
	int	palette_changed;
	int	saved_a2vid_palette, a2vid_palette;
	word32	tmp;
	int	diff0, diff1, diff2;
	int	val0, val1, val2;
	int	diffs;
	int	low_delta, low_color;
	int	delta;
	int	full;
	int	i, j, k;

	palette_changed = 0;

	a2vid_palette = g_a2vid_palette;
	saved_a2vid_palette = g_saved_a2vid_palette;

	g_saved_a2vid_palette = a2vid_palette;

	if(saved_a2vid_palette != a2vid_palette) {
		reparse = 1;
	}

	full = g_installed_full_superhires_colormap;

	ch_ptr = &(slow_mem_changed[0x9e00 >> CHANGE_SHIFT]);
	ch_mask = ch_ptr[0] | ch_ptr[1];
	ch_ptr[0] = 0;
	ch_ptr[1] = 0;

	if(!reparse && ch_mask == 0) {
		/* nothing changed, get out fast */
		return;
	}

	for(i = 0; i < 0x10; i++) {
		word_ptr = (word32 *)&(g_slow_memory_ptr[0x19e00 + i*0x20]);
		diffs = reparse;
		for(j = 0; j < 8; j++) {
			if(word_ptr[j] != g_saved_palettes[i][j]) {
				diffs = 1;
				break;
			}
		}

		if(diffs == 0) {
			continue;
		}

		palette_changed = 1;

		/* first, save this word_ptr into saved_palettes */
		byte_ptr = (byte *)word_ptr;
		for(j = 0; j < 8; j++) {
			g_saved_palettes[i][j] = word_ptr[j];
		}

		if(i == a2vid_palette && !full) {
			/* construct new color approximations from lores */
			for(j = 0; j < 16; j++) {
				tmp = *byte_ptr++;
				val2 = (*byte_ptr++) & 0xf;
				val0 = tmp & 0xf;
				val1 = (tmp >> 4) & 0xf;
				low_delta = 0x1000;
				low_color = 0x0;
				for(k = 0; k < 16; k++) {
					diff0 = g_expanded_col_0[k] - val0;
					diff1 = g_expanded_col_1[k] - val1;
					diff2 = g_expanded_col_2[k] - val2;
					if(diff0 < 0) {
						diff0 = -diff0;
					}
					if(diff1 < 0) {
						diff1 = -diff1;
					}
					if(diff2 < 0) {
						diff2 = -diff2;
					}
					delta = diff0 + diff1 + diff2;
					if(delta < low_delta) {
						low_delta = delta;
						low_color = k;
					}
				}

				g_a2vid_palette_remap[j] = low_color;
			}
		}

		byte_ptr = (byte *)word_ptr;
		/* this palette has changed */
		for(j = 0; j < 16; j++) {
			val0 = *byte_ptr++;
			val1 = *byte_ptr++;
			video_update_color(i*16 + j, (val1<<8) + val0);
		}
	}

	if(palette_changed) {
		g_palette_changed++;
	}
}

#define SUPER_TYPE	redraw_changed_super_hires_oneline_norm_320
#define SUPER_A2VID	0
#define SUPER_MODE640	0
#define SUPER_FILL	0
#include "superhires.h"
#undef	SUPER_TYPE
#undef	SUPER_A2VID
#undef	SUPER_MODE640
#undef	SUPER_FILL

#define SUPER_TYPE	redraw_changed_super_hires_oneline_norm_640
#define SUPER_A2VID	0
#define SUPER_MODE640	1
#define SUPER_FILL	0
#include "superhires.h"
#undef	SUPER_TYPE
#undef	SUPER_A2VID
#undef	SUPER_MODE640
#undef	SUPER_FILL

#define SUPER_TYPE	redraw_changed_super_hires_oneline_a2vid_320
#define SUPER_A2VID	1
#define SUPER_MODE640	0
#define SUPER_FILL	0
#include "superhires.h"
#undef	SUPER_TYPE
#undef	SUPER_A2VID
#undef	SUPER_MODE640
#undef	SUPER_FILL

#define SUPER_TYPE	redraw_changed_super_hires_oneline_a2vid_640
#define SUPER_A2VID	1
#define SUPER_MODE640	1
#define SUPER_FILL	0
#include "superhires.h"
#undef	SUPER_TYPE
#undef	SUPER_A2VID
#undef	SUPER_MODE640
#undef	SUPER_FILL

#define SUPER_TYPE	redraw_changed_super_hires_oneline_fill_320
#define SUPER_A2VID	0
#define SUPER_MODE640	0
#define SUPER_FILL	1
#include "superhires.h"
#undef	SUPER_TYPE
#undef	SUPER_A2VID
#undef	SUPER_MODE640
#undef	SUPER_FILL

#define SUPER_TYPE	redraw_changed_super_hires_oneline_a2vid_fill_320
#define SUPER_A2VID	1
#define SUPER_MODE640	0
#define SUPER_FILL	1
#include "superhires.h"
#undef	SUPER_TYPE
#undef	SUPER_A2VID
#undef	SUPER_MODE640
#undef	SUPER_FILL


void
redraw_changed_super_hires(int start_offset, int start_line, int in_reparse,
	byte *screen_data)
{
	word32	all_checks;
	word32	check[8];
	word32	this_check;
	int	line;
	int	y;
	int	i;
	word32	tmp;
	word32	line_mask;
	int	a2vid_palette;
	int	type;

	word32	*ch_ptr;
	word32	mask_per_line;
	int	ch_bitpos;
	int	bits_per_line;

	word32	pal;
	word32	scan;
	word32	old_scan;
	int	left, right;
	word32	kd_tmp_debug;


	if(!g_superhires_palette_checked) {
		g_superhires_palette_checked = 1;
		check_super_hires_palette_changes(in_reparse);
	}

	kd_tmp_debug = a2_screen_buffer_changed;

	a2_line_full_left_edge[start_line] = 0;
	a2_line_full_right_edge[start_line] = 640;

	line_mask = 1 << (start_line);

	ch_ptr = &(slow_mem_changed[(0x2000 + 8*start_line*0xa0) >>
								CHANGE_SHIFT]);
	ch_bitpos = 0;
	bits_per_line = 160 >> SHIFT_PER_CHANGE;
	mask_per_line = -(1 << (32 - bits_per_line));

	/* handle palette counting */
	for(y = 8*start_line; y < 8*(start_line + 1); y++) {
		scan = g_slow_memory_ptr[0x19d00 + y];
		pal = scan & 0xf;
		g_shr_palette_used[pal]++;
	}
	
	if(SHIFT_PER_CHANGE != 3) {
		halt_printf("SHIFT_PER_CHANGE must be 3!\n");
		return;
	}

	check[0] = ch_ptr[0] & mask_per_line;
	check[1] = ((ch_ptr[0] << 20) + (ch_ptr[1] >> 12)) & mask_per_line;
	check[2] = ((ch_ptr[1] << 8)) & mask_per_line;
	check[3] = ((ch_ptr[1] << 28) + (ch_ptr[2] >> 4)) & mask_per_line;
	check[4] = ((ch_ptr[2] << 16) + (ch_ptr[3] >> 16)) & mask_per_line;
	check[5] = ((ch_ptr[3] << 4)) & mask_per_line;
	check[6] = ((ch_ptr[3] << 24) + (ch_ptr[4] >> 8)) & mask_per_line;
	check[7] = ((ch_ptr[4] << 12)) & mask_per_line;

	ch_ptr[0] = 0;
	ch_ptr[1] = 0;
	ch_ptr[2] = 0;
	ch_ptr[3] = 0;
	ch_ptr[4] = 0;

	a2vid_palette = g_a2vid_palette;
	if(g_installed_full_superhires_colormap) {
		a2vid_palette = -1;
	}

	all_checks = 0;
	for(line = 0; line < 8; line++) {
		y = 8*start_line + line;
		scan = g_slow_memory_ptr[0x19d00 + y];
		old_scan = superhires_scan_save[y];
		superhires_scan_save[y] = scan;
		this_check = check[line];
		if(in_reparse || (scan ^ old_scan) & 0xaf) {
			this_check = -1;
		}

		if(this_check == 0) {
			continue;
		}

		type = ((scan & 0xa0) >> 5) +
				(((scan & 0xf) == a2vid_palette) << 1);
		if(type & 1) {
			/* fill mode--redraw whole line */
			this_check = -1;
		}

		all_checks |= this_check;

		a2_screen_buffer_changed |= line_mask;


		switch(type) {
		case 0:	/* no_a2vid, 320, nofill */
			redraw_changed_super_hires_oneline_norm_320(
				screen_data, y, scan, this_check);
			break;
		case 1:	/* no_a2vid, 320, fill */
			redraw_changed_super_hires_oneline_fill_320(
				screen_data, y, scan, this_check);
			break;
		case 2:	/* a2vid, 320, nofill */
			redraw_changed_super_hires_oneline_a2vid_320(
				screen_data, y, scan, this_check);
			break;
		case 3:	/* a2vid, 320, fill */
			redraw_changed_super_hires_oneline_a2vid_fill_320(
				screen_data, y, scan, this_check);
			break;
		case 4:	/* no_a2vid, 640, nofill */
			redraw_changed_super_hires_oneline_norm_640(
				screen_data, y, scan, this_check);
			break;
		case 5:	/* no_a2vid, 640, fill */
			/* No fill mode in 640 */
			redraw_changed_super_hires_oneline_norm_640(
				screen_data, y, scan, this_check);
			break;
		case 6:	/* a2vid, 640, nofill */
			redraw_changed_super_hires_oneline_a2vid_640(
				screen_data, y, scan, this_check);
			break;
		case 7:	/* a2vid, 640, fill */
			/* no fill mode in 640 */
			redraw_changed_super_hires_oneline_a2vid_640(
				screen_data, y, scan, this_check);
			break;
		default:
			halt_printf("type: %d bad!\n", type);
		}
	}

	left = 4*40;
	right = 0;

	tmp = all_checks;
	if(all_checks) {
		for(i = 0; i < 160; i += 8) {
			if(tmp & 0x80000000) {
				left = MIN(i, left);
				right = MAX(i + 8, right);
			}
			tmp = tmp << 1;
		}
	}

	a2_line_left_edge[start_line] = (4*left);
	a2_line_right_edge[start_line] = (4*right);

	if((a2_screen_buffer_changed & (1 << start_line)) != 0) {
		if(((g_full_refresh_needed & (1 << start_line)) == 0) &&
							left >= right) {
			halt_printf("shr: line: %d, left: %d, right:%d\n",
				start_line, left, right);
			ki_printf("mask_per_line: %08x, all_checks: %08x\n",
				mask_per_line, all_checks);
			for(i = 0; i < 8; i++) {
				ki_printf("check[%d] = %08x\n", i, check[i]);
			}
			ki_printf("a2_screen_chang: %08x, init_left: %d, "
				"init_right:%d\n", kd_tmp_debug,
				a2_line_full_left_edge[start_line],
				a2_line_full_right_edge[start_line]);
#ifdef HPUX
			U_STACK_TRACE();
#endif
		}
	}

	need_redraw = 0;
}

void
display_screen()
{
	int	i;

	for(i = 0; i < 25; i++) {
		a2_line_must_reparse[i] = 1;
		a2_line_full_left_edge[i] = 0 + BORDER_CENTER_X;
		a2_line_full_right_edge[i] = 560 + BORDER_CENTER_X;
	}

	refresh_screen();
}

void
video_full_redraw()
{
    int i;
    for(i = 0; i < 25; i++) {
        update_a2_ptrs(i, a2_line_stat[i]);
    }
    //a2_screen_buffer_changed = -1;
    //g_full_refresh_needed = -1;
    
    //g_border_sides_refresh_needed = 1;
    //g_border_special_refresh_needed = 1;
    //g_status_refresh_needed = 1;
    g_border_last_vbl_changes = 1;
    {
        int i;
        for(i = 0; i < 262; i++) {
            cur_border_colors[i] ^= 1;;
        }
    }
    video_update_physical_colormap();    
    display_screen();
}

word32 g_cycs_in_refresh_line = 0;
word32 g_cycs_in_refresh_video_image = 0;

int	g_num_lines_a2vid = 0;
int	g_num_lines_superhires = 0;

void
refresh_screen()
{
	register word32 start_time, start_time2;
	register word32 end_time;
	int	previous_superhires;
	int	i;


	GET_ITIMER(start_time);

	previous_superhires = g_num_lines_superhires;
	g_num_lines_superhires = 0;
	g_num_lines_a2vid = 0;
	g_superhires_palette_checked = 0;

	GET_ITIMER(start_time2);
	for(i = 0; i < 25; i++) {
		refresh_line(i);
	}

	if(g_palette_changed) {
		video_update_physical_colormap();
		g_palette_changed = 0;
	}
	if(previous_superhires && !g_num_lines_superhires) {
		/* switched out from superhires--refresh */
		g_border_sides_refresh_needed = 1;
	}

	if(g_status_refresh_needed) {
		g_status_refresh_needed = 0;
		video_redraw_status_lines();
	}

	GET_ITIMER(end_time);
	g_cycs_in_refresh_line += (end_time - start_time);

	GET_ITIMER(start_time);
	video_refresh_image();
	GET_ITIMER(end_time);
	g_cycs_in_refresh_video_image += (end_time - start_time);
}

void
refresh_line(int line)
{
	byte	*ptr;
	int	must_reparse;
	int	stat;
	int	mode;
	int	dbl;
	int	page;
	int	color;
	int	altchar, bg_color, text_color;

	stat = a2_line_stat[line];
	must_reparse = a2_line_must_reparse[line];
	ptr = a2_line_ptr[line];

	a2_line_left_edge[line] = 640;
	a2_line_right_edge[line] = 0;


	dbl = stat & 1;
	color = (stat >> 1) & 1;
	page = (stat >> 2) & 1;
	mode = (stat >> 4) & 7;

#if 0
	ki_printf("refresh line: %d, stat: %04x\n", line, stat);
#endif

	switch(mode) {
	case MODE_TEXT:
		g_num_lines_a2vid++;

		altchar = (stat >> 7) & 1;
		bg_color = (stat >> 8) & 0xf;
		text_color = (stat >> 12) & 0xf;
		if(dbl) {
			redraw_changed_text_80(0x000 + page*0x400, line,
				must_reparse, ptr, altchar, bg_color,
				text_color);
		} else {
			redraw_changed_text_40(0x000 + page*0x400, line,
				must_reparse, ptr, altchar, bg_color,
				text_color);
		}
		break;
	case MODE_GR:
		g_num_lines_a2vid++;
		if(dbl) {
			redraw_changed_dbl_gr(0x000 + page*0x400, line,
				must_reparse, ptr);
		} else {
			redraw_changed_gr(0x000 + page*0x400, line,
				must_reparse, ptr);
		}
		break;
	case MODE_HGR:
		g_num_lines_a2vid++;
		if(dbl) {
			redraw_changed_dbl_hires(0x000 + page*0x2000, line,
				color, must_reparse, ptr);
		} else {
			redraw_changed_hires(0x000 + page*0x2000, line,
				color, must_reparse, ptr);
		}
		break;
	case MODE_SUPER_HIRES:
		g_num_lines_superhires++;
		redraw_changed_super_hires(0, line, must_reparse, ptr);
		break;
	case MODE_BORDER:
		g_num_lines_a2vid++;
		if(line != 24) {
			halt_printf("Border line not 24!\n");
		}
		a2_line_full_left_edge[line] = 0 + BORDER_CENTER_X;
		a2_line_left_edge[line] = 0 + BORDER_CENTER_X;
		a2_line_full_right_edge[line] = 560 + BORDER_CENTER_X;
		a2_line_right_edge[line] = 560 + BORDER_CENTER_X;
		if(g_border_line24_refresh_needed) {
			g_border_line24_refresh_needed = 0;
			a2_screen_buffer_changed |= (1 << 24);
		}
		break;
	default:
		halt_printf("refresh screen: mode: 0x%02x unknown!\n", mode);
		my_exit(7);
	}

	a2_line_must_reparse[line] = 0;
}

void
read_a2_font()
{


	byte	*f40_e_ptr;
	byte	*f40_o_ptr;
	byte	*f80_0_ptr, *f80_1_ptr, *f80_2_ptr, *f80_3_ptr;
	int	char_num;
	int	j, k;
	int	val0;
	int	mask;
	int	pix;

	for(char_num = 0; char_num < 0x100; char_num++) {
		for(j = 0; j < 8; j++) {
			val0 = font_array[char_num][j];

			mask = 0x80;

			for(k = 0; k < 3; k++) {
				font80_off0_bits[char_num][j][k] = 0;
				font80_off1_bits[char_num][j][k] = 0;
				font80_off2_bits[char_num][j][k] = 0;
				font80_off3_bits[char_num][j][k] = 0;
				font40_even_bits[char_num][j][k] = 0;
				font40_odd_bits[char_num][j][k] = 0;
			}
			font40_even_bits[char_num][j][3] = 0;
			font40_odd_bits[char_num][j][3] = 0;

			f40_e_ptr = (byte *)&font40_even_bits[char_num][j][0];
			f40_o_ptr = (byte *)&font40_odd_bits[char_num][j][0];

			f80_0_ptr = (byte *)&font80_off0_bits[char_num][j][0];
			f80_1_ptr = (byte *)&font80_off1_bits[char_num][j][0];
			f80_2_ptr = (byte *)&font80_off2_bits[char_num][j][0];
			f80_3_ptr = (byte *)&font80_off3_bits[char_num][j][0];

			for(k = 0; k < 7; k++) {
				pix = 0;
				if(val0 & mask) {
					pix = 0xf;
				}

				f40_e_ptr[2*k] = pix;
				f40_e_ptr[2*k+1] = pix;

				f40_o_ptr[2*k+2] = pix;
				f40_o_ptr[2*k+3] = pix;

				f80_0_ptr[k] = pix;
				f80_1_ptr[k+1] = pix;
				f80_2_ptr[k+2] = pix;
				f80_3_ptr[k+3] = pix;

				mask = mask >> 1;
			}
		}
	}
}


#define MAX_STATUS_LINES	7
#define X_LINE_LENGTH		88


char	g_video_status_buf[MAX_STATUS_LINES][X_LINE_LENGTH + 1];

void
update_status_line(int line, const char *string)
{
	char	*buf;
	const char *ptr;
	int	i;

	if(line >= MAX_STATUS_LINES || line < 0) {
		ki_printf("update_status_line: line: %d!\n", line);
		my_exit(1);
	}

	ptr = string;
	buf = &(g_video_status_buf[line][0]);
	for(i = 0; i < X_LINE_LENGTH; i++) {
		if(*ptr) {
			buf[i] = *ptr++;
		} else {
			buf[i] = ' ';
		}
	}

	buf[X_LINE_LENGTH] = 0;
}

