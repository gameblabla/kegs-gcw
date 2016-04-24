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

#ifndef KEGS_ADB_H
#define KEGS_ADB_H

#include "defc.h"

/* there is a problem with arrow keys under GSOS if you enable this */
/*#define APPLE_EXTENDED_KEYBOARD*/

#define A2KEY_TAB 0x30
#define A2KEY_SPACE 0x31
#define A2KEY_BACKQUOTE 0x32
#define A2KEY_DELETE 0x33
#define A2KEY_ESCAPE 0x35
#define A2KEY_RCTRL 0x36
#define A2KEY_COMMAND 0x37
#define A2KEY_SHIFT 0x38
#define A2KEY_CAPSLOCK 0x39
#define A2KEY_OPTION 0x3a
#define A2KEY_LCTRL 0x3b
#define A2KEY_F1 0x7a
#ifdef APPLE_EXTENDED_KEYBOARD
#define A2KEY_F2 0x78
#else
#define A2KEY_F2 0x7b
#endif
#define A2KEY_F3 0x63
#define A2KEY_F4 0x76
#define A2KEY_F5 0x60
#define A2KEY_F6 0x61
#define A2KEY_F7 0x62
#define A2KEY_F8 0x64
#define A2KEY_F9 0x65
#define A2KEY_F10 0x6D
#define A2KEY_F11 0x67
#define A2KEY_F12 0x6F
#define A2KEY_F13 0x69
#define A2KEY_F14 0x6b
#define A2KEY_F15 0x71
#define A2KEY_INSERTHELP 0x72
#define A2KEY_HOME 0x73
#define A2KEY_PAGEUP 0x74
#define A2KEY_FWDDEL 0x75
#define A2KEY_END 0x77
#define A2KEY_PAGEDOWN 0x79
#ifdef APPLE_EXTENDED_KEYBOARD
#define A2KEY_LEFT 0x7b
#define A2KEY_RIGHT 0x7c
#define A2KEY_DOWN 0x7d
#define A2KEY_UP 0x7e
#define A2KEY_LCTRL 0x3b
#else
#define A2KEY_LEFT 0x3b
#define A2KEY_RIGHT 0x3c
#define A2KEY_DOWN 0x3d
#define A2KEY_UP 0x3e
#endif

#define A2KEY_RESET 0x7f
#define A2KEY_MASK 0x7f

extern int g_mouse_cur_x;	/* from adb.c */
extern int g_mouse_cur_y;
extern int g_warp_pointer;

int update_mouse(int x, int y, int button_state, int button_valid);
void adb_physical_key_update(int a2code, int is_up);
void adb_kbd_repeat_off(void);
word32 adb_read_c000(void);
word32 adb_access_c010(void);
int mouse_read_c024(void);
word32 adb_read_c025(void);
int adb_read_c026(void);
int adb_read_c027(void);
void adb_write_c026(int val);
void adb_write_c027(int val);
int adb_is_cmd_key_down(void);
int adb_is_option_key_down(void);
void show_adb_log(void);
void adb_init(void);
void adb_reset(void);
int get_warp_pointer();
int set_warp_pointer(int val);
#endif /* KEGS_ADB_H */
