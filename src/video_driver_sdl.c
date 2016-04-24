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

#include <assert.h>
#include "sim65816.h"
#include "video.h"
#include "videodriver.h"
#include "video_sdl.h"
#include "dis.h"
#include "adb.h"
#include "functions.h"
#include "engine.h" 

/* these ones are for callbacks variables */
#include "iwm.h"
#include "paddles.h"

#ifdef HAVE_VIDEO_SDL
short mouse_global_x = 0, mouse_global_y = 0;
rect_table* sdl_alloc_dirty_rects();
void sdl_reset_dirty_rects(rect_table *table);
void sdl_add_dirty_rect(rect_table *table, SDL_Rect rect);

static void sdl_refresh_lines(SDL_Surface *xim, int start_line, int end_line, int left_pix, int right_pix);
static void sdl_redraw_border_sides_lines(int end_x, int width, int start_line, int end_line);
static void sdl_refresh_border_sides(void);
static void sdl_refresh_border_special(void);
static SDL_Surface *get_sdlsurface(byte **data_ptr, int extended_info);
static void sdl_refresh_image (SDL_Surface *hbm, int srcx, int srcy,
                               int destx,int desty, int width,int height);
static void process_keysym(SDLKey, int);
SDL_Surface *screen = NULL;
static SDL_Surface *video_image_border_special;
static SDL_Surface *video_image_border_sides;
static SDL_Surface *video_image_status_lines = NULL;

static SDL_Color a2v_palette[256];
static SDL_Color shires_palette[256];
static int g_window_width;
static int g_window_height;
static int g_num_a2_keycodes;
static SDL_Color graymap[256]; 
static int g_sdl_need_refresh = 0;

static const int a2key_to_sdlksym[][3] = {
	{ A2KEY_ESCAPE,	SDLK_ESCAPE,	0 },
	{ A2KEY_F1,	SDLK_F1,	0 },
	{ A2KEY_F2,	SDLK_F2,	0 },
	{ A2KEY_F3,	SDLK_F3,	0 },
	{ A2KEY_F4,	SDLK_F4,	0 },
	{ A2KEY_F5,	SDLK_F5,	0 },
	{ A2KEY_F6,	SDLK_F6,	0 },
	{ A2KEY_F7,	SDLK_F7,	0 },
	{ A2KEY_F8,	SDLK_F8,	0 },
	{ A2KEY_F9,	SDLK_F9,	0 },
	{ A2KEY_F10,	SDLK_F10,	0 },
	{ A2KEY_F11,	SDLK_F11,	0 },
	{ A2KEY_F12,	SDLK_F12,	0 },
	{ A2KEY_F13,	SDLK_F13,	0 },
	{ A2KEY_F14,	SDLK_F14,	0 },
	{ A2KEY_F15,	SDLK_F15,	0 },
	{ A2KEY_RESET, SDLK_PAUSE, SDLK_BREAK }, /* Reset */
	{ A2KEY_BACKQUOTE,	'`', '~' },		/* Key number 18? */
	{ 0x12,	SDLK_1, '!' },
	{ 0x13,	SDLK_2, '@' },
	{ 0x14,	SDLK_3, '#' },
	{ 0x15,	SDLK_4, '$' },
	{ 0x17,	SDLK_5, '%' },
	{ 0x16,	SDLK_6, '^' },
	{ 0x1a,	SDLK_7, '&' },
	{ 0x1c,	SDLK_8, '*' },
	{ 0x19,	SDLK_9, '(' },
	{ 0x1d,	SDLK_0, ')' },
	{ 0x1b,	'-', '_' },
	{ 0x18,	'=', '+' },
	//{ A2KEY_DELETE,	SDLK_BACKSPACE, 0 },
	{ A2KEY_DELETE,	0, 0 },
	{ A2KEY_INSERTHELP,	SDLK_INSERT, SDLK_HELP },	/* Help, insert */
#if 0
	{ A2KEY_HOME,	SDLK_HOME, 0 },		/* Home */
#endif
	{ A2KEY_PAGEUP,	SDLK_PAGEUP, 0 },	/* Page up */
	{ 0x47,	SDLK_NUMLOCK, SDLK_CLEAR },	/* keypad Clear */
	{ 0x51,	SDLK_KP_EQUALS, SDLK_HOME },		/* Note SDLK_HOME alias! */
	{ 0x4b,	SDLK_KP_DIVIDE, 0 },
	{ 0x43,	SDLK_KP_MULTIPLY, 0 },

	{ A2KEY_TAB,	SDLK_TAB, 0 },
	{ 0x0c,	'q', 'Q' },
	{ 0x0d,	'w', 'W' },
	{ 0x0e,	'e', 'E' },
	{ 0x0f,	'r', 'R' },
	{ 0x11,	't', 'T' },
	{ 0x10,	'y', 'Y' },
	{ 0x20,	'u', 'U' },
	{ 0x22,	'i', 'I' },
	{ 0x1f,	'o', 'O' },
	{ 0x23,	'p', 'P' },
	{ 0x21,	'[', '{' },
	{ 0x1e,	']', '}' },
	{ 0x2a,	SDLK_BACKSLASH, '|' },	/* \, | */
	{ A2KEY_FWDDEL,	SDLK_DELETE, 0 },   /* keypad delete */
	{ A2KEY_END,	SDLK_END, 0 },	/* keypad end */
	{ A2KEY_PAGEDOWN,	SDLK_PAGEDOWN, 0 },	/* keypad page down */
	{ 0x59,	SDLK_KP7, 0 },	/* keypad 7 */
	{ 0x5b,	SDLK_KP8, 0 },	/* keypad 8 */
	{ 0x5c,	SDLK_KP9, 0 },	/* keypad 9 */
	{ 0x4e,	SDLK_KP_MINUS, 0 },	/* keypad - */

	{ A2KEY_CAPSLOCK,	SDLK_CAPSLOCK, 0 },
	{ 0x00,	'a', 'A' },
	{ 0x01,	's', 'S' },
	{ 0x02,	'd', 'D' },
	{ 0x03,	'f', 'F' },
	{ 0x05,	'g', 'G' },
	{ 0x04,	'h', 'H' },
	{ 0x26,	'j', 'J' },
	{ 0x28,	'k', 'K' },
	{ 0x25,	'l', 'L' },
	{ 0x29,	';', ':' },
	{ 0x27,	0x27, '"' },	/* single quote */
	{ 0x24,	SDLK_RETURN, 0 },	/* return */
	{ 0x56,	SDLK_KP4, 0 },	/* keypad 4 */
	{ 0x57,	SDLK_KP5, 0 },	/* keypad 5 */
	{ 0x58,	SDLK_KP6, 0 },	/* keypad 6 */
	{ 0x45,	SDLK_KP_PLUS, 0 },	/* keypad + */

	{ A2KEY_SHIFT,	SDLK_LSHIFT, SDLK_RSHIFT },	/* shift */
	{ 0x06,	'z', 'Z' },
	{ 0x07,	'x', 'X' },
	{ 0x08,	'c', 'C' },
	{ 0x09,	'v', 'V' },
	{ 0x0b,	'b', 'B' },
	{ 0x2d,	'n', 'N' },
	{ 0x2e,	'm', 'M' },
	{ 0x2b,	',', '<' },
	{ 0x2f,	'.', '>' },
	{ 0x2c,	'/', '?' },
	{ A2KEY_UP,	SDLK_UP, 0 },	/* up arrow */
	{ 0x53,	SDLK_KP1, 0 },	/* keypad 1 */
	{ 0x54,	SDLK_KP2, 0 },	/* keypad 2 */
	{ 0x55,	SDLK_KP3, 0 },	/* keypad 3 */

#ifdef APPLE_EXTENDED_KEYBOARD
	{ A2KEY_LCTRL,	SDLK_LCTRL,	0},	/* control */
	{ A2KEY_RCTRL,	SDLK_RCTRL,	0},	/* control */
#else
	{ A2KEY_RCTRL,	SDLK_RCTRL, 0 },
#endif

#if 0
	{ A2KEY_OPTION,	SDLK_PRINT, SDLK_SYSREQ },		/* Option */
	{ A2KEY_COMMAND,	SDLK_SCROLLOCK, 0 },		/* Command */
#else
	{ A2KEY_OPTION,	SDLK_LMETA, SDLK_RMETA },		/* Option.. add RSUPER and RSUPER? */
	{ A2KEY_COMMAND,	SDLK_LALT, SDLK_RALT },		/* Command/Open apple */
#endif
	{ A2KEY_SPACE,	SDLK_SPACE, 0 },
	{ A2KEY_LEFT,	SDLK_LEFT, 0 },	/* left */
	{ A2KEY_DOWN,	SDLK_DOWN, 0 },	/* down */
	{ A2KEY_RIGHT,	SDLK_RIGHT, 0 },	/* right */
	{ 0x52,	SDLK_KP0, 0 },	/* keypad 0 */
	{ 0x41,	SDLK_KP_PERIOD, 0 },	/* keypad . */
	{ 0x4c,	SDLK_KP_ENTER, 0 },	/* keypad enter */
	{ -1, -1, -1 }
};

