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

const char rcsid_smartport_c[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/smartport.c,v 1.3 2005/09/23 12:37:09 fredyd Exp $";

#include "sim65816.h"
#include "smartport.h"
#include "iwm.h"
#include "engine.h"
#include "dis.h"

static void read_partition_block(int fd, void *buf, int blk, int blk_size);
static void smartport_log(word32 start_addr, int cmd, int rts_addr, int cmd_list);

word32 g_cycs_in_io_read = 0;


#define MAX_BLOCK_SIZE		0x4000

word32	part_blk_buf[MAX_BLOCK_SIZE];

int
get_fd_size(int fd)
{
	struct stat stat_buf;
	int	ret;

	ret = fstat(fd, &stat_buf);
	if(ret != 0) {
		fprintf(stderr,"fstat returned %d on fd %d, errno: %d\n",
			ret, fd, errno);
		my_exit(2);
	}
	return stat_buf.st_size;

}

void
read_partition_block(int fd, void *buf, int blk, int blk_size)
{
	int	ret;

	ret = lseek(fd, blk * blk_size, SEEK_SET);
	if(ret != blk * blk_size) {
		ki_printf("lseek: %08x, wanted: %08x, errno: %d\n", ret,
			blk * blk_size, errno);
		my_exit(1);
	}

	ret = read(fd, (char *)buf, blk_size);
	if(ret != blk_size) {
		ki_printf("ret: %08x, wanted %08x, errno: %d\n", ret, blk_size,
			errno);
		my_exit(1);
	}
}


int
find_partition_by_name(int fd, char *name, Disk *dsk)
{
	Driver_desc *driver_desc_ptr;
	Part_map *part_map_ptr;
	int	block_size;
	int	map_blks;
	int	cur_blk;
	int	match_number;
	word32	start;
	word32	len;
	word32	data_off;
	word32	data_len;
	word32	sig;

	block_size = 512;

	match_number = -1;
	if(*name >= '0' && *name <= '9') {
		/* find partition by number! */
		match_number = atoi(name);
	}

	read_partition_block(fd, part_blk_buf, 0, block_size);

	driver_desc_ptr = (Driver_desc *)part_blk_buf;
	sig = GET_BE_WORD16(driver_desc_ptr->sig);
	block_size = GET_BE_WORD16(driver_desc_ptr->blk_size);
	if(block_size == 0) {
		block_size = 512;
	}
	if(sig != 0x4552 || block_size < 0x200 || block_size > MAX_BLOCK_SIZE) {
		ki_printf("Partition error: No driver descriptor map found\n");
		return -1;
	}

	map_blks = 1;
	cur_blk = 0;
	while(cur_blk < map_blks) {
		read_partition_block(fd, part_blk_buf, cur_blk + 1, block_size);
		part_map_ptr = (Part_map *)part_blk_buf;
		sig = GET_BE_WORD16(part_map_ptr->sig);
		if(cur_blk == 0) {
			map_blks = MIN(100,
				GET_BE_WORD32(part_map_ptr->map_blk_cnt));
		}
		if(sig != 0x504d) {
			ki_printf("Partition entry %d bad sig\n", cur_blk);
			return -1;
		}

		if((strncmp(name, part_map_ptr->part_name, 32) == 0) ||
						(cur_blk == match_number)) {
			/* found it, check for consistency */
			start = GET_BE_WORD32(part_map_ptr->phys_part_start);
			len = GET_BE_WORD32(part_map_ptr->part_blk_cnt);
			data_off = GET_BE_WORD32(part_map_ptr->data_start);
			data_len = GET_BE_WORD32(part_map_ptr->data_cnt);
			if(data_off + data_len > len) {
				ki_printf("Poorly formed entry\n");
				return -1;
			}

			if(data_len < 10 || start < 1) {
				ki_printf("Poorly formed entry3\n");
				return -1;
			}

			dsk->image_start = (start + data_off) * block_size;
			dsk->image_size = (data_len) * block_size;

			return 0;
		}

		cur_blk++;
	}

	return -1;
}

#define LEN_SMPT_LOG	16
STRUCT(Smpt_log) {
	word32	start_addr;
	int	cmd;
	int	rts_addr;
	int	cmd_list;
	int	extras;
	int	unit;
	int	buf;
	int	blk;
};

Smpt_log g_smpt_log[LEN_SMPT_LOG];
int	g_smpt_log_pos = 0;

void
smartport_error(void)
{
	int	pos;
	int	i;

	pos = g_smpt_log_pos;
	ki_printf("Smartport log pos: %d\n", pos);
	for(i = 0; i < LEN_SMPT_LOG; i++) {
		pos--;
		if(pos < 0) {
			pos = LEN_SMPT_LOG - 1;
		}
		ki_printf("%d:%d: t:%04x, cmd:%02x, rts:%04x, "
			"cmd_l:%04x, x:%d, unit:%d, buf:%04x, blk:%04x\n",
			i, pos,
			g_smpt_log[pos].start_addr,
			g_smpt_log[pos].cmd,
			g_smpt_log[pos].rts_addr,
			g_smpt_log[pos].cmd_list,
			g_smpt_log[pos].extras,
			g_smpt_log[pos].unit,
			g_smpt_log[pos].buf,
			g_smpt_log[pos].blk);
	}
}
void
smartport_log(word32 start_addr, int cmd, int rts_addr, int cmd_list)
{
	int	pos;

	pos = g_smpt_log_pos;
	if(start_addr != 0) {
		g_smpt_log[pos].start_addr = start_addr;
		g_smpt_log[pos].cmd = cmd;
		g_smpt_log[pos].rts_addr = rts_addr;
		g_smpt_log[pos].cmd_list = cmd_list;
		g_smpt_log[pos].extras = 0;
		g_smpt_log[pos].unit = 0;
		g_smpt_log[pos].buf = 0;
		g_smpt_log[pos].blk = 0;
	} else {
		pos--;
		if(pos < 0) {
			pos = LEN_SMPT_LOG - 1;
		}
		g_smpt_log[pos].extras = 1;
		g_smpt_log[pos].unit = cmd;
		g_smpt_log[pos].buf = rts_addr;
		g_smpt_log[pos].blk = cmd_list;
	}
	pos++;
	if(pos >= LEN_SMPT_LOG) {
		pos = 0;
	}
	g_smpt_log_pos = pos;
}

void
do_c70d(word32 arg0)
{
	int	cmd;
	int	cmd_list_lo, cmd_list_mid, cmd_list_hi;
	int	rts_lo, rts_hi;
	word32	rts_addr;
	word32	cmd_list;
	int	unit;
	int	param_cnt;
	int	status_ptr_lo, status_ptr_mid, status_ptr_hi;
	int	buf_ptr_lo, buf_ptr_hi;
	int	buf_ptr;
	int	block_lo, block_mid, block_hi;
	int	block;
	word32	status_ptr;
	int	status_code;
	int	ctl_ptr_lo, ctl_ptr_hi;
	int	ctl_ptr;
	int	ctl_code;
	int	mask;
	int	stat_val;
	int	size;
	int	ret;
	int	ext;
	int	i;

	set_memory_c(0x7f8, 0xc7, 0);

	if((engine.psr & 0x100) == 0) {
		disk_printf("c70d called in native mode!\n");
		if((engine.psr & 0x30) != 0x30) {
			halt_printf("c70d called native, psr: %03x!\n",
							engine.psr);
		}
	}

	engine.stack = ((engine.stack + 1) & 0xff) + 0x100;
	rts_lo = get_memory_c(engine.stack, 0);
	engine.stack = ((engine.stack + 1) & 0xff) + 0x100;
	rts_hi = get_memory_c(engine.stack, 0);
	rts_addr = (rts_lo + (256*rts_hi) + 1) & 0xffff;
	disk_printf("rts_addr: %04x\n", rts_addr);

	cmd = get_memory_c(rts_addr, 0);
	cmd_list_lo = get_memory_c((rts_addr + 1) & 0xffff, 0);
	cmd_list_mid = get_memory_c((rts_addr + 2) & 0xffff, 0);
	cmd_list_hi = 0;
	mask = 0xffff;
	if(cmd & 0x40) {
		/* extended */
		mask = 0xffffff;
		cmd_list_hi = get_memory_c((rts_addr + 3) & 0xffff, 0);
	}

	cmd_list = cmd_list_lo + (256*cmd_list_mid) + (65536*cmd_list_hi);

	disk_printf("cmd: %02x, cmd_list: %06x\n", cmd, cmd_list);
	param_cnt = get_memory_c(cmd_list, 0);

	ext = 0;
	if(cmd & 0x40) {
		ext = 2;
	}

	smartport_log(0xc70d, cmd, rts_addr, cmd_list);

	switch(cmd & 0x3f) {
	case 0x00:	/* Status == 0x00 and 0x40 */
		if(param_cnt != 3) {
			disk_printf("param_cnt %d is != 3!\n", param_cnt);
			my_exit(8);
		}
		unit = get_memory_c((cmd_list+1) & mask, 0);
		status_ptr_lo = get_memory_c((cmd_list+2) & mask, 0);
		status_ptr_mid = get_memory_c((cmd_list+3) & mask, 0);
		status_ptr_hi = 0;
		if(cmd & 0x40) {
			status_ptr_hi = get_memory_c((cmd_list+4) & mask, 0);
		}

		status_ptr = status_ptr_lo + (256*status_ptr_mid) +
			(65536*status_ptr_hi);
		if(cmd & 0x40) {
			status_code = get_memory_c((cmd_list+6) & mask, 0);
		} else {
			status_code = get_memory_c((cmd_list+4) & mask, 0);
		}

		smartport_log(0, unit, status_ptr, status_code);

		disk_printf("unit: %02x, status_ptr: %06x, code: %02x\n",
			unit, status_ptr, status_code);
		if(unit == 0 && status_code == 0) {
			/* Smartport driver status */
			/* see technotes/smpt/tn-smpt-002 */
			set_memory_c(status_ptr, g_highest_smartport_unit+1, 0);
			set_memory_c(status_ptr+1, 0xff, 0); /* interrupt stat*/
			set_memory_c(status_ptr+2, 0x02, 0); /* vendor id */
			set_memory_c(status_ptr+3, 0x00, 0); /* vendor id */
			set_memory_c(status_ptr+4, 0x00, 0); /* version lo */
			set_memory_c(status_ptr+5, 0x10, 0); /* version hi */
			set_memory_c(status_ptr+6, 0x00, 0);
			set_memory_c(status_ptr+7, 0x00, 0);

			engine.xreg = 8;
			engine.yreg = 0;
			engine.acc &= 0xff00;
			engine.psr &= ~1;
			engine.kpc = (rts_addr + 3 + ext) & mask;
			return;
		} else if(unit > 0 && status_code == 0) {
			/* status for unit x */
			if(unit > MAX_C7_DISKS || iwm.smartport[unit-1].fd < 0){
				stat_val = 0x80;
				size = 0;
			} else {
				stat_val = 0xf8;
				size = iwm.smartport[unit-1].image_size;
				size = (size+511) / 512;
			}
			set_memory_c(status_ptr, stat_val, 0);
			set_memory_c(status_ptr +1, size & 0xff, 0);
			set_memory_c(status_ptr +2, (size >> 8) & 0xff, 0);
			set_memory_c(status_ptr +3, (size >> 16) & 0xff, 0);
			engine.xreg = 4;
			if(cmd & 0x40) {
				set_memory_c(status_ptr + 4,
						(size >> 16) & 0xff, 0);
				engine.xreg = 5;
			}
			engine.yreg = 0;
			engine.acc &= 0xff00;
			engine.psr &= ~1;
			engine.kpc = (rts_addr + 3 + ext) & mask;

			disk_printf("just finished unit %d, stat 0\n", unit);
			return;
		} else if(status_code == 3) {
			if(unit > MAX_C7_DISKS || iwm.smartport[unit-1].fd < 0){
				stat_val = 0x80;
				size = 0;
			} else {
				stat_val = 0xf8;
				size = iwm.smartport[unit-1].image_size;
				size = (size+511) / 512;
			}
			if(cmd & 0x40) {
				disk_printf("extended for stat_code 3!\n");
			}
			/* DIB for unit 1 */
			set_memory_c(status_ptr, stat_val, 0);
			set_memory_c(status_ptr +1, size & 0xff, 0);
			set_memory_c(status_ptr +2, (size >> 8) & 0xff, 0);
			set_memory_c(status_ptr +3, (size >> 16) & 0xff, 0);
			if(cmd & 0x40) {
				set_memory_c(status_ptr + 4,
						(size >> 24) & 0xff, 0);
				status_ptr++;
			}
			set_memory_c(status_ptr +4, 4, 0);
			for(i = 5; i < 21; i++) {
				set_memory_c(status_ptr +i, 0x20, 0);
			}
			set_memory_c(status_ptr +5, 'K', 0);
			set_memory_c(status_ptr +6, 'D', 0);
			set_memory_c(status_ptr +7, '3', 0);
			set_memory_c(status_ptr +8, '5', 0);

			/* hard disk supporting extended calls */
			set_memory_c(status_ptr + 21, 0x02, 0);
			set_memory_c(status_ptr + 22, 0xa0, 0);

			set_memory_c(status_ptr + 23, 0x00, 0);
			set_memory_c(status_ptr + 24, 0x00, 0);

			if(cmd & 0x40) {
				engine.xreg = 26;
			} else {
				engine.xreg = 25;
			}
			engine.yreg = 0;
			engine.acc &= 0xff00;
			engine.psr &= ~1;
			engine.kpc = (rts_addr + 3 + ext) & 0xffff;

			disk_printf("Just finished unit %d, stat 3\n", unit);
			if(unit == 0 || unit > (g_highest_smartport_unit+1)) {
				engine.acc |= 0x28;
				engine.psr |= 1;
				if(unit == 0 || unit > MAX_C7_DISKS) {
					halt_printf("unit:%02x, stat 3\n",unit);
				}
			}
			return;
		}
		ki_printf("cmd: 00, unknown unit/status code!\n");
		break;
	case 0x01:	/* Read Block == 0x01 and 0x41 */
		if(param_cnt != 3) {
			halt_printf("param_cnt %d is != 3!\n", param_cnt);
			return;
		}
		unit = get_memory_c((cmd_list+1) & mask, 0);
		buf_ptr_lo = get_memory_c((cmd_list+2) & mask, 0);
		buf_ptr_hi = get_memory_c((cmd_list+3) & mask, 0);

		buf_ptr = buf_ptr_lo + (256*buf_ptr_hi);
		if(cmd & 0x40) {
			buf_ptr_lo = get_memory_c((cmd_list+4) & mask, 0);
			buf_ptr_hi = get_memory_c((cmd_list+5) & mask, 0);
			buf_ptr += ((buf_ptr_hi*256) + buf_ptr_lo)*65536;
			cmd_list += 2;
		}
		block_lo = get_memory_c((cmd_list+4) & mask, 0);
		block_mid = get_memory_c((cmd_list+5) & mask, 0);
		block_hi = get_memory_c((cmd_list+6) & mask, 0);
		block = ((block_hi*256) + block_mid)*256 + block_lo;
		disk_printf("smartport read unit %d of block %04x into %04x\n",
			unit, block, buf_ptr);
		if(unit < 1 || unit > MAX_C7_DISKS) {
			halt_printf("Unknown unit #: %d\n", unit);
		}

		smartport_log(0, unit - 1, buf_ptr, block);

		ret = do_read_c7(unit - 1, buf_ptr, block);
		engine.xreg = 0;
		engine.yreg = 2;
		engine.acc = (engine.acc & 0xff00) | (ret & 0xff);
		engine.psr &= ~1;
		if(ret != 0) {
			engine.psr |= 1;
		}
		engine.kpc = (rts_addr + 3 + ext) & 0xffff;

		return;
		break;
	case 0x02:	/* Write Block == 0x02 and 0x42 */
		if(param_cnt != 3) {
			halt_printf("param_cnt %d is != 3!\n", param_cnt);
			return;
		}
		unit = get_memory_c((cmd_list+1) & mask, 0);
		buf_ptr_lo = get_memory_c((cmd_list+2) & mask, 0);
		buf_ptr_hi = get_memory_c((cmd_list+3) & mask, 0);

		buf_ptr = buf_ptr_lo + (256*buf_ptr_hi);
		if(cmd & 0x40) {
			buf_ptr_lo = get_memory_c((cmd_list+4) & mask, 0);
			buf_ptr_hi = get_memory_c((cmd_list+5) & mask, 0);
			buf_ptr += ((buf_ptr_hi*256) + buf_ptr_lo)*65536;
			cmd_list += 2;
		}
		block_lo = get_memory_c((cmd_list+4) & mask, 0);
		block_mid = get_memory_c((cmd_list+5) & mask, 0);
		block_hi = get_memory_c((cmd_list+6) & mask, 0);
		block = ((block_hi*256) + block_mid)*256 + block_lo;
		disk_printf("smartport write unit %d of block %04x from %04x\n",
			unit, block, buf_ptr);
		if(unit < 1 || unit > MAX_C7_DISKS) {
			halt_printf("Unknown unit #: %d\n", unit);
		}

		smartport_log(0, unit - 1, buf_ptr, block);

		ret = do_write_c7(unit - 1, buf_ptr, block);
		engine.xreg = 0;
		engine.yreg = 2;
		engine.acc = (engine.acc & 0xff00) | (ret & 0xff);
		engine.psr &= ~1;
		if(ret != 0) {
			engine.psr |= 1;
		}
		engine.kpc = (rts_addr + 3 + ext) & 0xffff;

		HALT_ON(HALT_ON_C70D_WRITES, "c70d Write done\n");
		return;
		break;
	case 0x03:	/* Format == 0x03 and 0x43 */
		if(param_cnt != 1) {
			halt_printf("param_cnt %d is != 1!\n", param_cnt);
			return;
		}
		unit = get_memory_c((cmd_list+1) & mask, 0);

		if(unit < 1 || unit > MAX_C7_DISKS) {
			halt_printf("Unknown unit #: %d\n", unit);
		}

		smartport_log(0, unit - 1, 0, 0);

		ret = do_format_c7(unit - 1);
		engine.xreg = 0;
		engine.yreg = 2;
		engine.acc = (engine.acc & 0xff00) | (ret & 0xff);
		engine.psr &= ~1;
		if(ret != 0) {
			engine.psr |= 1;
		}
		engine.kpc = (rts_addr + 3 + ext) & 0xffff;

		HALT_ON(HALT_ON_C70D_WRITES, "c70d Format done\n");
		return;
		break;
	case 0x04:	/* Control == 0x04 and 0x44 */
		if(cmd == 0x44) {
			halt_printf("smartport code 0x44 not supported\n");
		}
		if(param_cnt != 3) {
			halt_printf("param_cnt %d is != 3!\n", param_cnt);
			return;
		}
		unit = get_memory_c((cmd_list+1) & mask, 0);
		ctl_ptr_lo = get_memory_c((cmd_list+2) & mask, 0);
		ctl_ptr_hi = get_memory_c((cmd_list+3) & mask, 0);
		
		ctl_ptr = (ctl_ptr_hi << 8) + ctl_ptr_lo;
		if(cmd & 0x40) {
			ctl_ptr_lo = get_memory_c((cmd_list+4) & mask, 0);
			ctl_ptr_hi = get_memory_c((cmd_list+5) & mask, 0);
			ctl_ptr += ((ctl_ptr_hi << 8) + ctl_ptr_lo) << 16;
			cmd_list += 2;
		}

		ctl_code = get_memory_c((cmd_list +4) & mask, 0);

		switch(ctl_code) {
		case 0x00:
			ki_printf("Performing a reset on unit %d\n", unit);
			break;
		default:
			halt_printf("control code: %02x unknown!\n", ctl_code);
		}

		engine.xreg = 0;
		engine.yreg = 2;
		engine.acc &= 0xff00;
		engine.psr &= ~1;
		engine.kpc = (rts_addr + 3 + ext) & 0xffff;
		return;
		break;
	default:	/* Unknown command! */
		/* set acc = 1, and set carry, and set kpc */
		engine.xreg = (rts_addr) & 0xff;
		engine.yreg = (rts_addr >> 8) & 0xff;
		engine.acc = (engine.acc & 0xff00) + 0x01;
		engine.psr |= 0x01;	/* set carry */
		engine.kpc = (rts_addr + 3 + ext) & 0xffff;
		if(cmd != 0x4a && cmd != 0x48) {
			/* Finder does 0x4a call before formatting disk */
			/* Many things do 0x48 call to see online drives */
			halt_printf("Just did smtport cmd:%02x rts_addr:%04x, "
				"cmdlst:%06x\n", cmd, rts_addr, cmd_list);
		}
		return;
	}

	halt_printf("Unknown smtport cmd:%02x, cmd_list:%06x, rts_addr:%06x\n",
		cmd, cmd_list, rts_addr);
}

void
do_c70a(word32 arg0)
{
	int	cmd, unit;
	int	buf_lo, buf_hi;
	int	blk_lo, blk_hi;
	int	blk, buf;
	int	prodos_unit;
	int	size;
	int	ret;

	set_memory_c(0x7f8, 0xc7, 0);

	cmd = get_memory_c((engine.direct + 0x42) & 0xffff, 0);
	prodos_unit = get_memory_c((engine.direct + 0x43) & 0xffff, 0);
	buf_lo = get_memory_c((engine.direct + 0x44) & 0xffff, 0);
	buf_hi = get_memory_c((engine.direct + 0x45) & 0xffff, 0);
	blk_lo = get_memory_c((engine.direct + 0x46) & 0xffff, 0);
	blk_hi = get_memory_c((engine.direct + 0x47) & 0xffff, 0);

	blk = (blk_hi << 8) + blk_lo;
	buf = (buf_hi << 8) + buf_lo;
	disk_printf("cmd: %02x, pro_unit: %02x, buf: %04x, blk: %04x\n",
		cmd, prodos_unit, buf, blk);

	if((prodos_unit & 0x7f) == 0x70) {
		unit = 0 + (prodos_unit >> 7);
	} else if((prodos_unit & 0x7f) == 0x40) {
		unit = 2 + (prodos_unit >> 7);
	} else {
		halt_printf("Unknown prodos_unit: %d\n", prodos_unit);
		return;
	}

	smartport_log(0xc70a, cmd, blk, buf);

	if(cmd == 0x01) {
		ret = do_read_c7(unit, buf, blk);
		smartport_log(0, unit, buf, blk);
		engine.psr &= ~1;
		if(ret != 0) {
			engine.psr |= 1;
		}
		engine.acc = (engine.acc & 0xff00) | (ret & 0xff);
		if(g_rom_version >= 3) {
			engine.kpc = 0xc764;
		} else {
			engine.kpc = 0xc765;
		}
		return;
	} else if(cmd == 0x00) {
		size = iwm.smartport[unit].image_size;
		size = (size+511) / 512;

		smartport_log(0, unit, size, 0);

		engine.psr &= ~1;
		ret = 0;
		engine.acc = (engine.acc & 0xff00) | (ret & 0xff);
		if(g_rom_version >= 3) {
			engine.kpc = 0xc764;
		} else {
			engine.kpc = 0xc765;
		}
		engine.xreg = size & 0xff;
		engine.yreg = size >> 8;
		return;
	} else if(cmd == 0x02) {
		smartport_log(0, unit, buf, blk);
		ret = do_write_c7(unit, buf, blk);
		engine.psr &= ~1;
		engine.acc = (engine.acc & 0xff00) | (ret & 0xff);
		if(g_rom_version >= 3) {
			engine.kpc = 0xc764;
		} else {
			engine.kpc = 0xc765;
		}
		return;
	}
	halt_printf("cmd unknown: %02x, unit: %02x!!!!\n", cmd, unit);
}

int
do_read_c7(int unit_num, word32 buf, int blk)
{
	byte	local_buf[0x200];
	register word32 start_time;
	register word32 end_time;
	word32	val;
	int	len;
	int	fd;
	int	image_start;
	int	image_size;
	int	ret;
	int	i;

	if(unit_num < 0 || unit_num > MAX_C7_DISKS) {
		halt_printf("do_read_c7: unit_num: %d\n", unit_num);
		smartport_error();
		return 0x28;
	}

	fd = iwm.smartport[unit_num].fd;
	image_start = iwm.smartport[unit_num].image_start;
	image_size = iwm.smartport[unit_num].image_size;
	if(fd < 0) {
		ki_printf("c7_fd == %d!\n", fd);
		if(blk != 2 && blk != 0) {
			/* don't print error if only reading directory */
			smartport_error();
			halt_printf("Read unit:%02x blk:%04x\n", unit_num, blk);
		}
		return 0x2f;
	}

	ret = lseek(fd, image_start + blk*0x200, SEEK_SET);
	if(ret != image_start + blk*0x200) {
		halt_printf("lseek returned %08x, errno: %d\n", ret, errno);
		smartport_error();
		return 0x27;
	}

	if(ret >= image_start + image_size) {
		halt_printf("Tried to read from pos %08x on disk, (blk:%04x)\n",
			ret, blk);
		smartport_error();
		return 0x27;
	}

	len = read(fd, &local_buf[0], 0x200);
	if(len != 0x200) {
		ki_printf("read returned %08x, errno:%d, blk:%04x, unit: %02x\n",
			len, errno, blk, unit_num);
		halt_printf("name: %s\n", iwm.smartport[unit_num].name_ptr);
		smartport_error();
		return 0x27;
	}

	g_io_amt += 0x200;

	if(buf >= 0xfe0000) {
		disk_printf("reading into ROM, just returning\n");
		return 0;
	}

	GET_ITIMER(start_time);

	for(i = 0; i < 0x200; i += 2) {
		val = (local_buf[i+1] << 8) + local_buf[i];
		set_memory16_c(buf + i, val, 0);
	}

	GET_ITIMER(end_time);

	g_cycs_in_io_read += (end_time - start_time);

	return 0;

}

int
do_write_c7(int unit_num, word32 buf, int blk)
{
	word32	local_buf[0x200/4];
	Disk	*dsk;
	word32	*ptr;
	word32	val1, val2;
	word32	val;
	int	len;
	int	ret;
	int	fd;
	int	image_start;
	int	image_size;
	int	i;

	if(unit_num < 0 || unit_num > MAX_C7_DISKS) {
		halt_printf("do_write_c7: unit_num: %d\n", unit_num);
		smartport_error();
		return 0x28;
	}

	dsk = &(iwm.smartport[unit_num]);
	fd = dsk->fd;
	image_start = dsk->image_start;
	image_size = dsk->image_size;
	if(fd < 0) {
		halt_printf("c7_fd == %d!\n", fd);
		smartport_error();
		return 0x28;
	}

	ptr = &(local_buf[0]);
	for(i = 0; i < 0x200; i += 4) {
		val1 = get_memory16_c(buf + i, 0);
		val2 = get_memory16_c(buf + i + 2, 0);
		/* reorder the little-endian bytes to be big-endian */
#ifdef KEGS_LITTLE_ENDIAN
		val = (val2 << 16) + val1;
#else
		val = (val1 << 24) + ((val1 << 8) & 0xff0000) +
			((val2 << 8) & 0xff00) + (val2 >> 8);
#endif
		*ptr++ = val;
	}

	ret = lseek(fd, image_start + blk*0x200, SEEK_SET);
	if(ret != image_start + blk*0x200) {
		halt_printf("lseek returned %08x, errno: %d\n", ret, errno);
		smartport_error();
		return 0x27;
	}

	if(ret >= image_start + image_size) {
		halt_printf("Tried to write to %08x\n", ret);
		smartport_error();
		return 0x27;
	}

	if(dsk->write_prot) {
		ki_printf("Write, but %s is write protected!\n", dsk->name_ptr);
		return 0x2b;
	}

	if(dsk->write_through_to_unix == 0) {
		halt_printf("Write to %s, but not wr_thru!\n", dsk->name_ptr);
		return 0x00;
	}

	len = write(fd, (byte *)&local_buf[0], 0x200);
	if(len != 0x200) {
		halt_printf("write ret %08x bytes, errno: %d\n", len, errno);
		smartport_error();
		return 0x27;
	}

	g_io_amt += 0x200;

	return 0;

}

int
do_format_c7(int unit_num)
{
	byte	local_buf[0x1000];
	Disk	*dsk;
	int	len;
	int	ret;
	int	sum;
	int	total;
	int	max;
	int	image_start;
	int	image_size;
	int	fd;
	int	i;

	if(unit_num < 0 || unit_num > MAX_C7_DISKS) {
		halt_printf("do_format_c7: unit_num: %d\n", unit_num);
		smartport_error();
		return 0x28;
	}

	dsk = &(iwm.smartport[unit_num]);
	fd = dsk->fd;
	image_start = dsk->image_start;
	image_size = dsk->image_size;
	if(fd < 0) {
		halt_printf("c7_fd == %d!\n", fd);
		smartport_error();
		return 0x28;
	}

	for(i = 0; i < 0x1000; i++) {
		local_buf[i] = 0;
	}

	ret = lseek(fd, image_start, SEEK_SET);
	if(ret != image_start) {
		halt_printf("lseek returned %08x, errno: %d\n", ret, errno);
		smartport_error();
		return 0x27;
	}

	if(dsk->write_prot) {
		ki_printf("Format, but %s is write protected!\n", dsk->name_ptr);
		return 0x2b;
	}

	if(dsk->write_through_to_unix == 0) {
		ki_printf("Format of %s ignored\n", dsk->name_ptr);
		return 0x00;
	}

	sum = 0;
	total = image_size;

	while(sum < total) {
		max = MIN(0x1000, total-sum);
		len = write(fd, &local_buf[0], max);
		if(len != max) {
			halt_printf("write ret %08x, errno:%d\n", len, errno);
			smartport_error();
			return 0x27;
		}
		sum += len;
	}

	return 0;
}


void
do_c700(word32 ret)
{
	disk_printf("do_c700 called, ret: %08x\n", ret);

	ret = do_read_c7(0, 0x800, 0);

	set_memory_c(0x7f8, 7, 0);
	set_memory_c(0x42, 0x01, 0);
	set_memory_c(0x43, 0x70, 0);
	set_memory_c(0x44, 0x0, 0);
	set_memory_c(0x45, 0x8, 0);
	set_memory_c(0x46, 0x0, 0);
	set_memory_c(0x47, 0x0, 0);
	engine.xreg = 0x70;
	engine.kpc = 0x801;

	if(ret != 0) {
		ki_printf("Failure reading boot disk in s7d1!\n");
		engine.kpc = 0xe000;
	}
}

