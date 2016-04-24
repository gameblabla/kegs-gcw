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

static const char rcsid[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/video_driver.c,v 1.3 2005/09/23 12:37:09 fredyd Exp $";

#include "video.h"
#include "videodriver.h"
#include "ki.h"

int g_fullscreen = 0;
int g_video_fast;
word32 g_cycs_in_xredraw = 0;
word32 g_refresh_bytes_xfer = 0;
videomode_e g_videomode = KEGS_FULL;

void *video_image_text[2];
void *video_image_hires[2];
void *video_image_superhires;

byte *video_data_text[2];
byte *video_data_hires[2];
byte *video_data_superhires;
byte *video_data_border_special;
byte *video_data_border_sides;
byte *video_data_status_lines;

static int g_video_devtype;

int
get_video_devtype()
{
    return g_video_devtype;
}

int
set_video_devtype(int devtype)
{
    video_shutdown();
    video_init_device(devtype);
    video_full_redraw();
    video_warp_pointer();
    return 1;
}

int
video_init_device(int devtype)
{
    g_video_devtype = VIDEO_NONE;

    switch(devtype) {
    case VIDEO_SDL:
        if(video_init_sdl()) {
            g_video_devtype = VIDEO_SDL;
            return 1;
        }
        break;
    default:
        ki_printf("video_init_device: unknown device type %d\n", devtype);
        break;
    }
    ki_printf("failed initializing video device type %d\n",devtype);
    return 0;
}

void
video_shutdown()
{
    switch(g_video_devtype) {
    case VIDEO_SDL:
        video_shutdown_sdl();
        break;
    default:
        break;
    }
}

void
video_update_physical_colormap()
{
    switch(g_video_devtype) {
    case VIDEO_SDL:
        video_update_physical_colormap_sdl();
        break;
    default:
        break;
    }
}

void
video_warp_pointer()
{
    switch(g_video_devtype) {
    case VIDEO_SDL:
        video_warp_pointer_sdl();
        break;
    default:
        break;
    }
}

void
video_check_input_events()
{
    switch(g_video_devtype) {
    case VIDEO_SDL:
        video_check_input_events_sdl();
        break;
    default:
        break;
    }
}

void
video_update_color(int col_num, int a2_color)
{
    switch(g_video_devtype) {
    case VIDEO_SDL:
        video_update_color_sdl(col_num, a2_color);
        break;
    default:
        break;
    }
}

void
video_redraw_status_lines(void)
{
    switch(g_video_devtype) {
    case VIDEO_SDL:
        video_redraw_status_lines_sdl();
        break;
    default:
        break;
    }
}

void
video_refresh_image()
{
    switch(g_video_devtype) {
    case VIDEO_SDL:
        video_refresh_image_sdl();
        break;
    default:
        break;
    }
}

void
video_auto_repeat_on(int must)
{
    switch(g_video_devtype) {
    case VIDEO_SDL:
        video_auto_repeat_on_sdl(must);
        break;
    default:
        break;
    }
}

void
video_auto_repeat_off(int must)
{
    switch(g_video_devtype) {
    case VIDEO_SDL:
        video_auto_repeat_off_sdl(must);
        break;
    default:
        break;
    }
}

int
get_videomode(void)
{
    return g_videomode;
}

int
set_videomode(int val)
{
    int retval;

    g_videomode = val;
    video_shutdown();
    retval = video_init_device(get_video_devtype());
    if(!retval)
        return 0;
    video_full_redraw();
    video_warp_pointer();
    return 1;
}

int
get_fullscreen(void)
{
    return g_fullscreen;
}

int
set_fullscreen(int val)
{
    g_fullscreen = val;
    video_shutdown();
    video_init_device(get_video_devtype());
    video_full_redraw();
    video_warp_pointer();
    return (g_fullscreen == val);
}