static int sdlksym_to_a2key[SDLK_LAST];


#if 0
static void
sdl_update_modifier_state(SDLMod state)
{
	SDLMod	state_xor;
	int	is_up;

	state = state & (KMOD_CTRL | KMOD_CAPS | KMOD_SHIFT);
	state_xor = g_mod_state ^ state;
	is_up = 0;
	if(state_xor & KMOD_CTRL) {
		is_up = ((state & KMOD_CTRL) == 0);
		adb_physical_key_update(0x36, is_up);
	}
	if(state_xor & KMOD_CAPS) {
		is_up = ((state & KMOD_CAPS) == 0);
		adb_physical_key_update(0x39, is_up);
	}
	if(state_xor & KMOD_SHIFT) {
		is_up = ((state & KMOD_SHIFT) == 0);
		adb_physical_key_update(0x38, is_up);
	}

	g_mod_state = state;
}
#endif

void
video_warp_pointer_sdl(void)
{
   if(g_fullscreen)
        g_warp_pointer |= 2;
    else
        g_warp_pointer &= ~2;

        SDL_ShowCursor(SDL_DISABLE);

    /*if(g_warp_pointer) {
        SDL_ShowCursor(SDL_DISABLE);
        SDL_WM_GrabInput(SDL_GRAB_ON);
        SDL_WarpMouse(X_A2_WINDOW_WIDTH/2,X_A2_WINDOW_HEIGHT/2);
	if (g_fullscreen) {
	    SDL_UpdateRect(screen,0,0,0,0);
	}
        ki_printf("Mouse Pointer grabbed\n");
    } else {
        SDL_ShowCursor(SDL_ENABLE);
        SDL_WM_GrabInput(SDL_GRAB_OFF);
        ki_printf("Mouse Pointer released\n");
    }*/
}

