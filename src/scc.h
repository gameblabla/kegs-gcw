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

#ifdef INCLUDE_RCSID_C
const char rcsid_scc_h[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/scc.h,v 1.3 2005/09/23 12:37:09 fredyd Exp $";
#endif

#ifndef KEGS_SCC_H
#define KEGS_SCC_H

#if defined(WIN32)
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/* my scc port 0 == channel A, port 1 = channel B */

#define	SCC_INBUF_SIZE		4096		/* must be a power of 2 */
#define	SCC_OUTBUF_SIZE		4096		/* must be a power of 2 */

STRUCT(Scc) {
	int	port;
	int	socket_state;
	int	accfd;
	int	rdwrfd;
	struct	sockaddr_in client_addr;
	int	client_len;

	int	mode;
	int	reg_ptr;
	int	reg[16];

	int	rx_queue_depth;
	byte	rx_queue[4];

	int	in_rdptr;
	int	in_wrptr;
	byte	in_buf[SCC_INBUF_SIZE];

	int	out_rdptr;
	int	out_wrptr;
	byte	out_buf[SCC_OUTBUF_SIZE];

	int	br_is_zero;
	int	tx_buf_empty;
	int	wantint_rx;
	int	wantint_tx;
	int	wantint_zerocnt;
	int	int_pending_rx;
	int	int_pending_tx;
	int	int_pending_zerocnt;

	double	br_dcycs;
	double	tx_dcycs;
	double	rx_dcycs;

	int	br_event_pending;
	int	rx_event_pending;
	int	tx_event_pending;
};

extern Scc	scc_stat[2];

void scc_init(void);
void scc_reset(void);
void scc_update(double dcycs);
void do_scc_event(int type, double dcycs);

void show_scc_state(void);
void show_scc_log(void);

word32 scc_read_reg(int port, double dcycs);
void scc_write_reg(int port, word32 val, double dcycs);

word32 scc_read_data(int port, double dcycs);
void scc_write_data(int port, word32 val, double dcycs);

void scc_add_to_readbuf(int port, word32 val, double dcycs);

/* scc_driver.c */
int scc_socket_init(int port);
void scc_accept_socket(int port);
void scc_try_fill_readbuf(int port, double dcycs);
void scc_try_to_empty_writebuf(int port);

#endif /* KEGS_SCC_H */
