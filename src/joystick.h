#ifndef KEGS_JOYSTICK_H
#define KEGS_JOYSTICK_H

#include "defc.h"
#include "paddles.h"
#include "adb.h"

/* Different Joystick defines */
#define JOYSTICK_NONE       0
#define JOYSTICK_MOUSE		1
#define JOYSTICK_LINUX		2
#define JOYSTICK_KEYPAD		3
#define JOYSTICK_WIN32      4
#define JOYSTICK_SDL		5

#ifndef DISABLE_JOYSTICK_NATIVE
#ifdef WIN32
#define HAVE_JOYSTICK_WIN32
#endif
#ifdef __linux__
#define HAVE_JOYSTICK_LINUX
#endif
#endif

#ifdef HAVE_SDL
#define HAVE_JOYSTICK_SDL
#include <SDL/SDL.h>
#endif

#ifdef HAVE_JOYSTICK_WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#ifdef HAVE_JOYSTICK_LINUX
#include <sys/ioctl.h>
#include <linux/joystick.h>
#include <errno.h>
#endif

int joystick_init(void);
int joystick_init_linux(void);
int joystick_init_win32(void);
int joystick_init_sdl(void);
void joystick_update(void);
void joystick_update_mouse(void);
void joystick_update_linux(void);
void joystick_update_win32(void);
void joystick_update_sdl(void);
void joystick_update_button_win32(void);
void joystick_close(void);
void joystick_close_linux(void);
void joystick_close_win32(void);
void joystick_close_sdl(void);

int get_joystick_type();
int set_joystick_type(int);
#endif /* KEGS_JOYSTICK_H */
