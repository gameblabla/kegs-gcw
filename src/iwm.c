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

const char rcsid_iwm_c[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/iwm.c,v 1.3 2005/09/23 12:37:09 fredyd Exp $";

#include "sim65816.h"
#include "moremem.h"
#include "video.h"
#include "smartport.h"
#include "engine.h"
#include "iwm.h"
#include "dis.h"
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

static void iwm_init_drive(Disk *dsk, int smartport, int drive, int disk_525);
static void iwm_flush_disk_to_unix(Disk *dsk);
static void iwm_touch_switches(int loc, double dcycs);
static void iwm_move_to_track(Disk *dsk, int new_track);
static void iwm525_phase_change(int drive, int phase);
static int iwm_read_status35(double dcycs);
static void iwm_do_action35(double dcycs);
static int iwm_read_enable2(double dcycs);
static int iwm_read_enable2_handshake(double dcycs);
static void iwm_write_enable2(int val, double dcycs);
static int iwm_read_data(Disk *dsk, int fast_disk_emul, double dcycs);
static void iwm_write_data(Disk *dsk, word32 val, int fast_disk_emul, double dcycs);
static int iwm_read_data_35(Disk *dsk, int fast_disk_emul, double dcycs);
static void iwm_write_data_35(Disk *dsk, word32 val, int fast_disk_emul, double dcycs);
static int iwm_read_data_525(Disk *dsk, int fast_disk_emul, double dcycs);
static void iwm_write_data_525(Disk *dsk, word32 val, int fast_disk_emul, double dcycs);
static void sector_to_partial_nib(byte *in, byte *nib_ptr);
static int disk_unnib_4x4(Disk *dsk);
static int iwm_denib_track525(Disk *dsk, Track *trk, int qtr_track, byte *outbuf);
static int iwm_denib_track35(Disk *dsk, Track *trk, int qtr_track, byte *outbuf);
static int disk_track_to_unix(Disk *dsk, int qtr_track, byte *outbuf);
static void show_hex_data(byte *buf, int count);
static void disk_check_nibblization(Disk *dsk, int qtr_track, byte *buf, int size);
static void disk_unix_to_nib(Disk *dsk, int qtr_track, int unix_pos, int unix_len, int nib_len);
static void iwm_nibblize_track_nib525(Disk *dsk, Track *trk, byte *track_buf, int qtr_track);
static void iwm_nibblize_track_525(Disk *dsk, Track *trk, byte *track_buf, int qtr_track);
static void iwm_nibblize_track_35(Disk *dsk, Track *trk, byte *track_buf, int qtr_track);
static void disk_4x4_nib_out(Disk *dsk, word32 val);
static void disk_nib_out(Disk *dsk, byte val, int size);
static void disk_nib_end_track(Disk *dsk);
static void iwm_show_a_track(Track *trk);
static void maybe_parse_disk_conf_file(void);
static void insert_disk(Disk *dsk, const char *name, int virtual_image, int size);
static void eject_named_disk(Disk *dsk, const char *name);
static void eject_if_untouched(Disk *dsk);
static void eject_disk(Disk *dsk);
static void kegs_file_copy(char *orig_name, char *new_name);
static void eject_disk_by_num(int slot, int drive);
/* prototypes for IWM special routs */
static int iwm_read_data_35(Disk *dsk, int fast_disk_emul, double dcycs);
static int iwm_read_data_525(Disk *dsk, int fast_disk_emul, double dcycs);
static void iwm_write_data_35(Disk *dsk, word32 val, int fast_disk_emul, double dcycs);
static void iwm_write_data_525(Disk *dsk, word32 val, int fast_disk_emul,double dcycs);

char g_kegs_conf_name[256];

static const char *g_kegs_conf_names[] = { "kegs_conf", "disk_conf", ".kegs_conf", 0 };

static const byte phys_to_dos_sec[] = {
	0x00, 0x07, 0x0e, 0x06,  0x0d, 0x05, 0x0c, 0x04,
	0x0b, 0x03, 0x0a, 0x02,  0x09, 0x01, 0x08, 0x0f
};

static const byte phys_to_prodos_sec[] = {
	0x00, 0x08, 0x01, 0x09,  0x02, 0x0a, 0x03, 0x0b,
	0x04, 0x0c, 0x05, 0x0d,  0x06, 0x0e, 0x07, 0x0f
};


static const byte to_disk_byte[] = {
	0x96, 0x97, 0x9a, 0x9b,  0x9d, 0x9e, 0x9f, 0xa6,
	0xa7, 0xab, 0xac, 0xad,  0xae, 0xaf, 0xb2, 0xb3,
/* 0x10 */
	0xb4, 0xb5, 0xb6, 0xb7,  0xb9, 0xba, 0xbb, 0xbc,
	0xbd, 0xbe, 0xbf, 0xcb,  0xcd, 0xce, 0xcf, 0xd3,
/* 0x20 */
	0xd6, 0xd7, 0xd9, 0xda,  0xdb, 0xdc, 0xdd, 0xde,
	0xdf, 0xe5, 0xe6, 0xe7,  0xe9, 0xea, 0xeb, 0xec,
/* 0x30 */
	0xed, 0xee, 0xef, 0xf2,  0xf3, 0xf4, 0xf5, 0xf6,
	0xf7, 0xf9, 0xfa, 0xfb,  0xfc, 0xfd, 0xfe, 0xff
};

static int	g_track_bytes_35[] = {
	0x200*12,
	0x200*11,
	0x200*10,
	0x200*9,
	0x200*8
};

static int	g_track_nibs_35[] = {
	816*12,
	816*11,
	816*10,
	816*9,
	816*8
};



int	g_fast_disk_emul = 1;
// OG : add slow parameters for 525
int	g_fast_disk_emul_525 = 0;
int	g_slow_525_emul_wr = 0;
static double	g_dcycs_end_emul_wr = 0.0;
static int	g_fast_disk_unnib = 0;
static int	g_iwm_fake_fast = 0;


static int	from_disk_byte[256];
static int	from_disk_byte_valid = 0;

Iwm	iwm;

int	g_apple35_sel = 0;
int	head_35 = 0;
int	g_iwm_motor_on = 0;

static int	g_check_nibblization = 0;

static time_t	g_disk_conf_mtime = 0;
static int	g_reparse_delay = 0;
int	g_highest_smartport_unit = -1;


void
iwm_init_drive(Disk *dsk, int smartport, int drive, int disk_525)
{
	int	num_tracks;
	int	i;

	num_tracks = MAX_TRACKS;

	dsk->fd = -1;
	dsk->name_ptr = 0;
	dsk->image_start = 0;
	dsk->image_size = 0;
	dsk->smartport = smartport;
	dsk->disk_525 = disk_525;
	dsk->drive = drive;
	dsk->cur_qtr_track = 0;
	dsk->image_type = 0;
	dsk->vol_num = 254;
	dsk->write_prot = 1;
	dsk->write_through_to_unix = 0;
	dsk->disk_dirty = 0;
	dsk->just_ejected = 0;
	dsk->dcycs_last_read = 0.0;
	dsk->last_phase = 0;
	dsk->nib_pos = 0;
	dsk->num_tracks = 0;

	for(i = 0; i < num_tracks; i++) {
		dsk->tracks[i].track_dirty = 0;
		dsk->tracks[i].overflow_size = 0;
		dsk->tracks[i].track_len = 0;
		dsk->tracks[i].dsk = dsk;
		dsk->tracks[i].nib_area = 0;
		dsk->tracks[i].unix_pos = -1;
		dsk->tracks[i].unix_len = -1;
	}
}

void
iwm_init()
{
	int	val;
	int	i;

	for(i = 0; i < 2; i++) {
		iwm_init_drive(&(iwm.drive525[i]), 0, i, 1);
		iwm_init_drive(&(iwm.drive35[i]), 0, i, 0);
	}

	for(i = 0; i < MAX_C7_DISKS; i++) {
		iwm_init_drive(&(iwm.smartport[i]), 1, i, 0);
	}

	/*
	// Reboot
	if(from_disk_byte_valid == 0) {
	*/
		for(i = 0; i < 256; i++) {
			from_disk_byte[i] = -1;
		}
		for(i = 0; i < 64; i++) {
			val = to_disk_byte[i];
			from_disk_byte[val] = i;
		}
		from_disk_byte_valid = 1;
	/*
	} else {
		halt_printf("iwm_init called twice!\n");
	}
	*/

	// Find any kegs_conf and disk_conf files
	#ifdef USE_CONFIG_FILE
	snprintf(g_kegs_conf_name, sizeof(g_kegs_conf_name), "%s", fileconfig);
	#else
	setup_kegs_file(&g_kegs_conf_name[0], sizeof(g_kegs_conf_name), 0,
					&g_kegs_conf_names[0]);
	#endif

	maybe_parse_disk_conf_file();

	iwm_reset();

}

void
iwm_reset()
{
	iwm.q6 = 0;
	iwm.q7 = 0;
	iwm.motor_on = 0;
	iwm.motor_on35 = 0;
	iwm.motor_off = 0;
	iwm.motor_off_vbl_count = 0;
	iwm.step_direction35 = 0;
	iwm.head35 = 0;
	iwm.drive_select = 0;
	iwm.iwm_mode = 0;
	iwm.enable2 = 0;
	iwm.reset = 0;
	iwm.iwm_phase[0] = 0;
	iwm.iwm_phase[1] = 0;
	iwm.iwm_phase[2] = 0;
	iwm.iwm_phase[3] = 0;
	iwm.previous_write_val = 0;
	iwm.previous_write_bits = 0;

	g_iwm_motor_on = 0;
	g_apple35_sel = 0;
}

void
draw_iwm_status(int line, char *buf)
{
	char	*flag[2][2];

	flag[0][0] = " ";
	flag[0][1] = " ";
	flag[1][0] = " ";
	flag[1][1] = " ";

	if(g_iwm_motor_on) {
		flag[g_apple35_sel][iwm.drive_select] = "*";
	}

	sprintf(buf, "s6d1:%2d%s   s6d2:%2d%s   s5d1:%2d/%d%s   "
		"s5d2:%2d/%d%s fast_disk_emul:%d,%d c036:%02x",
		iwm.drive525[0].cur_qtr_track >> 2, flag[0][0],
		iwm.drive525[1].cur_qtr_track >> 2, flag[0][1],
		iwm.drive35[0].cur_qtr_track >> 1,
		iwm.drive35[0].cur_qtr_track & 1, flag[1][0],
		iwm.drive35[1].cur_qtr_track >> 1,
		iwm.drive35[1].cur_qtr_track & 1, flag[1][1],
		g_fast_disk_emul, g_slow_525_emul_wr,
		(speed_fast << 7) + g_slot_motor_detect);

	update_status_line(line, buf);
}

void
iwm_flush_disk_to_unix(Disk *dsk)
{
	byte	buffer[0x4000];
	int	num_dirty;
	int	j;
	int	ret;
	int	unix_pos;
	int	unix_len;

	if(dsk->disk_dirty == 0 || dsk->write_through_to_unix == 0) {
		return;
	}

	ki_printf("Writing disk %s to Unix\n", dsk->name_ptr);
	dsk->disk_dirty = 0;
	num_dirty = 0;

	/* Dirty data! */
	for(j = 0; j < dsk->num_tracks; j++) {

		ret = disk_track_to_unix(dsk, j, &(buffer[0]));

		if(ret != 1 && ret != 0) {
			ki_printf("iwm_flush_disk_to_unix ret: %d, cannot write "
				"image to unix\n", ret);
			halt_printf("Adjusting image not to write through!\n");
			dsk->write_through_to_unix = 0;
			break;
		}

		if(ret != 1) {
			/* not at an even track, or not dirty */
			continue;
		}
		if((j & 3) != 0 && dsk->disk_525) {
			halt_printf("Valid data on a non-whole trk: %03x\n", j);
			continue;
		}

		num_dirty++;

		/* Write it out */
		unix_pos = dsk->tracks[j].unix_pos;
		unix_len = dsk->tracks[j].unix_len;
		if(unix_pos < 0 || unix_len < 0x1000) {
			halt_printf("Disk:%s trk:%d, unix_pos:%08x, len:%08x\n",
				dsk->name_ptr, j, unix_pos, unix_len);
			break;
		}

		ret = lseek(dsk->fd, unix_pos, SEEK_SET);
		if(ret != unix_pos) {
			halt_printf("lseek 525: %08x, errno: %d\n", ret, errno);
		}

		ret = write(dsk->fd, &(buffer[0]), unix_len);
		if(ret != unix_len) {
			ki_printf("write: %08x, errno:%d, qtrk: %02x, disk: %s\n",
				ret, errno, j, dsk->name_ptr);
		}
	}

	if(num_dirty == 0) {
		halt_printf("Drive %s was dirty, but no track was dirty!\n",
			dsk->name_ptr);
	}

}

/* Check for dirty disk and disk_conf changes 3 times a second */
#define IWM_VBL_COUNT	20
int g_iwm_vbl_count = 0;

void
iwm_vbl_update()
{
#ifndef DISABLE_CONFFILE
	struct stat stat_buf;
	time_t	mtime;
	int	ret;
#endif
	Disk	*dsk;
	int	motor_on;
	
	int	i;

	if(iwm.motor_on && iwm.motor_off) {
		if(iwm.motor_off_vbl_count <= g_vbl_count) {
			ki_printf("Disk timer expired, drive off: %08x\n",
				g_vbl_count);
			iwm.motor_on = 0;
			iwm.motor_off = 0;
		}
	}

	if(g_iwm_vbl_count > 0) {
		g_iwm_vbl_count--;
		return;
	}

	motor_on = iwm.motor_on;
	if(g_apple35_sel) {
		motor_on = iwm.motor_on35;
	}

	if(motor_on == 0 || iwm.motor_off) {
		/* Disk not spinning, see if any dirty tracks to flush */
		/*  out to Unix */
		for(i = 0; i < 2; i++) {
			dsk = &(iwm.drive525[i]);
			iwm_flush_disk_to_unix(dsk);
		}
		for(i = 0; i < 2; i++) {
			dsk = &(iwm.drive35[i]);
			iwm_flush_disk_to_unix(dsk);
		}

		g_iwm_vbl_count = IWM_VBL_COUNT;

		/* Also see if disk_conf has changed */

#ifndef DISABLE_CONFFILE
		ret = stat(g_kegs_conf_name, &stat_buf);
		if(ret != 0) {
			halt_printf("IWM: stat of disk_conf ret:%d, errno:%d\n",
				ret, errno);
		} else {
			mtime = stat_buf.st_mtime;
			if(mtime > g_disk_conf_mtime) {
				iwm_printf("%s has changed\n",g_kegs_conf_name);
				if(g_disk_conf_mtime != 0) {
					g_reparse_delay = 1;	/* 1 * 1/3 sec*/
				}
				g_disk_conf_mtime = mtime;
			}
		}

		maybe_parse_disk_conf_file();
#endif
	}
}


void
iwm_show_stats()
{
	ki_printf("IWM stats: q7,q6: %d, %d, reset,enable2: %d,%d, mode: %02x\n",
		iwm.q7, iwm.q6, iwm.reset, iwm.enable2, iwm.iwm_mode);
	ki_printf("motor: %d,%d, motor35:%d drive: %d, on: %d, head35: %d "
		"phs: %d %d %d %d\n",
		iwm.motor_on, iwm.motor_off, g_iwm_motor_on,
		iwm.drive_select, g_apple35_sel,
		head_35, iwm.iwm_phase[0], iwm.iwm_phase[1], iwm.iwm_phase[2],
		iwm.iwm_phase[3]);
	ki_printf("iwm.drive525[0].fd: %d, [1].fd: %d\n",
		iwm.drive525[0].fd, iwm.drive525[1].fd);
	ki_printf("iwm.drive525[0].last_phase: %d, [1].last_phase: %d\n",
		iwm.drive525[0].last_phase, iwm.drive525[1].last_phase);
}

void
iwm_touch_switches(int loc, double dcycs)
{
	Disk	*dsk;
	int	phase;
	int	on;
	int	drive;

	if(iwm.reset) {
		iwm_printf("IWM under reset: %d, enable2: %d\n", iwm.reset,
			iwm.enable2);
	}

	on = loc & 1;
	drive = iwm.drive_select;
	phase = loc >> 1;
	if(g_apple35_sel) {
		dsk = &(iwm.drive35[drive]);
	} else {
		dsk = &(iwm.drive525[drive]);
	}


	if(loc < 8) {
		/* phase adjustments.  See if motor is on */

		iwm.iwm_phase[phase] = on;

		if(iwm.motor_on) {
			if(g_apple35_sel) {
				if(phase == 3 && on) {
					iwm_do_action35(dcycs);
				}
			} else if(on) {
				/* Move apple525 head */
				iwm525_phase_change(drive, phase);
			}
		}
		/* See if enable or reset is asserted */
		if(iwm.iwm_phase[0] && iwm.iwm_phase[2]) {
			iwm.reset = 1;
			iwm_printf("IWM reset active\n");
		} else {
			iwm.reset = 0;
		}
		if(iwm.iwm_phase[1] && iwm.iwm_phase[3]) {
			iwm.enable2 = 1;
			iwm_printf("IWM ENABLE2 active\n");
		} else {
			iwm.enable2 = 0;
		}
	} else {
		/* loc >= 8 */
		switch(loc) {
		case 0x8:
			iwm_printf("Turning IWM motor off!\n");
			if(iwm.iwm_mode & 0x04) {
				/* Turn off immediately */
				iwm.motor_off = 0;
				iwm.motor_on = 0;
			} else {
				/* 1 second delay */
				if(iwm.motor_on && !iwm.motor_off) {
					iwm.motor_off = 1;
					iwm.motor_off_vbl_count = g_vbl_count
									+ 60;
				}
			}

			if(g_iwm_motor_on || g_slow_525_emul_wr) {
				/* recalc current speed */
				set_halt(HALT_EVENT);
			}

			g_iwm_motor_on = 0;
			g_slow_525_emul_wr = 0;
			break;
		case 0x9:
			iwm_printf("Turning IWM motor on!\n");
			iwm.motor_on = 1;
			iwm.motor_off = 0;

			if(g_iwm_motor_on == 0) {
				/* recalc current speed */
				set_halt(HALT_EVENT);
			}
			g_iwm_motor_on = 1;

			break;
		case 0xa:
		case 0xb:
			iwm.drive_select = on;
			break;
		case 0xc:
		case 0xd:
			iwm.q6 = on;
			break;
		case 0xe:
		case 0xf:
			iwm.q7 = on;
			break;
		default:
			ki_printf("iwm_touch_switches: loc: %02x unknown!\n", loc);
			my_exit(2);
		}
	}

	if(!iwm.q7) {
		iwm.previous_write_bits = 0;
	}

	if((dcycs > g_dcycs_end_emul_wr) && g_slow_525_emul_wr) {
		set_halt(HALT_EVENT);
		g_slow_525_emul_wr = 0;
	}
}

void
iwm_move_to_track(Disk *dsk, int new_track)
{
	int	disk_525;
	int	dr;

	disk_525 = dsk->disk_525;

	if(new_track < 0) {
		new_track = 0;
	}
	if(new_track >= dsk->num_tracks) {
		if(disk_525) {
			new_track = dsk->num_tracks - 4;
		} else {
			new_track = dsk->num_tracks - 2 + iwm.head35;
		}

		if(new_track <= 0) {
			new_track = 0;
		}
	}

	if(dsk->cur_qtr_track != new_track) {
		dr = dsk->drive + 1;
		if(disk_525) {
			iwm_printf("s6d%d Track: %d.%02d\n", dr,
				new_track >> 2, 25* (new_track & 3));
		} else {
			iwm_printf("s5d%d Track: %d Side: %d\n", dr,
				new_track >> 1, new_track & 1);
		}

		dsk->cur_qtr_track = new_track;
	}
}

void
iwm525_phase_change(int drive, int phase)
{
	Disk	*dsk;
	int	qtr_track;
	int	last_phase;
	int	phase_up;
	int	phase_down;
	int	delta;

	phase_up = (phase - 1) & 3;
	phase_down = (phase + 1) & 3;

	dsk = &(iwm.drive525[drive]);
	last_phase = dsk->last_phase;

	qtr_track = dsk->cur_qtr_track;

	delta = 0;
	if(last_phase == phase_up) {
		delta = 2;
		last_phase = phase;
	} else if(last_phase == phase_down) {
		delta = -2;
		last_phase = phase;
	}

	qtr_track += delta;
	if(qtr_track < 0) {
#if 0
		ki_printf("GRIND...GRIND...GRIND\n");
#endif
		qtr_track = 0;
		last_phase = 0;
	}
	if(qtr_track > 4*34) {
		ki_printf("Disk arm moved past track 0x21, moving it back\n");
		qtr_track = 4*34;
		last_phase = 0;
	}

	iwm_move_to_track(dsk, qtr_track);

	dsk->last_phase = last_phase;

	iwm_printf("Moving drive to qtr track: %04x, %d, %d, %d,   "
		"%d %d %d %d\n",
		qtr_track, phase, delta, last_phase, iwm.iwm_phase[0],
		iwm.iwm_phase[1], iwm.iwm_phase[2], iwm.iwm_phase[3]);

	/* sanity check stepping algorithm */
	if((qtr_track & 7) == 0) {
		/* check for just access phase 0 */
		if(last_phase != 0 ) {
			halt_printf("last_phase: %d!\n", last_phase);
		}
	}
}

int
iwm_read_status35(double dcycs)
{
	Disk	*dsk;
	int	drive;
	int	state;
	int	tmp;

	drive = iwm.drive_select;
	dsk = &(iwm.drive35[drive]);

	if(iwm.motor_on) {
		/* Read status */
		state = (iwm.iwm_phase[1] << 3) + (iwm.iwm_phase[0] << 2) +
			(head_35 << 1) + iwm.iwm_phase[2];

		iwm_printf("Iwm status read state: %02x\n", state);

		switch(state) {
		case 0x00:	/* step direction */
			return iwm.step_direction35;
			break;
		case 0x01:	/* lower head activate */
			/* also return instantaneous data from head */
			iwm.head35 = 0;
			iwm_move_to_track(dsk, (dsk->cur_qtr_track & (-2)));
			return (((int)dcycs) & 1);
			break;
		case 0x02:	/* disk in place */
			/* 1 = no disk, 0 = disk */
			iwm_printf("read disk in place, num_tracks: %d\n",
				dsk->num_tracks);
			tmp = (dsk->num_tracks <= 0);
			return tmp;
			break;
		case 0x03:	/* upper head activate */
			/* also return instantaneous data from head */
			iwm.head35 = 1;
			iwm_move_to_track(dsk, (dsk->cur_qtr_track | 1));
			return (((int)dcycs) & 1);
			break;
		case 0x04:	/* disk is stepping? */
			/* 1 = not stepping, 0 = stepping */
			return 1;
			break;
		case 0x05:	/* Unknown function of ROM 03? */
			/* 1 = or $20 into 0xe1/f24+drive, 0 = don't */
			return 1;
			break;
		case 0x06:	/* disk is locked */
			/* 0 = locked, 1 = unlocked */
			return (!dsk->write_prot);
			break;
		case 0x08:	/* motor on */
			/* 0 = on, 1 = off */
			return !iwm.motor_on35;
			break;
		case 0x09:	/* number of sides */
			/* 1 = 2 sides, 0 = 1 side */
			return 1;
			break;
		case 0x0a:	/* at track 0 */
			/* 1 = not at track 0, 0 = there */
			tmp = (dsk->cur_qtr_track != 0);
			iwm_printf("Read at track0_35: %d\n", tmp);
			return tmp;
			break;
		case 0x0b:	/* disk ready??? */
			/* 0 = ready, 1 = not ready? */
			tmp = !iwm.motor_on35;
			iwm_printf("Read disk ready, ret: %d\n", tmp);
			return tmp;
			break;
		case 0x0c:	/* disk switched?? */
			/* 0 = not switched, 1 = switched? */
			tmp = (dsk->just_ejected != 0);
			iwm_printf("Read disk switched: %d\n", tmp);
			return tmp;
			break;
		case 0x0d:	/* false read when ejecting disk */
			return 1;
		case 0x0e:	/* tachometer */
			halt_printf("Reading tachometer!\n");
			return (((int)dcycs) & 1);
			break;
		case 0x0f:	/* drive installed? */
			/* 0 = drive exists, 1 = no drive */
			if(drive) {
				/* pretend no drive 1 */
				return 1;
			}
			return 0;
			break;
		default:
			halt_printf("Read 3.5 status, state: %02x\n", state);
			return 1;
		}
	} else {
		iwm_printf("Read 3.5 status with drive off!\n");
		return 1;
	}
}

void
iwm_do_action35(double dcycs)
{
	Disk	*dsk;
	int	drive;
	int	state;

	drive = iwm.drive_select;
	dsk = &(iwm.drive35[drive]);

	if(iwm.motor_on) {
		/* Perform action */
		state = (iwm.iwm_phase[1] << 3) + (iwm.iwm_phase[0] << 2) +
			(head_35 << 1) + iwm.iwm_phase[2];
		switch(state) {
		case 0x00:	/* Set step direction inward */
			/* towards higher tracks */
			iwm.step_direction35 = 0;
			iwm_printf("Iwm set step dir35 = 0\n");
			break;
		case 0x01:	/* Set step direction outward */
			/* towards lower tracks */
			iwm.step_direction35 = 1;
			iwm_printf("Iwm set step dir35 = 1\n");
			break;
		case 0x03:	/* reset disk-switched flag? */
			iwm_printf("Iwm reset disk switch\n");
			dsk->just_ejected = 0;
			/* set_halt(HALT_WANTTOQUIT); */
			break;
		case 0x04:	/* step disk */
			if(iwm.step_direction35) {
				iwm_move_to_track(dsk, dsk->cur_qtr_track - 2);
			} else {
				iwm_move_to_track(dsk, dsk->cur_qtr_track + 2);
			}
			break;
		case 0x08:	/* turn motor on */
			iwm_printf("Iwm set motor_on35 = 1\n");
			iwm.motor_on35 = 1;
			break;
		case 0x09:	/* turn motor off */
			iwm_printf("Iwm set motor_on35 = 0\n");
			iwm.motor_on35 = 0;
			break;
		case 0x0d:	/* eject disk */
			ki_printf("Ejecting disk!\n");
			dsk->just_ejected = 4;
			eject_disk_by_num(5, drive+1);
			break;
		case 0x02:
		case 0x07:
		case 0x0b: /* hacks to allow AE 1.6MB driver to not crash me */
			break;
		default:
			halt_printf("Do 3.5 action, state: %02x\n", state);
			return;
		}
	} else {
		halt_printf("Set 3.5 status with drive off!\n");
		return;
	}
}

void
iwm_set_apple35_sel(int newval)
{
	if(g_apple35_sel != newval) {
		/* Handle speed changes */
		set_halt(HALT_EVENT);
	}

	g_apple35_sel = newval;
}

int
iwm_read_c0ec(double dcycs)
{
	Disk	*dsk;
	int	drive;

	iwm.q6 = 0;

	if(iwm.q7 == 0 && iwm.enable2 == 0 && iwm.motor_on) {
		drive = iwm.drive_select;
		if(g_apple35_sel) {
			dsk = &(iwm.drive35[drive]);
			return iwm_read_data_35(dsk, g_fast_disk_emul, dcycs);
		} else {
			dsk = &(iwm.drive525[drive]);
			return iwm_read_data_525(dsk, g_fast_disk_emul_525, dcycs);
		}

	}

	return read_iwm(0xc, dcycs);
}


int
read_iwm(int loc, double dcycs)
{
	Disk	*dsk;
	word32	status;
	double	diff_dcycs;
	int	cmp;
	int	on;
	int	state;
	int	drive;
	int	val;

#ifdef DISABLE_CONFFILE
	maybe_parse_disk_conf_file();
#endif
	loc = loc & 0xf;
	on = loc & 1;

	if(loc == 0xc) {
		iwm.q6 = 0;
	} else {
		iwm_touch_switches(loc, dcycs);
	}

	state = (iwm.q7 << 1) + iwm.q6;
	drive = iwm.drive_select;
	if(g_apple35_sel) {
		dsk = &(iwm.drive35[drive]);
	} else {
		dsk = &(iwm.drive525[drive]);
	}

	if(on) {
		/* odd address, return 0 */
		return 0;
	} else {
		/* even address */
		switch(state) {
		case 0x00:	/* q7 = 0, q6 = 0 */
			if(iwm.enable2) {
				return iwm_read_enable2(dcycs);
			} else {
				if(iwm.motor_on) {
					return iwm_read_data(dsk,
						g_fast_disk_emul, dcycs);
				} else {
					iwm_printf("read iwm st 0, m off!\n");
/* HACK!!!! */
					return 0xff;
					return (((int)dcycs) & 0x7f) + 0x80;
				}
			}
			break;
		case 0x01:	/* q7 = 0, q6 = 1 */
			/* read IWM status reg */
			if(iwm.enable2) {
				iwm_printf("Read status under enable2: 1\n");
				status = 1;
			} else {
				if(g_apple35_sel) {
					status = iwm_read_status35(dcycs);
				} else {
					status = dsk->write_prot;
				}
			}

			val = (status << 7) + (iwm.motor_on << 5) +
				iwm.iwm_mode;
			iwm_printf("Read status: %02x\n", val);

			return val;
			break;
		case 0x02:	/* q7 = 1, q6 = 0 */
			/* read handshake register */
			if(iwm.enable2) {
				return iwm_read_enable2_handshake(dcycs);
			} else {
				status = 0xc0;
				diff_dcycs = dcycs - dsk->dcycs_last_read;
				cmp = 16;
				if(dsk->disk_525 == 0) {
					cmp = 32;
				}
				if(diff_dcycs > cmp) {
					iwm_printf("Write underrun!\n");
					iwm_printf("cur: %f, dc_last: %f\n",
						dcycs, dsk->dcycs_last_read);
					status = status & 0xbf;
				}
				return status;
			}
			break;
		case 0x03:	/* q7 = 1, q6 = 1 */
			halt_printf("read iwm state 3!\n");
			return 0;
		break;
		}
		
	}
	halt_printf("Got to end of read_iwm, loc: %02x!\n", loc);

	return 0;
}

void
write_iwm(int loc, int val, double dcycs)
{
	Disk	*dsk;
	int	on;
	int	state;
	int	drive;
	int	fast_writes;

	loc = loc & 0xf;
	on = loc & 1;

	iwm_touch_switches(loc, dcycs);

	state = (iwm.q7 << 1) + iwm.q6;
	drive = iwm.drive_select;
	fast_writes = g_fast_disk_emul;
	if(g_apple35_sel) {
		dsk = &(iwm.drive35[drive]);
	} else {
		dsk = &(iwm.drive525[drive]);
		fast_writes = !g_slow_525_emul_wr && fast_writes;
	}

	if(on) {
		/* odd address, write something */
		if(state == 0x03) {
			/* q7, q6 = 1,1 */
			if(iwm.motor_on) {
				if(iwm.enable2) {
					iwm_write_enable2(val, dcycs);
				} else {
					iwm_write_data(dsk, val,
						fast_writes, dcycs);
				}
			} else {
				/* write mode register */
				val = val & 0x1f;
				iwm.iwm_mode = val;
				if(val != 0 && val != 0x0f && val != 0x07 &&
						val != 0x04 && val != 0x0b) {
					halt_printf("set iwm_mode:%02x!\n",val);
				}
			}
		} else {
			if(iwm.enable2) {
				iwm_write_enable2(val, dcycs);
			} else {
				ki_printf("Write iwm1, st: %02x, loc: %x: %02x\n",
					state, loc, val);
			}
		}
		return;
	} else {
		/* even address */
		if(iwm.enable2) {
			iwm_write_enable2(val, dcycs);
		} else {
			iwm_printf("Write iwm2, st: %02x, loc: %x: %02x\n",
				state, loc, val);
		}
		return;
	}

	halt_printf("Got to end of write_iwm, loc:%02x, val: %02x\n", loc, val);

	return;

}



int
iwm_read_enable2(double dcycs)
{
	iwm_printf("Read under enable2!\n");
	return 0xff;
}

int g_cnt_enable2_handshake = 0;

int
iwm_read_enable2_handshake(double dcycs)
{
	int	val;

	iwm_printf("Read handshake under enable2!\n");

	val = 0xc0;
	g_cnt_enable2_handshake++;
	if(g_cnt_enable2_handshake > 3) {
		g_cnt_enable2_handshake = 0;
		val = 0x80;
	}

	return val;
}

void
iwm_write_enable2(int val, double dcycs)
{
	iwm_printf("Write under enable2: %02x!\n", val);

	return;
}

int
iwm_read_data(Disk *dsk, int fast_disk_emul, double dcycs)
{
	if(dsk->disk_525) {
		return iwm_read_data_525(dsk, fast_disk_emul, dcycs);
	} else {
		return iwm_read_data_35(dsk, fast_disk_emul, dcycs);
	}
}

void
iwm_write_data(Disk *dsk, word32 val, int fast_disk_emul, double dcycs)
{
	if(dsk->disk_525) {
		iwm_write_data_525(dsk, val, fast_disk_emul, dcycs);
	} else {
		iwm_write_data_35(dsk, val, fast_disk_emul, dcycs);
	}
}

#undef IWM_READ_ROUT
#undef IWM_WRITE_ROUT
#undef IWM_CYC_MULT
#undef IWM_DISK_525

#define IWM_READ_ROUT		iwm_read_data_35
#define IWM_WRITE_ROUT		iwm_write_data_35
#define IWM_CYC_MULT		1
#define IWM_DISK_525		0

#define INCLUDE_IWM_RCSID_C
#include "iwm_35_525.h"
#undef INCLUDE_IWM_RCSID_C

#undef IWM_READ_ROUT
#undef IWM_WRITE_ROUT
#undef IWM_CYC_MULT
#undef IWM_DISK_525

#define IWM_READ_ROUT		iwm_read_data_525
#define IWM_WRITE_ROUT		iwm_write_data_525
#define IWM_CYC_MULT		2
#define IWM_DISK_525		1
#include "iwm_35_525.h"

#undef IWM_READ_ROUT
#undef IWM_WRITE_ROUT
#undef IWM_CYC_MULT
#undef IWM_DISK_525





/* c600 */
void
sector_to_partial_nib(byte *in, byte *nib_ptr)
{
	byte	*aux_buf;
	byte	*nib_out;
	int	val;
	int	val2;
	int	x;
	int	i;

	/* Convert 256(+1) data bytes to 342+1 disk nibbles */

	aux_buf = nib_ptr;
	nib_out = nib_ptr + 0x56;

	for(i = 0; i < 0x56; i++) {
		aux_buf[i] = 0;
	}

	x = 0x55;
	for(i = 0x101; i >= 0; i--) {
		val = in[i];
		if(i >= 0x100) {
			val = 0;
		}
		val2 = (aux_buf[x] << 1) + (val & 1);
		val = val >> 1;
		val2 = (val2 << 1) + (val & 1);
		val = val >> 1;
		nib_out[i] = val;
		aux_buf[x] = val2;
		x--;
		if(x < 0) {
			x = 0x55;
		}
	}
}


int
disk_unnib_4x4(Disk *dsk)
{
	int	val1;
	int	val2;

	val1 = iwm_read_data(dsk, 1, 0);
	val2 = iwm_read_data(dsk, 1, 0);

	return ((val1 << 1) + 1) & val2;
}

int
iwm_denib_track525(Disk *dsk, Track *trk, int qtr_track, byte *outbuf)
{
	byte	aux_buf[0x80];
	byte	*buf;
	int	sector_done[16];
	int	num_sectors_done;
	int	track_len;
	int	vol, track, phys_sec, log_sec, cksum;
	int	val;
	int	val2;
	int	prev_val;
	int	x;
	int	my_nib_cnt;
	int	save_qtr_track;
	int	save_nib_pos;
	int	tmp_nib_pos;
	int	status;
	int	i;

	save_qtr_track = dsk->cur_qtr_track;
	save_nib_pos = dsk->nib_pos;

	iwm_move_to_track(dsk, qtr_track);

	dsk->nib_pos = 0;
	g_fast_disk_unnib = 1;

	track_len = trk->track_len;

	for(i = 0; i < 16; i++) {
		sector_done[i] = 0;
	}

	num_sectors_done = 0;

	val = 0;
	status = -1;
	my_nib_cnt = 0;
	while(my_nib_cnt++ < 2*track_len) {
		/* look for start of a sector */
		if(val != 0xd5) {
			val = iwm_read_data(dsk, 1, 0);
			continue;
		}

		val = iwm_read_data(dsk, 1, 0);
		if(val != 0xaa) {
			continue;
		}

		val = iwm_read_data(dsk, 1, 0);
		if(val != 0x96) {
			continue;
		}

		/* It's a sector start */
		vol = disk_unnib_4x4(dsk);
		track = disk_unnib_4x4(dsk);
		phys_sec = disk_unnib_4x4(dsk);
		if(phys_sec < 0 || phys_sec > 15) {
			ki_printf("Track %02x, read sec as %02x\n", qtr_track>>2,
				phys_sec);
			break;
		}
		if(dsk->image_type == DSK_TYPE_PRODOS) {
			log_sec = phys_to_prodos_sec[phys_sec];
		} else {
			log_sec = phys_to_dos_sec[phys_sec];
		}
		cksum = disk_unnib_4x4(dsk);
		if((vol ^ track ^ phys_sec ^ cksum) != 0) {
			/* not correct format */
			ki_printf("Track %02x not DOS 3.3 since hdr cksum, %02x "
				"%02x %02x %02x\n",
				qtr_track>>2, vol, track, phys_sec, cksum);
			break;
		}

		/* see what sector it is */
		if(track != (qtr_track>>2) || (phys_sec < 0)||(phys_sec > 15)) {
			ki_printf("Track %02x bad since track: %02x, sec: %02x\n",
				qtr_track>>2, track, phys_sec);
			break;
		}

		if(sector_done[phys_sec]) {
			ki_printf("Already done sector %02x on track %02x!\n",
				phys_sec, qtr_track>>2);
			break;
		}

		/* So far so good, let's do it! */
		val = 0;
		i = 0;
		while(i < NIBS_FROM_ADDR_TO_DATA) {
			i++;
			if(val != 0xd5) {
				val = iwm_read_data(dsk, 1, 0);
				continue;
			}

			val = iwm_read_data(dsk, 1, 0);
			if(val != 0xaa) {
				continue;
			}

			val = iwm_read_data(dsk, 1, 0);
			if(val != 0xad) {
				continue;
			}

			/* got it, just break */
			break;
		}

		if(i >= NIBS_FROM_ADDR_TO_DATA) {
			ki_printf("No data header, track %02x, sec %02x\n",
				qtr_track>>2, phys_sec);
			ki_printf("nib_pos: %08x\n", dsk->nib_pos);
			break;
		}

		buf = outbuf + 0x100*log_sec;

		/* Data start! */
		prev_val = 0;
		for(i = 0x55; i >= 0; i--) {
			val = iwm_read_data(dsk, 1, 0);
			val2 = from_disk_byte[val];
			if(val2 < 0) {
				ki_printf("Bad data area1, val:%02x,val2:%02x\n",
								val, val2);
				ki_printf(" i:%03x,n_pos:%04x\n", i, dsk->nib_pos);
				break;
			}
			prev_val = val2 ^ prev_val;
			aux_buf[i] = prev_val;
		}

		/* rest of data area */
		for(i = 0; i < 0x100; i++) {
			val = iwm_read_data(dsk, 1, 0);
			val2 = from_disk_byte[val];
			if(val2 < 0) {
				ki_printf("Bad data area2, read: %02x\n", val);
				ki_printf("  nib_pos: %04x\n", dsk->nib_pos);
				break;
			}
			prev_val = val2 ^ prev_val;
			buf[i] = prev_val;
		}

		/* checksum */
		val = iwm_read_data(dsk, 1, 0);
		val2 = from_disk_byte[val];
		if(val2 < 0) {
			ki_printf("Bad data area3, read: %02x\n", val);
			ki_printf("  nib_pos: %04x\n", dsk->nib_pos);
			break;
		}
		if(val2 != prev_val) {
			ki_printf("Bad data cksum, got %02x, wanted: %02x\n",
				val2, prev_val);
			ki_printf("  nib_pos: %04x\n", dsk->nib_pos);
			break;
		}

		/* Got this far, data is good, merge aux_buf into buf */
		x = 0x55;
		for(i = 0; i < 0x100; i++) {
			val = aux_buf[x];
			val2 = (buf[i] << 1) + (val & 1);
			val = val >> 1;
			val2 = (val2 << 1) + (val & 1);
			buf[i] = val2;
			val = val >> 1;
			aux_buf[x] = val;
			x--;
			if(x < 0) {
				x = 0x55;
			}
		}
		sector_done[phys_sec] = 1;
		num_sectors_done++;
		if(num_sectors_done >= 16) {
			status = 0;
			break;
		}
	}

	tmp_nib_pos = dsk->nib_pos;
	iwm_move_to_track(dsk, save_qtr_track);
	dsk->nib_pos = save_nib_pos;
	g_fast_disk_unnib = 0;

	if(status == 0) {
		return 1;
	}

	ki_printf("Nibblization not done, %02x sectors found on track %02x\n",
		num_sectors_done, qtr_track>>2);
	ki_printf("my_nib_cnt: %04x, nib_pos: %04x, trk_len: %04x\n", my_nib_cnt,
		tmp_nib_pos, track_len);
	for(i = 0; i < 16; i++) {
		ki_printf("sector_done[%d] = %d\n", i, sector_done[i]);
	}
	return -1;
}

int
iwm_denib_track35(Disk *dsk, Track *trk, int qtr_track, byte *outbuf)
{
	word32	buf_c00[0x100];
	word32	buf_d00[0x100];
	word32	buf_e00[0x100];
	byte	*buf;
	word32	tmp_5c, tmp_5d, tmp_5e;
	word32	tmp_66, tmp_67;
	int	sector_done[16];
	int	num_sectors_done;
	int	track_len;
	int	phys_track, phys_sec, phys_side, phys_capacity, cksum;
	int	tmp;
	int	track, side;
	int	num_sectors;
	int	val;
	int	val2;
	int	x, y;
	int	carry;
	int	my_nib_cnt;
	int	save_qtr_track;
	int	save_nib_pos;
	int	status;
	int	i;

	save_qtr_track = dsk->cur_qtr_track;
	save_nib_pos = dsk->nib_pos;

	iwm_move_to_track(dsk, qtr_track);

	dsk->nib_pos = 0;
	g_fast_disk_unnib = 1;

	track_len = trk->track_len;

	num_sectors = g_track_bytes_35[qtr_track >> 5] >> 9;

	for(i = 0; i < num_sectors; i++) {
		sector_done[i] = 0;
	}

	num_sectors_done = 0;

	val = 0;
	status = -1;
	my_nib_cnt = 0;

	track = qtr_track >> 1;
	side = qtr_track & 1;

	while(my_nib_cnt++ < 2*track_len) {
		/* look for start of a sector */
		if(val != 0xd5) {
			val = iwm_read_data(dsk, 1, 0);
			continue;
		}

		val = iwm_read_data(dsk, 1, 0);
		if(val != 0xaa) {
			continue;
		}

		val = iwm_read_data(dsk, 1, 0);
		if(val != 0x96) {
			continue;
		}

		/* It's a sector start */
		val = iwm_read_data(dsk, 1, 0);
		phys_track = from_disk_byte[val];
		if(phys_track != (track & 0x3f)) {
			ki_printf("Track %02x.%d, read track %02x, %02x\n",
				track, side, phys_track, val);
			break;
		}

		phys_sec = from_disk_byte[iwm_read_data(dsk, 1, 0)];
		if(phys_sec < 0 || phys_sec >= num_sectors) {
			ki_printf("Track %02x.%d, read sector %02x??\n",
				track, side, phys_sec);
			break;
		}
		phys_side = from_disk_byte[iwm_read_data(dsk, 1, 0)];

		if(phys_side != ((side << 5) + (track >> 6))) {
			ki_printf("Track %02x.%d, read side %02x??\n",
				track, side, phys_side);
			break;
		}
		phys_capacity = from_disk_byte[iwm_read_data(dsk, 1, 0)];
		if(phys_capacity != 0x24 && phys_capacity != 0x22) {
			ki_printf("Track %02x.%x capacity: %02x != 0x24/22\n",
				track, side, phys_capacity);
		}
		cksum = from_disk_byte[iwm_read_data(dsk, 1, 0)];

		tmp = phys_track ^ phys_sec ^ phys_side ^ phys_capacity;
		if(cksum != tmp) {
			ki_printf("Track %02x.%d, sector %02x, cksum: %02x.%02x\n",
				track, side, phys_sec, cksum, tmp);
			break;
		}


		if(sector_done[phys_sec]) {
			ki_printf("Already done sector %02x on track %02x.%x!\n",
				phys_sec, track, side);
			break;
		}

		/* So far so good, let's do it! */
		val = 0;
		for(i = 0; i < 38; i++) {
			val = iwm_read_data(dsk, 1, 0);
			if(val == 0xd5) {
				break;
			}
		}
		if(val != 0xd5) {
			ki_printf("No data header, track %02x.%x, sec %02x\n",
				track, side, phys_sec);
			break;
		}

		val = iwm_read_data(dsk, 1, 0);
		if(val != 0xaa) {
			ki_printf("Bad data hdr1,val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			ki_printf("nib_pos: %08x\n", dsk->nib_pos);
			break;
		}

		val = iwm_read_data(dsk, 1, 0);
		if(val != 0xad) {
			ki_printf("Bad data hdr2,val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			break;
		}

		buf = outbuf + (phys_sec << 9);

		/* check sector again */
		val = from_disk_byte[iwm_read_data(dsk, 1, 0)];
		if(val != phys_sec) {
			ki_printf("Bad data hdr3,val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			break;
		}

		/* Data start! */
		tmp_5c = 0;
		tmp_5d = 0;
		tmp_5e = 0;
		y = 0xaf;
		carry = 0;

		while(y > 0) {
/* 626f */
			val = iwm_read_data(dsk, 1, 0);
			val2 = from_disk_byte[val];
			if(val2 < 0) {
				ki_printf("Bad data area1b, read: %02x\n", val);
				ki_printf(" i:%03x,n_pos:%04x\n", i, dsk->nib_pos);
				break;
			}
			tmp_66 = val2;

			tmp_5c = tmp_5c << 1;
			carry = (tmp_5c >> 8);
			tmp_5c = (tmp_5c + carry) & 0xff;

			val = iwm_read_data(dsk, 1, 0);
			val2 = from_disk_byte[val];
			if(val2 < 0) {
				ki_printf("Bad data area2, read: %02x\n", val);
				break;
			}

			val2 = val2 + ((tmp_66 << 2) & 0xc0);

			val2 = val2 ^ tmp_5c;
			buf_c00[y] = val2;

			tmp_5e = val2 + tmp_5e + carry;
			carry = (tmp_5e >> 8);
			tmp_5e = tmp_5e & 0xff;
/* 62b8 */
			val = iwm_read_data(dsk, 1, 0);
			val2 = from_disk_byte[val];
			val2 = val2 + ((tmp_66 << 4) & 0xc0);
			val2 = val2 ^ tmp_5e;
			buf_d00[y] = val2;
			tmp_5d = val2 + tmp_5d + carry;

			carry = (tmp_5d >> 8);
			tmp_5d = tmp_5d & 0xff;

			y--;
			if(y <= 0) {
				break;
			}

/* 6274 */
			val = iwm_read_data(dsk, 1, 0);
			val2 = from_disk_byte[val];
			val2 = val2 + ((tmp_66 << 6) & 0xc0);
			val2 = val2 ^ tmp_5d;
			buf_e00[y+1] = val2;

			tmp_5c = val2 + tmp_5c + carry;
			carry = (tmp_5c >> 8);
			tmp_5c = tmp_5c & 0xff;
		}

/* 62d0 */
		val = iwm_read_data(dsk, 1, 0);
		val2 = from_disk_byte[val];

		tmp_66 = (val2 << 6) & 0xc0;
		tmp_67 = (val2 << 4) & 0xc0;
		val2 = (val2 << 2) & 0xc0;

		val = iwm_read_data(dsk, 1, 0);
		val2 = from_disk_byte[val] + val2;
		if(tmp_5e != val2) {
			ki_printf("Checksum 5e bad: %02x vs %02x\n", tmp_5e, val2);
			ki_printf("val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			break;
		}

		val = iwm_read_data(dsk, 1, 0);
		val2 = from_disk_byte[val] + tmp_67;
		if(tmp_5d != val2) {
			ki_printf("Checksum 5d bad: %02x vs %02x\n", tmp_5e, val2);
			ki_printf("val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			break;
		}

		val = iwm_read_data(dsk, 1, 0);
		val2 = from_disk_byte[val] + tmp_66;
		if(tmp_5c != val2) {
			ki_printf("Checksum 5c bad: %02x vs %02x\n", tmp_5e, val2);
			ki_printf("val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			break;
		}

		/* Whew, got it!...check for DE AA */
		val = iwm_read_data(dsk, 1, 0);
		if(val != 0xde) {
			ki_printf("Bad data epi1,val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			ki_printf("nib_pos: %08x\n", dsk->nib_pos);
			break;
		}

		val = iwm_read_data(dsk, 1, 0);
		if(val != 0xaa) {
			ki_printf("Bad data epi2,val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			break;
		}

		/* Now, convert buf_c/d/e to output */
/* 6459 */
		y = 0;
		for(x = 0xab; x >= 0; x--) {
			*buf++ = buf_c00[x];
			y++;
			if(y >= 0x200) {
				break;
			}

			*buf++ = buf_d00[x];
			y++;
			if(y >= 0x200) {
				break;
			}

			*buf++ = buf_e00[x];
			y++;
			if(y >= 0x200) {
				break;
			}
		}

		sector_done[phys_sec] = 1;
		num_sectors_done++;
		if(num_sectors_done >= num_sectors) {
			status = 0;
			break;
		}
		val = 0;
	}

	if(status < 0) {
		ki_printf("dsk->nib_pos: %04x, status: %d\n", dsk->nib_pos,
			status);
		for(i = 0; i < num_sectors; i++) {
			ki_printf("sector done[%d] = %d\n", i, sector_done[i]);
		}
	}

	iwm_move_to_track(dsk, save_qtr_track);
	dsk->nib_pos = save_nib_pos;
	g_fast_disk_unnib = 0;

	if(status == 0) {
		return 1;
	}

	ki_printf("Nibblization not done, %02x sectors found on track %02x\n",
		num_sectors_done, qtr_track>>2);
	return -1;



}

/* ret = 1 -> dirty data written out */
/* ret = 0 -> not dirty, no error */
/* ret < 0 -> error */
int
disk_track_to_unix(Disk *dsk, int qtr_track, byte *outbuf)
{
	Track	*trk;
	int	disk_525;

	disk_525 = dsk->disk_525;

	trk = &(dsk->tracks[qtr_track]);

	if(trk->track_len == 0 || trk->track_dirty == 0) {
#if 0
		ki_printf("disk_track_to_unix: dirty: %d\n", trk->track_dirty);
#endif
		return 0;
	}

	trk->track_dirty = 0;

	if((qtr_track & 3) && disk_525) {
		halt_printf("You wrote to phase %02x!  Can't wr bk to unix!\n",
			qtr_track);
		dsk->write_through_to_unix = 0;
		return -1;
	}

	if(disk_525) {
		return iwm_denib_track525(dsk, trk, qtr_track, outbuf);
	} else {
		return iwm_denib_track35(dsk, trk, qtr_track, outbuf);
	}
}


void
show_hex_data(byte *buf, int count)
{
	int	i;

	for(i = 0; i < count; i += 16) {
		ki_printf("%04x: %02x %02x %02x %02x %02x %02x %02x %02x "
			"%02x %02x %02x %02x %02x %02x %02x %02x\n", i,
			buf[i+0], buf[i+1], buf[i+2], buf[i+3], 
			buf[i+4], buf[i+5], buf[i+6], buf[i+7], 
			buf[i+8], buf[i+9], buf[i+10], buf[i+11], 
			buf[i+12], buf[i+13], buf[i+14], buf[i+15]);
	}

}

void
disk_check_nibblization(Disk *dsk, int qtr_track, byte *buf, int size)
{
	byte	buffer[0x3000];
	Track	*trk;
	int	ret, ret2;
	int	i;

	for(i = 0; i < size; i++) {
		buffer[i] = 0;
	}

	trk = &(dsk->tracks[qtr_track]);

	if(dsk->disk_525) {
		ret = iwm_denib_track525(dsk, trk, qtr_track, &(buffer[0]));
	} else {
		ret = iwm_denib_track35(dsk, trk, qtr_track, &(buffer[0]));
	}

	ret2 = -1;
	for(i = 0; i < size; i++) {
		if(buffer[i] != buf[i]) {
			ki_printf("buffer[%04x]: %02x != %02x\n", i, buffer[i],
				buf[i]);
			ret2 = i;
			break;
		}
	}

	if(ret != 1 || ret2 >= 0) {
		ki_printf("disk_check_nib ret:%d, ret2:%d for q_track %03x\n",
			ret, ret2, qtr_track);
		show_hex_data(buf, 0x1000);
		show_hex_data(buffer, 0x1000);
		iwm_show_a_track(&(dsk->tracks[qtr_track]));

		my_exit(2);
	}
}


#define TRACK_BUF_LEN		0x2000

void
disk_unix_to_nib(Disk *dsk, int qtr_track, int unix_pos, int unix_len,
	int nib_len)
{
	byte	track_buf[TRACK_BUF_LEN];
	Track	*trk;
	int	must_clear_track;
	int	ret;
	int	len;
	int	i;
	
	/* Read track from dsk int track_buf */

	must_clear_track = 0;

	if(unix_len > TRACK_BUF_LEN) {
		ki_printf("diks_unix_to_nib: requested len of image %s = %05x\n",
			dsk->name_ptr, unix_len);
	}

	if(unix_pos >= 0) {
		ret = lseek(dsk->fd, unix_pos, SEEK_SET);
		if(ret != unix_pos) {
			ki_printf("lseek of disk %s len 0x%x ret: %d, errno: %d\n",
				dsk->name_ptr, unix_pos, ret, errno);
			must_clear_track = 1;
		}

		len = read(dsk->fd, track_buf, unix_len);
		if(len != unix_len) {
			ki_printf("read of disk %s q_trk %d ret: %d, errno: %d\n",
				dsk->name_ptr, qtr_track, ret, errno);
			must_clear_track = 1;
		}
	}

	if(must_clear_track) {
		for(i = 0; i < TRACK_BUF_LEN; i++) {
			track_buf[i] = 0;
		}
	}

#if 0
	ki_printf("Q_track %02x dumped out\n", qtr_track);

	for(i = 0; i < 4096; i += 32) {
		ki_printf("%04x: %02x%02x%02x%02x%02x%02x%02x%02x "
			"%02x%02x%02x%02x%02x%02x%02x%02x "
			"%02x%02x%02x%02x%02x%02x%02x%02x "
			"%02x%02x%02x%02x%02x%02x%02x%02x\n", i,
			track_buf[i+0], track_buf[i+1], track_buf[i+2],
			track_buf[i+3], track_buf[i+4], track_buf[i+5],
			track_buf[i+6], track_buf[i+7], track_buf[i+8],
			track_buf[i+9], track_buf[i+10], track_buf[i+11],
			track_buf[i+12], track_buf[i+13], track_buf[i+14],
			track_buf[i+15], track_buf[i+16], track_buf[i+17],
			track_buf[i+18], track_buf[i+19], track_buf[i+20],
			track_buf[i+21], track_buf[i+22], track_buf[i+23],
			track_buf[i+24], track_buf[i+25], track_buf[i+26],
			track_buf[i+27], track_buf[i+28], track_buf[i+29],
			track_buf[i+30], track_buf[i+31]);
	}
#endif

	trk = &(dsk->tracks[qtr_track]);
	trk->track_dirty = 0;
	trk->overflow_size = 0;
	trk->track_len = 2*nib_len;
	trk->unix_pos = unix_pos;
	trk->unix_len = unix_len;
	trk->dsk = dsk;
	trk->nib_area = (byte *)malloc(trk->track_len);

	/* create nibblized image */

	if(dsk->disk_525 && dsk->image_type == DSK_TYPE_NIB) {
		iwm_nibblize_track_nib525(dsk, trk, track_buf, qtr_track);
	} else if(dsk->disk_525) {
		iwm_nibblize_track_525(dsk, trk, track_buf, qtr_track);
	} else {
		iwm_nibblize_track_35(dsk, trk, track_buf, qtr_track);
	}
}

void
iwm_nibblize_track_nib525(Disk *dsk, Track *trk, byte *track_buf, int qtr_track)
{
	byte	*nib_ptr;
	byte	*trk_ptr;
	int	len;
	int	i;

	len = trk->track_len;
	trk_ptr = track_buf;
	nib_ptr = &(trk->nib_area[0]);
	for(i = 0; i < len; i += 2) {
		nib_ptr[i] = 8;
		nib_ptr[i+1] = *trk_ptr++;;
	}

	iwm_printf("Nibblized q_track %02x\n", qtr_track);
}

void
iwm_nibblize_track_525(Disk *dsk, Track *trk, byte *track_buf, int qtr_track)
{
	byte	partial_nib_buf[0x300];
	word32	*word_ptr;
	word32	val;
	word32	last_val;
	int	phys_sec;
	int	log_sec;
	int	num_sync;
	int	i;


	word_ptr = (word32 *)&(trk->nib_area[0]);
#ifdef KEGS_LITTLE_ENDIAN
	val = 0xff08ff08;
#else
	val = 0x08ff08ff;
#endif
	for(i = 0; i < trk->track_len; i += 4) {
		*word_ptr++ = val;
	}


	for(phys_sec = 0; phys_sec < 16; phys_sec++) {
		if(dsk->image_type == DSK_TYPE_PRODOS) {
			log_sec = phys_to_prodos_sec[phys_sec];
		} else {
			log_sec = phys_to_dos_sec[phys_sec];
		}

		/* Create sync headers */
		if(phys_sec == 0) {
			num_sync = 70;
		} else {
			num_sync = 14;
		}

		for(i = 0; i < num_sync; i++) {
			disk_nib_out(dsk, 0xff, 10);
		}
		disk_nib_out(dsk, 0xd5, 10);		/* prolog */
		disk_nib_out(dsk, 0xaa, 8);		/* prolog */
		disk_nib_out(dsk, 0x96, 8);		/* prolog */
		disk_4x4_nib_out(dsk, dsk->vol_num);
		disk_4x4_nib_out(dsk, qtr_track >> 2);
		disk_4x4_nib_out(dsk, phys_sec);
		disk_4x4_nib_out(dsk, dsk->vol_num ^ (qtr_track>>2) ^ phys_sec);
		disk_nib_out(dsk, 0xde, 8);		/* epi */
		disk_nib_out(dsk, 0xaa, 8);		/* epi */
		disk_nib_out(dsk, 0xeb, 8);		/* epi */

		/* Inter sync */
		disk_nib_out(dsk, 0xff, 8);
		for(i = 0; i < 5; i++) {
			disk_nib_out(dsk, 0xff, 10);
		}
		disk_nib_out(dsk, 0xd5, 10);	/* data prolog */
		disk_nib_out(dsk, 0xaa, 8);	/* data prolog */
		disk_nib_out(dsk, 0xad, 8);	/* data prolog */

		sector_to_partial_nib( &(track_buf[log_sec*256]),
			&(partial_nib_buf[0]));

		last_val = 0;
		for(i = 0; i < 0x156; i++) {
			val = partial_nib_buf[i];
			disk_nib_out(dsk, to_disk_byte[last_val ^ val], 8);
			last_val = val;
		}
		disk_nib_out(dsk, to_disk_byte[last_val], 8);

		/* data epilog */
		disk_nib_out(dsk, 0xde, 8);	/* epi */
		disk_nib_out(dsk, 0xaa, 8);	/* epi */
		disk_nib_out(dsk, 0xeb, 8);	/* epi */
		disk_nib_out(dsk, 0xff, 8);
		for(i = 0; i < 6; i++) {
			disk_nib_out(dsk, 0xff, 10);
		}
	}

	/* finish nibblization */
	disk_nib_end_track(dsk);

	iwm_printf("Nibblized q_track %02x\n", qtr_track);

	if(g_check_nibblization) {
		disk_check_nibblization(dsk, qtr_track, &(track_buf[0]),0x1000);
	}
}

void
iwm_nibblize_track_35(Disk *dsk, Track *trk, byte *track_buf, int qtr_track)
{
	int	phys_to_log_sec[16];
	word32	buf_c00[0x100]={0};
	word32	buf_d00[0x100]={0};
	word32	buf_e00[0x100]={0};
	byte	*buf;
	word32	*word_ptr;
	word32	val;
	int	num_sectors;
	int	unix_len;
	int	log_sec;
	int	phys_sec;
	int	track;
	int	side;
	int	interleave;
	int	num_sync;
	word32	phys_track, phys_side, capacity, cksum;
	word32	tmp_5c, tmp_5d, tmp_5e, tmp_5f;
	word32	tmp_63, tmp_64, tmp_65;
	word32	acc_hi;
	int	carry;
	int	x, y;
	int	i;

	word_ptr = (word32 *)&(trk->nib_area[0]);
#ifdef KEGS_LITTLE_ENDIAN
	val = 0xff08ff08;
#else
	val = 0x08ff08ff;
#endif
	for(i = 0; i < trk->track_len; i += 4) {
		*word_ptr++ = val;
	}

	unix_len = trk->unix_len;

	num_sectors = (unix_len >> 9);

	for(i = 0; i < num_sectors; i++) {
		phys_to_log_sec[i] = -1;
	}

	phys_sec = 0;
	interleave = 2;
	for(log_sec = 0; log_sec < num_sectors; log_sec++) {
		while(phys_to_log_sec[phys_sec] >= 0) {
			phys_sec++;
			if(phys_sec >= num_sectors) {
				phys_sec = 0;
			}
		}
		phys_to_log_sec[phys_sec] = log_sec;
		phys_sec += interleave;
		if(phys_sec >= num_sectors) {
			phys_sec -= num_sectors;
		}
	}

	track = qtr_track >> 1;
	side = qtr_track & 1;
	for(phys_sec = 0; phys_sec < num_sectors; phys_sec++) {

		log_sec = phys_to_log_sec[phys_sec];
		if(log_sec < 0) {
			ki_printf("Track: %02x.%x phys_sec: %02x = %d!\n",
				track, side, phys_sec, log_sec);
			my_exit(2);
		}

		/* Create sync headers */
		if(phys_sec == 0) {
			num_sync = 400;
		} else {
			num_sync = 54;
		}

		for(i = 0; i < num_sync; i++) {
			disk_nib_out(dsk, 0xff, 10);
		}

		disk_nib_out(dsk, 0xd5, 10);		/* prolog */
		disk_nib_out(dsk, 0xaa, 8);		/* prolog */
		disk_nib_out(dsk, 0x96, 8);		/* prolog */

		phys_track = track & 0x3f;
		phys_side = (side << 5) + (track >> 6);
		capacity = 0x22;
		disk_nib_out(dsk, to_disk_byte[phys_track], 8);	/* trk */
		disk_nib_out(dsk, to_disk_byte[log_sec], 8);	/* sec */
		disk_nib_out(dsk, to_disk_byte[phys_side], 8);	/* sides+trk */
		disk_nib_out(dsk, to_disk_byte[capacity], 8);	/* capacity*/

		cksum = (phys_track ^ log_sec ^ phys_side ^ capacity) & 0x3f;
		disk_nib_out(dsk, to_disk_byte[cksum], 8);	/* cksum*/

		disk_nib_out(dsk, 0xde, 8);		/* epi */
		disk_nib_out(dsk, 0xaa, 8);		/* epi */

		/* Inter sync */
		for(i = 0; i < 5; i++) {
			disk_nib_out(dsk, 0xff, 10);
		}
		disk_nib_out(dsk, 0xd5, 10);	/* data prolog */
		disk_nib_out(dsk, 0xaa, 8);	/* data prolog */
		disk_nib_out(dsk, 0xad, 8);	/* data prolog */
		disk_nib_out(dsk, to_disk_byte[log_sec], 8);	/* sec again */

		/* do nibblizing! */
		buf = track_buf + (log_sec << 9);

/* 6320 */
		tmp_5e = 0;
		tmp_5d = 0;
		tmp_5c = 0;
		y = 0;
		x = 0xaf;
		for(y = 0x4; y > 0; y--) {
			buf_c00[x] = 0;
			buf_d00[x] = 0;
			buf_e00[x] = 0;
			x--;
		}

		while(x >= 0) {
/* 6338 */
			tmp_5c = tmp_5c << 1;
			carry = (tmp_5c >> 8);
			tmp_5c = (tmp_5c + carry) & 0xff;

			val = buf[y];
			tmp_5e = val + tmp_5e + carry;
			carry = (tmp_5e >> 8);
			tmp_5e = tmp_5e & 0xff;

			val = val ^ tmp_5c;
			buf_c00[x] = val;
			y++;
/* 634c */
			val = buf[y];
			tmp_5d = tmp_5d + val + carry;
			carry = (tmp_5d >> 8);
			tmp_5d = tmp_5d & 0xff;
			val = val ^ tmp_5e;
			buf_d00[x] = val;
			y++;
			x--;
			if(x <= 0) {
				break;
			}

/* 632a */
			val = buf[y];
			tmp_5c = tmp_5c + val + carry;
			carry = (tmp_5c >> 8);
			tmp_5c = tmp_5c & 0xff;

			val = val ^ tmp_5d;
			buf_e00[x+1] = val;
			y++;
		}

/* 635f */
		val = ((tmp_5c >> 2) ^ tmp_5d) & 0x3f;
/* 6367 */
		val = (val ^ tmp_5d) >> 2;
/* 636b */
		val = (val ^ tmp_5e) & 0x3f;
/* 636f */
		val = (val ^ tmp_5e) >> 2;
/* 6373 */
		tmp_5f = val;
/* 6375 */
		tmp_63 = 0;
		tmp_64 = 0;
		tmp_65 = 0;
		acc_hi = 0;


		y = 0xae;
		while(y >= 0) {
/* 63e4 */
			/* write out acc_hi */
			val = to_disk_byte[acc_hi];
			disk_nib_out(dsk, val, 8);

/* 63f2 */
			val = to_disk_byte[tmp_63 & 0x3f];
			tmp_63 = buf_c00[y];
			acc_hi = tmp_63 >> 6;
			disk_nib_out(dsk, val, 8);
/* 640b */
			val = to_disk_byte[tmp_64 & 0x3f];
			tmp_64 = buf_d00[y];
			acc_hi = (acc_hi << 2) + (tmp_64 >> 6);
			disk_nib_out(dsk, val, 8);
			y--;
			if(y < 0) {
				break;
			}

/* 63cb */
			val = to_disk_byte[tmp_65 & 0x3f];
			tmp_65 = buf_e00[y+1];
			acc_hi = (acc_hi << 2) + (tmp_65 >> 6);
			disk_nib_out(dsk, val, 8);
		}
/* 6429 */
		val = to_disk_byte[tmp_5f & 0x3f];
		disk_nib_out(dsk, val, 8);

		val = to_disk_byte[tmp_5e & 0x3f];
		disk_nib_out(dsk, val, 8);

		val = to_disk_byte[tmp_5d & 0x3f];
		disk_nib_out(dsk, val, 8);

		val = to_disk_byte[tmp_5c & 0x3f];
		disk_nib_out(dsk, val, 8);

/* 6440 */
		/* data epilog */
		disk_nib_out(dsk, 0xde, 8);	/* epi */
		disk_nib_out(dsk, 0xaa, 8);	/* epi */
		disk_nib_out(dsk, 0xff, 8);
	}


	disk_nib_end_track(dsk);

	if(g_check_nibblization) {
		disk_check_nibblization(dsk, qtr_track, &(track_buf[0]),
			unix_len);
	}
}

void
disk_4x4_nib_out(Disk *dsk, word32 val)
{
	disk_nib_out(dsk, 0xaa | (val >> 1), 8);
	disk_nib_out(dsk, 0xaa | val, 8);
}

void
disk_nib_out(Disk *dsk, byte val, int size)
{
	Track	*trk;
	int	pos;
	int	old_size;
	int	track_len;
	int	overflow_size;
	int	qtr_track;


	qtr_track = dsk->cur_qtr_track;

	trk = &(dsk->tracks[qtr_track]);

	track_len = trk->track_len;

	if(track_len <= 10) {
		ki_printf("Writing to an invalid qtr track: %02x!\n", qtr_track);
		ki_printf("name: %s, track_len: %08x, val: %08x, size: %d\n",
			dsk->name_ptr, track_len, val, size);
		my_exit(1);
		return;
	}

	trk->track_dirty = 1;
	dsk->disk_dirty = 1;

	pos = trk->dsk->nib_pos;
	overflow_size = trk->overflow_size;
	if(pos >= track_len) {
		pos = 0;
	}

	old_size = trk->nib_area[pos];


	while(size >= (10 + old_size)) {
		size = size - old_size;
		pos += 2;
		if(pos >= track_len) {
			pos = 0;
		}
		old_size = trk->nib_area[pos];
	}

	if(size > 10) {
		size = 10;
	}

	if((val & 0x80) == 0) {
		val |= 0x80;
	}

	trk->nib_area[pos++] = size;
	trk->nib_area[pos++] = val;
	if(pos >= track_len) {
		pos = 0;
	}

	overflow_size += (size - old_size);
	if((overflow_size > 8) && (size > 8)) {
		overflow_size -= trk->nib_area[pos];
		trk->nib_area[pos++] = 0;
		trk->nib_area[pos++] = 0;
		if(pos >= track_len) {
			pos = 0;
		}
	} else if(overflow_size < -64) {
		halt_printf("overflow_size:%03x, pos:%02x\n",overflow_size,pos);
	}

	trk->dsk->nib_pos = pos;
	trk->overflow_size = overflow_size;

	if((val & 0x80) == 0 || size < 8) {
		halt_printf("disk_nib_out, wrote %02x, size: %d\n", val, size);
	}
}

void
disk_nib_end_track(Disk *dsk)
{
	int	qtr_track;

	dsk->nib_pos = 0;
	qtr_track = dsk->cur_qtr_track;
	dsk->tracks[qtr_track].track_dirty = 0;

	dsk->disk_dirty = 0;
}

void
iwm_show_track(int slot_drive, int track)
{
	Disk	*dsk;
	Track	*trk;
	int	drive;
	int	sel35;
	int	qtr_track;

	if(slot_drive < 0) {
		drive = iwm.drive_select;
		sel35 = g_apple35_sel;
	} else {
		drive = slot_drive & 1;
		sel35 = !((slot_drive >> 1) & 1);
	}
	
	if(sel35) {
		dsk = &(iwm.drive35[drive]);
	} else {
		dsk = &(iwm.drive525[drive]);
	}

	if(track < 0) {
		qtr_track = dsk->cur_qtr_track;
	} else {
		qtr_track = track;
	}
	trk = &(dsk->tracks[qtr_track]);

	if(trk->track_len <= 0) {
		ki_printf("Track_len: %d\n", trk->track_len);
		ki_printf("No track for type: %d, drive: %d, qtrk: %02x\n",
			g_apple35_sel, drive, qtr_track);
		return;
	}

	ki_printf("Current drive: %d, q_track: %02x\n", drive, qtr_track);

	iwm_show_a_track(trk);
}

void
iwm_show_a_track(Track *trk)
{
	int	sum;
	int	len;
	int	pos;
	int	i;

	ki_printf("  Showtrack:dirty: %d, pos: %04x, ovfl: %04x, len: %04x\n",
		trk->track_dirty, trk->dsk->nib_pos,
		trk->overflow_size, trk->track_len);

	len = trk->track_len;
	ki_printf("Track len in bytes: %04x\n", len);
	if(len >= 2*15000) {
		len = 2*15000;
		ki_printf("len too big, using %04x\n", len);
	}

	pos = 0;
	for(i = 0; i < len; i += 16) {
		ki_printf("%04x: %2d,%02x %2d,%02x %2d,%02x %2d,%02x "
			"%2d,%02x %2d,%02x %2d,%02x %2d,%02x\n", pos,
			trk->nib_area[pos], trk->nib_area[pos+1],
			trk->nib_area[pos+2], trk->nib_area[pos+3],
			trk->nib_area[pos+4], trk->nib_area[pos+5],
			trk->nib_area[pos+6], trk->nib_area[pos+7],
			trk->nib_area[pos+8], trk->nib_area[pos+9],
			trk->nib_area[pos+10], trk->nib_area[pos+11],
			trk->nib_area[pos+12], trk->nib_area[pos+13],
			trk->nib_area[pos+14], trk->nib_area[pos+15]);
		pos += 16;
		if(pos >= len) {
			pos -= len;
		}
	}

	sum = 0;
	for(i = 0; i < len; i += 2) {
		sum += trk->nib_area[i];
	}

	ki_printf("bit_sum: %d, expected: %d, overflow_size: %d\n",
		sum, len*8/2, trk->overflow_size);
}

#define CONF_BUF_LEN	1024

void
maybe_parse_disk_conf_file()
{

	char	buf[CONF_BUF_LEN];
	FILE	*fconf;
	char	*ptr;
	char	*name_ptr;
	Disk	*dsk;
	int	reparse_delay;
	int	virtual_image;
	int	line;
	int	pos;
	int	disk_525;
	int	drive;
	int	smartport;
	int	size;
	int	len;
	int	ret;
	int	i;

	reparse_delay = g_reparse_delay;

	if(reparse_delay < 0) {
		return;
	}
	if(reparse_delay > 0) {
		g_reparse_delay = reparse_delay - 1;
		return;
	}

	ki_printf("Parsing disk_conf_file\n");
	g_reparse_delay = -1;

	g_highest_smartport_unit = -1;

	/* First, mark all drives as being in just_ejected | 0x80 state */
	for(i = 0; i < MAX_C7_DISKS; i++) {
		if(i < 2) {
			iwm.drive525[i].just_ejected |= 0x80;
			iwm.drive35[i].just_ejected |= 0x80;
		}
		iwm.smartport[i].just_ejected |= 0x80;
	}

	fconf = fopen(g_kegs_conf_name, "rt");
	if(fconf == 0) {
		ki_printf("cannot open disk_conf!  Stopping!\n");
		my_exit(3);
	}

	line = 0;
	while(1) {
		ptr = fgets(buf, CONF_BUF_LEN, fconf);
		if(ptr == 0) {
			iwm_printf("Done reading disk_conf\n");
			break;
		}

		line++;
		/* strip off newline(s) */
		len = strlen(buf);
		for(i = len - 1; i >= 0; i--) {
			if((buf[i] != 0x0d) && (buf[i] != 0x0a)) {
				break;
			}
			len = i;
			buf[i] = 0;
		}

		iwm_printf("disk_conf[%d]: %s\n", line, buf);
		if(len) {
			if(buf[0] == '#') {
				iwm_printf("Skipping comment\n");
				continue;
			}
		}

		/* determine what this is */
		pos = 0;

		while(pos < len && (buf[pos] == ' ' || buf[pos] == '\t') ) {
			pos++;
		}
		if((pos + 4) > len || buf[pos] != 's' || buf[pos+2] != 'd') {
			iwm_printf("Cannot find slot/drive on line %d:%d\n",
				line, pos);
			iwm_printf("line %d was %s\n", line, buf);
			continue;
		}

		disk_525 = buf[pos+1] - '5';
		smartport = 0;
		if(disk_525 == 2) {
			disk_525 = 0;
			smartport = 1;
		}

		drive = buf[pos+3] - '0';

		/* skip over slot, drive */
		pos += 4;
		if(buf[pos] >= '0' && buf[pos] <= '9') {
			drive = drive * 10 + buf[pos] - '0';
			pos++;
		}

		/*	make s6d1 mean index 0 */
		drive--;

		if(disk_525 != 0 && disk_525 != 1) {
			ki_printf("Not valid slot: %c\n", buf[pos]);
			continue;
		}
		if(drive < 0 || (smartport && (drive >= MAX_C7_DISKS)) ||
						(!smartport && (drive > 1))) {
			ki_printf("Not valid drive: %c\n", buf[pos+3]);
			continue;
		}

		size = 0;
		if(buf[pos] == ',') {
			/* read optional size parameter */
			pos++;
			while(pos < len && buf[pos] >= '0' && buf[pos] <= '9'){
				size = size * 10 + buf[pos] - '0';
				pos++;
			}
			ki_printf("Read optional size as: %d\n", size);
			size = size * 1024;
		}


		while(pos < len && (buf[pos] == ' ' || buf[pos] == '\t' ||
					buf[pos] == '=' || buf[pos] == ':') ) {
			pos++;
		}

		/* Get filename */
		if(smartport) {
			g_highest_smartport_unit = MAX(drive,
						g_highest_smartport_unit);
			dsk = &(iwm.smartport[drive]);
		} else if(disk_525) {
			dsk = &(iwm.drive525[drive]);
		} else {
			dsk = &(iwm.drive35[drive]);
		}

		virtual_image = 0;
		name_ptr = &(buf[pos]);
		if(*name_ptr == 0) {
			virtual_image = 1;
			name_ptr = "virtual_image";
		}

		insert_disk(dsk, name_ptr, virtual_image, size);

	}

	ret = fclose(fconf);
	if(ret != 0) {
		ki_printf("Closing disk_conf ret: %d, errno: %d\n", ret, errno);
		my_exit(4);
	}


	/* and unmount/eject any disks that are now gone */
	for(i = 0; i < MAX_C7_DISKS; i++) {
		if(i < 2) {
			eject_if_untouched(&iwm.drive525[i]);
			eject_if_untouched(&iwm.drive35[i]);
		}
		eject_if_untouched(&iwm.smartport[i]);
	}

	iwm_printf("Done parsing disk_conf file\n");
}


void
insert_disk(Disk *dsk, const char *name, int virtual_image, int size)
{
	char	tmp_buf[1024];
	byte	buf_2img[512];
	char	*name_ptr;
	char	*partition_name;
	int	cmp_o, cmp_p, cmp_dot;
	int	cmp_b, cmp_i, cmp_n;
	int	can_write;
	int	len;
	int	nibs;
	int	unix_pos;
	int	name_len;
	int	save_track;
	int	ret;
	int	tmp;
	int	i;
#ifdef HAVE_ZLIB
	gzFile gf;
#endif

	
	if (!name) return;

	tmp = dsk->disk_525 + 5;
	if(dsk->smartport) {
		/* print smartport disks at s7dx */
		tmp += 2;
	}
	ki_printf("Inserting disk %s in slot %d, drive: %d\n", name,
		tmp, dsk->drive + 1);

	dsk->just_ejected &= 0x7f;

	if(dsk->fd >= 0) {
		/* See if it has the same name--if so, just leave it */
		if(!strcmp(dsk->name_ptr, name)) {
			/* It's a match! Do nothing */
			ki_printf("Not remounting s%dd%d: %s\n", dsk->disk_525+5,
				dsk->drive, dsk->name_ptr);
			return;
		}

		/* Else, it's different, eject old disk */
		eject_disk(dsk);
	}

	/* Before opening, make sure no other mounted disk has this name */
	/* If so, unmount it */
	for(i = 0; i < MAX_C7_DISKS; i++) {
		if(i < 2) {
			eject_named_disk(&iwm.drive525[i], name);
			eject_named_disk(&iwm.drive35[i], name);
		}
		eject_named_disk(&iwm.smartport[i], name);
	}

	if(dsk->name_ptr != 0) {
		/* free old name_ptr */
		free(dsk->name_ptr);
	}

	name_len = strlen(name);
	name_ptr = (char *)malloc(name_len + 1);
	memcpy(name_ptr, name, name_len + 1);
	dsk->name_ptr = name_ptr;

	iwm_printf("Opening up disk image named: %s\n", name_ptr);

	if (name_len >= sizeof(tmp_buf) - 3) {
	    ki_printf("Disk image name too long: %s\n", name_ptr);
	    return;
	}

	iwm_printf("Opening up disk image named: %s\n", name_ptr);

	/* see if it has a partition name */
	partition_name = 0;
	tmp_buf[0] = 0;
	for(i = 0; i < name_len; i++) {
		/* ignore drive letters--look for partition names at char 3 */
		if((i > 2) && (name_ptr[i] == ':')) {
			/* yup, it's got a partition name! */
			partition_name = &name_ptr[i+1];
			break;
		}
		tmp_buf[i] = name_ptr[i];
		tmp_buf[i+1] = 0;
	}

	dsk->fd = -1;
	can_write = 1;
	
	snprintf(tmp_buf, sizeof(tmp_buf), "%s/.kegs/%s", getenv("HOME"), name);

#ifdef HAVE_ZLIB
	/* try opening gzipped image read-only first */
	if(!((name_len > 3) && (strcmp(&name_ptr[name_len - 3], ".gz")) == 0)) {
	    tmp_buf[name_len] = '.';
	    tmp_buf[name_len+1] = 'g';
	    tmp_buf[name_len+2] = 'z';
	    tmp_buf[name_len+3] = 0;
	}
	gf = gzopen(tmp_buf, "rb");
	if (gf == NULL) {
	    /* remove .gz extension */
	    tmp_buf[name_len] = 0;
	}
	else {
	    const size_t buflen = 64 * 1024;

	    iwm_printf("gzipped file %s opened successfully\n", tmp_buf);
	    strncpy(tmp_buf,"/tmp/kegs-XXXXXX", sizeof(tmp_buf));
	    dsk->fd = mkstemp (tmp_buf);
	    if (dsk->fd < 0) {
		ki_printf("Error: cannot create temp file %s\n",tmp_buf);
		gzclose(gf);
		return;
	    }
	    unlink (tmp_buf);
	    char *buffer = malloc (buflen);
	    while (!gzeof (gf)) {
		size_t rcount, wcount;
		rcount = gzread (gf,buffer,buflen);
		if (rcount < 1)
		    break;
		wcount = write (dsk->fd, buffer, rcount);
		if (wcount != rcount) {
		    ki_printf("Error: cannot create temp file %s (disk full?)\n",tmp_buf);
		    free(buffer);
		    gzclose (gf);
		    close(dsk->fd);
		    dsk->fd = -1;
		    return;
		}
	    }
	    free (buffer);
	    gzclose (gf);
	    lseek(dsk->fd, 0, SEEK_SET);
	}
#endif

	/* first, see if file exists.  If it doesn't just return */
	if(dsk->fd < 0)
	    dsk->fd = open(tmp_buf, O_RDWR | O_BINARY, 0x1b6);
	if(dsk->fd < 0) {
		ki_printf("Trying to open %s read-only, errno: %d\n", tmp_buf,
								errno);
		dsk->fd = open(tmp_buf, O_RDONLY | O_BINARY, 0x1b6);
		can_write = 0;
		if(dsk->fd < 0 && !dsk->smartport) {
			dsk->fd = open(tmp_buf, O_RDWR | O_CREAT | O_BINARY,
									0x1b6);
			if(dsk->fd >= 0) {
				can_write = 1;
			}
		}
	}

	iwm_printf("open returned: %d\n", dsk->fd);

	if(dsk->fd < 0) {
		if(virtual_image) {
			can_write = 1;
		} else {
			return;
		}
	}

	if(can_write != 0) {
		dsk->write_prot = 0;
		dsk->write_through_to_unix = 1;
	} else {
		dsk->write_prot = 1;
		dsk->write_through_to_unix = 0;
	}

	save_track = dsk->cur_qtr_track;	/* save arm position */
	dsk->image_type = DSK_TYPE_PRODOS;
	dsk->image_start = 0;

	/* See if it is in 2IMG format */
	ret = read(dsk->fd, (char *)&buf_2img[0], 512);
	if(buf_2img[0] == '2' && buf_2img[1] == 'I' && buf_2img[2] == 'M' &&
			buf_2img[3] == 'G') {
		/* It's a 2IMG disk */
		ki_printf("Image named %s is in 2IMG format\n", dsk->name_ptr);

		if(buf_2img[12] == 0) {
			ki_printf("2IMG is in DOS 3.3 sector order\n");
			dsk->image_type = DSK_TYPE_DOS33;
		}
		if(buf_2img[19] & 0x80) {
			/* disk is locked */
			ki_printf("2IMG is write protected\n");
			dsk->write_prot = 1;
			dsk->write_through_to_unix = 0;
		}
		if((buf_2img[17] & 1) && (dsk->image_type == DSK_TYPE_DOS33)) {
			dsk->vol_num = buf_2img[16];
			ki_printf("Setting DOS 3.3 vol num to %d\n", dsk->vol_num);
		}
		//	Some 2IMG archives have the size byte reversed
		size = (buf_2img[31] << 24) + (buf_2img[30] << 16) +
				(buf_2img[29] << 8) + buf_2img[28];
		unix_pos = (buf_2img[27] << 24) + (buf_2img[26] << 16) +
				(buf_2img[25] << 8) + buf_2img[24];
		if(size == 0x800c00) {
			//	Byte reversed 0x0c8000
			size = 0x0c8000;
		}
                
                // Gilles Tschopp - Patch
		// 2IMG created by Bernie have always a size of zero.
		// Therefore we have to calculate the file length minus 64 bits (XGS header size)
		if (size == 0) {
			size = get_fd_size(dsk->fd) - 64 ;
		}	

		dsk->image_start = unix_pos;
		dsk->image_size = size;
	} else {
		/* Assume raw image */
		dsk->image_start = 0;
		if(size <= 0) {
			size = get_fd_size(dsk->fd);
		}
		dsk->image_size = size;
		if(dsk->disk_525) {
			dsk->image_type = 0;
			if(name_len >= 5) {
				cmp_o = dsk->name_ptr[name_len-2];
				cmp_p = dsk->name_ptr[name_len-3];
				cmp_dot = dsk->name_ptr[name_len-4];
				if(cmp_dot == '.' &&
					  (cmp_p == 'p' || cmp_p == 'P') &&
					  (cmp_o == 'o' || cmp_o == 'O')) {
					dsk->image_type = DSK_TYPE_PRODOS;
				}

				cmp_b = dsk->name_ptr[name_len-2];
				cmp_i = dsk->name_ptr[name_len-3];
				cmp_n = dsk->name_ptr[name_len-4];
				cmp_dot = dsk->name_ptr[name_len-5];
				if(cmp_dot == '.' &&
					  (cmp_n == 'n' || cmp_n == 'N') &&
					  (cmp_i == 'i' || cmp_i == 'I') &&
					  (cmp_b == 'b' || cmp_b == 'B')) {
					dsk->image_type = DSK_TYPE_NIB;
					dsk->write_prot = 1;
					dsk->write_through_to_unix = 0;
				}
			}
		} else {
			dsk->image_type = 0;
		}
	}

	dsk->disk_dirty = 0;
	dsk->nib_pos = 0;

	if(dsk->smartport) {
		if(partition_name) {
			ret = find_partition_by_name(dsk->fd, partition_name,
							dsk);
			ki_printf("partition %s mounted, write_prot: %d\n",
				partition_name, dsk->write_prot);

			if(ret < 0) {
				close(dsk->fd);
				dsk->fd = -1;
				return;
			}
		}
		iwm_printf("adding smartport device[%d], size:%08x, "
			"img_sz:%08x\n", dsk->drive, dsk->tracks[0].unix_len,
			dsk->image_size);
	} else if(dsk->disk_525) {
		unix_pos = dsk->image_start;
		size = dsk->image_size;
		dsk->num_tracks = 4*35;
		len = 0x1000;
		nibs = NIB_LEN_525;
		if(dsk->image_type == DSK_TYPE_NIB) {
			len = dsk->image_size / 35;;
			nibs = len;
		}
		if(size != 35*len) {
			ki_printf("Disk 5.25 error: size is %d, not %d\n",size,
					35*len);
		}
		for(i = 0; i < 35; i++) {
			iwm_move_to_track(dsk, 4*i);
			disk_unix_to_nib(dsk, 4*i, unix_pos, len, nibs);
			unix_pos += len;
		}
	} else {
		/* disk_35 */
		unix_pos = dsk->image_start;
		size = dsk->image_size;
		if(size != 800*1024) {
			ki_printf("Disk 3.5 error: size is %d, not 800K\n", size);
		}
		dsk->num_tracks = 2*80;
		for(i = 0; i < 2*80; i++) {
			iwm_move_to_track(dsk, i);
			len = g_track_bytes_35[i >> 5];
			nibs = g_track_nibs_35[i >> 5];
			if(virtual_image) {
				unix_pos = -1;
			}
			iwm_printf("Trk: %d.%d = unix: %08x, %04x, %04x\n",
				i>>1, i & 1, unix_pos, len, nibs);
			disk_unix_to_nib(dsk, i, unix_pos, len, nibs);
			unix_pos += len;

			iwm_printf(" trk_len:%05x\n",dsk->tracks[i].track_len);
		}
	}

	iwm_move_to_track(dsk, save_track);

}

void
eject_named_disk(Disk *dsk, const char *name)
{

	if(dsk->fd < 0) {
		return;
	}

	/* If name matches, eject the disk! */
	if(!strcmp(dsk->name_ptr, name)) {
		/* It matches, eject it */
		eject_disk(dsk);
	}
}

void
eject_if_untouched(Disk *dsk)
{

	if(dsk->fd < 0) {
		return;
	}

	if(dsk->just_ejected & 0x80) {
		/* It was not touched, eject it */
		eject_disk(dsk);
	}
}

void
eject_disk(Disk *dsk)
{
	int	motor_on;
	int	i;

	motor_on = iwm.motor_on;
	if(g_apple35_sel) {
		motor_on = iwm.motor_on35;
	}
	if(motor_on) {
		halt_printf("Try eject dsk:%s, but motor_on!\n", dsk->name_ptr);
	}

	iwm_flush_disk_to_unix(dsk);

	ki_printf("Ejecting disk: %s\n", dsk->name_ptr);

	/* Free all memory, close file */
	
	/* free the tracks first */
	for(i = 0; i < dsk->num_tracks; i++) {
		if(dsk->tracks[i].nib_area) {
			free(dsk->tracks[i].nib_area);
		}
		dsk->tracks[i].nib_area = 0;
		dsk->tracks[i].track_len = 0;
	}
	dsk->num_tracks = 0;

	/* close file, clean up dsk struct */
	close(dsk->fd);

	free(dsk->name_ptr);
	dsk->name_ptr = 0;
	dsk->image_start = 0;
	dsk->image_size = 0;
	dsk->nib_pos = 0;
	dsk->disk_dirty = 0;
	dsk->write_through_to_unix = 0;
	dsk->write_prot = 1;
	dsk->fd = -1;
	dsk->just_ejected = 1;
}

#define	COPY_BUF_SIZE	4096

void
kegs_file_copy(char *orig_name, char *new_name)
{
	char	copy_buf[COPY_BUF_SIZE];
	int	fd_in, fd_out;
	int	len;
	int	ret;

	fd_in = open(orig_name, O_RDONLY | O_BINARY, 0x1b6);
	if(fd_in < 0) {
		ki_printf("kegs_file_copy: open %s failed: %d %d\n",
			orig_name, fd_in, errno);
		my_exit(1);
	}

	fd_out = open(new_name, O_WRONLY | O_BINARY | O_TRUNC | O_CREAT, 0x1b6);
	if(fd_out < 0) {
		ki_printf("kegs_file_copy: open %s failed: %d %d\n",
			orig_name, fd_out, errno);
		my_exit(1);
	}

	while(1) {
		len = read(fd_in, &copy_buf[0], COPY_BUF_SIZE);
		if(len == 0) {
			break;
		}
		if(len < 0) {
			if(errno == EINTR) {
				continue;
			}
			ki_printf("Error reading from file %s, errno: %d\n",
				orig_name, errno);
			my_exit(1);
		}
		ret = write(fd_out, &copy_buf[0], len);
		if(ret != len) {
			ki_printf("Error writing to file %s: ret:%d, %d\n",
				new_name, ret, errno);
			my_exit(1);
		}
	}
	close(fd_in);
	close(fd_out);
}

void
eject_disk_by_num(int slot, int drive)
{
	char	buf[CONF_BUF_LEN];
	char	tmp_buf2[1024];
	FILE	*fconf_old, *fconf_new;
	char	*ptr;
	int	line;

	sprintf(tmp_buf2, "%s.ktmp1", g_kegs_conf_name);

	(void)unlink(tmp_buf2);			/* "rm -f tmp_buf2" */
	kegs_file_copy(g_kegs_conf_name, tmp_buf2);

	fconf_old = fopen(tmp_buf2, "rt");
	fconf_new = fopen(g_kegs_conf_name, "wt+");
	if(fconf_old == 0 || fconf_new == 0) {
		ki_printf("Cannot open %s or %s: Stopping\n", tmp_buf2,
				g_kegs_conf_name);
		my_exit(3);
	}
	line = 0;
	while(1) {
		ptr = fgets(buf, CONF_BUF_LEN, fconf_old);
		if(ptr == 0) {
			/* done */
			break;
		}
		line++;
		if((buf[0] == 's') && (buf[1] == (0x30 + slot)) &&
				(buf[2] == 'd') && (buf[3] == (0x30 + drive))){
			/* comment out this line */
			ki_printf("Ejecting s%dd%d from line %d of %s\n",
				slot, drive, line, g_kegs_conf_name);
			fputs("#", fconf_new);
		}
		fputs(buf, fconf_new);
	}
	fclose(fconf_old);
	fclose(fconf_new);

	(void)unlink(tmp_buf2);			/* "rm -f tmp_buf2" */

	/* and make sure it gets reparsed */
	g_reparse_delay = 0;
	g_iwm_vbl_count = 0;
}



int
get_fast_disk_emul()
{
    return g_fast_disk_emul;
}

int
set_fast_disk_emul(int val)
{
    g_fast_disk_emul = val;
    return 1;
}

// OG

void iwm_shut()
{
	int i;

//	from_disk_byte_valid = 0;
	g_reparse_delay = 0;

	g_dcycs_end_emul_wr = 0.0;
	g_fast_disk_unnib = 0;
	g_iwm_fake_fast = 0;
	from_disk_byte_valid = 0;

	g_apple35_sel = 0;
	head_35 = 0;
	g_iwm_motor_on = 0;

	g_check_nibblization = 0;
	g_disk_conf_mtime = 0;
	g_highest_smartport_unit = -1;

	// eject tousl es disks
	for(i = 0; i < 2; i++) {
		if (iwm.drive525[i].fd>=0) close(iwm.drive525[i].fd);
		if (iwm.drive35[i].fd>=0) close(iwm.drive35[i].fd);	
	}

	for(i = 0; i < MAX_C7_DISKS; i++) {
		if (iwm.smartport[i].fd>=0) close(iwm.smartport[i].fd);	
	}

	g_iwm_vbl_count = 0;
	g_cnt_enable2_handshake = 0;

	memset(&iwm,0,sizeof(iwm));
}
