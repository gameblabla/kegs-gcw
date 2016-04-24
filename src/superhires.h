
/* This file is included by video.c */

#ifndef SUPERHIRES_INCLUDED
const char rcsid_superhires_h[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/superhires.h,v 1.3 2005/09/23 12:37:09 fredyd Exp $";
# define SUPERHIRES_INCLUDED
#endif


void
SUPER_TYPE(byte *screen_data, int y, int scan, word32 ch_mask)
{
	word32	mem_ptr;
	byte	val0, val1;
	int	x1, x2;
	byte	*b_ptr;
	word32	*img_ptr, *img_ptr2;
	word32	tmp;

	word32	ch_tmp;
	byte	*slow_mem_ptr;
	int	shift_per;

	word32	pal;
	word32	pal_word;
	word32	pix0, pix1, pix2, pix3;
	word32	save_pix;


	mem_ptr = 0xa0 * y + 0x12000;

	shift_per = (1 << SHIFT_PER_CHANGE);
	if(SUPER_A2VID) {
		pal = (g_a2vid_palette & 0xf);
	} else {
		pal = (scan & 0xf);
	}

	if(SUPER_FILL) {
		ch_mask = -1;
		save_pix = 0;
	}

	for(x1 = 0; x1 < 0xa0; x1 += shift_per) {

		CH_LOOP_A2_VID(ch_mask, ch_tmp);

		pal_word = (pal << 28) + (pal << 20) + (pal << 12) +
			(pal << 4);

		if(SUPER_MODE640 && !SUPER_A2VID) {
#ifdef KEGS_LITTLE_ENDIAN
			pal_word += 0x04000c08;
#else
			pal_word += 0x080c0004;
#endif
		}


		slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr + x1]);
		b_ptr = &screen_data[(y*2)*A2_WINDOW_WIDTH + x1*4];
		img_ptr = (word32 *)b_ptr;
		img_ptr2 = (word32 *)(b_ptr + A2_WINDOW_WIDTH);

		for(x2 = 0; x2 < shift_per; x2 += 2) {
			val0 = *slow_mem_ptr++;
			val1 = *slow_mem_ptr++;
			if(SUPER_MODE640) {
				pix0 = (val0 >> 6) & 0x3;
				pix1 = (val0 >> 4) & 0x3;
				pix2 = (val0 >> 2) & 0x3;
				pix3 = val0 & 0x3;
				if(SUPER_A2VID) {
					pix0 = g_a2vid_palette_remap[pix0+8];
					pix1 = g_a2vid_palette_remap[pix1+12];
					pix2 = g_a2vid_palette_remap[pix2+0];
					pix3 = g_a2vid_palette_remap[pix3+4];
				}
#ifdef KEGS_LITTLE_ENDIAN
				tmp = (pix3 << 24) + (pix2 << 16) +
					(pix1 << 8) + pix0 + pal_word;
#else
				tmp = (pix0 << 24) + (pix1 << 16) +
					(pix2 << 8) + pix3 + pal_word;
#endif
				*img_ptr++ = tmp; *img_ptr2++ = tmp;

				pix0 = (val1 >> 6) & 0x3;
				pix1 = (val1 >> 4) & 0x3;
				pix2 = (val1 >> 2) & 0x3;
				pix3 = val1 & 0x3;
				if(SUPER_A2VID) {
					pix0 = g_a2vid_palette_remap[pix0+8];
					pix1 = g_a2vid_palette_remap[pix1+12];
					pix2 = g_a2vid_palette_remap[pix2+0];
					pix3 = g_a2vid_palette_remap[pix3+4];
				}
#ifdef KEGS_LITTLE_ENDIAN
				tmp = (pix3 << 24) + (pix2 << 16) +
					(pix1 << 8) + pix0 + pal_word;
#else
				tmp = (pix0 << 24) + (pix1 << 16) +
					(pix2 << 8) + pix3 + pal_word;
#endif
				*img_ptr++ = tmp; *img_ptr2++ = tmp;
			} else {
				pix0 = (val0 >> 4);
				if(SUPER_FILL) {
					if(pix0) {
						save_pix = pix0;
					} else {
						pix0 = save_pix;
					}
				}
				pix1 = (val0 & 0xf);
				if(SUPER_FILL) {
					if(pix1) {
						save_pix = pix1;
					} else {
						pix1 = save_pix;
					}
				}
				if(SUPER_A2VID) {
					pix0 = g_a2vid_palette_remap[pix0];
					pix1 = g_a2vid_palette_remap[pix1];
				}
#ifdef KEGS_LITTLE_ENDIAN
				tmp = (pix1 << 24) + (pix1 << 16) +
					(pix0 << 8) + pix0 + pal_word;
#else
				tmp = (pix0 << 24) + (pix0 << 16) +
					(pix1 << 8) + pix1 + pal_word;
#endif
				*img_ptr++ = tmp; *img_ptr2++ = tmp;

				pix0 = (val1 >> 4);
				if(SUPER_FILL) {
					if(pix0) {
						save_pix = pix0;
					} else {
						pix0 = save_pix;
					}
				}
				pix1 = (val1 & 0xf);
				if(SUPER_FILL) {
					if(pix1) {
						save_pix = pix1;
					} else {
						pix1 = save_pix;
					}
				}
				if(SUPER_A2VID) {
					pix0 = g_a2vid_palette_remap[pix0];
					pix1 = g_a2vid_palette_remap[pix1];
				}
#ifdef KEGS_LITTLE_ENDIAN
				tmp = (pix1 << 24) + (pix1 << 16) +
					(pix0 << 8) + pix0 + pal_word;
#else
				tmp = (pix0 << 24) + (pix0 << 16) +
					(pix1 << 8) + pix1 + pal_word;
#endif
				*img_ptr++ = tmp; *img_ptr2++ = tmp;
			}
		}
	}
}