static void 
sdl_handle_keysym(const SDL_KeyboardEvent *key)
{
    Uint8 *keystate = SDL_GetKeyState(NULL);
    SDL_keysym keysym = key->keysym;
    const Uint8 type = key->type;
    const Uint8 state = key->state;
    int is_up;

	vid_printf("type: %d, state:%d, sym: %08x\n",
		type, state, keysym.sym);

    /* Check the state for caps lock,shift and control */
//    sdl_update_modifier_state(keysym.mod);

	is_up = 0;
    if( type == SDL_KEYUP ) {
      is_up = 1;
    }

	if(keystate[SDLK_RETURN] && keystate[SDLK_ESCAPE])
	{
		set_halt(HALT_WANTTOQUIT);
	}

	if (is_up == 0)
	{
		if(keystate[SDLK_ESCAPE]) 
		{
			if (keysym.sym == SDLK_UP)
				keysym.sym = SDLK_k;
			else if (keysym.sym == SDLK_RIGHT)
				keysym.sym = SDLK_j;
			else if (keysym.sym == SDLK_DOWN)
				keysym.sym = SDLK_i;
			else if (keysym.sym == SDLK_LEFT)
				keysym.sym = SDLK_s;
			else if (keysym.sym == SDLK_LCTRL)
				keysym.sym = SDLK_r;
		}
		else if(keystate[SDLK_RETURN])
		{
			if (keysym.sym == SDLK_UP)
				keysym.sym = SDLK_1;
			else if (keysym.sym == SDLK_RIGHT)
				keysym.sym = SDLK_2;
			else if (keysym.sym == SDLK_DOWN)
				keysym.sym = SDLK_3;
			else if (keysym.sym == SDLK_LEFT)
				keysym.sym = SDLK_4;
			else if (keysym.sym == SDLK_LCTRL)
				keysym.sym = SDLK_5;
			else if (keysym.sym == SDLK_LALT)
				keysym.sym = SDLK_6;
			else if (keysym.sym == SDLK_LSHIFT)
				keysym.sym = SDLK_7;
			else if (keysym.sym == SDLK_SPACE)
				keysym.sym = SDLK_8;
			else if (keysym.sym == SDLK_TAB)
				keysym.sym = SDLK_9;
			else if (keysym.sym == SDLK_BACKSPACE)
				keysym.sym = SDLK_0;
		}
	}

    
    if(keysym.sym == SDLK_BACKSPACE)
    {
		if(is_up == 0) 
		{
			update_mouse(mouse_global_x, mouse_global_y, 1, 1);
		}
		else if(is_up == 1) 
		{
			update_mouse(mouse_global_x, mouse_global_y, 0, 1);
		}
	}


    /* ctrl-apple-tab = kegs configuration menu */
    /*if((keysym.sym == SDLK_TAB) && !is_up &&
       ((keysym.mod & KMOD_LCTRL) || (keysym.mod & KMOD_RCTRL)) &&
       ((keysym.mod & KMOD_LALT) || (keysym.mod & KMOD_RALT))) {
        ki_printf("Configuration menu!\n");
        configuration_menu_sdl();
        adb_init();
        adb_physical_key_update(A2KEY_TAB, 1);
        adb_physical_key_update(A2KEY_COMMAND, 1);
        adb_physical_key_update(A2KEY_RCTRL, 1);
    }
    else if((keysym.sym == SDLK_F6) && !is_up) {
	if(function_execute(func_f6,-1,-1))
	    return;
    }
    else if((keysym.sym == SDLK_F7) && !is_up) {
	if(function_execute(func_f7,-1,-1))
	    return;
    }
    else if((keysym.sym == SDLK_F8) && !is_up) {
	if(function_execute(func_f8,-1,-1))
	    return;
    }
    else if((keysym.sym == SDLK_F9) && !is_up) {
	if(function_execute(func_f9,-1,-1))
	    return;
    }
    else if((keysym.sym == SDLK_F10) && !is_up) {
	if(function_execute(func_f10,-1,-1))
	    return;
    }
    else if((keysym.sym == SDLK_F11) && !is_up) {
	if(function_execute(func_f11,-1,-1))
	    return;
    }
    else if((keysym.sym == SDLK_F12) && !is_up) {
	if(function_execute(func_f12,-1,-1))
	    return;
    }
    // ctrl-delete = Mac OS X reset ˆ la Bernie !
    else if((keysym.sym == SDLK_BACKSPACE) && !is_up &&
	    ((keysym.mod & KMOD_LCTRL) || (keysym.mod & KMOD_RCTRL))) 
	{
		keysym.sym = SDLK_PAUSE;
    }*/
    process_keysym(keysym.sym, is_up);
}

void
process_keysym(SDLKey sym, int is_up)
{
    int	a2code;
    
    a2code = sdlksym_to_a2key[sym];
      
    if (a2code >= 0) {
        adb_physical_key_update(a2code, is_up);
        /*ki_printf("keysym %x -> a2code %x\n",keysym.sym,a2code);*/
	} 
    else {
	if((sym >= SDLK_F6) && (sym <= SDLK_F12)) {
	    /* just get out quietly all FKeys */
	    return;
	}
	ki_printf("Keysym: %04x unknown\n",
               sym);
    }
}

