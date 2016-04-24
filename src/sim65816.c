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

const char rcsid_sim65816_c[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/sim65816.c,v 1.4 2005/09/23 12:37:09 fredyd Exp $";
#ifdef USE_CONFIG_FILE
char fileconfig[256];
#endif

#if defined(WIN32)
#include <windows.h>
#endif

#include <math.h>

#define INCLUDE_RCSID_C
#include "sim65816.h"
#undef INCLUDE_RCSID_C
#include "sim65816.h"
#include "moremem.h"
#include "video.h"
#include "videodriver.h"
#include "sound.h"
#include "sounddriver.h"
#include "iwm.h"
#include "dis.h"
#include "paddles.h"
#include "smartport.h"
#include "adb.h"
#include "joystick.h"
#include "engine.h"
#include "clock.h"
#include "scc.h"
#include "configmenu.h"

void fixed_memory_ptrs_shut();

static void show_regs_act(Engine_reg *eptr);
static void check_engine_asm_defines(void);
static void memory_ptr_init(void);
static void initialize_events(void);
static void check_for_one_event_type(int type);
static void add_event_entry(double dcycs, int type);
static double remove_event_entry(int type);
static void add_event_vbl(void);
static void take_irq(int is_it_brk);
static void update_60hz(double dcycs, double dtime_now);
static void do_vbl_int(void);
static void do_scan_int(double dcycs, int line);
static void check_scan_line_int(double dcycs, int cur_video_line);
static void init_reg(void);
static void handle_action(word32 ret);
static void do_break(word32 ret);
static void do_cop(word32 ret);
static	void sim65816_shut();
static void load_roms_init_memory();

#ifdef __APPLE__
    /* MacOS X resources folder */
const char *g_kegs_default_paths[] = {"","./KEGS-OSX.app/Contents/Resources/", 0};
#else

const char *g_kegs_default_paths[] = { "", "./", "~/", "/usr/local/lib/",
	"/usr/local/kegs/", "/usr/local/lib/kegs/", "/usr/share/kegs/",
	"/usr/share/", "/var/lib/", "/usr/lib/", "/lib/", "/etc/",
	"/etc/kegs/",
	"./KEGS.app/Contents/Resources/", /* MacOS X resources folder */
	0 };
#endif

#define MAX_EVENTS	64

/* All EV_* must be less than 256, since upper bits reserved for other use */
/*  e.g., DOC_INT uses upper bits to encode oscillator */
#define EV_60HZ		1
#define EV_STOP		2
#define EV_SCAN_INT	3
#define EV_DOC_INT	4
#define EV_VBL_INT	5
#define EV_SCC		6
#define MEMORY_MARKER (('K'<<24) + ('E'<<16) + ('G'<<8) + 'S')

Engine_reg engine;

void U_STACK_TRACE();

double	g_fcycles_stop = 0.0;
int	halt_sim = 0;
int enter_debug = 0;
int	g_rom_version = 0;
int	g_halt_on_bad_read = 0;
int	g_ignore_bad_acc = 0;

#if 0
const double g_drecip_cycles_in_16ms = (1.0/(DCYCS_IN_16MS));
const double g_dcycles_in_16ms = DCYCS_IN_16MS;
#endif

#define START_DCYCS	(0.0)

double	g_last_vbl_dcycs = START_DCYCS;
double	g_cur_dcycs = START_DCYCS;

double	g_last_vbl_dadjcycs = 0.0;
double	g_dadjcycs = 0.0;


int	g_wait_pending = 0;
int	g_irq_pending = 0;

int	g_num_irq = 0;
int	g_num_brk = 0;
int	g_num_cop = 0;
int	g_num_enter_engine = 0;
int	g_io_amt = 0;
int	g_engine_action = 0;
int	g_engine_halt_event = 0;
int	g_engine_scan_int = 0;
int	g_engine_doc_int = 0;

int	g_testing = 0;


word32 stop_run_at;

int g_25sec_cntr = 0;
int g_1sec_cntr = 0;

int Verbose = 0;
int Halt_on = 0;

word32 g_mem_size_base = 256*1024;	/* size of motherboard memory */
word32 g_mem_size_exp = 4*1024*1024;	/* size of expansion RAM card */
word32 g_mem_size_total = 256*1024;	/* Total contiguous RAM from 0 */

byte *g_slow_memory_ptr = 0;
byte *g_memory_ptr = 0;
byte *g_dummy_memory1_ptr = 0;
byte *g_rom_fc_ff_ptr = 0;
byte *g_rom_cards_ptr = 0;

void *g_memory_alloc_ptr = 0;		/* for freeing memory area */

Page_info page_info_rd_wr[2*65536 + PAGE_INFO_PAD_SIZE];

int kbd_in_end = 0;
byte kbd_in_buf[LEN_KBD_BUF];


#define PC_LOG_LEN	(16*1024)

Pc_log pc_log_array[PC_LOG_LEN + 2];

Pc_log	*log_pc_ptr = &(pc_log_array[0]);
Pc_log	*log_pc_start_ptr = &(pc_log_array[0]);
Pc_log	*log_pc_end_ptr = &(pc_log_array[PC_LOG_LEN]);

#define TOOLBOX_LOG_LEN		64

int g_toolbox_log_pos = 0;
word32 g_toolbox_log_array[TOOLBOX_LOG_LEN][8];

double	g_dtime_last_vbl = 0.0;
double	g_dtime_expected = (1.0/60.0);

int g_scan_int_events = 0;



void
show_pc_log()
{
	FILE *pcfile;
	double	dcycs;
	double	start_dcycs;
	word32	instr;
	word32	psr;
	word32	acc, xreg, yreg;
	word32	stack, direct;
	word32	dbank;
	word32	kpc;
	int	accsize, xsize;
	int	num;
	int	i;

	pcfile = fopen("pc_log_out", "wt");
	if(pcfile == 0) {
		fprintf(stderr,"fopen failed...errno: %d\n", errno);
		my_exit(2);
	}
	fprintf(pcfile, "current pc_log_ptr: %p, start: %p, end: %p\n",
		log_pc_ptr, log_pc_start_ptr, log_pc_end_ptr);

	start_dcycs = log_pc_ptr->dcycs;

	for(i = 0; i < PC_LOG_LEN; i++) {
		dbank = (log_pc_ptr->dbank_kpc >> 24) & 0xff;
		kpc = log_pc_ptr->dbank_kpc & 0xffffff;
		instr = log_pc_ptr->instr;
		psr = (log_pc_ptr->psr_acc >> 16) & 0xffff;;
		acc = log_pc_ptr->psr_acc & 0xffff;;
		xreg = (log_pc_ptr->xreg_yreg >> 16) & 0xffff;;
		yreg = log_pc_ptr->xreg_yreg & 0xffff;;
		stack = (log_pc_ptr->stack_direct >> 16) & 0xffff;;
		direct = log_pc_ptr->stack_direct & 0xffff;;
		dcycs = log_pc_ptr->dcycs;

		num = (int)( log_pc_ptr - log_pc_start_ptr );

		accsize = 2;
		xsize = 2;
		if(psr & 0x20) {
			accsize = 1;
		}
		if(psr & 0x10) {
			xsize = 1;
		}

		fprintf(pcfile, "%04x: A:%04x X:%04x Y:%04x P:%03x "
			"S:%04x D:%04x B:%02x %9.2f ", i,
			acc, xreg, yreg, psr, stack, direct, dbank,
			(dcycs-start_dcycs));

		do_dis(pcfile, kpc, accsize, xsize, 1, instr);
		log_pc_ptr++;
		if(log_pc_ptr >= log_pc_end_ptr) {
			log_pc_ptr = log_pc_start_ptr;
		}
	}

	fclose(pcfile);
}


#if 0
word32
toolbox_debug_4byte(word32 addr)
{
	word32	part1, part2;

	/* If addr looks safe, use it */
	if(addr > 0xbffc) {
		return (word32)-1;
	}

	part1 = get_memory16_c(addr, 0);
	part1 = (part1 >> 8) + ((part1 & 0xff) << 8);
	part2 = get_memory16_c(addr+2, 0);
	part2 = (part2 >> 8) + ((part2 & 0xff) << 8);

	return (part1 << 16) + part2;
}

void
toolbox_debug_c(word32 xreg, word32 stack, double *cyc_ptr)
{
	int	pos;

	pos = g_toolbox_log_pos;

	stack += 9;
	g_toolbox_log_array[pos][0] = g_last_vbl_dcycs + *cyc_ptr;
	g_toolbox_log_array[pos][1] = stack+1;
	g_toolbox_log_array[pos][2] = xreg;
	g_toolbox_log_array[pos][3] = toolbox_debug_4byte(stack+1);
	g_toolbox_log_array[pos][4] = toolbox_debug_4byte(stack+5);
	g_toolbox_log_array[pos][5] = toolbox_debug_4byte(stack+9);
	g_toolbox_log_array[pos][6] = toolbox_debug_4byte(stack+13);
	g_toolbox_log_array[pos][7] = toolbox_debug_4byte(stack+17);

	pos++;
	if(pos >= TOOLBOX_LOG_LEN) {
		pos = 0;
	}

	g_toolbox_log_pos = pos;
}
#endif

void
show_toolbox_log()
{
	int	pos;
	int	i;

	pos = g_toolbox_log_pos;

	for(i = TOOLBOX_LOG_LEN - 1; i >= 0; i--) {
		ki_printf("%2d:%2d: %08x %06x  %04x: %08x %08x %08x %08x %08x\n",
			i, pos,
			g_toolbox_log_array[pos][0],
			g_toolbox_log_array[pos][1],
			g_toolbox_log_array[pos][2],
			g_toolbox_log_array[pos][3],
			g_toolbox_log_array[pos][4],
			g_toolbox_log_array[pos][5],
			g_toolbox_log_array[pos][6],
			g_toolbox_log_array[pos][7]);
		pos++;
		if(pos >= TOOLBOX_LOG_LEN) {
			pos = 0;
		}
	}
}

#if 0
/* get_memory_c is not used, get_memory_asm is, but this does what the */
/*  assembly language would do */
word32
get_memory_c(word32 loc, int diff_cycles)
{
	byte	*addr;
	word32	result;
	int	index;

#ifdef CHECK_BREAKPOINTS
	check_breakpoints_c(loc);
#endif

	index = loc >> 8;
	result = page_info[index].rd;
	if(result & BANK_IO_BIT) {
		return get_memory_io(loc, diff_cycles);
	}

	addr = (byte *)((result & 0xffffff00) + (loc & 0xff));

	return *addr;
}
#endif


word32
get_memory_io(word32 loc, double *cyc_ptr)
{
	int	tmp;

	if(loc > 0xffffff) {
		halt_printf("get_memory_io:%08x out of range==halt!\n", loc);
		return 0;
	}

	tmp = loc & 0xfef000;
	if(tmp == 0xc000 || tmp == 0xe0c000) {
		return(io_read(loc & 0xfff, cyc_ptr));
	}

	/* Else it's an illegal addr...skip if memory sizing */
	if(loc >= g_mem_size_total) {
		if((loc & 0xfffe) == 0) {
#if 0
			ki_printf("get_io assuming mem sizing, not halting\n");
#endif
			return 0;
		}
	}

	/* Skip reads to f80000 and f00000, just return 0 */
	if((loc & 0xf70000) == 0xf00000) {
		return 0;
	}

	if((loc & 0xff0000) == 0xef0000) {
		/* DOC RAM */
		return (doc_ram[loc & 0xffff]);
	}

	if(g_ignore_bad_acc) {
		/* print no message, just get out.  User doesn't want */
		/*  to be bothered by buggy programs */
		return 0;
	}

	ki_printf("get_memory_io for addr: %06x\n", loc);
	ki_printf("stat for addr: %06x = %p\n", loc,
				GET_PAGE_INFO_RD((loc >> 8) & 0xffff));
	set_halt(g_halt_on_bad_read);

	return 0;
}

#if 0
word32
get_memory16_pieces(word32 loc, int diff_cycles)
{
	return(get_memory_c(loc, diff_cycles) +
		(get_memory_c(loc+1, diff_cycles) << 8));
}

word32
get_memory24(word32 loc, int diff_cycles)
{
	return(get_memory_c(loc, diff_cycles) +
		(get_memory_c(loc+1, diff_cycles) << 8) +
		(get_memory_c(loc+2, diff_cycles) << 16));
}
#endif

#if 0
void
set_memory(word32 loc, int val, int diff_cycles)
{
	byte *ptr;
	word32	new_addr;
	word32	tmp;
	word32	or_val;
	int	or_pos;
	int	old_slow_val;

#ifdef CHECK_BREAKPOINTS
	check_breakpoints_c(loc);
#endif

	tmp = GET_PAGE_INFO_WR((loc>>8) & 0xffff);
	if(tmp & BANK_IO) {
		set_memory_io(loc, val, diff_cycles);
		return;
	}

	if((loc & 0xfef000) == 0xe0c000) {
		ki_printf("set_memory_special: non-io for addr %08x, %02x, %d\n",
			loc, val, diff_cycles);
		halt_printf("tmp: %08x\n", tmp);
	}

	ptr = (byte *)(tmp & (~0xff));

	new_addr = loc & 0xffff;
	old_slow_val = val;

	if(tmp & BANK_SHADOW) {
		old_slow_val = g_slow_memory_ptr[new_addr];
	} else if(tmp & BANK_SHADOW2) {
		new_addr += 0x10000;
		old_slow_val = g_slow_memory_ptr[new_addr];
	}

	if(old_slow_val != val) {
		g_slow_memory_ptr[new_addr] = val;
		or_pos = (new_addr >> SHIFT_PER_CHANGE) & 0x1f;
		or_val = DEP1(1, or_pos, 0);
		if((new_addr >> CHANGE_SHIFT) >= SLOW_MEM_CH_SIZE) {
			ki_printf("new_addr: %08x\n", new_addr);
			my_exit(12);
		}
		slow_mem_changed[(new_addr & 0xffff) >> CHANGE_SHIFT] |= or_val;
	}

	ptr[loc & 0xff] = val;

}
#endif

void
set_memory_io(word32 loc, int val, double *cyc_ptr)
{
	word32	tmp;

	tmp = loc & 0xfef000;
	if(tmp == 0xc000 || tmp == 0xe0c000) {
		io_write(loc, val, cyc_ptr);
		return;
	}

	/* Else it's an illegal addr */
	if(loc >= g_mem_size_total) {
		if((loc & 0xfffe) == 0) {
#if 0
			ki_printf("set_io assuming mem sizing, not halting\n");
#endif
			return;
		}
	}

	/* ignore writes to ROM */
	if((loc & 0xfc0000) == 0xfc0000) {
		return;
	}
        /* The Gog's patch : Photonix writes to E2 : ignore!!!  */
	if((loc & 0xff0000) == 0xE20000) {
		return;
	}
        /* The Gog's patch : Pom's demo write to 7F : ignore!!  */
	if((loc & 0xff0000) == 0x7F0000) {
		return;
	}

	if((loc & 0xff0000) == 0xef0000) {
		/* DOC RAM */
		doc_ram[loc & 0xffff] = val;
		return;
	}

	if(g_ignore_bad_acc) {
		/* print no message, just get out.  User doesn't want */
		/*  to be bothered by buggy programs */
		return;
	}

	if((loc & 0xffc000) == 0x00c000) {
		ki_printf("set_memory %06x = %02x, warning\n", loc, val);
		return;
	}

	halt_printf("set_memory %06x = %02x, stopping\n", loc, val);

	return;
}


#if 0
void
check_breakpoints_c(word32 loc)
{
	int	index;
	int	count;
	int	i;

	index = (loc & (MAX_BP_INDEX-1));
	count = breakpoints[index].count;
	if(count) {
		for(i = 0; i < count; i++) {
			if(loc == breakpoints[index].addrs[i]) {
				halt_printf("Write hit breakpoint %d!\n", i);
			}
		}
	}
}
#endif


void
show_regs_act(Engine_reg *eptr)
{
	int	tmp_acc, tmp_x, tmp_y, tmp_psw;
	int	kpc;
	int	direct_page, dbank;
	int	stack;
	
	kpc = eptr->kpc;
	tmp_acc = eptr->acc;
	direct_page = eptr->direct;
	dbank = eptr->dbank;
	stack = eptr->stack;

	tmp_x = eptr->xreg;
	tmp_y = eptr->yreg;

	tmp_psw = eptr->psr;

	ki_printf("  PC=%02x.%04x A=%04x X=%04x Y=%04x P=%03x",
		kpc>>16, kpc & 0xffff ,tmp_acc,tmp_x,tmp_y,tmp_psw);
	ki_printf(" S=%04x D=%04x B=%02x,cyc:%.3f\n", stack, direct_page,
		dbank, g_cur_dcycs);
}

void
show_regs()
{
	show_regs_act(&engine);
}

void
my_exit(int ret)
{
	// OG
	// Should only be used for fatal error (use halt_printf instead)
	ki_printf("FATAL ERROR (%d) : Exiting...\n",ret);
	exit(ret);
	
	/*
    sound_shutdown();
	video_shutdown();
    joystick_close();
	ki_printf("exiting\n");
	my_exit(ret);
	*/
}

extern void initzipspeed();

void
do_reset()
{
	int	i;

	statereg = 0x08 + 0x04 + 0x01; /* rdrom, lcbank2, intcx */

	wrdefram = 1;
	for(i = 1; i < 7; i++) {
		int_crom[i] = 0;
	}
	int_crom[7] = 0;

	engine.psr = (engine.psr | 0x134) & ~(0x08);
	engine.stack = 0x100 + (engine.stack & 0xff);
	engine.dbank = 0;
	engine.direct = 0;
	engine.xreg &= 0xff;
	engine.yreg &= 0xff;
	g_wait_pending = 0;


	video_reset();
	adb_reset();
	iwm_reset();
	scc_reset();
	sound_reset(g_cur_dcycs);
	setup_pageinfo();
	change_display_mode(g_cur_dcycs);

	engine.kpc = get_memory16_c(0x00fffc, 0);

	g_stepping = 0;

	// OG : Reset all the remaining irqs

#define REMOVEIRQ(ADR) { extern int ADR; if (ADR) { remove_irq(); ADR=0; } }
	
	REMOVEIRQ(g_adb_kbd_srq_sent);
	REMOVEIRQ(g_adb_data_int_sent);
	REMOVEIRQ(g_adb_mouse_int_sent);
	REMOVEIRQ(c046_vbl_irq_pending);
	REMOVEIRQ(c046_25sec_irq_pend);
	REMOVEIRQ(c023_scan_int_irq_pending);
	REMOVEIRQ(c023_1sec_int_irq_pending);
	REMOVEIRQ(c046_vbl_irq_pending);
		
	g_scan_int_events = 0;
	initialize_events();

	if (g_irq_pending) ki_printf("*** irq remainings...\n");

	 g_c023_val = 0;            // Clear the interrupt flags
     g_irq_pending = 0;    

	// ~OG

	initzipspeed();
}

#define CHECK(start, var, value, var1, var2)				\
	var2 = PTR2WORD(&(var));					\
	var1 = PTR2WORD((start));					\
	if((var2 - var1) != value) {					\
		ki_printf("CHECK: " #var " is 0x%x, but " #value " is 0x%x\n", \
			(var2 - var1), value);				\
		my_exit(5);						\
	}

void
check_engine_asm_defines()
{
	Fplus	fplus;
	Fplus	*fplusptr;
	Pc_log	pclog;
	Pc_log	*pcptr;
	Engine_reg ereg;
	Engine_reg *eptr;
	word32	val1;
	word32	val2;

	eptr = &ereg;
	CHECK(eptr, eptr->fcycles, ENGINE_FCYCLES, val1, val2);
	CHECK(eptr, eptr->fplus_ptr, ENGINE_FPLUS_PTR, val1, val2);
	CHECK(eptr, eptr->acc, ENGINE_REG_ACC, val1, val2);
	CHECK(eptr, eptr->xreg, ENGINE_REG_XREG, val1, val2);
	CHECK(eptr, eptr->yreg, ENGINE_REG_YREG, val1, val2);
	CHECK(eptr, eptr->stack, ENGINE_REG_STACK, val1, val2);
	CHECK(eptr, eptr->dbank, ENGINE_REG_DBANK, val1, val2);
	CHECK(eptr, eptr->direct, ENGINE_REG_DIRECT, val1, val2);
	CHECK(eptr, eptr->psr, ENGINE_REG_PSR, val1, val2);
	CHECK(eptr, eptr->kpc, ENGINE_REG_KPC, val1, val2);

	pcptr = &pclog;
	CHECK(pcptr, pcptr->dbank_kpc, LOG_PC_DBANK_KPC, val1, val2);
	CHECK(pcptr, pcptr->instr, LOG_PC_INSTR, val1, val2);
	CHECK(pcptr, pcptr->psr_acc, LOG_PC_PSR_ACC, val1, val2);
	CHECK(pcptr, pcptr->xreg_yreg, LOG_PC_XREG_YREG, val1, val2);
	CHECK(pcptr, pcptr->stack_direct, LOG_PC_STACK_DIRECT, val1, val2);
	if(LOG_PC_SIZE != sizeof(pclog)) {
		ki_printf("LOG_PC_SIZE: %d != sizeof=%d\n", LOG_PC_SIZE,
			(int)sizeof(pclog));
		my_exit(2);
	}

	fplusptr = &fplus;
	CHECK(fplusptr, fplusptr->plus_1, FPLUS_PLUS_1, val1, val2);
	CHECK(fplusptr, fplusptr->plus_2, FPLUS_PLUS_2, val1, val2);
	CHECK(fplusptr, fplusptr->plus_3, FPLUS_PLUS_3, val1, val2);
	CHECK(fplusptr, fplusptr->plus_x_minus_1, FPLUS_PLUS_X_M1, val1, val2);
}

// OG
void memfree_align(byte* ptr)
{
    if (*(int*)(ptr-8)!=MEMORY_MARKER) ki_alert("memory corrupted");
	free(*(void**)(ptr-4));
}
// ~OG

byte *
memalloc_align(int size, int skip_amt, void **alloc_ptr)
{
	byte	*bptr;
	word32	addr;
	word32	offset;

	skip_amt = MAX(256, skip_amt);	
	bptr = calloc(size + skip_amt + 256, 1);
	if(alloc_ptr) {
		/* Save allocation address */
		*alloc_ptr = bptr;
	}

	addr = PTR2WORD(bptr) & 0xff;

	/* must align bptr to be 256-byte aligned */
	/* this code should work even if ptrs are > 32 bits */

	offset = ((addr + skip_amt - 1) & (~0xff)) - addr;
	
	offset+=256;

	// OG : garde l'ancien pointeur pour le libérer
	*((int*)(bptr+offset-8)) = MEMORY_MARKER;
	*((void**)(bptr+offset-4)) = (void*)bptr;

	return (bptr + offset);
}

void
memory_ptr_init()
{
	word32	mem_size;

	/* This routine may be called several times--each time the ROM file */
	/*  changes this will be called */
	mem_size = MIN(0xdf0000, g_mem_size_base + g_mem_size_exp);
	g_mem_size_total = mem_size;
	if(g_memory_alloc_ptr) {
		free(g_memory_alloc_ptr);
		g_memory_alloc_ptr = 0;
	}
	g_memory_ptr = memalloc_align(mem_size, 256, &g_memory_alloc_ptr);

	ki_printf("RAM size is 0 - %06x (%.2fMB)\n", mem_size,
		(double)mem_size/(1024.0*1024.0));
}

void
memory_ptr_shut()
{
	/*if (g_memory_ptr)
		memfree_align(g_memory_ptr);*/
	g_memory_ptr = NULL;
	
}

static char g_display_env[512];
int	g_force_depth = -1;
int	g_screen_depth = 8;

void
usage(const char *progname)
{
  ki_printf("Usage: %s [OPTION]...\n",progname);
  puts("Apple IIgs emulator.\n");
  puts("  --badrd            Halt on bad reads.");
  puts("  --ignbadacc        Ignore bad memory accesses (e.g. for Appleworks GS).");
  puts("  --mem BYTES        Set memory size to BYTES.");
  puts("  --skip FRAMES      Skip FRAMES frames between screen redraws.");
  puts("  --audio DEVNUM     Use audio device type DEVNUM (0=none, 1=native, 2=SDL,");
  puts("                     3=Alib).");
  puts("  --arate RATE       Use RATE as preferred audio rate.");
  puts("  --video DEVNUM     Use video device type DEVNUM (0=none, 1=X11, 2=Win32,");
  puts("                     3=SDL).");
  puts("  --dhr140           Use simple dhires colormap.");
  puts("  --joystick DEVNUM  Use joystick device type DEVNUM (0=none, 1=mouse, 2=linux,");
  puts("                     3=keypad, 4=Win32, 5=SDL).");
  puts("  --verbose MODE     Set verbose mode to MODE (obtained by or-ing the following");
  puts("                     values: 0x001=DISK, 0x002=IRQ, 0x004=CLK, 0x008=SHADOW,");
  puts("                     0x010=IWM, 0x020=DOC, 0x040=ADB, 0x080=SCC, 0x100=TEST,");
  puts("                     0x200=VIDEO).");
}


int
main(int argc, char *argv[])
{
	int	skip_amt;
	int	diff;
	int	tmp1;
	int	i;
    int audio_devtype = SOUND_DEFAULT;
    int video_devtype = VIDEO_DEFAULT;
    
    #ifdef USE_CONFIG_FILE
    if (argc > 1)
    {
		sprintf(fileconfig, "%s", argv[1]);
	}
	#endif

	/* parse args */
	for(i = 1; i < argc; i++) {
		if(!strcmp("--badrd", argv[i])) {
			ki_printf("Halting on bad reads\n");
			g_halt_on_bad_read = 1;
		} else if(!strcmp("--ignbadacc", argv[i])) {
			ki_printf("Ignoring bad memory accesses\n");
			g_ignore_bad_acc = 1;
		} else if(!strcmp("--mem", argv[i])) {
			if((i+1) >= argc) {
				ki_printf("Missing argument\n");
				my_exit(1);
			}
			g_mem_size_exp = strtol(argv[i+1], 0, 0) & 0x00ff0000;
			ki_printf("Using %d as memory size\n", g_mem_size_exp);
			i++;
		} else if(!strcmp("--skip", argv[i])) {
			if((i+1) >= argc) {
				ki_printf("Missing argument\n");
				my_exit(1);
			}
			skip_amt = strtol(argv[i+1], 0, 0);
			ki_printf("Using %d as skip_amt\n", skip_amt);
			g_screen_redraw_skip_amt = skip_amt;
			i++;
		} else if(!strcmp("--audio", argv[i])) {
			if((i+1) >= argc) {
				ki_printf("Missing argument\n");
				my_exit(1);
			}
			tmp1 = strtol(argv[i+1], 0, 0);
			ki_printf("Using %d as audio device type\n", tmp1);
			audio_devtype = tmp1;
			i++;
		} else if(!strcmp("--video", argv[i])) {
			if((i+1) >= argc) {
				ki_printf("Missing argument\n");
				my_exit(1);
			}
			tmp1 = strtol(argv[i+1], 0, 0);
			ki_printf("Using %d as video device type\n", tmp1);
			video_devtype = tmp1;
			i++;
		} else if(!strcmp("--arate", argv[i])) {
			if((i+1) >= argc) {
				ki_printf("Missing argument\n");
				my_exit(1);
			}
			tmp1 = strtol(argv[i+1], 0, 0);
			ki_printf("Using %d as preferred audio rate\n", tmp1);
			g_preferred_rate = tmp1;
			i++;
		} else if(!strcmp("-v", argv[i]) || !strcmp("--verbose", argv[i])) {
			if((i+1) >= argc) {
				ki_printf("Missing argument\n");
				my_exit(1);
			}
			tmp1 = strtol(argv[i+1], 0, 0);
			ki_printf("Setting Verbose = 0x%03x\n", tmp1);
			Verbose = tmp1;
			i++;
#ifndef __NeXT__
		} else if(!strcmp("--display", argv[i])) {
			if((i+1) >= argc) {
				ki_printf("Missing argument\n");
				my_exit(1);
			}
			ki_printf("Using %s as display\n", argv[i+1]);
			sprintf(g_display_env, "DISPLAY=%s", argv[i+1]);
			putenv(&g_display_env[0]);
			i++;
#endif
		} else if(!strcmp("-joystick", argv[i])) {
			if((i+1) >= argc) {
				ki_printf("Missing argument\n");
				my_exit(1);
			}
			tmp1 = strtol(argv[i+1], 0, 0);
			ki_printf("Using %d as joystick type\n", tmp1);
			g_joystick_type = tmp1;
		} else if(!strcmp("-dhr140", argv[i])) {
			ki_printf("Using simple dhires color map\n");
			g_use_dhr140 = 1;
		}
	}
    ki_printf("OK\n");
    fprintf(stderr,"ERR\n");
    fprintf(stdout,"OK2\n");

	check_engine_asm_defines();
	fixed_memory_ptrs_init();

	if(sizeof(word32) != 4) {
		ki_printf("sizeof(word32) = %d, must be 4!\n",
							(int)sizeof(word32));
		my_exit(1);
	}

	if(!g_engine_c_mode) {
		diff = &defs_instr_end_8 - &defs_instr_start_8;
		if(diff != 1) {
			ki_printf("defs_instr_end_8 - start is %d\n",diff);
			my_exit(1);
		}

		diff = &defs_instr_end_16 - &defs_instr_start_16;
		if(diff != 1) {
			ki_printf("defs_instr_end_16 - start is %d\n", diff);
			my_exit(1);
		}

		diff = &op_routs_end - &op_routs_start;
		if(diff != 1) {
			ki_printf("op_routs_end - start is %d\n", diff);
			my_exit(1);
		}
	}

	iwm_init();

	load_roms_init_memory();

	init_reg();
	clear_halt();

	initialize_events();

	video_init(video_devtype);

	sound_init(audio_devtype);

	scc_init();
	setup_bram();		/* load_roms must be called first! */
	adb_init();
	joystick_init();
#ifndef DISABLE_CONFFILE
	configuration_load(1);
#endif

        do_reset();
	g_stepping = 0;
	do_go();

	/* If we get here, we hit a breakpoint, call debug intfc */
	if (halt_sim&HALT_WANTTOBRK)
		do_debug_intfc();


	do_reset();	// to clear interrupt

    sound_shutdown();
	video_shutdown();
    joystick_close();

	// OG
	fixed_memory_ptrs_shut();
	sim65816_shut(); 
	iwm_shut();
	clock_shut();
	moremem_shut();
	memory_ptr_shut();

	return 0;
}

static void
load_roms_init_memory()
{
	load_roms();
	memory_ptr_init();
	/* clk_setup_bram_version(); */	/* Must be after config_load_roms */
#if 0
	if(g_rom_version >= 3) {
		g_c036_val_speed |= 0x40;	/* set power-on bit */
	} else {
		g_c036_val_speed &= (~0x40);	/* clear the bit */
	}
#endif
	do_reset();

	/* if user booted ROM 01, switches to ROM 03, then switches back */
	/*  to ROM 01, then the reset routines call to Tool $0102 looks */
	/*  at uninitialized $e1/15fe and if it is negative it will JMP */
	/*  through $e1/1688 which ROM 03 left pointing to fc/0199 */
	/* So set e1/15fe = 0 */
	set_memory16_c(0xe115fe, 0, 0);
}

void
setup_kegs_file(char *outname, int maxlen, int ok_if_missing,
			const char **name_ptr)
{
	struct stat stat_buf;
	const char **path_ptr;
	const char **cur_name_ptr, **save_path_ptr;
	int	ret;

	outname[0] = 0;
	
#ifndef _TINSPIRE
	char home[128];
	snprintf(home, sizeof(home), "%s/.kegs", getenv("HOME"));
	sprintf(outname, "%s/.kegs/%s", getenv("HOME"), *name_ptr);
	if(access( home, F_OK ) == -1) 
	{
		mkdir(home, 0755);	
	}
	ret = stat(outname, &stat_buf);
	if(ret == 0) 
	{
		/* got it! */
		return;
	}
#else

	path_ptr = &g_kegs_default_paths[0];

	save_path_ptr = path_ptr;
	while(*path_ptr) {
		cur_name_ptr = name_ptr;
		while(*cur_name_ptr) {
			sprintf(outname, "%s%s", *path_ptr, *cur_name_ptr);
			ki_printf("Trying =%s=\n", outname);
			ret = stat(outname, &stat_buf);
			if(ret == 0) {
				/* got it! */
				return;
			}
			cur_name_ptr++;
		}
		path_ptr++;
	}
#endif

	if(ok_if_missing) {
		outname[0] = 0;
		return;
	}

	/* couldn't find it, print out all the attempts */
	path_ptr = save_path_ptr;
	ki_printf("Could not find %s in any of these directories:\n", *name_ptr);
	ki_printf("outname %s\n", outname);
	/*while(*path_ptr) {
		ki_printf("  %s\n", *path_ptr++);
	}*/

	my_exit(2);
}

Event g_event_list[MAX_EVENTS];
Event g_event_free;
Event g_event_start;

void
initialize_events()
{
	int	i;

	for(i = 1; i < MAX_EVENTS; i++) {
		g_event_list[i-1].next = &g_event_list[i];
	}
	g_event_free.next = &g_event_list[0];
	g_event_list[MAX_EVENTS-1].next = 0;

	g_event_start.next = 0;
	g_event_start.dcycs = 0.0;

	// OG add even entry since the first events
	add_event_entry(g_cur_dcycs+DCYCS_IN_16MS, EV_60HZ);
}

void
check_for_one_event_type(int type)
{
	Event	*ptr;
	int	count;
	int	depth;

	count = 0;
	depth = 0;
	ptr = g_event_start.next;
	while(ptr != 0) {
		depth++;
		if(ptr->type == type) {
			count++;
			if(count != 1) {
				halt_printf("in check_for_1, type %d found at "
					"depth: %d, count: %d, at %f\n",
					type, depth, count, ptr->dcycs);
			}
		}
		ptr = ptr->next;
	}
}


void
add_event_entry(double dcycs, int type)
{
	Event	*this_event;
	Event	*ptr, *prev_ptr;
	int	tmp_type;
	int	done;

	this_event = g_event_free.next;
	if(this_event == 0) {
		halt_printf("Out of queue entries!\n");
		show_all_events();
		return;
	}
	g_event_free.next = this_event->next;

	this_event->type = type;

	tmp_type = type & 0xff;
	if((dcycs < 0.0) || (dcycs > (g_cur_dcycs + 50*1000*1000.0)) ||
			((dcycs < g_cur_dcycs) && (tmp_type != EV_SCAN_INT))) {
		halt_printf("add_event: dcycs: %f, type:%05x, cur_dcycs: %f!\n",
			dcycs, type, g_cur_dcycs);
		dcycs = g_cur_dcycs + 1000.0;
	}

	ptr = g_event_start.next;
	if(ptr && (dcycs < ptr->dcycs)) {
		/* create event before next expected event */
		/* do this by setting HALT_EVENT */
		set_halt(HALT_EVENT);
	}

	prev_ptr = &g_event_start;
	ptr = g_event_start.next;

	done = 0;
	while(!done) {
		if(ptr == 0) {
			this_event->next = ptr;
			this_event->dcycs = dcycs;
			prev_ptr->next = this_event;
			return;
		} else {
			if(ptr->dcycs < dcycs) {
				/* step across this guy */
				prev_ptr = ptr;
				ptr = ptr->next;
			} else {
				/* go in front of this guy */
				this_event->dcycs = dcycs;
				this_event->next = ptr;
				prev_ptr->next = this_event;
				return;
			}
		}
	}
}

double
remove_event_entry(int type)
{
	Event	*ptr, *prev_ptr;
	Event	*next_ptr;

	ptr = g_event_start.next;
	prev_ptr = &g_event_start;

	while(ptr != 0) {
		if((ptr->type & 0xffff) == type) {
			/* got it, remove it */
			next_ptr = ptr->next;
			prev_ptr->next = next_ptr;

			/* Add ptr to free list */
			ptr->next = g_event_free.next;
			g_event_free.next = ptr;

			return ptr->dcycs;
		}
		prev_ptr = ptr;
		ptr = ptr->next;
	}

	halt_printf("remove event_entry: %08x, but not found!\n", type);
	if((type & 0xff) == EV_DOC_INT) {
		ki_printf("DOC, g_doc_saved_ctl = %02x\n", g_doc_saved_ctl);
	}
#ifdef HPUX
	U_STACK_TRACE();
#endif
	show_all_events();

	return 0.0;
}

void
add_event_stop(double dcycs)
{
	add_event_entry(dcycs, EV_STOP);
}

void
add_event_doc(double dcycs, int osc)
{
	if(dcycs < g_cur_dcycs) {
		dcycs = g_cur_dcycs;
#if 0
		halt_printf("add_event_doc: dcycs: %f, cur_dcycs: %f\n",
			dcycs, g_cur_dcycs);
#endif
	}

	add_event_entry(dcycs, EV_DOC_INT + (osc << 8));
}

void
add_event_scc(double dcycs, int type)
{
	if(dcycs < g_cur_dcycs) {
		dcycs = g_cur_dcycs;
	}

	add_event_entry(dcycs, EV_SCC + (type << 8));
}

void
add_event_vbl()
{
	double	dcycs;

	dcycs = g_last_vbl_dcycs + (DCYCS_IN_16MS * (192.0/262.0));
	add_event_entry(dcycs, EV_VBL_INT);
}

double
remove_event_doc(int osc)
{
	return remove_event_entry(EV_DOC_INT + (osc << 8));
}

#if 0
double
remove_event_scc(int type)
{
	return remove_event_entry(EV_SCC + (type << 8));
}
#endif

void
show_all_events()
{
	Event	*ptr;
	int	count;
	double	dcycs;

	count = 0;
	ptr = g_event_start.next;
	while(ptr != 0) {
		dcycs = ptr->dcycs;
		ki_printf("Event: %02x: type: %05x, dcycs: %f (%f)\n",
			count, ptr->type, dcycs, dcycs - g_cur_dcycs);
		ptr = ptr->next;
		count++;
	}

}

word32	g_vbl_count = 0;
int	g_vbl_index_count = 0;
double	dtime_array[60];
double	g_dadjcycs_array[60];
double	g_dtime_diff3_array[60];
double	g_dtime_this_vbl_array[60];
double	g_dtime_exp_array[60];
double	g_dtime_pmhz_array[60];
double	g_dtime_eff_pmhz_array[60];
int	speed_changed = 0;
int	g_limit_speed = SPEED_GS;
double	sim_time[60];
double	g_sim_sum = 0.0;

double	g_cur_sim_dtime = 0.0;
double	g_projected_pmhz = 1.0;

Fplus	g_recip_projected_pmhz_slow;
Fplus	g_recip_projected_pmhz_fast;
Fplus	g_recip_projected_pmhz_unl;
Fplus	g_recip_projected_pmhz_zip;
double	g_speed_zip_mult;
Fplus	*g_cur_fplus_ptr = 0;

void
show_pmhz()
{
	ki_printf("Pmhz: %f, plus_1: %f, fast: %d, limit: %d\n",
		g_projected_pmhz, g_cur_fplus_ptr->plus_1, speed_fast,
		g_limit_speed);

}

double	fspeed_mult;

void
run_prog()
{
	Fplus	*fplus_ptr;
	Event	*this_event;
	Event	*db1;
	double	dcycs;
	double	now_dtime;
	double	prev_dtime;
	double	prerun_fcycles;
/*	double	fspeed_mult; */
	double	fcycles_stop;
	word32	ret;
	int	type;
	int	motor_on;
	int	iwm_1;
	int	iwm_25;
	int	limit_speed;
	int	apple35_sel;
	int	fast;
	int	this_type;


	fflush(stdout);

	g_cur_sim_dtime = 0.0;

	g_recip_projected_pmhz_slow.plus_1 = 1.0;
	g_recip_projected_pmhz_slow.plus_2 = 2.0;
	g_recip_projected_pmhz_slow.plus_3 = 3.0;
	g_recip_projected_pmhz_slow.plus_x_minus_1 = 0.9;

	g_recip_projected_pmhz_fast.plus_1 = (1.0 / 2.5);
	g_recip_projected_pmhz_fast.plus_2 = (2.0 / 2.5);
	g_recip_projected_pmhz_fast.plus_3 = (3.0 / 2.5);
	g_recip_projected_pmhz_fast.plus_x_minus_1 = (1.98 - (1.0/2.5));

	if(g_cur_fplus_ptr == 0) {
		g_recip_projected_pmhz_unl = g_recip_projected_pmhz_slow;
	}

	while(1) {
		fflush(stdout);

		if(g_irq_pending && !(engine.psr & 0x4)) {
			irq_printf("taking an irq!\n");
			take_irq(0);
			/* Interrupt! */
		}


		motor_on = g_iwm_motor_on;
		limit_speed = g_limit_speed;
		apple35_sel = g_apple35_sel;
		fast = speed_fast;

		iwm_1 = motor_on && !apple35_sel &&
				(g_slot_motor_detect & 0x4) &&
				(g_slow_525_emul_wr || !g_fast_disk_emul);
		iwm_25 = (motor_on && apple35_sel) && !g_fast_disk_emul;
                if(fast && (!iwm_1 && !iwm_25) && (limit_speed == 0)) {
			/* unlimited speed */
			fspeed_mult = g_projected_pmhz;
			fplus_ptr = &g_recip_projected_pmhz_unl;
		} else if(fast && (iwm_25 || (!iwm_1 && limit_speed == 2)) ) {
			/* else original speed */
                        fspeed_mult = 2.6;
                        fplus_ptr = &g_recip_projected_pmhz_fast;
                } else if(fast && (iwm_25 || (!iwm_1 && limit_speed == 3)) ) {
                        /* else ZipGS speed */
                        fspeed_mult = g_speed_zip_mult;
                        fplus_ptr = &g_recip_projected_pmhz_zip;
			
		} else {
			/* else run slow */
			fspeed_mult = 1.0;
			fplus_ptr = &g_recip_projected_pmhz_slow;
		}

		g_cur_fplus_ptr = fplus_ptr;
		engine.fplus_ptr = fplus_ptr;

		this_type = g_event_start.next->type;

		prerun_fcycles = g_cur_dcycs - g_last_vbl_dcycs;
		engine.fcycles = prerun_fcycles;
		fcycles_stop = (g_event_start.next->dcycs - g_last_vbl_dcycs) +
							0.001;
		if(g_stepping) {
			fcycles_stop = prerun_fcycles;
		}
		g_fcycles_stop = fcycles_stop;

#if 0
		ki_printf("Enter engine, fcycs: %f, stop: %f\n",
			prerun_fcycles, fcycles_stop);
		ki_printf("g_cur_dcycs: %f, last_vbl_dcyc: %f\n", g_cur_dcycs,
			g_last_vbl_dcycs);
#endif

		g_num_enter_engine++;
		prev_dtime = get_dtime();

		ret = enter_engine(&engine);

		now_dtime = get_dtime();

        /* XXX patch from kegs32. useful? */
        /*
        if (now_dtime < prev_dtime) {
            ki_printf ("Wrap around time\n");
            prev_dtime = prev_dtime-(~(int)0);
        }
        */
        /* XXX */

		g_cur_sim_dtime += (now_dtime - prev_dtime);

		dcycs = g_last_vbl_dcycs + (double)(engine.fcycles);

		g_dadjcycs += (engine.fcycles - prerun_fcycles) *
					fspeed_mult;

#if 0
		ki_printf("...back, engine.fcycles: %f, dcycs: %f\n",
			(double)engine.fcycles, dcycs);
#endif

		g_cur_dcycs = dcycs;

		if(ret != 0) {
			g_engine_action++;
			handle_action(ret);
		}

		if(halt_sim == HALT_EVENT) {
			g_engine_halt_event++;
			/* if we needed to stop to check for interrupts, */
			/*  clear halt */
			halt_sim = 0;
		}

#if 0
		if(!g_testing && run_cycles < -2000000) {
			halt_printf("run_cycles: %d, cycles: %d\n", run_cycles,
								cycles);
			ki_printf("this_type: %05x\n", this_type);
			ki_printf("duff_cycles: %d\n", duff_cycles);
			ki_printf("start.next->rel_time: %d, type: %05x\n",
				g_event_start.next->rel_time,
				g_event_start.next->type);
		}
#endif

		this_event = g_event_start.next;
		while(dcycs >= this_event->dcycs) {
			/* Pop this guy off of the queue */
			g_event_start.next = this_event->next;

			type = this_event->type;
			this_event->next = g_event_free.next;
			g_event_free.next = this_event;
			switch(type & 0xff) {
			case EV_60HZ:
				update_60hz(dcycs, now_dtime);
				break;
			case EV_STOP:
				ki_printf("type: EV_STOP\n");
				ki_printf("next: %p, dcycs: %f\n",
						g_event_start.next, dcycs);
				db1 = g_event_start.next;
				halt_printf("next.dcycs: %f\n", db1->dcycs);
				break;
			case EV_SCAN_INT:
				g_engine_scan_int++;
				irq_printf("type: scan int\n");
				do_scan_int(dcycs, type >> 8);
				break;
			case EV_DOC_INT:
				g_engine_doc_int++;
				doc_handle_event(type >> 8, dcycs);
				break;
			case EV_VBL_INT:
				do_vbl_int();
				break;
			case EV_SCC:
				do_scc_event(type >> 8, dcycs);
				break;
			default:
				ki_printf("Unknown event: %d!\n", type);
				my_exit(3);
			}

			this_event = g_event_start.next;

		}

		if(g_event_start.next == 0) {
			halt_printf("ERROR...run_prog, event_start.n=0!\n");
		}

#if 0
		if(!g_testing && g_event_start.next->rel_time > 2000000) {
			ki_printf("Z:start.next->rel_time: %d, duff_cycles: %d\n",
				g_event_start.next->rel_time, duff_cycles);
			halt_printf("Zrun_cycles:%d, cycles:%d\n", run_cycles,
						cycles);

			show_all_events();
		}
#endif

		if(halt_sim != 0 && halt_sim != HALT_EVENT) {
			break;
		}
		if(g_stepping) {
			break;
		}
	}

	if(!g_testing) {
		ki_printf("leaving run_prog, halt_sim:%d\n", halt_sim);
	}
    /*set_warp_pointer(0);
    if(get_fullscreen())
	set_fullscreen(0);*/
	
    video_auto_repeat_on(0);
}

void
add_irq()
{
	g_irq_pending++;
	set_halt(HALT_EVENT);
}

void
remove_irq()
{
	g_irq_pending--;
	if(g_irq_pending < 0) {
		halt_printf("remove_irq: g_irq_pending: %d\n", g_irq_pending);
	}
}

void
take_irq(int is_it_brk)
{
	word32	new_kpc;
	word32	va;

	irq_printf("Taking irq, at: %02x/%04x, psw: %02x, dcycs: %f\n",
			engine.kpc>>16, engine.kpc & 0xffff, engine.psr,
			g_cur_dcycs);

	g_num_irq++;
	if(g_wait_pending) {
		/* step over WAI instruction */
		engine.kpc++;
		g_wait_pending = 0;
	}

	if(g_irq_pending < 0) {
		halt_printf("g_irq_pending: %d!\n", g_irq_pending);
	}

	if(engine.psr & 0x100) {
		/* Emulation */
		set_memory_c(engine.stack, (engine.kpc >> 8) & 0xff, 0);
		engine.stack = ((engine.stack -1) & 0xff) + 0x100;

		set_memory_c(engine.stack, engine.kpc & 0xff, 0);
		engine.stack = ((engine.stack -1) & 0xff) + 0x100;

		set_memory_c(engine.stack,
					(engine.psr & 0xef)|(is_it_brk<<4),0);
			/* Clear B bit in psr on stack */
		engine.stack = ((engine.stack -1) & 0xff) + 0x100;

		va = 0xfffffe;
		if(shadow_reg & 0x40) {
			/* I/O shadowing off...use ram locs */
			va = 0x00fffe;
		}

	} else {
		/* native */
		set_memory_c(engine.stack, (engine.kpc >> 16) & 0xff, 0);
		engine.stack = ((engine.stack -1) & 0xffff);

		set_memory_c(engine.stack, (engine.kpc >> 8) & 0xff, 0);
		engine.stack = ((engine.stack -1) & 0xffff);

		set_memory_c(engine.stack, engine.kpc & 0xff, 0);
		engine.stack = ((engine.stack -1) & 0xffff);

		set_memory_c(engine.stack, engine.psr & 0xff, 0);
		engine.stack = ((engine.stack -1) & 0xffff);

		if(is_it_brk) {
			/* break */
			va = 0xffffe6;
			if(shadow_reg & 0x40) {
				va = 0xffe6;
			}
		} else {
			/* irq */
			va = 0xffffee;
			if(shadow_reg & 0x40) {
				va = 0xffee;
			}
		}

	}

	new_kpc = get_memory_c(va, 0);
	new_kpc = new_kpc + (get_memory_c(va+1, 0) << 8);

	engine.psr = ((engine.psr & 0x1f3) | 0x4);

	engine.kpc = new_kpc;
	HALT_ON(HALT_ON_IRQ, "Halting on IRQ\n");

}



void
show_dtime_array()
{
	double	dfirst_time;
	double	first_total_cycs;
	int	i;
	int	pos;

	dfirst_time = 0.0;
	first_total_cycs = 0.0;


	for(i = 0; i < 60; i++) {
		pos = (g_vbl_index_count + i) % 60;
		ki_printf("%2d:%2d dt:%.5f adjc:%9.1f this_vbl:%.6f "
			"exp:%.5f p:%2.2f ep:%2.2f\n",
			i, pos,
			dtime_array[pos] - dfirst_time,
			g_dadjcycs_array[pos] - first_total_cycs,
			g_dtime_this_vbl_array[pos],
			g_dtime_exp_array[pos] - dfirst_time,
			g_dtime_pmhz_array[pos],
			g_dtime_eff_pmhz_array[pos]);
		dfirst_time = dtime_array[pos];
		first_total_cycs = g_dadjcycs_array[pos];
	}
}

void
update_60hz(double dcycs, double dtime_now)
{
	char	status_buf[1024];
	char	sim_mhz_buf[128];
	char	total_mhz_buf[128];
	char	*sim_mhz_ptr, *total_mhz_ptr;
	double	eff_pmhz;
	double	planned_dcycs;
	double	predicted_pmhz;
	double	recip_predicted_pmhz;
	double	dtime_this_vbl_sim;
	double	dtime_diff_1sec;
	double	dratio;
	double	dtime_till_expected;
	double	dtime_diff;
	double	dtime_this_vbl;
	double	dadjcycs_this_vbl;
	double	dadj_cycles_1sec;
	int	tmp;
	int	cycs_int;
	int	cur_vbl_index;
	int	prev_vbl_index;

	g_vbl_count++;

	/* NOTE: this event is defined to occur before line 0 */
	/* It's actually happening at the start of the border for line (-1) */
	/* All other timings should be adjusted for this */

	irq_printf("vbl_60hz: vbl: %d, dcycs: %f, last_vbl_dcycs: %f\n",
		g_vbl_count, dcycs, g_last_vbl_dcycs);

	planned_dcycs = DCYCS_IN_16MS;

	g_last_vbl_dcycs = g_last_vbl_dcycs + planned_dcycs;

	add_event_entry(g_last_vbl_dcycs + planned_dcycs, EV_60HZ);
	check_for_one_event_type(EV_60HZ);

	cur_vbl_index = g_vbl_index_count;

	/* figure out dtime spent running SIM, not all the overhead */
	dtime_this_vbl_sim = g_cur_sim_dtime;
	g_cur_sim_dtime = 0.0;
	g_sim_sum = g_sim_sum - sim_time[cur_vbl_index] + dtime_this_vbl_sim;
	sim_time[cur_vbl_index] = dtime_this_vbl_sim;

	dadj_cycles_1sec = g_dadjcycs - g_dadjcycs_array[cur_vbl_index];

	/* dtime_diff_1sec is dtime total spent over the last 60 ticks */
	dtime_diff_1sec = dtime_now - dtime_array[cur_vbl_index];

	dtime_array[cur_vbl_index] = dtime_now;
	g_dadjcycs_array[cur_vbl_index] = g_dadjcycs;

	prev_vbl_index = cur_vbl_index;
	cur_vbl_index = prev_vbl_index + 1;
	if(cur_vbl_index >= 60) {
		cur_vbl_index = 0;
	}
	g_vbl_index_count = cur_vbl_index;

	if(prev_vbl_index == 0) {
		if(g_sim_sum < (1.0/190.0)) {
			sim_mhz_ptr = "???";
		} else {
			sprintf(sim_mhz_buf, "%2.2f",
				(dadj_cycles_1sec / g_sim_sum) /
							(1000.0*1000.0));
			sim_mhz_ptr = sim_mhz_buf;
		}
		if(dtime_diff_1sec < (1.0/128.0)) {
			total_mhz_ptr = "???";
		} else {
			sprintf(total_mhz_buf, "%2.2f",
				(dadj_cycles_1sec / dtime_diff_1sec) /
								(1000000.0));
			total_mhz_ptr = total_mhz_buf;
		}
		cycs_int = (word32)dadj_cycles_1sec;
		sprintf(status_buf, "dcycs:%13.1f sim MHz:%s "
			"Eff MHz:%s, c:%06x, sec:%1.3f vol:%02x pal:%x",
			dcycs, sim_mhz_ptr, total_mhz_ptr, cycs_int,
			dtime_diff_1sec, g_doc_vol, g_a2vid_palette);
		update_status_line(0, status_buf);

		sprintf(status_buf, "xfer:%08x, xred_cs:%03x_%05x, "
			"ch_in:%08x ref_l:%08x ref_x:%08x",
			g_refresh_bytes_xfer,
			g_cycs_in_xredraw >> 20, g_cycs_in_xredraw & 0xfffff,
			g_cycs_in_check_input, g_cycs_in_refresh_line,
			g_cycs_in_refresh_video_image);
		update_status_line(1, status_buf);

		sprintf(status_buf, "Ints:%3d I/O:%4dK BRK:%3d COP:%2d "
			"Eng:%3d act:%3d hev:%3d esi:%3d edi:%3d",
			g_num_irq, g_io_amt>>10, g_num_brk, g_num_cop,
			g_num_enter_engine, g_engine_action,
			g_engine_halt_event, g_engine_scan_int,
			g_engine_doc_int);
		update_status_line(2, status_buf);

		sprintf(status_buf, "snd1:%03x_%05x, 2:%03x_%05x, "
			"3:%03x_%05x, st:%03x_%05x est:%03x_%05x %4.2f",
			g_cycs_in_sound1 >> 20, g_cycs_in_sound1 & 0xfffff,
			g_cycs_in_sound2 >> 20, g_cycs_in_sound2 & 0xfffff,
			g_cycs_in_sound3 >> 20, g_cycs_in_sound3 & 0xfffff,
			g_cycs_in_start_sound>>20,g_cycs_in_start_sound&0xfffff,
			g_cycs_in_est_sound>>20, g_cycs_in_est_sound & 0xfffff,
			g_fvoices);
		update_status_line(3, status_buf);

		sprintf(status_buf, "snd_plays: %4d, doc_ev: %4d, st_snd: %4d "
			"scan_osc: %4d, snd_parms: %4d",
			g_num_snd_plays, g_num_doc_events, g_num_start_sounds,
			g_num_scan_osc, g_num_recalc_snd_parms);
		update_status_line(4, status_buf);

		draw_iwm_status(5, status_buf);

		update_status_line(6, "KEGS v0.64");

		g_status_refresh_needed = 1;

		g_num_irq = 0;
		g_num_brk = 0;
		g_num_cop = 0;
		g_num_enter_engine = 0;
		g_io_amt = 0;
		g_engine_action = 0;
		g_engine_halt_event = 0;
		g_engine_scan_int = 0;
		g_engine_doc_int = 0;

		g_cycs_in_40col = 0;
		g_cycs_in_xredraw = 0;
		g_cycs_in_check_input = 0;
		g_cycs_in_refresh_line = 0;
		g_cycs_in_refresh_video_image = 0;
		g_cycs_in_io_read = 0;
		g_cycs_in_sound1 = 0;
		g_cycs_in_sound2 = 0;
		g_cycs_in_sound3 = 0;
		g_cycs_in_sound4 = 0;
		g_cycs_in_start_sound = 0;
		g_cycs_in_est_sound = 0;
		g_refresh_bytes_xfer = 0;

		g_num_snd_plays = 0;
		g_num_doc_events = 0;
		g_num_start_sounds = 0;
		g_num_scan_osc = 0;
		g_num_recalc_snd_parms = 0;

		g_fvoices = (float)0.0;
	}

	dtime_this_vbl = dtime_now - g_dtime_last_vbl;
	if(dtime_this_vbl < 0.001) {
		dtime_this_vbl = 0.001;
	}

	g_dtime_last_vbl = dtime_now;

	dadjcycs_this_vbl = g_dadjcycs - g_last_vbl_dadjcycs;
	g_last_vbl_dadjcycs = g_dadjcycs;

	g_dtime_expected += (1.0/60.0);

	eff_pmhz = ((dadjcycs_this_vbl) / (dtime_this_vbl)) /
							DCYCS_1_MHZ;

	/* using eff_pmhz, predict how many cycles can be run by */
	/*   g_dtime_expected */

	dtime_till_expected = g_dtime_expected - dtime_now;

	dratio = 60.0 * dtime_till_expected;

	predicted_pmhz = eff_pmhz * dratio;

	if(! (predicted_pmhz < (1.4 * g_projected_pmhz))) {
		predicted_pmhz = 1.4 * g_projected_pmhz;
	}

	if(! (predicted_pmhz > (0.7 * g_projected_pmhz))) {
		predicted_pmhz = 0.7 * g_projected_pmhz;
	}

	if(!(predicted_pmhz >= 1.0)) {
		irq_printf("predicted: %f, setting to 1.0\n", predicted_pmhz);
		predicted_pmhz = 1.0;
	}

	if(!(predicted_pmhz < 150.0)) {
		irq_printf("predicted: %f, setting to 90.0\n", predicted_pmhz);
		predicted_pmhz = 90.0;
	}

	recip_predicted_pmhz = 1.0/predicted_pmhz;
	g_projected_pmhz = predicted_pmhz;

	g_recip_projected_pmhz_unl.plus_1 = 1.0*recip_predicted_pmhz;
	g_recip_projected_pmhz_unl.plus_2 = 2.0*recip_predicted_pmhz;
	g_recip_projected_pmhz_unl.plus_3 = 3.0*recip_predicted_pmhz;
	g_recip_projected_pmhz_unl.plus_x_minus_1 = 1.01 - recip_predicted_pmhz;

	if(dtime_till_expected < -0.125) {
		/* If we were way off, get back on track */
		/* this happens because our sim took much longer than */
		/* expected, so we're going to skip some VBL */
		irq_printf("adj1: dtexp:%f, dt_new:%f\n",
			g_dtime_expected, dtime_now);

		dtime_diff = -dtime_till_expected;

		irq_printf("dtime_till_exp: %f, dtime_diff: %f, dcycs: %f\n",
			dtime_till_expected, dtime_diff, dcycs);

		g_dtime_expected += dtime_diff;
	}

	if(dtime_till_expected > (3/60.0)) {
		/* we're running fast, usleep */
		micro_sleep(dtime_till_expected - (1/60.0));
	}

	g_dtime_this_vbl_array[prev_vbl_index] = dtime_this_vbl;
	g_dtime_exp_array[prev_vbl_index] = g_dtime_expected;
	g_dtime_pmhz_array[prev_vbl_index] = predicted_pmhz;
	g_dtime_eff_pmhz_array[prev_vbl_index] = eff_pmhz;


	if(c041_en_vbl_ints) {
		add_event_vbl();
	}

	g_25sec_cntr++;
	if(g_25sec_cntr >= 16) {
		g_25sec_cntr = 0;
		if(c041_en_25sec_ints && !c046_25sec_irq_pend) {
			g_c046_val |= 0x10;
			c046_25sec_irq_pend = 1;
			add_irq();
			irq_printf("Setting c046 .25 sec int to 1, "
				"g_irq_pend: %d\n", g_irq_pending);
		}
	}

	g_1sec_cntr++;
	if(g_1sec_cntr >= 60) {
		g_1sec_cntr = 0;
		tmp = g_c023_val;
		tmp |= 0x40;	/* set 1sec int */
		if((tmp & 0x04) && !c023_1sec_int_irq_pending) {
			c023_1sec_int_irq_pending = 1;
			tmp |= 0x80;
			add_irq();
			irq_printf("Setting c023 to %02x irq_pend: %d\n",
				tmp, g_irq_pending);
		}
		g_c023_val = tmp;
	}

	if(!g_scan_int_events) {
		check_scan_line_int(dcycs, 0);
	}

	iwm_vbl_update();
	video_update();
	sound_update(dcycs);
	clock_update();
	scc_update(dcycs);
}

void
do_vbl_int()
{
	if(c041_en_vbl_ints && !c046_vbl_irq_pending) {
		g_c046_val |= 0x08;
		c046_vbl_irq_pending = 1;
		add_irq();
		irq_printf("Setting c046 vbl_int_status to 1, irq_pend: %d\n",
			g_irq_pending);
	}
}


void
do_scan_int(double dcycs, int line)
{
	int	c023_val;
	g_scan_int_events = 0;

	c023_val = g_c023_val;
	if(c023_val & 0x20) {
		halt_printf("c023 scan_int and another on line %03x\n", line);
	}

	/* make sure scan int is still enabled for this line */
	if(g_slow_memory_ptr[0x19d00 + line] & 0x40) {
		/* valid interrupt, do it */
		c023_val |= 0xa0;	/* vgc_int and scan_int */
		if((c023_val & 0x02) && !c023_scan_int_irq_pending) {
			add_irq();
			c023_scan_int_irq_pending = 1;
			irq_printf("Setting c023 to %02x, irq_pend: %d\n",
				c023_val, g_irq_pending);
		}
		g_c023_val = c023_val;
		HALT_ON(HALT_ON_SCAN_INT, "In do_scan_int\n");
	} else {
		/* scan int bit cleared on scan line control byte */
		/* look for next line, if any */
		check_scan_line_int(dcycs, line+1);
	}
}


void
check_scan_line_int(double dcycs, int cur_video_line)
{
	int	delay;
	int	start;
	int	line;
	int	i;
	/* Called during VBL interrupt phase */

	if(!(g_cur_a2_stat & ALL_STAT_SUPER_HIRES)) {
		return;
	}

	if(g_c023_val & 0x20) {
		/* don't check for any more */
		return;
	}

	start = cur_video_line;
	if(start < 0) {
		halt_printf("check_scan_line_int: cur_video_line: %d\n",
			cur_video_line);
		start = 0;
	}
	
	for(line = start; line < 200; line++) {
		i = line;

		if(i < 0 || i >= 200) {
			halt_printf("check_new_scan_int:i:%d, line:%d, st:%d\n",
				i, line, start);
			i = 0;
		}
		if(g_slow_memory_ptr[0x19d00+i] & 0x40) {
			irq_printf("Adding scan_int for line %d\n", i);
			delay = (int) ((DCYCS_IN_16MS/262.0) * ((double)line));
			add_event_entry(g_last_vbl_dcycs + delay, EV_SCAN_INT +
					(line << 8));
			g_scan_int_events = 1;
			check_for_one_event_type(EV_SCAN_INT);
			break;
		}
	}
}

void
check_for_new_scan_int(double dcycs)
{
	int	cur_video_line;
	
	cur_video_line = get_lines_since_vbl(dcycs) >> 8;

	check_scan_line_int(dcycs, cur_video_line);
}

void
init_reg()
{
	engine.acc = 0;
	engine.xreg = 0;
	engine.yreg = 0;
	engine.stack = 0x1ff;
	engine.direct = 0;
	engine.psr = 0x134;

}


void
handle_action(word32 ret)
{
	int	type;

	type = EXTRU(ret,3,4);
	switch(type) {
	case RET_BREAK:
		do_break(ret & 0xff);
		break;
	case RET_COP:
		do_cop(ret & 0xff);
		break;
#if 0
	case RET_MVN:
		do_mvn(ret & 0xffff);
		break;
#endif
	case RET_C700:
		do_c700(ret);
		break;
	case RET_C70A:
		do_c70a(ret);
		break;
	case RET_C70D:
		do_c70d(ret);
		break;
#if 0
	case RET_ADD_DEC_8:
		do_add_dec_8(ret);
		break;
	case RET_ADD_DEC_16:
		do_add_dec_16(ret);
		break;
#endif
	case RET_IRQ:
		irq_printf("Special fast IRQ response.  irq_pending: %x\n",
			g_irq_pending);
		break;
        /* Treat WDM as NOP NOP. The Gog's patch for Bouncing Ferno */        
	case RET_WDM:
		engine.kpc = (engine.kpc + 2);
		break;
	default:
		halt_printf("Unknown special action: %08x!\n", ret);
	}

}

#if 0
void
do_add_dec_8(word32 ret)
{
	halt_printf("do_add_dec_8 called, ret: %08x\n", ret);
}

void
do_add_dec_16(word32 ret)
{
	halt_printf("do_add_dec_16 called, ret: %08x\n", ret);
}
#endif

void
do_break(word32 ret)
{
	if(!g_testing) {
		ki_printf("I think I got a break, second byte: %02x!\n", ret);
		ki_printf("kpc: %06x\n", engine.kpc);
	}

	halt_printf("do_break, kpc: %06x\n", engine.kpc);
	enter_debug = 1;
}

void
do_cop(word32 ret)
{
	halt_printf("COP instr %02x!\n", ret);
	fflush(stdout);
}

#if 0
void
do_mvn(word32 banks)
{
	int	src_bank, dest_bank;
	int	dest, src;
	int	num;
	int	i;
	int	val;

	halt_printf("In MVN...just quitting\n");
	return;
	ki_printf("MVN instr with %04x, cycles: %08x\n", banks, engine.cycles);
	src_bank = banks >> 8;
	dest_bank = banks & 0xff;
	ki_printf("psr: %03x\n", engine.psr);
	if((engine.psr & 0x30) != 0) {
		halt_printf("MVN in non-native mode unimplemented!\n");
	}

	dest = dest_bank << 16 | engine.yreg;
	src = src_bank << 16 | engine.xreg;
	num = engine.acc;
	ki_printf("Moving %08x+1 bytes from %08x to %08x\n", num, src, dest);

	for(i = 0; i <= num; i++) {
		val = get_memory_c(src, 0);
		set_memory_c(dest, val, 0);
		src = (src_bank << 16) | ((src + 1) & 0xffff);
		dest = (dest_bank << 16) | ((dest + 1) & 0xffff);
	}
	engine.dbank = dest_bank;
	engine.acc = 0xffff;
	engine.yreg = dest & 0xffff;
	engine.xreg = src & 0xffff;
	engine.kpc = (engine.kpc + 3);
	ki_printf("move done. db: %02x, acc: %04x, y: %04x, x: %04x, num: %08x\n",
		engine.dbank, engine.acc, engine.yreg, engine.xreg, num);
}
#endif

#if 0
void
do_wdm()
{
	halt_printf("do_wdm!\n");
}

void
do_wai()
{
	halt_printf("do_wai!\n");
}

void
do_stp()
{
	halt_printf("Hit do_stp at addr: %06x\n", engine.kpc);
}

void
size_fail(int val, word32 v1, word32 v2)
{
	halt_printf("Size failure, val: %08x, %08x %08x\n", val, v1, v2);
}
#endif

int
get_limit_speed()
{
    return g_limit_speed;
}

int
set_limit_speed(int rate)
{
    g_limit_speed = rate;
    return 1;
}

// OG

void sim65816_shut()
{

	g_fcycles_stop = 0.0;
	halt_sim = 0;
	enter_debug = 0;
	g_rom_version = 0;
	g_halt_on_bad_read = 0;
	g_ignore_bad_acc = 0;

	g_last_vbl_dcycs = START_DCYCS; 
	g_cur_dcycs = START_DCYCS;

	g_last_vbl_dadjcycs = 0.0;
	g_dadjcycs = 0.0;

	g_wait_pending = 0;
	if (g_irq_pending)
		ki_printf("irq pending...(%d)\n",g_irq_pending);
	g_irq_pending = 0;

	g_num_irq = 0;
	g_num_brk = 0;
	g_num_cop = 0;
	g_num_enter_engine = 0;
	g_io_amt = 0;
	g_engine_action = 0;
	g_engine_halt_event = 0;
	g_engine_scan_int = 0;
	g_engine_doc_int = 0;

	g_testing = 0;

	g_25sec_cntr = 0;
	g_1sec_cntr = 0;

	Verbose = 0;
	Halt_on = 0;

	kbd_in_end = 0;

	g_toolbox_log_pos = 0;
	

}
