/* makerom, by Frederic Devernay <frederic.devernay@m4x.org> */
/* place a 128K file named "ROM.01" in the same directory, and execute */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "defc.h"

int main(int argc, char **argv)
{
	char	*name_buf = "ROM.01";
	struct stat stat_buf;
	int	len;
	int	fd;
	int	ret;
	int	i,j;
	unsigned char *gsrom01;
	FILE *romfile;

	romfile = fopen("rom.c", "w");
	if (romfile == NULL) {
	    fprintf(stderr,"Error: cannot create rom.c\n");
	    exit(1);
	}

	fprintf(romfile, "#ifdef DISABLE_CONFFILE\n"
		"const unsigned char gsrom01[]={\n");

	fd = open(name_buf, O_RDONLY | O_BINARY);
	if(fd < 0) {
		fprintf(stderr, "Open ROM file %s failed:%d, errno:%d\n", name_buf, fd,
				errno);
		fprintf(stderr, "Please place a readable ROM.01 file in the src subdirectory\n");
		fprintf(stderr, "Creating an empty rom.c file\n");
		fprintf(romfile, "#error \"no ROM.01 file available at compile-time\"\n};\n#endif\n");
		fclose(romfile);
		exit(0);
	}

	ret = fstat(fd, &stat_buf);
	if(ret != 0) {
		fprintf(stderr, "fstat returned %d on fd %d, errno: %d\n",
			ret, fd, errno);
		exit(2);
	}

	len = stat_buf.st_size;
	if(len != 128*1024) {
	    fprintf(stderr, "ROM size %d, not 128K\n", len);
	    exit(4);
	}

	gsrom01 = malloc(128*1024);
	ret = read(fd, gsrom01, len);
	fprintf(stderr,"Read: %d bytes of ROM\n", ret);
	if(ret != len) {
		printf("errno: %d\n", errno);
		exit(-3);
	}
	close(fd);

	for(i=0; i<len; i+=16) {
	    for(j=0; j<15; j++)
		fprintf(romfile, "0x%02X, ", gsrom01[i+j]);
	    if ((i+j) != (len-1))
		fprintf(romfile, "0x%02X,\n", gsrom01[i+j]);
	    else
		fprintf(romfile, "0x%02X};\n", gsrom01[i+j]);
	}
	fprintf(romfile, "#endif\n");
	fclose(romfile);
	exit(0);
}
