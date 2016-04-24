#ifndef KEGS_SIM65816_H
#define KEGS_SIM65816_H

#include "defc.h"
#include "ki.h"

#ifdef USE_CONFIG_FILE
extern char fileconfig[256];
#endif

extern word32 g_vbl_count;
extern int g_rom_version;
extern byte *g_memory_ptr;
extern byte *g_slow_memory_ptr;
extern byte *g_rom_fc_ff_ptr;
extern byte *g_rom_cards_ptr;
extern word32 g_mem_size_base, g_mem_size_exp;
extern byte *g_dummy_memory1_ptr;
extern int halt_sim;
extern int enter_debug;
extern int Verbose;
extern word32 stop_run_at;
extern int Halt_on;
extern int g_irq_pending;
extern Engine_reg engine;
extern double g_fcycles_stop;
extern int g_wait_pending;
extern int g_testing;
extern int g_num_brk;
extern int g_num_cop;
extern Pc_log *log_pc_ptr;
extern Pc_log *log_pc_start_ptr;
extern Pc_log *log_pc_end_ptr;

extern Page_info page_info_rd_wr[2*65536 + PAGE_INFO_PAD_SIZE];
extern double g_last_vbl_dcycs;
extern int speed_changed;
extern Fplus *g_cur_fplus_ptr;
extern double g_cur_dcycs;
extern int g_limit_speed;
extern int g_screen_depth;
extern int g_force_depth;
extern int g_io_amt;

/* engine_c.c */
extern int g_engine_c_mode;
extern int defs_instr_start_8;
extern int defs_instr_start_16;
extern int defs_instr_end_8;
extern int defs_instr_end_16;
extern int op_routs_start;
extern int op_routs_end;

void add_irq(void);
void remove_irq(void);
void add_event_stop(double dcycs);
void add_event_doc(double dcycs, int osc);
double remove_event_doc(int osc);
void add_event_scc(double dcycs, int type);
void my_exit(int ret);
word32 get_memory_io(word32 loc, double *cyc_ptr);
void set_memory_io(word32 loc, int val, double *cyc_ptr);
byte *memalloc_align(int size, int skip_amt, void **alloc_ptr);
void	memfree_align(byte* ptr);
void show_dtime_array(void);
void show_all_events(void);
void show_toolbox_log(void);
void show_pc_log(void);
void show_pmhz(void);
void show_regs(void);
void do_reset(void);
void run_prog(void);
void setup_kegs_file(char *outname, int maxlen, int ok_if_missing, const char **name_ptr);
void setup_kegs_file_conf(char *outname, int maxlen, int ok_if_missing, const char **name_ptr);
void check_for_new_scan_int(double dcycs);

int get_limit_speed(void);
int set_limit_speed(int);

#endif /* KEGS_SIM65816_H */