static void
sdl_init_keycodes()
{
    int keycode;

    for(keycode = 0; keycode<SDLK_LAST; keycode++) {
        sdlksym_to_a2key[keycode] = -1;
    }
    for(keycode=0; a2key_to_sdlksym[keycode][0] != -1; keycode++) {
        if (a2key_to_sdlksym[keycode][1] != 0)
            sdlksym_to_a2key[a2key_to_sdlksym[keycode][1]] =
                a2key_to_sdlksym[keycode][0];
        if (a2key_to_sdlksym[keycode][2] != 0)
            sdlksym_to_a2key[a2key_to_sdlksym[keycode][2]] =
                a2key_to_sdlksym[keycode][0];
    }
    /* add more aliases */
    /* option key */
    sdlksym_to_a2key[SDLK_LSUPER] = sdlksym_to_a2key[SDLK_RSUPER] = 0x3a;
    sdlksym_to_a2key[SDLK_MODE] = sdlksym_to_a2key[SDLK_COMPOSE] = 0x3a;
    /* reset on the mac keyboard */
    sdlksym_to_a2key[SDLK_POWER] = 0x7f;
#if 0
    for(keycode=0; keycode<SDLK_LAST; keycode++)
        if(sdlksym_to_a2key[keycode] != -1)
            ki_printf("%x->%x ",keycode,sdlksym_to_a2key[keycode]);
    ki_printf("\n");
#endif

    g_num_a2_keycodes = keycode;
}

void
video_update_color_sdl(int col_num, int a2_color)
{
    Uint8 r,g,b;
    int palette;
    int full;
    int doit=0;

    doit=1;

    if(col_num >= 256 || col_num < 0) {
        halt_printf("update_color_array called: col: %03x\n", col_num);
        return;
    }

    r = ((a2_color >> 8) & 0xf)<<4;
    g = ((a2_color >> 4) & 0xf)<<4;
    b = ((a2_color) & 0xf)<<4;
    a2v_palette[col_num].r = r;
    a2v_palette[col_num].g = g;
    a2v_palette[col_num].b = b;
    a2v_palette[col_num].unused = 0;

    full = g_installed_full_superhires_colormap;
    palette = col_num >> 4;

    shires_palette[col_num].r = r;
    shires_palette[col_num].g = g;
    shires_palette[col_num].b = b;
    shires_palette[col_num].unused = 0;

    g_full_refresh_needed = -1;
}

void
video_update_physical_colormap_sdl()
{
    int    palette;
    int    full;
    int    i;
    Uint8 r,g,b;
    int value;
    SDL_Color *sdl_palette;
    static SDL_Color *prev_palette = NULL;

    full = g_installed_full_superhires_colormap;

    if(!full) {
        palette = g_a2vid_palette << 4;
        for(i = 0; i < 16; i++) {
            value=lores_colors[i];

            b = (lores_colors[i%16]    & 0xf) << 4;
            g = (lores_colors[i%16]>>4 & 0xf) << 4;
            r = (lores_colors[i%16]>>8 & 0xf) << 4;

            a2v_palette[palette+i].r = r;
            a2v_palette[palette+i].g = g;
            a2v_palette[palette+i].b = b;
            /*a2v_palette[palette+i].unused = 0;*/
        }
    }

    if(full) {
        sdl_palette=&(shires_palette[0]);
    } else {
        sdl_palette=&(a2v_palette[0]);
    }

	if (!(screen->flags & SDL_HWSURFACE)) {
	
	#if 0
	/* The old way... might be faster for some machines*/
	 {
		SDL_Rect r;
		r.x = 0;
		r.y = 0;
		r.w = screen->w;
		r.h = screen->h;
		sdl_add_dirty_rect(gRectTable, r);
		g_sdl_need_refresh = 1;
	}
	
	#else
		/* For software surfaces, or when the screen isn't 8bit,
		   SDL forces a SDL_UpdateRects() after changing the palette.
		   This is bad for Mac OS X (and perhaps others) because a
		   *full* redraw is required. This code finds what pixels
		   are affected and only updates the scanlines with those pixels.
		   This assumes you have changed SDL_setphyspal() so that it does
		   not call SDL_UpdateRects() otherwise this is pretty useless but
		   shouldn't harm anything. - Darrell
		   
		   This method seems much faster than redrawing the entire window on
		   most machines. */
		Uint8 colors[256];
		Uint8 ncolors;
		
		/* count how many colors changed */
		Uint32 *src, *dst;
		int i;
		src = (Uint32*) sdl_palette;
		dst = (Uint32*) prev_palette;
		ncolors = 0;
		
		/* the first time we create the previous palette we must
		   refresh everything */
		if (prev_palette == NULL) {
			prev_palette = (SDL_Color*) malloc(sizeof(*prev_palette) * 256);
			assert (prev_palette != NULL);
			memcpy(prev_palette, sdl_palette, sizeof(*prev_palette) * 256);
			SDL_SetPalette(screen, SDL_PHYSPAL, sdl_palette, 0, 256);
			g_full_refresh_needed = -1;
			return;
		}
		
		for (i = 0; i < 256; i++) {
			if (((*src++ & 0xFFFFFF00) ^ (*dst++ & 0xFFFFFF00)) != 0) {
				colors[ncolors++] = i;
			}
		}
		
		if (ncolors > 0) {
			
			Uint8 *pixels;
			int x, y, w, h, pitch;
			int k;
						
			if (screen->format->BitsPerPixel != 8) {
				ki_printf ("whoops, not 8 bit!\n");
				return;
			}
			pixels = screen->pixels;
			pitch = screen->pitch;
			w = screen->w;
			h = screen->h;
			y = 0;
			NEXTLINE:
			for (; y < h; y++) {
				for (x = 0; x < w; x++) {
					for (k = 0; k < ncolors; k++) {
						if (*(pixels + x + (y * pitch)) == colors[k]) {
							/* refresh next 4 scanlines after hit */
							SDL_Rect r;
							r.x = 0;
							r.y = y;
							r.w = w;
							r.h = 4;
							sdl_add_dirty_rect (gRectTable, r);
							y += 4;
							goto NEXTLINE;
						}
					}	
				}
			}
			/* copy in colors that changed for next time */
			for (i = 0; i < ncolors; i++) {
				prev_palette[colors[i]] = sdl_palette[colors[i]];
			}			
		}
	#endif
	}
	

    SDL_SetPalette(screen, SDL_PHYSPAL, sdl_palette, 0, 256);

    /* since SDL emulates the hardware palette, we don't, and shouldn't */
    /* force a full video refresh */
    /* a2_screen_buffer_changed = -1; */
    /* g_full_refresh_needed = -1; */

    /* The palette changed though, so these need to be updated */
    g_border_sides_refresh_needed = -1;
    g_border_special_refresh_needed = -1;
    g_status_refresh_needed = -1;
    
    g_sdl_need_refresh = 1;
}

