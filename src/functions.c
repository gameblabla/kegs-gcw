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

#include "sim65816.h"
#include "iwm.h"
#include "adb.h"
#include "videodriver.h"
#include "functions.h"
#include "paddles.h"
#include "dis.h"

function_e func_f6 = FUNCTION_ENTER_DEBUGGER;
function_e func_f7 = FUNCTION_TOGGLE_FAST_DISK_EMUL;
function_e func_f8 = FUNCTION_TOGGLE_WARP_POINTER;
function_e func_f9 = FUNCTION_TOGGLE_SLOW_PADDLES;
function_e func_f10 = FUNCTION_TOGGLE_VIDEOMODE;
function_e func_f11 = FUNCTION_TOGGLE_FULLSCREEN;
function_e func_f12 = FUNCTION_TOGGLE_LIMIT_SPEED;
function_e func_button2 = FUNCTION_ENTER_DEBUGGER;
function_e func_button3 = FUNCTION_TOGGLE_LIMIT_SPEED;

static void
function_toggle_fast_disk_emul(int x, int y)
{
    set_fast_disk_emul(!get_fast_disk_emul());
}

static void
function_toggle_warp_pointer(int x, int y)
{
    set_warp_pointer(!get_warp_pointer());
}

static void
function_toggle_videomode(int x, int y)
{
    videomode_e newmode;

    ki_printf("g_videomode was %d\n",get_videomode());
    switch(get_videomode()) {
    case KEGS_FULL:
        newmode = KEGS_640X480;
        break;
    case KEGS_640X480:
        newmode = KEGS_640X400;
        break;
    case KEGS_640X400:
        newmode = KEGS_FULL;
        break;
    }
    set_videomode(newmode);
}

static void
function_toggle_fullscreen(int x, int y)
{
    set_fullscreen(!get_fullscreen());
}

static void
function_toggle_swap_paddles(int x, int y)
{
    set_swap_paddles(!get_swap_paddles());
    ki_printf("Swap paddles is now: %d\n", get_swap_paddles());
}

static void
function_toggle_invert_paddles(int x, int y)
{
    set_invert_paddles(!get_invert_paddles());
    ki_printf("Invert paddles is now: %d\n", get_invert_paddles());
}

static void
function_toggle_slow_paddles(int x, int y)
{
    set_slow_paddles(!get_slow_paddles());
    if (get_slow_paddles()) {
        ki_printf("Paddles slowed\n");
    }
    else {
        ki_printf("Paddles at full speed\n");
    }
}

static void
function_toggle_limit_speed(int x, int y)
{
    int limit_speed = get_limit_speed() + 1;
    if(limit_speed > SPEED_ENUMSIZE) {
        limit_speed = 0;
    }
    
    ki_printf("Toggling limit_speed to %d\n",
           limit_speed);
    switch(limit_speed) {
    case SPEED_UNLIMITED:
        ki_printf("...as fast as possible!\n");
        break;
    case SPEED_1MHZ:
        ki_printf("... 1.024MHz (Slow) \n");
        break;
    case SPEED_GS:
        ki_printf("... 2.8MHz (Normal) \n");
        break;
    case SPEED_ZIP:
        ki_printf("... 8.0MHz (Zip Speed)\n");
        break;
    }
    set_limit_speed(limit_speed);
}

static void
function_enter_debugger(int x, int y)
{
    enter_debugger(1);
}

int
function_execute(function_e which, int x, int y)
{
    switch(which) {
    case FUNCTION_NONE:
        return 0;
        break;
    case FUNCTION_TOGGLE_FAST_DISK_EMUL:
        function_toggle_fast_disk_emul(x,y);
        break;
    case FUNCTION_TOGGLE_WARP_POINTER:
        function_toggle_warp_pointer(x,y);
        break;
    case FUNCTION_TOGGLE_VIDEOMODE:
        function_toggle_videomode(x,y);
        break;
    case FUNCTION_TOGGLE_FULLSCREEN:
        function_toggle_fullscreen(x,y);
        break;
    case FUNCTION_TOGGLE_SWAP_PADDLES:
        function_toggle_swap_paddles(x,y);
        break;
    case FUNCTION_TOGGLE_INVERT_PADDLES:
        function_toggle_invert_paddles(x,y);
        break;
    case FUNCTION_TOGGLE_SLOW_PADDLES:
        function_toggle_slow_paddles(x,y);
        break;
    case FUNCTION_TOGGLE_LIMIT_SPEED:
        function_toggle_limit_speed(x,y);
        break;
    case FUNCTION_ENTER_DEBUGGER:
        function_enter_debugger(x,y);
        break;
    }
    return 1;
}


int
get_button2_function(void)
{
    return func_button2;
}

int
set_button2_function(int val)
{
    func_button2 = val;
    return 1;
}

int
get_button3_function(void)
{
    return func_button3;
}

int
set_button3_function(int val)
{
    func_button3 = val;
    return 1;
}

int
get_func_f6(void)
{
    return func_f6;
}

int
set_func_f6(int val)
{
    func_f6 = val;
    return 1;
}

int
get_func_f7(void)
{
    return func_f7;
}

int
set_func_f7(int val)
{
    func_f7 = val;
    return 1;
}

int
get_func_f8(void)
{
    return func_f8;
}

int
set_func_f8(int val)
{
    func_f8 = val;
    return 1;
}

int
get_func_f9(void)
{
    return func_f9;
}

int
set_func_f9(int val)
{
    func_f9 = val;
    return 1;
}

int
get_func_f10(void)
{
    return func_f10;
}

int
set_func_f10(int val)
{
    func_f10 = val;
    return 1;
}

int
get_func_f11(void)
{
    return func_f11;
}

int
set_func_f11(int val)
{
    func_f11 = val;
    return 1;
}

int
get_func_f12(void)
{
    return func_f12;
}

int
set_func_f12(int val)
{
    func_f12 = val;
    return 1;
}
