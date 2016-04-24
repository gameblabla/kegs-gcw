#ifndef KEGS_PADDLES_H
#define KEGS_PADDLES_H

extern int g_joystick_type;		/* in paddles.c */
extern int g_paddle_button[4];
extern int g_paddle_val[4];
extern double g_paddle_trig_dcycs;
extern int g_swap_paddles;
extern int g_invert_paddles;

int read_paddles(int paddle, double dcycs);
void paddle_trigger(double dcycs);

int get_swap_paddles(void);
int set_swap_paddles(int);
int get_invert_paddles(void);
int set_invert_paddles(int);
int get_slow_paddles(void);
int set_slow_paddles(int);

#endif /* KEGS_PADDLES_H */
