/****************************************************************/
/*			Apple IIgs emulator			*/
/*			Copyright 1999 Kent Dickey		*/
/*								*/
/*	This code may not be used in a commercial product	*/
/*	without prior written permission of the author.		*/
/*								*/
/*	You may freely distribute this code.			*/ 
/*								*/
/*	You can contact the author at kentd@cup.hp.com.		*/
/*	HP has nothing to do with this software.		*/
/*								*/
/*	Joystick routines by Jonathan Stark			*/
/*	Written for KEGS May 3, 1999				*/
/*								*/
/****************************************************************/

const char rcsid_joystick_driver_c[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/joystick_driver.c,v 1.3 2005/09/23 12:37:09 fredyd Exp $";

#include "joystick.h"
#include "video.h"
#include "ki.h"

#ifdef HAVE_JOYSTICK_SDL
SDL_Joystick *joy0;
#endif

#ifdef HAVE_JOYSTICK_LINUX

#define MAX_JOY_NAME	128

static const char *g_joystick_dev = "/dev/js0";	/* default joystick dev file */
static int	g_joystick_fd = -1;
static int	g_joystick_num_axes = 0;
static int	g_joystick_num_buttons = 0;
#endif


/* Joystick handling routines for SDL */
int
joystick_init()
{
    return joystick_init_sdl();
}

void
joystick_update()
{
        joystick_update_sdl();
}

void
joystick_close()
{
        joystick_close_sdl();
}


void
joystick_update_mouse()
{
	/*int	val_x;
	int	val_y;
    
	val_x = 0;
	if(g_mouse_cur_x > BASE_MARGIN_LEFT) {
		val_x = (g_mouse_cur_x - BASE_MARGIN_LEFT) * 117;
		val_x = val_x >> 8;
	}

	val_y = 0;
	if(g_mouse_cur_y > BASE_MARGIN_TOP) {
		val_y = ((g_mouse_cur_y - BASE_MARGIN_TOP) * 170) >> 8;
	}

	if(val_x > 280) {
		val_x = 280;
	}
	if(val_y > 280) {
		val_y = 280;
	}

	g_paddle_val[0] = val_x;
	g_paddle_val[1] = val_y;
	g_paddle_val[2] = 255;
	g_paddle_val[3] = 255;
	g_paddle_button[2] = 1;
	g_paddle_button[3] = 1;*/
}

int
joystick_init_linux()
{
#ifndef HAVE_JOYSTICK_LINUX
    ki_printf("--Linux joystick not available\n");
    return 0;
#else
	char	joy_name[MAX_JOY_NAME];
	int	version;
	int	fd;
	int	i;

	fd = open(g_joystick_dev, O_RDONLY | O_NONBLOCK);
	if(fd < 0) {
		ki_printf("Unable to open joystick dev file: %s, errno: %d\n",
			g_joystick_dev, errno);
		ki_printf("Defaulting to mouse joystick\n");
		return 0;
	}

	strcpy(&joy_name[0], "Unknown Joystick");
	version = 0x800;

	ioctl(fd, JSIOCGNAME(MAX_JOY_NAME), &joy_name[0]);
	ioctl(fd, JSIOCGAXES, &g_joystick_num_axes);
	ioctl(fd, JSIOCGBUTTONS, &g_joystick_num_buttons);
	ioctl(fd, JSIOCGVERSION, &version);

	ki_printf("Detected joystick: %s [%d axes, %d buttons vers: %08x]\n",
		joy_name, g_joystick_num_axes, g_joystick_num_buttons,
		version);

	g_joystick_type = JOYSTICK_LINUX;
	g_joystick_fd = fd;
	for(i = 0; i < 4; i++) {
		g_paddle_val[i] = 280;
		g_paddle_button[i] = 1;
	}

	joystick_update();
    return 1;
#endif /* HAVE_JOYSTICK_LINUX */
}

/* joystick_update_linux() called from paddles.c.  Update g_paddle_val[] */
/*  and g_paddle_button[] arrays with current information */
void
joystick_update_linux()
{
#ifdef HAVE_JOYSTICK_LINUX
	struct js_event js;	/* the linux joystick event record */
	int	val;
	int	num;
	int	type;
	int	ret;
	int	len;
	int	i;

	/* suck up to 20 events, then give up */
	len = sizeof(struct js_event);
	for(i = 0; i < 20; i++) {
		ret = read(g_joystick_fd, &js, len);
		if(ret != len) {
			/* just get out */
			return;
		}
		type = js.type & ~JS_EVENT_INIT;
		val = js.value;
		num = js.number & 3;		/* clamp to 0-3 */
		switch(type) {
		case JS_EVENT_BUTTON:
			g_paddle_button[num] = val;
			break;
		case JS_EVENT_AXIS:
			/* val is -32767 to +32767, convert to 0->280 */
			/* (want just 255, but go a little over for robustness*/
			g_paddle_val[num] = ((val + 32767) * 9) >> 11;
			break;
		}
	}
#endif
}

void
joystick_close_linux(void)
{
#ifdef HAVE_JOYSTICK_LINUX
    close(g_joystick_fd);
#endif
}

/* Joystick handling routines for WIN32 */
int
joystick_init_win32()
{
#ifndef HAVE_JOYSTICK_WIN32
    ki_printf("--Win32 joystick not available\n");
    return 0;
#else
    int i;
    JOYINFO info;
    JOYCAPS joycap;

    // Check that there is a joystick device
    if (joyGetNumDevs()<=0) {
        ki_printf ("--No joystick hardware detected\n");
        return 0;
    }

    // Check that at least joystick 1 or joystick 2 is available 
    if (joyGetPos(JOYSTICKID1,&info) != JOYERR_NOERROR && 
        joyGetPos(JOYSTICKID2,&info) != JOYERR_NOERROR) {
        ki_printf ("--No joystick attached\n");
        return 0;
    }

    // Print out the joystick device name being emulated
    if (joyGetDevCaps(JOYSTICKID1,&joycap,sizeof(joycap)) == JOYERR_NOERROR) {
        ki_printf ("--Joystick #1 = %s\n",joycap.szPname);
    }
    if (joyGetDevCaps(JOYSTICKID2,&joycap,sizeof(joycap)) == JOYERR_NOERROR) {
        ki_printf ("--Joystick #1 = %s\n",joycap.szPname);
    }
    
    g_joystick_type = JOYSTICK_WIN32;
    for(i = 0; i < 4; i++) {
        g_paddle_val[i] = 280;
        g_paddle_button[i] = 1;
    }

    joystick_update();
    return 1;
#endif
}

void
joystick_update_win32()
{
#ifdef HAVE_JOYSTICK_WIN32
    JOYCAPS joycap;
    JOYINFO info;

    if (joyGetDevCaps(JOYSTICKID1,&joycap,sizeof(joycap)) == JOYERR_NOERROR &&
        joyGetPos(JOYSTICKID1,&info) == JOYERR_NOERROR) {
        g_paddle_val[0] = (info.wXpos-joycap.wXmin)*280/
                          (joycap.wXmax - joycap.wXmin);
        g_paddle_val[1] = (info.wYpos-joycap.wYmin)*280/
                          (joycap.wYmax - joycap.wYmin);
        g_paddle_button[0] = ((info.wButtons & JOY_BUTTON1) > 0) ? 1:0;
        g_paddle_button[1] = ((info.wButtons & JOY_BUTTON2) > 0) ? 1:0;
    }
    if (joyGetDevCaps(JOYSTICKID2,&joycap,sizeof(joycap)) == JOYERR_NOERROR &&
        joyGetPos(JOYSTICKID2,&info) == JOYERR_NOERROR) {
        g_paddle_val[2] = (info.wXpos-joycap.wXmin)*280/
                          (joycap.wXmax - joycap.wXmin);
        g_paddle_val[3] = (info.wYpos-joycap.wYmin)*280/
                          (joycap.wYmax - joycap.wYmin);
        g_paddle_button[2] = ((info.wButtons & JOY_BUTTON1) > 0) ? 1:0;
        g_paddle_button[3] = ((info.wButtons & JOY_BUTTON2) > 0) ? 1:0;
    }
#endif
}

void
joystick_update_button_win32()
{
#ifdef HAVE_JOYSTICK_WIN32        
    JOYINFOEX info;
    info.dwSize=sizeof(JOYINFOEX);
    info.dwFlags=JOY_RETURNBUTTONS;
    if (joyGetPosEx(JOYSTICKID1,&info) == JOYERR_NOERROR) {
        g_paddle_button[0] = ((info.dwButtons & JOY_BUTTON1) > 0) ? 1:0;
        g_paddle_button[1] = ((info.dwButtons & JOY_BUTTON2) > 0) ? 1:0;
    }
    if (joyGetPosEx(JOYSTICKID2,&info) == JOYERR_NOERROR) {
        g_paddle_button[2] = ((info.dwButtons & JOY_BUTTON1) > 0) ? 1:0;
        g_paddle_button[3] = ((info.dwButtons & JOY_BUTTON2) > 0) ? 1:0;
    }
#endif
}

void
joystick_close_win32()
{
}



int
joystick_init_sdl()
{
#ifndef HAVE_JOYSTICK_SDL
    ki_printf("--SDL joystick not available\n");
    return 0;
#else
    int i;

    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    
    /* Check that there is a joystick device */
    if (SDL_NumJoysticks()<=0) {
        ki_printf ("--No joystick hardware detected\n");
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
        return 0;
    }

    joy0=SDL_JoystickOpen(0);
    
    if(joy0) {
        ki_printf("Opened Joystick 0\n");
        ki_printf("Name: %s\n", SDL_JoystickName(0));
        ki_printf("Number of Axes: %d\n", SDL_JoystickNumAxes(joy0));
        ki_printf("Number of Buttons: %d\n", SDL_JoystickNumButtons(joy0));
        ki_printf("Number of Balls: %d\n", SDL_JoystickNumBalls(joy0));
    }
    else
        ki_printf("Couldn't open Joystick 0\n");
 
   
    g_joystick_type = JOYSTICK_SDL;
    for(i = 0; i < 4; i++) {
        g_paddle_val[i] = 280;
        g_paddle_button[i] = 1;
    }

    joystick_update();
    return 1;
#endif /* HAVE_JOYSTICK_SDL */
}

void
joystick_update_sdl()
{
#ifdef HAVE_SDL  
	Uint8 *keystate = SDL_GetKeyState(NULL);
	g_paddle_button[0] = keystate[SDLK_LALT] ? 1 : 0;
	g_paddle_button[1] = keystate[SDLK_LCTRL] ? 1 : 0;
    if (joy0) 
    {
        g_paddle_val[0] = ((int)SDL_JoystickGetAxis(joy0, 0) + 32768)
            * 280 / 65535;
        g_paddle_val[1] = ((int)SDL_JoystickGetAxis(joy0, 1) + 32768)
            * 280 / 65535;
        /*g_paddle_button[0] = SDL_JoystickGetButton(joy0, 0);
        g_paddle_button[1] = SDL_JoystickGetButton(joy0, 1);*/
    }
    SDL_JoystickUpdate();
#endif
}

/* not used, but here just in case... */
void
joystick_close_sdl()
{
#ifdef HAVE_SDL
    if (joy0 && SDL_JoystickOpened(0)) {
        SDL_JoystickClose(joy0);
    }
    joy0 = 0;
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
#endif
}

int
get_joystick_type(void)
{
    return g_joystick_type;
}

int
set_joystick_type(int val)
{
    int oldval = g_joystick_type;
    g_joystick_type = val;
    joystick_close();
    if(!joystick_init()) {
        joystick_close();
        g_joystick_type = oldval;
        joystick_init();
        return 0;
    }
    return 1;
}
