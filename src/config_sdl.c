/****************************************************************/
/*    	Apple IIgs emulator                                     */
/*                                                              */
/*    This code may not be used in a commercial product         */
/*    without prior written permission of the authors.          */
/*                                                              */
/*    SDL Code by Frederic Devernay	                        */
/*    You may freely distribute this code.                      */ 
/*                                                              */
/****************************************************************/

#include <assert.h>
#include "sim65816.h"
#include "video.h"
#include "videodriver.h"
#include "video_sdl.h"
#include "configmenu.h"
#include "engine.h"

#ifdef HAVE_VIDEO_SDL


void
configuration_menu_colors(Uint8 *fg, Uint8 *bg)
{
    int stat;

	stat = a2_line_stat[0];
    *bg = ((stat >> 8) & 0xf);
    *fg = ((stat >> 12) & 0xf);
    if(*bg == *fg) {
        *bg = 0x0;
        *fg = 0xf;
    }
    *bg += (g_a2vid_palette<<4);
    *fg += (g_a2vid_palette<<4);
}

void
configuration_menu_scale(Uint16 *sx, Uint16 *sy)
{
    *sx = (g_font_size >> 4) & 0xf;
    *sy = g_font_size & 0xf;
}

static void
configuration_menu_size(Uint16 *tlx, Uint16 *tly,
                        Uint16 *width, Uint16 *height,
                        Uint16 *xinc, Uint16 *yinc,
                        Uint16 sx, Uint16 sy)
{
    /* window width (in characters) */
    *width = A2_WINDOW_WIDTH/(sx*7);
    /* window height (in characters) */
    *height = A2_WINDOW_HEIGHT/(sy*8);
    /* centering offset */
    *tlx = (A2_WINDOW_WIDTH-*width*7*sx)/2;
    *tly = (A2_WINDOW_HEIGHT-*height*8*sy)/2;
    if(g_videomode == KEGS_FULL) {
        *tlx += BORDER_WIDTH;
        *tly += BASE_MARGIN_TOP;
    }
    *yinc = 8*sy;
    *xinc = 7*sx;
}


static void
configuration_menu_drawframe(const unsigned char *title, const unsigned char *subtitle, Uint16 sx, Uint16 sy, Uint8 fg, Uint8 bg)
{
    Uint16 tlx, tly, x, y, row;
    unsigned char line[128];
    int len;
    Uint16 width, height;
    Uint16 xinc, yinc;

    configuration_menu_size(&tlx, &tly, &width, &height, &xinc, &yinc, sx, sy);
    x = tlx;
    y = tly;
    row = 0;
    memset(&line[1],'_',width-2);
    line[0] = ' ';
    line[width-1] = ' ';
    sdl_draw_string(screen,x,y,line,width,sx,sy,fg,bg);
    y += yinc; row++;
    /* title bar */
    memset(&line[1],' ',width-2);
    line[0] = A2_RIGHTLINE;
    line[width-1] = A2_LEFTLINE;
    len = strlen(title);
    memcpy(&line[2],title,len);
    if (!subtitle)
        memset(&line[3+len], 32+128, width-4-len);
    sdl_draw_string(screen,x,y,line,width,sx,sy,fg,bg);
    y += yinc; row++;
    if(subtitle) {
        /* subtitle */
        memset(&line[1],A2_OVERUNDERLINE,width-2);
        sdl_draw_string(screen,x,y,line,width,sx,sy,fg,bg);
        y += yinc; row++;
        len = strlen(subtitle);
        memcpy(&line[2],subtitle,len);
        line[1] = ' ';
        line[2+len] = ' ';
        memset(&line[3+len], 32+128, width-4-len);
        sdl_draw_string(screen,x,y,line,width,sx,sy,fg,bg);
        y += yinc; row++;
    }
    memset(&line[1],A2_OVERLINE,width-2);
    sdl_draw_string(screen,x,y,line,width,sx,sy,fg,bg);
    y += yinc; row++;
    /* draw menu contents here */

    /* fill with white lines */
    memset(&line[1],' ',width-2);
    while(row<(height-2)) {
        sdl_draw_string(screen,x,y,line,width,sx,sy,fg,bg);
        y += yinc; row++;
    }
    if (!subtitle) {
        memcpy(&line[2],"Select:",7);
        line[10] = A2_DARROW;
        line[12] = A2_UARROW;
        memcpy(&line[width-9],"Open:",5);
        line[width-3] = A2_RETURN;
    }
    else {
        memcpy(&line[2],"Select:",7);
        line[10] = A2_LARROW;
        line[12] = A2_RARROW;
        line[14] = A2_DARROW;
        line[16] = A2_UARROW;
        /*memcpy(&line[width-21],"Cancel:Esc  Save:",17);*/
        memcpy(&line[width-20],"Cancel:Esc  Set:",17);
        line[width-3] = A2_RETURN;
    }
    sdl_draw_string(screen,x,y,line,width,sx,sy,fg,bg);
    y += yinc; row++;
    memset(&line[1],A2_OVERLINE,width-2);
    line[0] = line[width-1] = ' ';
    sdl_draw_string(screen,x,y,line,width,sx,sy,fg,bg);
    y += yinc; row++;
}