int
video_init_sdl()
{
    unsigned int vidflags = 0;
    int i;

    SDL_InitSubSystem(SDL_INIT_VIDEO);
    vidflags = SDL_SWSURFACE;

    g_videomode = KEGS_640X400;
    
    switch(g_videomode) {
    case KEGS_640X480:
        g_window_width = 640;
        g_window_height = 480;
        break;
    case KEGS_640X400:
        g_window_width = 640;
        g_window_height = 400;
        break;
    case KEGS_FULL:
        g_window_width = BASE_WINDOW_WIDTH;
        g_window_height = BASE_WINDOW_HEIGHT;
        break;
    default:
        fprintf(stderr,"sdl: unknown video mode  %d\n",g_videomode);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 0;
        break;
    }
    

    screen = SDL_SetVideoMode(g_window_width, g_window_height, 8, vidflags);
	
    /*if(!screen) {
        fprintf(stderr, "sdl: Couldn't set %dx%dx%d/%d video mode: %s!\n",
                g_window_width, g_window_height, 8, vidflags, SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 0;
    }
    */
    
    /* Set the titlebar */
    SDL_WM_SetCaption("KEGS-SDL 0.64", "kegs");

    /* Grab input if necessary */
    video_warp_pointer_sdl();

    /* set the logical colormap to a gray map for fast blits */
    for(i=0; i<256; i++)
    {
        graymap[i].r = graymap[i].g = graymap[i].b = i;
	}
    SDL_SetPalette(screen, SDL_LOGPAL, graymap, 0, 256);

    ki_printf("Calling get_sdlsurface!\n");
    
    video_image_text[0] = get_sdlsurface(&video_data_text[0], 0);
    video_image_text[1] = get_sdlsurface(&video_data_text[1], 0);
    video_image_hires[0] = get_sdlsurface(&video_data_hires[0], 0);
    video_image_hires[1] = get_sdlsurface(&video_data_hires[1], 0);
    video_image_superhires = get_sdlsurface(&video_data_superhires, 0);
    video_image_border_special = get_sdlsurface(&video_data_border_special, 1);
    video_image_border_sides = get_sdlsurface(&video_data_border_sides, 2);
    video_image_status_lines = get_sdlsurface(&video_data_status_lines, 3);

    sdl_init_keycodes();
    
    /* init dirty rects */
	if (gRectTable == NULL) {
		gRectTable = sdl_alloc_dirty_rects();
	}
	else {
		sdl_reset_dirty_rects(gRectTable);
	}
        
    return 1;
}

static SDL_Surface *
get_sdlsurface(byte **data_ptr, int extended_info)
{
	SDL_Surface	*xim;
	byte	*ptr;
	int	width;
	int	height;

	width = A2_WINDOW_WIDTH;
	height = A2_WINDOW_HEIGHT;
	if((extended_info & 0xf) == 1) {
		/* border at top and bottom of screen */
		width = X_A2_WINDOW_WIDTH;
		height = X_A2_WINDOW_HEIGHT - A2_WINDOW_HEIGHT + 2*8;
	}
	if((extended_info & 0xf) == 2) {
		/* border at sides of screen */
		width = EFF_BORDER_WIDTH;
		height = A2_WINDOW_HEIGHT;
	}
	if((extended_info & 0xf) == 3) {
		/* status lines */
		width = STATUS_LINES_WIDTH;
		height = STATUS_LINES_HEIGHT;
	}

	ptr = (byte *)malloc(width * height);

	vid_printf("ptr: %p\n", ptr);

	if(ptr == 0) {
		ki_printf("malloc for data failed\n");
		my_exit(2);
	}

	*data_ptr = ptr;

	xim = SDL_CreateRGBSurfaceFrom(ptr, width, height, 8, width, 0, 0, 0, 0);
    /* set the palette to the logical screen palette so that blits
       won't be translated */
    SDL_SetColors(xim, screen->format->palette->colors, 0, 256);
	vid_printf("xim.pixels: %p\n", xim->pixels);

	return xim;
}

static void
free_sdlsurface(SDL_Surface **surf, byte **data)
{
    assert((*data) == (*surf)->pixels);
    SDL_FreeSurface(*surf);
    *surf = NULL;
    free(*data);
    *data = NULL;
}

