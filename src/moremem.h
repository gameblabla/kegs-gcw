#ifndef KEGS_MOREMEM_H
#define KEGS_MOREMEM_H

extern int statereg;
extern int stop_on_c03x;
extern int speed_fast;
extern word32 g_slot_motor_detect;
extern int wrdefram;
extern int int_crom[8];
extern int shadow_reg;
extern int g_c023_val;
extern int c023_1sec_int_irq_pending;
extern int c023_scan_int_irq_pending;
extern int c041_en_25sec_ints;
extern int c041_en_vbl_ints;
extern int g_c046_val;
extern int c046_25sec_irq_pend;
extern int c046_vbl_irq_pending;
extern int g_border_color;

void fixup_brks(void);
void show_bankptrs_bank0rdwr(void);
void setup_pageinfo(void);
int io_read(word32 loc, double *cyc_ptr);
void io_write(word32 loc, int val, double *cyc_ptr);
word32 get_lines_since_vbl(double dcycs);
void moremem_shut();

#endif /* KEGS_MOREMEM_H */
