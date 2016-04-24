/****************************************************************/
/*			Apple //gs emulator			*/
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

typedef unsigned char byte;
typedef unsigned short word16;
typedef unsigned int word32;

extern int errno;

#define STRUCT(a)	typedef struct _ ## a a; struct _ ## a

STRUCT(Dc_hdr) {
	char	disk_name[64];
	word32	data_size;
	word32	tag_size;
	word32	data_cksum;
	word32	tag_cksum;
	byte	disk_format;
	byte	format;
	word16	private;
};

#define MAX_DC_SIZE	2000*1024
byte	rawbuf[MAX_DC_SIZE];

int
main(int argc, char **argv)
{
	Dc_hdr	*dchdr;
	byte	*rawptr;
	int	ret;
	int	len;
	int	rawlen;

	len = read(0, rawbuf, MAX_DC_SIZE);
	fprintf(stderr, "Read 0x%x bytes\n", len);
	if(len >= MAX_DC_SIZE) {
		fprintf(stderr, "..which is too big!  > 0x%x\n", MAX_DC_SIZE);
		exit(1);
	}
	dchdr = (Dc_hdr *)&(rawbuf[0]);

	rawptr = (byte *)&(dchdr[1]);

	rawlen = len - sizeof(Dc_hdr);
	if(rawlen > 819200) {
		rawlen = 819200;
	}
	ret = write(1, rawptr, rawlen);
	if(ret != rawlen) {
		fprintf(stderr, "Write of %08x ret %08x, errno: %d\n",
			rawlen, ret, errno);
		exit(2);
	}
    exit(0);
}
