/****************************************************************/
/*			Apple IIgs emulator			*/
/*			Copyright 1998 Kent Dickey		*/
/*								*/
/*	This code may not be used in a commercial product	*/
/*	without prior written permission of the author.		*/
/*								*/
/*	You may freely distribute this code.			*/ 
/*								*/
/*	You can contact the author at kentd@cup.hp.com.		*/
/*	HP has nothing to do with this software.		*/
/****************************************************************/

const char rcsid_scc_driver_h[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/scc_driver.c,v 1.3 2005/09/23 12:37:09 fredyd Exp $";

#include "sim65816.h"
#include "scc.h"

/* This file contains the Unix socket calls */

#if defined(HPUX) || defined(__linux__) || defined(SOLARIS)
int
scc_socket_init(int port)
{
	struct sockaddr_in sa_in;
	int	ret;
	int	sockfd;
	int	inc;

	int	on;

	inc = 0;

	while(1) {
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if(sockfd < 0) {
			ki_printf("socket ret: %d, errno: %d\n", sockfd, errno);
			my_exit(3);
		}
		/* ki_printf("socket ret: %d\n", sockfd); */

		on = 1;
		ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
					(char *)&on, sizeof(on));
		if(ret < 0) {
			ki_printf("setsockopt REUSEADDR ret: %d, err:%d\n",
				ret, errno);
			my_exit(3);
		}

		memset(&sa_in, 0, sizeof(sa_in));
		sa_in.sin_family = AF_INET;
		sa_in.sin_port = htons(6501 + port + inc);
		sa_in.sin_addr.s_addr = htonl(INADDR_ANY);

		ret = bind(sockfd, (struct sockaddr *)&sa_in, sizeof(sa_in));

		if(ret < 0) {
			ki_printf("bind ret: %d, errno: %d\n", ret, errno);
			inc++;
			close(sockfd);
			ki_printf("Trying next port: %d\n", 6501 + port + inc);
			if(inc >= 10) {
				ki_printf("Too many retries, quitting\n");
				my_exit(3);
			}
		} else {
			break;
		}
	}

	ki_printf("SCC port %d is at unix port %d\n", port, 6501 + port + inc);

	ret = listen(sockfd, 1);

	on = 1;
#ifdef FIOSNBIO
	ret = ioctl(sockfd, FIOSNBIO, (char *)&on);
#else
	ret = ioctl(sockfd, FIONBIO, (char *)&on);
#endif
	if(ret == -1) {
		ki_printf("ioctl ret: %d, errno: %d\n", ret,errno);
		my_exit(3);
	}

	return sockfd;
}

void
scc_accept_socket(int port)
{
	Scc	*scc_ptr;
	int	rdwrfd;

	scc_ptr = &(scc_stat[port]);

	if(scc_ptr->socket_state == 0) {
		scc_ptr->client_len = sizeof(scc_ptr->client_addr);
		rdwrfd = accept(scc_ptr->accfd,
			(struct sockaddr *)&(scc_ptr->client_addr),
			&(scc_ptr->client_len));
		if(rdwrfd < 0) {
			return;
		}
		scc_ptr->rdwrfd = rdwrfd;
	}
}

void
scc_try_fill_readbuf(int port, double dcycs)
{
	byte	tmp_buf[256];
	Scc	*scc_ptr;
	int	rdwrfd;
	int	i;
	int	ret;

	scc_ptr = &(scc_stat[port]);

	/* Accept socket if not already open */
	scc_accept_socket(port);

	rdwrfd = scc_ptr->rdwrfd;
	if(rdwrfd < 0) {
		return;
	}

	/* Try reading some bytes */
	ret = read(rdwrfd, tmp_buf, 256);
	if(ret > 0) {
		for(i = 0; i < ret; i++) {
			if(tmp_buf[i] == 0) {
				/* Skip null chars */
				continue;
			}
			scc_add_to_readbuf(port, tmp_buf[i], dcycs);
		}
	}
	
}

void
scc_try_to_empty_writebuf(int port)
{
	Scc	*scc_ptr;
	int	rdptr;
	int	wrptr;
	int	rdwrfd;
	int	done;
	int	ret;
	int	len;

	scc_ptr = &(scc_stat[port]);

	scc_accept_socket(port);

	rdwrfd = scc_ptr->rdwrfd;
	if(rdwrfd < 0) {
		return;
	}

	/* Try writing some bytes */
	done = 0;
	while(!done) {
		rdptr = scc_ptr->out_rdptr;
		wrptr = scc_ptr->out_wrptr;
		if(rdptr == wrptr) {
			done = 1;
			break;
		}
		len = wrptr - rdptr;
		if(len < 0) {
			len = SCC_OUTBUF_SIZE - rdptr;
		}
		if(len > 32) {
			len = 32;
		}
		if(len <= 0) {
			done = 1;
			break;
		}
		ret = write(rdwrfd, &(scc_ptr->out_buf[rdptr]), len);
		if(ret <= 0) {
			done = 1;
			break;
		} else {
			rdptr = rdptr + ret;
			if(rdptr >= SCC_OUTBUF_SIZE) {
				rdptr = rdptr - SCC_OUTBUF_SIZE;
			}
			scc_ptr->out_rdptr = rdptr;
		}
	}
}

#else
/* OS2 and others */
/* unknown or unsupported environment---just stub out the calls */
int
scc_socket_init(int port)
{
	return -1;
}

void
scc_accept_socket(int port)
{
}

void
scc_try_fill_readbuf(int port, double dcycs)
{
}

void
scc_try_to_empty_writebuf(int port)
{
}
#endif
