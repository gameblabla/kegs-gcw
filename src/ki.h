/****************************************************************/
/*    	Apple IIgs emulator                                     */
/*                                                              */
/*    This code may not be used in a commercial product         */
/*    without prior written permission of the authors.          */
/*                                                              */
/*    KEGS External Interface (KI) by Olivier Goguel.           */
/*    You may freely distribute this code.                      */ 
/*                                                              */
/****************************************************************/


#ifndef _KI_H_
#define _KI_H_

#include <sys/types.h>

// Functions to be redefined by host


#ifdef DISABLE_CONFFILE
int	ki_getBootSlot(void);
const char* ki_getLocalIMG(int _slot,int _drive);
void ki_ejectIMG(int _slot,int _drive);
int	ki_mountImages(void);
#endif

// Communication functions with host

#define	HALT_WANTTOQUIT	0x20
#define	HALT_WANTTOBRK	0x40

enum _speed
{
	SPEED_UNLIMITED = 0,
	SPEED_1MHZ = 1,
	SPEED_GS = 2,	// 2.8Mhz
	SPEED_ZIP = 3,	// Zip speed	
	SPEED_ENUMSIZE=3
};
typedef enum _speed ESpeed;
 
int ki_main(int argc, char** argv);
int ki_getHaltSim(void);
void ki_setHaltSim(int _value);
extern  void ki_clearHaltSim(void);
int	ki_getLimitSpeed();
int	ki_setLimitSpeed(int);
float	ki_getEmulatorSpeed(void);
int ki_showConsole();
int ki_hideConsole();

#ifdef WIN32
int ki_printf(const char* format,...);
int ki_printfnl(const char* format, ...);
int ki_alert(const char* format, ...);
ssize_t ki_read(int fd, void *buf, size_t count);
void ki_loading(int motorOn);
#else
#define	ki_printf	printf
#define	ki_alert	printf
#define ki_printfnl	puts
#define ki_read read
	/* static void ki_loading(int) { } */
#endif

#endif
