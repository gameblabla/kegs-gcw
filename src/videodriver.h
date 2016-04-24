#ifndef KEGS_VIDEODRIVER_H
#define KEGS_VIDEODRIVER_H
#include "defc.h"

#ifndef X_DISPLAY_MISSING
#ifndef DISABLE_VIDEO_X11
#define HAVE_VIDEO_X11
#endif
#endif

#ifdef HAVE_SDL
#ifndef DISABLE_VIDEO_SDL
#define HAVE_VIDEO_SDL
#endif
#endif

#ifdef WIN32
#ifndef DISABLE_VIDEO_WIN32
#define HAVE_VIDEO_WIN32
#endif
#endif

#if defined(HAVE_VIDEO_WIN32)
#define VIDEO_DEFAULT VIDEO_WIN32
#elif defined(HAVE_VIDEO_SDL)
#define VIDEO_DEFAULT VIDEO_SDL
#elif defined(HAVE_VIDEO_X11)
#define VIDEO_DEFAULT VIDEO_X11
#else
#define VIDEO_DEFAULT VIDEO_NONE
#endif

#define VIDEO_NONE 0
#define VIDEO_X11 1
#define VIDEO_WIN32 2
#define VIDEO_SDL 3

typedef enum { KEGS_640X480, KEGS_640X400, KEGS_FULL } videomode_e;
extern videomode_e g_videomode;
extern int g_fullscreen;


extern int g_video_fast;
extern word32 g_cycs_in_xredraw;
extern word32 g_refresh_bytes_xfer;

extern void *video_image_text[2];
extern void *video_image_hires[2];
extern void *video_image_superhires;

extern byte *video_data_text[2];
extern byte *video_data_hires[2];
extern byte *video_data_superhires;
extern byte *video_data_border_special;
extern byte *video_data_border_sides;
extern byte *video_data_status_lines;

/* these have device-specific implementations */
int video_init_device(int);
void video_shutdown(void);
void video_update_physical_colormap(void);
void video_update_color(int col_num, int a2_color);
void video_check_input_events(void);
void video_redraw_status_lines(void);
void video_refresh_image(void);
void video_auto_repeat_on(int must);
void video_auto_repeat_off(int must);
void video_warp_pointer(void);

int video_init_x11();
void video_shutdown_x11(void);
void video_update_physical_colormap_x11(void);
void video_update_color_x11(int col_num, int a2_color);
void video_check_input_events_x11(void);
void video_redraw_status_lines_x11(void);
void video_refresh_image_x11(void);
void video_auto_repeat_on_x11(int must);
void video_auto_repeat_off_x11(int must);
void video_warp_pointer_x11(void);

int video_init_win32();
void video_shutdown_win32(void);
void video_update_physical_colormap_win32(void);
void video_update_color_win32(int col_num, int a2_color);
void video_check_input_events_win32(void);
void video_redraw_status_lines_win32(void);
void video_refresh_image_win32(void);
void video_auto_repeat_on_win32(int must);
void video_auto_repeat_off_win32(int must);
void video_warp_pointer_win32(void);

int video_init_sdl();
void video_shutdown_sdl(void);
void video_update_physical_colormap_sdl(void);
void video_update_color_sdl(int col_num, int a2_color);
void video_check_input_events_sdl(void);
void video_redraw_status_lines_sdl(void);
void video_refresh_image_sdl(void);
void video_auto_repeat_on_sdl(int must);
void video_auto_repeat_off_sdl(int must);
void video_warp_pointer_sdl(void);

int get_video_devtype(void);
int set_video_devtype(int);
int get_fullscreen(void);
int set_fullscreen(int);
int get_videomode(void);
int set_videomode(int);

#endif /* KEGS_VIDEODRIVER_H */