void
video_shutdown_sdl()
{
    free_sdlsurface((SDL_Surface**)&(video_image_text[0]),&(video_data_text[0]));
    free_sdlsurface((SDL_Surface**)&(video_image_text[1]),&(video_data_text[1]));
    free_sdlsurface((SDL_Surface**)&(video_image_hires[0]),&(video_data_hires[0]));
    free_sdlsurface((SDL_Surface**)&(video_image_hires[1]),&(video_data_hires[1]));
    free_sdlsurface((SDL_Surface**)&(video_image_superhires),&(video_data_superhires));
    free_sdlsurface(&(video_image_border_special),&(video_data_border_special));
    free_sdlsurface(&(video_image_border_sides),&(video_data_border_sides));
    free_sdlsurface(&(video_image_status_lines),&(video_data_status_lines));
    
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

rect_table *gRectTable = NULL;

rect_table* sdl_alloc_dirty_rects() {

	rect_table *table;
	table = (rect_table*) malloc(sizeof(*table));
	assert(table != NULL);
    table->size = 160;
	table->count = 0;
	table->rects = (SDL_Rect*) malloc(sizeof(*(table->rects)) * table->size);
	assert(table->rects != NULL);
	
	return table;
}

void sdl_reset_dirty_rects(rect_table *table) {
	table->count = 0;
}

void sdl_add_dirty_rect(rect_table *table, SDL_Rect rect) {

	if (table->count == table->size) {
	    table->size += 10;
		table->rects = realloc(table->rects, 
							   sizeof(*(table->rects)) * table->size);
		assert(table->rects != NULL);
		/*ki_printf ("reallocate dirty rect table: %d\n", table->size);
		*/
	}
	
	table->rects[table->count] = rect;
	table->count++;
}

void sdl_update_dirty_rects(rect_table *table, SDL_Surface *screen) {

	if (table->count > 0) {
	  if ((screen->flags & SDL_HWSURFACE) != SDL_HWSURFACE) {
		SDL_UpdateRects(screen, table->count, table->rects);
	  }
	  sdl_reset_dirty_rects(table);
	}
}

void
video_refresh_image_sdl()
{
    register word32 start_time;
    register word32 end_time;
    int    start;
    word32    mask;
    int    line;
    int    left_pix, right_pix;
    int    left, right;
    SDL_Surface *last_xim, *cur_xim;

   if(a2_screen_buffer_changed == 0 && !g_sdl_need_refresh) {
        return;
    }

    GET_ITIMER(start_time);

    if(g_border_sides_refresh_needed) {
        g_border_sides_refresh_needed = 0;
        sdl_refresh_border_sides();
    }
    if(g_border_special_refresh_needed) {
        g_border_special_refresh_needed = 0;
        sdl_refresh_border_special();
    }

    if (a2_screen_buffer_changed || g_full_refresh_needed) {

    start = -1;
    mask = 1;
    last_xim = NULL;

    left_pix = 640;
    right_pix = 0;

    for(line = 0; line < 25; line++) {
        if((g_full_refresh_needed & (1 << line)) != 0) {
            left = a2_line_full_left_edge[line];
            right = a2_line_full_right_edge[line];
        } else {
            left = a2_line_left_edge[line];
            right = a2_line_right_edge[line];
        }

        if(!(a2_screen_buffer_changed & mask)) {
            /* No need to update this line */
            /* Refresh previous chunks of lines, if any */
            if(start >= 0) {
                sdl_refresh_lines(last_xim, start, line,
                    left_pix, right_pix);
                start = -1;
                left_pix = 640;
                right_pix =  0;
            }
        } else {
            /* Need to update this line */
            cur_xim = a2_line_xim[line];
            if(start < 0) {
                start = line;
                last_xim = cur_xim;
            }
            if(cur_xim != last_xim) {
                /* do the refresh */
                sdl_refresh_lines(last_xim, start, line,
                    left_pix, right_pix);
                last_xim = cur_xim;
                start = line;
                left_pix = left;
                right_pix = right;
            }
            left_pix = MIN(left, left_pix);
            right_pix = MAX(right, right_pix);
        }
        mask = mask << 1;
    }

    if(start >= 0) {
        sdl_refresh_lines(last_xim, start, 25, left_pix, right_pix);
    }

    a2_screen_buffer_changed = 0;
    g_full_refresh_needed = 0;
}
    sdl_update_dirty_rects(gRectTable,screen);
    g_sdl_need_refresh = 0;
    /* And redraw border rectangle? */

    GET_ITIMER(end_time);

    g_cycs_in_xredraw += (end_time - start_time);
}

static void
sdl_refresh_image (SDL_Surface *hbm, int srcx, int srcy,
                   int destx,int desty, int width,int height)
{
    SDL_Rect  srcrect, dstrect;
    int result;

    if (width==0 || height == 0) {
        ki_printf("width==0 || height == 0\n");
        return;
    }

    srcrect.x = srcx;
    srcrect.y = srcy;
    dstrect.x = destx;
    dstrect.y = desty;
    if(g_videomode != KEGS_FULL) {
        dstrect.x -= BORDER_WIDTH;
        dstrect.y -= BASE_MARGIN_TOP;
    }
    srcrect.w = dstrect.w = width;
    srcrect.h = dstrect.h = height;

    result = SDL_BlitSurface(hbm, &srcrect, screen, &dstrect);
    sdl_add_dirty_rect(gRectTable, dstrect);

    if(result!=0) {
        ki_printf("sdl_refresh_image(%p,%d,%d,%d,%d,%d,%d)->%d\n",
               hbm, srcx, srcy, destx, desty, width, height, result);
    }
}

void
sdl_refresh_lines(SDL_Surface *xim, int start_line, int end_line, int left_pix,
        int right_pix)
{
    int srcy;

    if(left_pix >= right_pix || left_pix < 0 || right_pix <= 0) {
        halt_printf("sdl_refresh_lines: lines %d to %d, pix %d to %d\n",
            start_line, end_line, left_pix, right_pix);
        ki_printf("a2_screen_buf_ch:%08x, g_full_refr:%08x\n",
            a2_screen_buffer_changed, g_full_refresh_needed);
        show_a2_line_stuff();
    }

    srcy = 16*start_line;

    if(xim == video_image_border_special) {
        /* fix up y pos in src */
        ki_printf("sdl_refresh_lines called, video_image_border_special!!\n");
        srcy = 0;
    }

    g_refresh_bytes_xfer += 16*(end_line - start_line) *
                            (right_pix - left_pix);

//    if (g_cur_a2_stat & 0xa0) {
    if(1) {
        sdl_refresh_image(xim,left_pix,srcy,BASE_MARGIN_LEFT+left_pix,
                          BASE_MARGIN_TOP+16*start_line,
                          right_pix-left_pix,16*(end_line-start_line));
    } else {
        sdl_refresh_image(xim,left_pix,srcy,BASE_MARGIN_LEFT+left_pix+
                          BORDER_WIDTH,
                          BASE_MARGIN_TOP+16*start_line,
                          right_pix-left_pix,16*(end_line-start_line));
    }
}

void
sdl_redraw_border_sides_lines(int end_x, int width, int start_line,
    int end_line)
{
    SDL_Surface *xim;

    if(start_line < 0 || width < 0) {
        return;
    }

#if 0
    ki_printf("redraw_border_sides lines:%d-%d from %d to %d\n",
        start_line, end_line, end_x - width, end_x);
#endif
    xim = video_image_border_sides;
    g_refresh_bytes_xfer += 16 * (end_line - start_line) * width;

    sdl_refresh_image (xim,0,16*start_line,end_x-width,
                       BASE_MARGIN_TOP+16*start_line,width,
                       16*(end_line-start_line));
}

void
sdl_refresh_border_sides()
{
    int    old_width;
    int    prev_line;
    int    width;
    int    mode;
    int    i;

#if 0
    ki_printf("refresh border sides!\n");
#endif

    /* can be "jagged" */
    prev_line = -1;
    old_width = -1;
    for(i = 0; i < 25; i++) {
        mode = (a2_line_stat[i] >> 4) & 7;
        width = EFF_BORDER_WIDTH;
        if(mode == MODE_SUPER_HIRES) {
            width = BORDER_WIDTH;
        }
        if(width != old_width) {
            //if (g_cur_a2_stat & 0xa0) {
            if (1) {
                sdl_redraw_border_sides_lines(old_width,
                    old_width, prev_line, i);    
                sdl_redraw_border_sides_lines(X_A2_WINDOW_WIDTH,
                    old_width, prev_line, i);    
            } else {
                sdl_redraw_border_sides_lines(old_width-BORDER_WIDTH,
                    old_width-BORDER_WIDTH, prev_line, i);    
                sdl_redraw_border_sides_lines(X_A2_WINDOW_WIDTH,
                    old_width-BORDER_WIDTH, prev_line, i);    
            }

            prev_line = i;
            old_width = width;
        }
    }
    //if (g_cur_a2_stat & 0xa0) {
    if(1) {
        sdl_redraw_border_sides_lines(old_width, 
                                    old_width, prev_line,25);
        sdl_redraw_border_sides_lines(X_A2_WINDOW_WIDTH, 
                                    old_width, prev_line,25);
    } else {
        sdl_redraw_border_sides_lines(old_width-BORDER_WIDTH, 
                                    old_width-BORDER_WIDTH, prev_line,25);
        sdl_redraw_border_sides_lines(X_A2_WINDOW_WIDTH, 
                                    old_width-BORDER_WIDTH, prev_line,25);
    }
}

void
sdl_refresh_border_special()
{
    SDL_Surface *xim;
    int    width, height;

    if(g_videomode != KEGS_FULL)
        return;
    width = X_A2_WINDOW_WIDTH;
    height = BASE_MARGIN_TOP;

    xim = video_image_border_special;
    g_refresh_bytes_xfer += 16 * width *
                (BASE_MARGIN_TOP + BASE_MARGIN_BOTTOM);

    sdl_refresh_image (xim,0,0,0,BASE_MARGIN_TOP+A2_WINDOW_HEIGHT,
                       width,BASE_MARGIN_BOTTOM);
    sdl_refresh_image (xim,0,BASE_MARGIN_BOTTOM,0,0,
                       width,BASE_MARGIN_TOP);
}

void sdl_mouse_emulation();

void
video_check_input_events_sdl()
{
	unsigned char	refresh_needed, motion;
    SDL_Event event; /* Event structure */

	motion = 0;
	refresh_needed = 0;
    
    /* Check for events */
    while(SDL_PollEvent(&event))
    { 
        switch(event.type){  
        case SDL_QUIT:
            set_halt(HALT_WANTTOQUIT);
            break;
        case SDL_ACTIVEEVENT:
			if(event.active.state == SDL_APPINPUTFOCUS) {
                if (event.active.gain == 0) {
                    vid_printf("Left window, auto repeat on\n");
                    video_auto_repeat_on_sdl(0);
                } else {
                    vid_printf("Enter window, auto repeat off\n");
                    video_auto_repeat_off_sdl(0);
                }
            }
            break;
        /*case SDL_MOUSEBUTTONDOWN:
			vid_printf("Got button press of button %d!\n",
				event.button.button);
			if(event.button.button == 1) {
				vid_printf("mouse button pressed\n");
				motion = update_mouse(event.button.x,
							event.button.y, 1, 1);
			} else if(event.button.button == 2) {
                function_execute(func_button2, event.button.x, event.button.y);
			} else if(event.button.button == 3) {
                function_execute(func_button3, event.button.x, event.button.y);
			}
			break;
        case SDL_MOUSEBUTTONUP:
			if(event.button.button == 1) {
				vid_printf("mouse button released\n");
				motion = update_mouse(event.button.x,
							event.button.y, 0, 1);
			}
			break;*/
		case SDL_VIDEOEXPOSE:
			refresh_needed = -1;
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            sdl_handle_keysym(&(event.key));
            break;
		/*case SDL_MOUSEMOTION:
			motion = update_mouse(event.motion.x, event.motion.y, 0, 0);
            break;*/
        }
    }

	if(motion && g_warp_pointer) {
		SDL_WarpMouse(X_A2_WINDOW_WIDTH/2, X_A2_WINDOW_HEIGHT/2);
		update_mouse(-1,-1,-1,-1);
	}

	sdl_mouse_emulation();

	/*if(refresh_needed) {
		ki_printf("Full refresh needed\n");
		a2_screen_buffer_changed = -1;
		g_full_refresh_needed = -1;

		g_border_sides_refresh_needed = 1;
		g_border_special_refresh_needed = 1;
		g_status_refresh_needed = 1;
	}*/
}

#ifndef NOMOUSE_EMULATION
extern SDL_Joystick *joy0;

void sdl_mouse_emulation()
{
	static unsigned char refresh = 0;
	refresh++;
	
	if (refresh > 1)
	{
		if (mouse_global_y > 480) mouse_global_y = 480;
		if (mouse_global_x > 1280) mouse_global_x = 1280;
		 
		if(g_paddle_val[1] < 60) 
		{
			mouse_global_y = mouse_global_y - 4;
		}
		else if(g_paddle_val[1] > 220) 
		{
			mouse_global_y = mouse_global_y + 4;
		}
		
		if(g_paddle_val[0] < 60) 
		{
			mouse_global_x = mouse_global_x - 4;
		}
		else if(g_paddle_val[0] > 220) 
		{
			mouse_global_x = mouse_global_x + 4;
		}
	 
		update_mouse(mouse_global_x, mouse_global_y, 0, 0);
		joystick_update();
		refresh=0;
	}
	
   
}

#endif

void
sdl_draw_string(SDL_Surface *surf, Sint16 x, Sint16 y, const unsigned char *string, Uint16 maxstrlen, Uint16 xscale, Uint16 yscale, Uint8 fg, Uint8 bg)
{
    int strlen;
    SDL_Surface *linesurf;
    Sint16 ypixel;
    Uint8 * yptr;
    int col, bit;
    byte b;
    SDL_Rect  srcrect, dstrect;
    int xrepeat, yrepeat;

    assert(string!=NULL);
    for(strlen = 0; strlen<maxstrlen && string[strlen]; strlen++) {}
    srcrect.x = srcrect.y = 0;
    srcrect.w = strlen * 7 * xscale;
    srcrect.h = 8 * yscale;
    linesurf = SDL_CreateRGBSurface(SDL_SWSURFACE, srcrect.w, srcrect.h,
                                    8, 0, 0, 0, 0);
    SDL_SetColors(linesurf, graymap, 0, 256);
    yptr = linesurf->pixels;
    for(ypixel = 0; ypixel<8; ypixel++) {
        for(col=0; col<strlen; col++) {
            b = font_array[string[col]^0x80][ypixel];
            for(bit=0; bit<7; bit++, yptr++) {
                *yptr = (b & (1<<(7-bit))) ? fg : bg;
                for(xrepeat = 1; xrepeat < xscale; xrepeat++, yptr++) {
                    yptr[1] = *yptr;
                }
            }
        }
        yptr += linesurf->pitch - srcrect.w;
        for(yrepeat = 1; yrepeat < yscale; yrepeat++) {
            for(xrepeat = 0; xrepeat<srcrect.w; xrepeat++, yptr++)
                *yptr = yptr[-linesurf->pitch];
            yptr += linesurf->pitch - srcrect.w;
        }
    }
    dstrect.x = x;
    dstrect.y = y;
    SDL_BlitSurface(linesurf, &srcrect, surf, &dstrect);
    SDL_FreeSurface(linesurf);
    sdl_add_dirty_rect(gRectTable, dstrect);
    
    /*SDL_UpdateRect(surf, dstrect.x, dstrect.y, dstrect.w, dstrect.h);*/
}

void
video_redraw_status_lines_sdl()
{
    char *buf;
    int line;
    Sint16 x,y;
    
    if(g_videomode == KEGS_640X400)
        return;
    if(g_videomode == KEGS_FULL) {
        y = X_A2_WINDOW_HEIGHT+8;
        x = BORDER_WIDTH+8;
    }
    else {
        y = A2_WINDOW_HEIGHT+8;
        x = 8;
    }
	for(line = 0; line < MAX_STATUS_LINES; line++, y+=8) {
		buf = &(g_video_status_buf[line][0]);
        sdl_draw_string(screen, x, y, buf, X_LINE_LENGTH, 1, 1, 0xef, 0xe0);
	}
        
        /*sdl_update_dirty_rects(gRectTable, screen);*/
	 g_sdl_need_refresh = 1;
}

void
video_auto_repeat_on_sdl(int must)
{
}

void
video_auto_repeat_off_sdl(int must)
{
}

#else  /* !HAVE_VIDEO_SDL */
int video_init_sdl() { return 0; }
void video_shutdown_sdl() {}
void video_update_physical_colormap_sdl() {}
void video_update_color_sdl(int col_num, int a2_color) {}
void video_refresh_image_sdl() {}
void video_check_input_events_sdl() {}
void video_redraw_status_lines_sdl() {}
void video_auto_repeat_on_sdl(int must) {}
void video_auto_repeat_off_sdl(int must) {}
void video_warp_pointer_sdl(void) {}
#endif /* !HAVE_VIDEO_SDL */

