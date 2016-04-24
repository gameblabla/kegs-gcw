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

const char rcsid_paddles_c[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/paddles.c,v 1.3 2005/09/23 12:37:09 fredyd Exp $";

#include "sim65816.h"
#include "joystick.h"

double	g_paddle_trig_dcycs = 0.0;
int	g_swap_paddles = 0;
int	g_invert_paddles = 0;
int	g_slow_paddles = 0;

int	g_joystick_type = JOYSTICK_MOUSE;

int	g_paddle_button[4] = { 0, 0, 0, 0 };
		/* g_paddle_button[0] = button 0, etc */

int	g_paddle_val[4] = { 0, 0, 0, 0 };
		/* g_paddle_val[0]: Joystick X coord, [1]:Y coord */


void
paddle_trigger(double dcycs)
{
	/* Called by read/write to $c070 */
	g_paddle_trig_dcycs = dcycs;

	/* Determine what all the paddle values are right now */

    joystick_update();
}

int
read_paddles(int paddle, double dcycs)
{
	double	trig_dcycs;
	int	val;

	/* This routine is called by any read to $c064-$c067 */
	if(g_swap_paddles) {
		paddle = paddle ^ 1;
	}

	paddle = paddle & 3;

	val = g_paddle_val[paddle];

	if(g_invert_paddles) {
		val = 255 - val;
	}

	if (g_slow_paddles) {
        /* original suggestion: val = val - 66; */
        val = 128 + (val - 128)*190/255;
	}

	/* convert 0->255 into 0->2816.0 cycles (the paddle delay const) */
	trig_dcycs = g_paddle_trig_dcycs + (val * 11.0);

	if(dcycs < trig_dcycs) {
		return 0x80;
	} else {
		return 0x00;
	}
}

int
get_swap_paddles(void)
{
    return g_swap_paddles;
}

int
set_swap_paddles(int val)
{
    g_swap_paddles = val;
    return 1;
}

int
get_invert_paddles(void)
{
    return g_invert_paddles;
}

int
set_invert_paddles(int val)
{
    g_invert_paddles = val;
    return 1;
}

int
get_slow_paddles(void)
{
    return g_slow_paddles;
}

int
set_slow_paddles(int val)
{
    g_slow_paddles = val;
    return 1;
}
