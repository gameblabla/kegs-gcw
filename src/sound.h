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
const char rcsid_sound_h[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/sound.h,v 1.3 2005/09/23 12:37:09 fredyd Exp $";
#endif

#ifndef KEGS_SOUND_H
#define KEGS_SOUND_H


#define SOUND_SHM_SAMP_SIZE		(32*1024)

#define SOUND_NONE 0
#define SOUND_NATIVE 1
#define SOUND_SDL 2
#define SOUND_ALIB 3

#define SAMPLE_SIZE		2
#define NUM_CHANNELS		2
#define SAMPLE_CHAN_SIZE	(SAMPLE_SIZE * NUM_CHANNELS)
#if 0
# define DO_DOC_LOG
#endif

#ifdef DO_DOC_LOG
# define DOC_LOG(a,b,c,d)	doc_log_rout(a,b,c,d)
#else
# define DOC_LOG(a,b,c,d)
#endif


STRUCT(Doc_reg) {
	double	dsamp_ev;
	double	dsamp_ev2;
	double	complete_dsamp;
	int	samps_left;
	word32	cur_acc;
	word32	cur_inc;
	word32	cur_start;
	word32	cur_end;
	word32	cur_mask;
	int	size_bytes;
	int	event;
	int	running;
	int	has_irq_pending;
	word32	freq;
	word32	vol;
	word32	waveptr;
	word32	ctl;
	word32	wavesize;
	word32	last_samp_val;
};

extern int g_audio_enable;
extern word32 doc_ptr;
extern int g_send_sound_to_file;
extern byte doc_ram[0x10000 + 16];
extern int g_doc_vol;
extern word32	*g_sound_shm_addr;
extern int	g_sound_shm_pos;
extern double g_last_sound_play_dsamp;

extern word32 g_cycs_in_sound1;
extern word32 g_cycs_in_sound2;
extern word32 g_cycs_in_sound3;
extern word32 g_cycs_in_sound4;
extern word32 g_cycs_in_start_sound;
extern word32 g_cycs_in_est_sound;

extern float g_fvoices;
extern int g_num_snd_plays;
extern int g_num_doc_events;
extern int g_num_start_sounds;
extern int g_num_scan_osc;
extern int g_num_recalc_snd_parms;
extern int g_doc_saved_ctl;
extern int g_audio_rate;

void doc_show_ensoniq_state(int osc);
int doc_read_c030(double dcycs);
int doc_read_c03c(double dcycs);
int doc_read_c03d(double dcycs);
void doc_write_c03c(int val, double dcycs);
void doc_write_c03d(int val, double dcycs);
void doc_write_c03e(int val);
void doc_write_c03f(int val);
void sound_init(int);
void sound_reset(double dcycs);
void sound_update(double dcycs);
void doc_handle_event(int osc, double dcycs);
void set_audio_rate(int rate);


#endif /* KEGS_SOUND_H */
