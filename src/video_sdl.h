/****************************************************************/
/*    	Apple IIgs emulator                                     */
/*                                                              */
/*    This code may not be used in a commercial product         */
/*    without prior written permission of the authors.          */
/*                                                              */
/*    SDL Code by Frederic Devernay	                        */
/*    Mac OS X port by Darrell Walisser & Benoit Dardelet	*/
/*    You may freely distribute this code.                      */ 
/*                                                              */
/****************************************************************/

#ifndef KEGS_VIDEO_SDL_H
#define KEGS_VIDEO_SDL_H

#include "videodriver.h"
#ifdef HAVE_VIDEO_SDL
#include <SDL/SDL.h>

/* some mousetext characters */
enum { A2_CLOSEDAPPLE = 192,
       A2_OPENAPPLE = 193,
       A2_POINTER = 194,
       A2_HOURGLASS = 195,
       A2_CHECK = 196,
       A2_CHECKINV = 197,
       A2_RETURNINV = 198,
       A2_TITLEBAR = 199,
       A2_LARROW = 200,
       A2_ELLIPSIS = 201,
       A2_DARROW = 202,
       A2_UARROW = 203,
       A2_OVERLINE = 204,
       A2_RETURN = 205,
       A2_SOLID = 206,
       A2_LSCROLL = 207,
       A2_RSCROLL = 208,
       A2_DSCROLL = 209,
       A2_USCROLL = 210,
       A2_CENTERLINE = 211,
       A2_BLCORNER = 212,
       A2_RARROW = 213,
       A2_GREY1 = 214,
       A2_GREY2 = 215,
       A2_FOLDER1 = 216,
       A2_FOLDER2 = 217,
       A2_RIGHTLINE = 218,
       A2_DIAMOND = 219,
       A2_OVERUNDERLINE = 220,
       A2_CROSS = 221,

       A2_LEFTLINE = 223
};


extern SDL_Surface *screen;

void sdl_draw_string(SDL_Surface *surf, Sint16 x, Sint16 y, const unsigned char *string, Uint16 maxstrlen, Uint16 xscale, Uint16 yscale, Uint8 fg, Uint8 bg);
void configuration_menu_sdl(void);
void sdl_warp_pointer(void);

    /* The dirty rectangles for fast Mac OSX drawing */
typedef struct {
	SDL_Rect *rects;
	int       size;
	int       count;
} rect_table;

extern rect_table *gRectTable;
void sdl_update_dirty_rects(rect_table *table, SDL_Surface *screen);

#endif

#endif /* KEGS_VIDEO_SDL_H */
