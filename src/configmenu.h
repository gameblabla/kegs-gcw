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

#define CONFIG_MAX_CHOICES 11
#define CONFIG_MAX_ITEMS 8

typedef struct config_choice {
    const unsigned char *name;
    int value;
} config_choice_t;

/* Configuration callbacks */
/* callbacks should return 1 if configuration change is accepted */
/* after calling the set_* callbacks, one must:
   - check if g_video_devtype changed (if it is not SDL, exit config)
   - redraw the whole screen
*/

typedef struct config_item {
    const unsigned char *title;
    int (*get)();
    int (*set)(int);
    config_choice_t choice[CONFIG_MAX_CHOICES];
} config_item_t;

typedef struct config_menu {
    const unsigned char *title;
    config_item_t item[CONFIG_MAX_ITEMS];
} config_menu_t;

extern const unsigned char config_panel_title[];

extern const config_menu_t config_panel[];

extern int g_font_size;

int configuration_save(int);
int configuration_load(int);