void
configuration_menu_drawmenu(int menu, int selected, int *init_values, int *choices, Uint16 sx, Uint16 sy, Uint8 fg, Uint8 bg)
{
    Uint16 tlx, tly, x, y;
    unsigned char line[128];
    int len;
    Uint16 width, height;
    Uint16 yinc;
    Uint16 xinc;
    int nbitems;
    int i;

    configuration_menu_size(&tlx, &tly, &width, &height, &xinc, &yinc, sx, sy);
    
    for(nbitems = 0; config_panel[menu].item[nbitems].title != NULL; nbitems++) {}
    y = tly+5*yinc;
    x = tlx+4*sx*7;
    for(i=0; i<nbitems; i++) {
        memset(line,' ',width-4);
        if(init_values[i] == config_panel[menu].item[i].choice[choices[i]].value) {
            line[0] = A2_CHECK;
        }
        sdl_draw_string(screen,x-2*xinc,y,line,width-4,sx,sy,fg,bg);
        len = strlen(config_panel[menu].item[i].title);
        memcpy(line, config_panel[menu].item[i].title, len);
        line[len] = ':';
        line[len+1] = 0;
        if(selected == i)
            sdl_draw_string(screen,x,y,line,width-6,sx,sy,bg,fg);
        else
            sdl_draw_string(screen,x,y,line,width-6,sx,sy,fg,bg);
        sdl_draw_string(screen,x+(len+2)*xinc,y,config_panel[menu].item[i].choice[choices[i]].name,width-9-len,sx,sy,fg,bg);
        y += yinc;
    }
}

void
configuration_menu(int menu, Uint16 sx, Uint16 sy, Uint8 fg, Uint8 bg)
{
    int selected = 0;
    int quit = 0, save = 0;
    SDL_Event event;
    int nbitems;
    int init_values[CONFIG_MAX_ITEMS];
    int choices[CONFIG_MAX_ITEMS];
    int i;

    assert(menu >= 0);
    for(i = 0; config_panel[menu].item[i].title != NULL; i++) {
        init_values[i] = config_panel[menu].item[i].get();
        choices[i] = 0;
        while((config_panel[menu].item[i].choice[choices[i]].name != NULL) &&
              (config_panel[menu].item[i].choice[choices[i]].value != init_values[i]))
            choices[i]++;
        if(config_panel[menu].item[i].choice[choices[i]].name == NULL) {
            /* the value is not part of the given choices */
            choices[i] = 0;
        }
    }
    nbitems = i;
    configuration_menu_drawframe(config_panel_title, config_panel[menu].title, sx, sy, fg, bg);
    configuration_menu_drawmenu(menu,selected, init_values, choices, sx, sy, fg, bg);
	sdl_update_dirty_rects(gRectTable, screen);
    while(!quit && SDL_WaitEvent(&event)) {
        switch(event.type) {
        case SDL_KEYUP:
            switch(event.key.keysym.sym) {
            case SDLK_ESCAPE:
                quit = 1;
                break;
            case SDLK_RETURN:
                quit = 1;
                save = 1;
                break;
            case SDLK_UP:
                if(selected == 0)
                    selected = nbitems-1;
                else
                    selected--;
                break;
            case SDLK_DOWN:
                if(selected == nbitems-1)
                    selected = 0;
                else
                    selected++;
                break;
            case SDLK_RIGHT:
                choices[selected]++;
                if(config_panel[menu].item[selected].choice[choices[selected]].name == NULL)
                    choices[selected] = 0;
                break;
            case SDLK_LEFT:
                if(choices[selected] != 0)
                    choices[selected]--;
                else {
                    while(config_panel[menu].item[selected].choice[choices[selected]].name != NULL)
                        choices[selected]++;
                    choices[selected]--;
                }
                break;
            default:
                /* ignored */
                break;
            }
            configuration_menu_drawmenu(menu,selected, init_values, choices, sx, sy, fg, bg);
            sdl_update_dirty_rects(gRectTable, screen);
			break;
        case SDL_QUIT:
            set_halt(HALT_WANTTOQUIT);
            break;
        default:
            /* ignored */
            break;
        }
    }
    if(save) {
        int value;
        Uint8 fg, bg;
        Uint16 sx, sy;

        for(i = 0; i<nbitems; i++) {
            value = config_panel[menu].item[i].choice[choices[i]].value;
            if(init_values[i] != value)
                config_panel[menu].item[i].set(value);
        }
        configuration_menu_colors(&fg,&bg);
        configuration_menu_scale(&sx,&sy);
        configuration_menu_drawframe(config_panel_title, NULL, sx, sy, fg, bg);
		sdl_update_dirty_rects(gRectTable, screen);
    }
}

