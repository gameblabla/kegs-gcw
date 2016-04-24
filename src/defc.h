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

#ifndef KEGS_DEFC_H
#define KEGS_DEFC_H

#ifdef _WIN32
#define CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif


#include "defcomm.h"

#define STRUCT(a) typedef struct _ ## a a; struct _ ## a

typedef unsigned char byte;
typedef unsigned short word16;
typedef unsigned int word32;
#ifdef _MSC_VER
typedef unsigned __int64 word64;
#else
typedef unsigned long long word64;
#endif

void U_STACK_TRACE();

/* 28MHz crystal, plus every 65th 1MHz cycle is stretched 140ns */
#define CYCS_28_MHZ		(28636360)
#define DCYCS_28_MHZ		(1.0*CYCS_28_MHZ)
#define CYCS_3_5_MHZ		(CYCS_28_MHZ/8)
#define DCYCS_1_MHZ		((DCYCS_28_MHZ/28.0)*(65.0*7/(65.0*7+1.0)))
#define CYCS_1_MHZ		((int)DCYCS_1_MHZ)

#define DCYCS_IN_16MS_RAW	(DCYCS_1_MHZ / 60.0)
#define DCYCS_IN_16MS		((double)((int)DCYCS_IN_16MS_RAW))
#define DRECIP_DCYCS_IN_16MS	(1.0 / (DCYCS_IN_16MS))

#ifdef KEGS_LITTLE_ENDIAN
# define BIGEND(a)    ((((a) >> 24) & 0xff) +			\
			(((a) >> 8) & 0xff00) + 		\
			(((a) << 8) & 0xff0000) + 		\
			(((a) << 24) & 0xff000000))
# define GET_BE_WORD16(a)	((((a) >> 8) & 0xff) + (((a) << 8) & 0xff00))
# define GET_BE_WORD32(a)	(BIGEND(a))
#else
# define BIGEND(a)	(a)
# define GET_BE_WORD16(a)	(a)
# define GET_BE_WORD32(a)	(a)
#endif

#define MAXNUM_HEX_PER_LINE     32

#ifdef __NeXT__
# include <libc.h>
#endif
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#if !defined(WIN32)
#include <unistd.h>
#include <sys/ioctl.h>
#endif
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HPUX
# include <machine/inline.h>		/* for GET_ITIMER */
#endif

#ifdef SOLARIS
# include <sys/filio.h>
#endif

#ifndef O_BINARY
/* work around some Windows junk */
# define O_BINARY	0
#endif

#ifndef W_OK		// OG Missing Windows declaration
#define	W_OK		2
#endif

STRUCT(Pc_log) {
	double	dcycs;
	word32	dbank_kpc;
	word32	instr;
	word32	psr_acc;
	word32	xreg_yreg;
	word32	stack_direct;
	word32	pad;
};

STRUCT(Event) {
	double	dcycs;
	int	type;
	Event	*next;
};

STRUCT(Fplus) {
	double	plus_1;
	double	plus_2;
	double	plus_3;
	double	plus_x_minus_1;
};

STRUCT(Engine_reg) {
	double	fcycles;
	word32	kpc;
	word32	acc;

	word32	xreg;
	word32	yreg;

	word32	stack;
	word32	dbank;

	word32	direct;
	word32	psr;
	Fplus	*fplus_ptr;
};

typedef byte *Pg_info;
STRUCT(Page_info) {
	Pg_info rd_wr;
};

#ifdef __LP64__
# define PTR2WORD(a)	((unsigned long)(a))
#else
# define PTR2WORD(a)	((unsigned int)(a))
#endif


#define ALTZP	(statereg & 0x80)
#define PAGE2	(statereg & 0x40)
#define RAMRD	(statereg & 0x20)
#define RAMWRT	(statereg & 0x10)
#define RDROM	(statereg & 0x08)
#define LCBANK2	(statereg & 0x04)
#define ROMB	(statereg & 0x02)
#define INTCX	(statereg & 0x01)

#define EXTRU(val, pos, len) 				\
	( ( (len) >= (pos) + 1) ? ((val) >> (31-(pos))) : \
	  (((val) >> (31-(pos)) ) & ( (1<<(len) ) - 1) ) )

#define DEP1(val, pos, old_val)				\
	(((old_val) & ~(1 << (31 - (pos))) ) |		\
	 ( ((val) & 1) << (31 - (pos))) )

#define set_halt(val) \
	if(val) { set_halt_act(val); }

#define clear_halt() \
	clr_halt_act()

extern int errno;

#define GET_PAGE_INFO_RD(page) \
	(page_info_rd_wr[page].rd_wr)

#define GET_PAGE_INFO_WR(page) \
	(page_info_rd_wr[0x10000 + PAGE_INFO_PAD_SIZE + (page)].rd_wr)

#define SET_PAGE_INFO_RD(page,val) \
	;page_info_rd_wr[page].rd_wr = (Pg_info)val;

#define SET_PAGE_INFO_WR(page,val) \
	;page_info_rd_wr[0x10000 + PAGE_INFO_PAD_SIZE + (page)].rd_wr = \
							(Pg_info)val;

#define VERBOSE_DISK	0x001
#define VERBOSE_IRQ	0x002
#define VERBOSE_CLK	0x004
#define VERBOSE_SHADOW	0x008
#define VERBOSE_IWM	0x010
#define VERBOSE_DOC	0x020
#define VERBOSE_ADB	0x040
#define VERBOSE_SCC	0x080
#define VERBOSE_TEST	0x100
#define VERBOSE_VIDEO	0x200

#ifdef NO_VERB
# define DO_VERBOSE	0
#else
# define DO_VERBOSE	1
#endif

#define disk_printf	if(DO_VERBOSE && (Verbose & VERBOSE_DISK)) printf
#define irq_printf	if(DO_VERBOSE && (Verbose & VERBOSE_IRQ)) printf
#define clk_printf	if(DO_VERBOSE && (Verbose & VERBOSE_CLK)) printf
#define shadow_printf	if(DO_VERBOSE && (Verbose & VERBOSE_SHADOW)) printf
#define iwm_printf	if(DO_VERBOSE && (Verbose & VERBOSE_IWM)) printf
#define doc_printf	if(DO_VERBOSE && (Verbose & VERBOSE_DOC)) printf
#define adb_printf	if(DO_VERBOSE && (Verbose & VERBOSE_ADB)) printf
#define scc_printf	if(DO_VERBOSE && (Verbose & VERBOSE_SCC)) printf
#define test_printf	if(DO_VERBOSE && (Verbose & VERBOSE_TEST)) printf
#define vid_printf	if(DO_VERBOSE && (Verbose & VERBOSE_VIDEO)) printf


#define HALT_ON_SCAN_INT	0x001
#define HALT_ON_IRQ		0x002
#define HALT_ON_SHADOW_REG	0x004
#define HALT_ON_C70D_WRITES	0x008

#define HALT_ON(a, msg)			\
	if(Halt_on & a) {		\
		halt_printf(msg);	\
	}


#ifndef MIN
# define MIN(a,b)	(((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
# define MAX(a,b)	(((a) < (b)) ? (b) : (a))
#endif

#ifdef HPUX
# define GET_ITIMER(dest)	dest = get_itimer();
#else
# define GET_ITIMER(dest)	dest = 0;
#endif

#endif /* KEGS_DEFC_H */
