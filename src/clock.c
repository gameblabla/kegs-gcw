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

const char rcsid_clock_c[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/clock.c,v 1.3 2005/09/23 12:37:09 fredyd Exp $";

#if defined(WIN32)
#include <windows.h>
#include <mmsystem.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif
#include <time.h>

#include "sim65816.h"
#include "clock.h"
#include "dis.h"

static void update_cur_time(void);
static void clock_update_if_needed(void);
static void do_clock_data(void);

#define CLK_IDLE		1
#define CLK_TIME		2
#define CLK_INTERNAL		3
#define CLK_BRAM1		4
#define CLK_BRAM2		5

int	g_clk_mode = CLK_IDLE;
int	g_clk_read = 0;
int	g_clk_reg1 = 0;

word32	c033_data = 0;
word32	c034_val = 0;

int	bram_fd = -1;
static byte	bram[256];
word32	g_clk_cur_time = 0xa0000000;
int	g_clk_next_vbl_update = 0;

double
get_dtime()
{
	double	dtime;
#ifndef WIN32
	double	dsec;
	double	dusec;
#endif

	/* Routine used to return actual Unix time as a double */
	/* No routine cares about the absolute value, only deltas--maybe */
	/*  take advantage of that in future to increase usec accuracy */

#if defined(WIN32)
    dtime = GetTickCount()/(1000.0); 
#else  /* !WIN32 */
	struct timeval tp1;
#ifdef SOLARIS
	gettimeofday(&tp1, (void *)0);
#else
	gettimeofday(&tp1, (struct timezone *)0);
#endif

	dsec = (double)tp1.tv_sec;
	dusec = (double)tp1.tv_usec;

	dtime = dsec + (dusec / (1000.0 * 1000.0));
#endif /* !WIN32 */

	return dtime;
}

int
micro_sleep(double dtime)
{
#if !defined(WIN32)
	struct timeval Timer;
    fd_set fdr;
	int	ret;
    int soc;
#endif

	if(dtime <= 0.0) {
		return 0;
	}
	if(dtime >= 1.0) {
		halt_printf("micro_sleep called with %f!!\n", dtime);
		return -1;
	}

#if 0
	ki_printf("usleep: %f\n", dtime);
#endif

#if defined(WIN32)
    Sleep((DWORD)(dtime*1000));
#else
    soc=socket(AF_INET,SOCK_STREAM,0);
	Timer.tv_sec = 0;
	Timer.tv_usec = (dtime * 1000000.0);
	/* if( (ret = select(0, 0, 0, 0, &Timer)) < 0) { */
    FD_ZERO(&fdr);
    FD_SET(soc,&fdr);
	if( (ret = select(0, &fdr, 0, 0, &Timer)) < 0) {
		ki_printf("micro_sleep (select) ret: %d, errno: %d\n",
			ret, errno);
		return -1;
	}
    close(soc);
#endif
	return 0;
}


void
setup_bram()
{
char	bram_buf[256];
int	len;
int	i;

	char tmp_path[256];
	sprintf(bram_buf, "bram.data.%d", g_rom_version);
	snprintf(tmp_path, sizeof(tmp_path), "%s/.kegs/%s", getenv("HOME"), bram_buf);
	
	bram_fd = open(tmp_path, O_RDWR | O_CREAT | O_BINARY, 0x1b6);
	
	if(bram_fd < 0) {
		ki_printf("Couldn't open %s: %d, %d\n", bram_buf, bram_fd, errno);
		my_exit(14);
	}

	len = lseek(bram_fd, 0, SEEK_SET);
	if(len != 0) {
		ki_printf("bram lseek returned %d, %d\n", len, errno);
		my_exit(2);
	}

	len = read(bram_fd, bram, 256);
	if(len != 256) {
		ki_printf("Reading in bram failed, initing to all 0.  %d\n",len);
		for(i = 0; i < 256; i++) {
			bram[i] = 0;
		}
	}
}

void
update_cur_time()
{
	time_t	cur_time;
	unsigned int secs, secs2;

	cur_time = time(0);

	/* Figure out the timezone (effectively) by diffing two times. */
	/* this is probably not right for a few hours around daylight savings*/
	/*  time transition */
	secs2 = mktime(gmtime(&cur_time));
	secs = mktime(localtime(&cur_time));

	secs = (unsigned int)cur_time - (secs2 - secs);

	/* add in secs to make date based on Apple Jan 1, 1904 instead of */
	/*   Unix's Jan 1, 1970 */
	/*  So add in 66 years and 17 leap year days (1904 is a leap year) */
	secs += ((66*365) + 17) * (24*3600);

	g_clk_cur_time = secs;

	clk_printf("Update g_clk_cur_time to %08x\n", g_clk_cur_time);
	g_clk_next_vbl_update = g_vbl_count + 5;
}

/* clock_update called by sim65816 every VBL */
void
clock_update()
{
	/* Nothing to do */
}

void
clock_update_if_needed()
{
	int	diff;

	diff = g_clk_next_vbl_update - g_vbl_count;
	if(diff < 0 || diff > 60) {
		/* Been a while, re-read the clock */
		update_cur_time();
	}
}

word32
clock_read_c033()
{
	return c033_data;
}

word32
clock_read_c034()
{
	return c034_val;
}

void
clock_write_c033(word32 val)
{
	c033_data = val;
}

void
clock_write_c034(word32 val)
{
	c034_val = val & 0x7f;
	if((val & 0x80) != 0) {
		if((val & 0x20) == 0) {
			ki_printf("c034 write not last = 1\n");
			/* set_halt(HALT_WANTTOQUIT); */
		}
		do_clock_data();
	}
}


void
do_clock_data()
{
	word32	mask;
	int	read;
	int	op;

	clk_printf("In do_clock_data, g_clk_mode: %02x\n", g_clk_mode);

	read = c034_val & 0x40;
	switch(g_clk_mode) {
	case CLK_IDLE:
		g_clk_read = (c033_data >> 7) & 1;
		g_clk_reg1 = (c033_data >> 2) & 3;
		op = (c033_data >> 4) & 7;
		if(!read) {
			/* write */
			switch(op) {
			case 0x0:	/* Read/write seconds register */
				g_clk_mode = CLK_TIME;
				clock_update_if_needed();
				break;
			case 0x3:	/* internal registers */
				g_clk_mode = CLK_INTERNAL;
				if(g_clk_reg1 & 0x2) {
					/* extend BRAM read */
					g_clk_mode = CLK_BRAM2;
					g_clk_reg1 = (c033_data & 7) << 5;
				}
				break;
			case 0x2:	/* read/write ram 0x10-0x13 */
				g_clk_mode = CLK_BRAM1;
				g_clk_reg1 += 0x10;
				break;
			case 0x4:	/* read/write ram 0x00-0x0f */
			case 0x5: case 0x6: case 0x7:
				g_clk_mode = CLK_BRAM1;
				g_clk_reg1 = (c033_data >> 2) & 0xf;
				break;
			default:
				halt_printf("Bad c033_data in CLK_IDLE: %02x\n",
					c033_data);
			}
		} else {
			ki_printf("clk read from IDLE mode!\n");
			/* set_halt(HALT_WANTTOQUIT); */
			g_clk_mode = CLK_IDLE;
		}
		break;
	case CLK_BRAM2:
		if(!read) {
			/* get more bits of bram addr */
			if((c033_data & 0x83) == 0x00) {
				/* more address bits */
				g_clk_reg1 |= ((c033_data >> 2) & 0x1f);
				g_clk_mode = CLK_BRAM1;
			} else {
				/* halt_printf("CLK_BRAM2: c033_data: %02x!\n",
						c033_data); */
				/* Treat all BRAM. The Gog's patch for NoiseTracker */
				g_clk_mode = CLK_IDLE;
			}
		} else {
			halt_printf("CLK_BRAM2: clock read!\n");
			g_clk_mode = CLK_IDLE;
		}
		break;
	case CLK_BRAM1:
		/* access battery ram addr g_clk_reg1 */
		if(read) {
			if(g_clk_read) {
				/* Yup, read */
				c033_data = bram[g_clk_reg1];
				clk_printf("Reading BRAM loc %02x: %02x\n",
					g_clk_reg1, c033_data);
			} else {
				halt_printf("CLK_BRAM1: said wr, now read\n");
			}
		} else {
			if(g_clk_read) {
				halt_printf("CLK_BRAM1: said rd, now write\n");
			} else {
				/* Yup, write */
				clk_printf("Writing BRAM loc %02x with %02x\n",
					g_clk_reg1, c033_data);
				bram[g_clk_reg1] = c033_data;
				if(g_clk_reg1 == 0xff) {
					int len = lseek(bram_fd, 0, SEEK_SET);
					if(len != 0) {
						ki_printf("bram_wr lseek: %d,%d\n",
							len, errno);
						my_exit(14);
					}
					len = write(bram_fd, bram, 256);
					if(len != 256) {
						halt_printf("bram wr fail! %d "
							"%d\n", len, errno);
					}
				}
			}
		}
		g_clk_mode = CLK_IDLE;
		break;
	case CLK_TIME:
		if(read) {
			if(g_clk_read == 0) {
				halt_printf("Reading time, but in set mode!\n");
			}
			c033_data = (g_clk_cur_time >> (g_clk_reg1 * 8)) & 0xff;
			clk_printf("Returning time byte %d: %02x\n",
				g_clk_reg1, c033_data);
		} else {
			/* Write */
			if(g_clk_read) {
				halt_printf("Write time, but in read mode!\n");
			}
			clk_printf("Writing TIME loc %d with %02x\n",
				g_clk_reg1, c033_data);
			mask = 0xff << (8 * g_clk_reg1);

			g_clk_cur_time = (g_clk_cur_time & (~mask)) |
				((c033_data & 0xff) << (8 *g_clk_reg1));
		}
		g_clk_mode = CLK_IDLE;
		break;
	case CLK_INTERNAL:
		if(read) {
			ki_printf("Attempting to read internal reg %02x!\n",
				g_clk_reg1);
		} else {
			switch(g_clk_reg1) {
			case 0x0:	/* test register */
				if(c033_data & 0xc0) {
					ki_printf("Writing test reg: %02x!\n",
						c033_data);
					/* set_halt(HALT_WANTTOQUIT); */
				}
				break;
			case 0x1:	/* write protect reg */
				clk_printf("Writing clk wr_protect: %02x\n",
					c033_data);
				if(c033_data & 0x80) {
					ki_printf("Stop, wr clk wr_prot: %02x\n",
						c033_data);
					/* set_halt(HALT_WANTTOQUIT); */
				}
				break;
			default:
				halt_printf("Writing int reg: %02x with %02x\n",
					g_clk_reg1, c033_data);
			}
		}
		g_clk_mode = CLK_IDLE;
		break;
	default:
		halt_printf("clk mode: %d unknown!\n", g_clk_mode);
		g_clk_mode = CLK_IDLE;
		break;
	}
}

void clock_shut(void)
{
	g_clk_mode = CLK_IDLE;
	g_clk_read = 0;
	g_clk_reg1 = 0;

	c033_data = 0;
	c034_val = 0;

}