void
configuration_menu_drawpanel(int selected, Uint16 sx, Uint16 sy, Uint8 fg, Uint8 bg)
{
    Uint16 tlx, tly, x, y;
    Uint16 width, height;
    Uint16 xinc, yinc;
    int nbmenus;
    int i;

    configuration_menu_size(&tlx, &tly, &width, &height, &xinc, &yinc, sx, sy);
    for(nbmenus = 0; config_panel[nbmenus].title != NULL; nbmenus++) {}
    y = tly+5*yinc;
    x = tlx+4*sx*7;
    for(i=0; i<nbmenus; i++) {
        if(selected == i)
            sdl_draw_string(screen,x,y,config_panel[i].title,width-7,sx,sy,bg,fg);
        else
            sdl_draw_string(screen,x,y,config_panel[i].title,width-7,sx,sy,fg,bg);
        y += yinc;
    }
    y+=yinc;
    /*
    if(selected == -2)
        sdl_draw_string(screen,x,y,"Save to disk",width-7,sx,sy,bg,fg);
    else
        sdl_draw_string(screen,x,y,"Save to disk",width-7,sx,sy,fg,bg);
    y+=yinc;
    */
    if(selected == -1)
        sdl_draw_string(screen,x,y,"Quit",width-7,sx,sy,bg,fg);
    else
        sdl_draw_string(screen,x,y,"Quit",width-7,sx,sy,fg,bg);
    y+=yinc;
}

void
configuration_menu_panel(Uint16 sx, Uint16 sy, Uint8 fg, Uint8 bg)
{
    int selected = -1;
    int quit = 0;
    SDL_Event event;
    int nbmenus;

    for(nbmenus = 0; config_panel[nbmenus].title != NULL; nbmenus++) {}
    configuration_menu_drawframe(config_panel_title, NULL, sx, sy, fg, bg);
    configuration_menu_drawpanel(selected, sx, sy, fg, bg);
	sdl_update_dirty_rects(gRectTable, screen);
    while(!quit && SDL_WaitEvent(&event)) {
        switch(event.type) {
        case SDL_KEYUP:
            switch(event.key.keysym.sym) {
            case SDLK_ESCAPE:
                selected = -1;
                break;
            case SDLK_RETURN:
                if(selected == -1)
                    quit = 1;
                /*
                else if(selected == -2)
                    configuration_save(1);
                */
                else {
                    configuration_menu(selected,sx,sy,fg,bg);
                    configuration_menu_scale(&sx, &sy);
                    configuration_menu_drawframe(config_panel_title, NULL, sx, sy, fg, bg);
                    sdl_update_dirty_rects(gRectTable, screen);
				}
                break;
            case SDLK_UP:
                /*if(selected == -2)*/
                if(selected == -1)
                    selected = nbmenus-1;
                else
                    selected--;
                break;
            case SDLK_DOWN:
                if(selected == nbmenus-1)
                    /*selected = -2;*/
                    selected = -1;
                else
                    selected++;
                break;
            default:
                /* ignored */
                break;
            }
            configuration_menu_drawpanel(selected, sx, sy, fg, bg);
			sdl_update_dirty_rects(gRectTable, screen);
            break;
        case SDL_QUIT:
            set_halt(HALT_WANTTOQUIT);
            break;
        default:
            /* ignored */
            break;
        }
    }
}


void
configuration_menu_sdl(void)
{
    Uint8 bg;
    Uint8 fg;
    SDL_Rect r;
    Uint16 sx, sy;

    if(g_videomode == KEGS_FULL) {
        r.x = BORDER_WIDTH;
        r.y = BASE_MARGIN_TOP;
    } else {
        r.x = r.y = 0;
    }
    r.w = A2_WINDOW_WIDTH;
    r.h = A2_WINDOW_HEIGHT;
    configuration_menu_colors(&fg,&bg);
    configuration_menu_scale(&sx,&sy);
    SDL_FillRect(screen, &r, bg);
    SDL_UpdateRect(screen, r.x, r.y, r.w, r.h);

    configuration_menu_panel(sx, sy, fg, bg);
    video_full_redraw();
}

#endif
