#ifndef KEGS_VIDEO_H
#define KEGS_VIDEO_H

#include "videodriver.h"

#define MAX_STATUS_LINES	7
#define X_LINE_LENGTH		88
#define STATUS_LINES_WIDTH 640
#define STATUS_LINES_HEIGHT 80

#define BORDER_WIDTH		32
#define BASE_MARGIN_TOP		32
#define BASE_MARGIN_BOTTOM	30
/*
#define BORDER_WIDTH		0
#define BASE_MARGIN_TOP		0
#define BASE_MARGIN_BOTTOM	0
*/

/* to center text/gr/hgr */
#define BORDER_CENTER_X (A2_WINDOW_WIDTH-40*14)/2

#define EFF_BORDER_WIDTH	(BORDER_WIDTH + BORDER_CENTER_X)

#define BASE_MARGIN_LEFT	BORDER_WIDTH
#define BASE_MARGIN_RIGHT	BORDER_WIDTH

#define A2_BORDER_TOP		0
#define A2_BORDER_BOTTOM	0
#define A2_BORDER_LEFT		0
#define A2_BORDER_RIGHT		0
#define A2_WINDOW_WIDTH		(640 + A2_BORDER_LEFT + A2_BORDER_RIGHT)
#define A2_WINDOW_HEIGHT	(400 + A2_BORDER_TOP + A2_BORDER_BOTTOM)

#if 0
#define A2_DATA_OFF		(A2_BORDER_TOP*A2_WINDOW_WIDTH + A2_BORDER_LEFT)
#endif

#define X_A2_WINDOW_WIDTH	(A2_WINDOW_WIDTH + BASE_MARGIN_LEFT + \
							BASE_MARGIN_RIGHT)
#define X_A2_WINDOW_HEIGHT	(A2_WINDOW_HEIGHT + BASE_MARGIN_TOP + \
							BASE_MARGIN_BOTTOM)

#define STATUS_WINDOW_HEIGHT	(7*13)

#define BASE_WINDOW_WIDTH	(X_A2_WINDOW_WIDTH)
#define BASE_WINDOW_HEIGHT	(X_A2_WINDOW_HEIGHT + STATUS_WINDOW_HEIGHT)


#define A2_BORDER_COLOR_NUM	0xfe

#if 0
#define A2_TEXT_COLOR_ALT_NUM	0x01
#define A2_BG_COLOR_ALT_NUM	0x00
#define A2_TEXT_COLOR_PRIM_NUM	0x02
#define A2_BG_COLOR_PRIM_NUM	0x00
#define A2_TEXT_COLOR_FLASH_NUM	0x0c
#define A2_BG_COLOR_FLASH_NUM	0x08
#endif



extern char g_video_status_buf[MAX_STATUS_LINES][X_LINE_LENGTH + 1];
extern int g_show_screen;
extern word32 slow_mem_changed[SLOW_MEM_CH_SIZE];
extern int g_cur_a2_stat;
extern word32 g_full_refresh_needed;
extern int g_border_sides_refresh_needed;
extern int g_border_special_refresh_needed;
extern int g_status_refresh_needed;
extern const int lores_colors[16];
extern int g_a2vid_palette;
extern int g_installed_full_superhires_colormap;
extern int g_screen_redraw_skip_amt;
extern int g_use_dhr140;
extern word32 g_cycs_in_refresh_video_image;
extern word32 g_cycs_in_check_input;
extern word32 g_cycs_in_refresh_line;
extern word32 g_cycs_in_40col;

extern word32 a2_screen_buffer_changed;
extern void *a2_line_xim[25];
extern int a2_line_stat[25];
extern int a2_line_left_edge[25];
extern int a2_line_right_edge[25];
extern int a2_line_full_left_edge[25];
extern int a2_line_full_right_edge[25];

extern byte font_array[256][8];

void update_status_line(int line, const char *string);
void show_a2_line_stuff(void);
void refresh_screen(void);
void change_display_mode(double dcycs);
void change_border_color(double dcycs, int val);
void video_init(int);
void video_reset(void);
void video_update(void);
void end_screen(void);
void change_a2vid_palette(int new_palette);
void video_full_redraw(void);

#endif
