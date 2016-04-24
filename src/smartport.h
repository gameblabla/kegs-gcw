#ifndef KEGS_SMARTPORT_H
#define KEGS_SMARTPORT_H

#include "iwm.h"

extern word32 g_cycs_in_io_read;

void smartport_error(void);
void do_c700(word32 ret);
void do_c70a(word32 arg0);
void do_c70d(word32 arg0);
int do_read_c7(int unit_num, word32 buf, int blk);
int do_write_c7(int unit_num, word32 buf, int blk);
int do_format_c7(int unit_num);
int get_fd_size(int fd);
int find_partition_by_name(int fd, char *name, Disk *dsk);

#endif /* KEGS_SMARTPORT_H */
