/****************************************************************/
/*    	Apple IIgs emulator                                     */
/*                                                              */
/*    This code may not be used in a commercial product         */
/*    without prior written permission of the authors.          */
/*                                                              */
/*    SDL Code by Frederic Devernay	                        */
/*    You may freely distribute this code.                      */ 
/*                                                              */
/****************************************************************/

typedef enum {
    FUNCTION_NONE,
    FUNCTION_TOGGLE_FAST_DISK_EMUL,
    FUNCTION_TOGGLE_WARP_POINTER,
    FUNCTION_TOGGLE_VIDEOMODE,
    FUNCTION_TOGGLE_FULLSCREEN,
    FUNCTION_TOGGLE_SWAP_PADDLES,
    FUNCTION_TOGGLE_INVERT_PADDLES,
    FUNCTION_TOGGLE_SLOW_PADDLES,
    FUNCTION_TOGGLE_LIMIT_SPEED,
    FUNCTION_ENTER_DEBUGGER
} function_e;

extern function_e func_f6, func_f7, func_f8, func_f9, func_f10, func_f11, func_f12;
extern function_e func_button2, func_button3;

int function_execute(function_e,int x,int y);
int get_button2_function(void);
int set_button2_function(int);
int get_button3_function(void);
int set_button3_function(int);
int get_func_f6(void);
int set_func_f6(int);
int get_func_f7(void);
int set_func_f7(int);
int get_func_f8(void);
int set_func_f8(int);
int get_func_f9(void);
int set_func_f9(int);
int get_func_f10(void);
int set_func_f10(int);
int get_func_f11(void);
int set_func_f11(int);
int get_func_f12(void);
int set_func_f12(int);
