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
const char rcsid_iwm_h[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/iwm.h,v 1.3 2005/09/23 12:37:09 fredyd Exp $";
#endif

#ifndef KEGS_IWM_H
#define KEGS_IWM_H

#define MAX_TRACKS	(2*80)
#define MAX_C7_DISKS	32

#define NIB_LEN_525		0x1900		/* 51072 bits per track */
#define NIBS_FROM_ADDR_TO_DATA	20

#define DSK_TYPE_PRODOS		0
#define DSK_TYPE_DOS33		1
#define DSK_TYPE_NIB		2

typedef struct _Disk Disk;

STRUCT(Track) {
	int	track_dirty;
	int	overflow_size;
	int	track_len;
	int	unix_pos;
	int	unix_len;
	Disk	*dsk;
	byte	*nib_area;
	int	pad1;
};

struct _Disk {
	int	fd;
	char	*name_ptr;
	int	image_start;
	int	image_size;
	int	smartport;
	int	disk_525;
	int	drive;
	int	cur_qtr_track;
	int	image_type;
	int	vol_num;
	int	write_prot;
	int	write_through_to_unix;
	int	disk_dirty;
	int	just_ejected;
	double	dcycs_last_read;
	int	last_phase;
	int	nib_pos;
	int	num_tracks;
	Track	tracks[MAX_TRACKS];
};


STRUCT(Iwm) {
	Disk	drive525[2];
	Disk	drive35[2];
	Disk	smartport[MAX_C7_DISKS];
	int	motor_on;
	int	motor_off;
	int	motor_off_vbl_count;
	int	motor_on35;
	int	head35;
	int	step_direction35;
	int	iwm_phase[4];
	int	iwm_mode;
	int	drive_select;
	int	q6;
	int	q7;
	int	enable2;
	int	reset;

	word32	previous_write_val;
	int	previous_write_bits;
};


STRUCT(Driver_desc) {
	word16	sig;
	word16	blk_size;
	word32	blk_count;
	word16	dev_type;
	word16	dev_id;
	word32	data;
	word16	drvr_count;
};

STRUCT(Part_map) {
	word16	sig;
	word16	sigpad;
	word32	map_blk_cnt;
	word32	phys_part_start;
	word32	part_blk_cnt;
	char	part_name[32];
	char	part_type[32];
	word32	data_start;
	word32	data_cnt;
	word32	part_status;
	word32	log_boot_start;
	word32	boot_size;
	word32	boot_load;
	word32	boot_load2;
	word32	boot_entry;
	word32	boot_entry2;
	word32	boot_cksum;
	char	processor[16];
	char	junk[128];
};

extern int head_35;
extern int g_apple35_sel;
extern int cur_drive;
extern int g_fast_disk_emul;
extern int g_iwm_motor_on;
extern int g_slow_525_emul_wr;
extern char g_kegs_conf_name[256];
extern Iwm iwm;
extern int g_highest_smartport_unit;

void iwm_show_track(int slot_drive, int track);
void iwm_show_stats(void);
void iwm_set_apple35_sel(int newval);
int iwm_read_c0ec(double dcycs);
int read_iwm(int loc, double dcycs);
void write_iwm(int loc, int val, double dcycs);
void iwm_init(void);
void iwm_reset(void);
void draw_iwm_status(int line, char *buf);
void iwm_vbl_update(void);
void iwm_shut();

int get_fast_disk_emul(void);
int set_fast_disk_emul(int);

#endif /* KEGS_IWM_H */
