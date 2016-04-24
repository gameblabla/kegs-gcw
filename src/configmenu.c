/****************************************************************/
/*    	Apple IIgs emulator                                     */
/*                                                              */
/*    This code may not be used in a commercial product         */
/*    without prior written permission of the authors.          */
/*                                                              */
/*    SDL Code by Frederic Devernay	                        */
/*    Mac OS X changes by Benoit Dardelet			*/
/*    You may freely distribute this code.                      */ 
/*                                                              */
/****************************************************************/

#include <assert.h>
#include "sim65816.h"
#include "adb.h"
#include "paddles.h"
#include "joystick.h"
#include "sound.h"
#include "sounddriver.h"
#include "iwm.h"
#include "dis.h"
#include "functions.h"
#include "video.h"
#include "videodriver.h"
#include "configmenu.h"

static int get_no(void);
static int cold_start(int);
static int get_font_size(void);
static int set_font_size(int);

#ifdef __APPLE__
    /* Name of the profile file for MacOS X users */
static const char g_kegsrc_name[] = "Config.kegs";
void ki_loading(int _motorOn) { };
#else

static const char g_kegsrc_name[] = ".kegsrc";
#endif

const unsigned char config_panel_title[]="KEGS Configuration";

const config_menu_t config_panel[] = {
    {"Display", {
        {"Fullscreen", get_fullscreen, set_fullscreen, {
            {"No", 0},
            {"Yes", 1},
            {NULL}}},
        {"Size", get_videomode, set_videomode, {
            {"Full", KEGS_FULL},
            {"640x480", KEGS_640X480},
            {"640x400", KEGS_640X400},
            {NULL}}},
        {"Driver", get_video_devtype, set_video_devtype, {
            {"SDL", VIDEO_SDL},
            {"X11", VIDEO_X11},
            {"Win32", VIDEO_WIN32},
            {NULL}}},
        {NULL}}},
    {"Sound", {
        {"Frequency", get_preferred_rate, set_preferred_rate, {
            {"44100", 44100},
            {"48000", 48000},
            {"22050", 22050},
            {NULL}}},
        {"Driver", get_audio_devtype, set_audio_devtype, {
            {"None", SOUND_NONE},
            {"Native", SOUND_NATIVE},
            {"SDL", SOUND_SDL},
            {"Alib", SOUND_ALIB},
            {NULL}}},
        {NULL}}},
    {"Keyboard", {
        {"F6 Function", get_func_f6, set_func_f6, {
            {"None", FUNCTION_NONE},
            {"Enter Debugger", FUNCTION_ENTER_DEBUGGER},
            {"Toggle Fast Disk Emul.", FUNCTION_TOGGLE_FAST_DISK_EMUL},
            {"Toggle Grab Mouse", FUNCTION_TOGGLE_WARP_POINTER},
            {"Toggle Display Size", FUNCTION_TOGGLE_VIDEOMODE},
            {"Toggle Fullscreen", FUNCTION_TOGGLE_FULLSCREEN},
            {"Toggle Swap Paddles", FUNCTION_TOGGLE_SWAP_PADDLES},
            {"Toggle Invert Paddles", FUNCTION_TOGGLE_INVERT_PADDLES},
            {"Toggle Slow Paddles", FUNCTION_TOGGLE_SLOW_PADDLES},
            {"Toggle Emulation Speed", FUNCTION_TOGGLE_LIMIT_SPEED},
            {NULL}}},
        {"F7 Function", get_func_f7, set_func_f7, {
            {"None", FUNCTION_NONE},
            {"Enter Debugger", FUNCTION_ENTER_DEBUGGER},
            {"Toggle Fast Disk Emul.", FUNCTION_TOGGLE_FAST_DISK_EMUL},
            {"Toggle Grab Mouse", FUNCTION_TOGGLE_WARP_POINTER},
            {"Toggle Display Size", FUNCTION_TOGGLE_VIDEOMODE},
            {"Toggle Fullscreen", FUNCTION_TOGGLE_FULLSCREEN},
            {"Toggle Swap Paddles", FUNCTION_TOGGLE_SWAP_PADDLES},
            {"Toggle Invert Paddles", FUNCTION_TOGGLE_INVERT_PADDLES},
            {"Toggle Slow Paddles", FUNCTION_TOGGLE_SLOW_PADDLES},
            {"Toggle Emulation Speed", FUNCTION_TOGGLE_LIMIT_SPEED},
            {NULL}}},
        {"F8 Function", get_func_f8, set_func_f8, {
            {"None", FUNCTION_NONE},
            {"Enter Debugger", FUNCTION_ENTER_DEBUGGER},
            {"Toggle Fast Disk Emul.", FUNCTION_TOGGLE_FAST_DISK_EMUL},
            {"Toggle Grab Mouse", FUNCTION_TOGGLE_WARP_POINTER},
            {"Toggle Display Size", FUNCTION_TOGGLE_VIDEOMODE},
            {"Toggle Fullscreen", FUNCTION_TOGGLE_FULLSCREEN},
            {"Toggle Swap Paddles", FUNCTION_TOGGLE_SWAP_PADDLES},
            {"Toggle Invert Paddles", FUNCTION_TOGGLE_INVERT_PADDLES},
            {"Toggle Slow Paddles", FUNCTION_TOGGLE_SLOW_PADDLES},
            {"Toggle Emulation Speed", FUNCTION_TOGGLE_LIMIT_SPEED},
            {NULL}}},
        {"F9 Function", get_func_f9, set_func_f9, {
            {"None", FUNCTION_NONE},
            {"Enter Debugger", FUNCTION_ENTER_DEBUGGER},
            {"Toggle Fast Disk Emul.", FUNCTION_TOGGLE_FAST_DISK_EMUL},
            {"Toggle Grab Mouse", FUNCTION_TOGGLE_WARP_POINTER},
            {"Toggle Display Size", FUNCTION_TOGGLE_VIDEOMODE},
            {"Toggle Fullscreen", FUNCTION_TOGGLE_FULLSCREEN},
            {"Toggle Swap Paddles", FUNCTION_TOGGLE_SWAP_PADDLES},
            {"Toggle Invert Paddles", FUNCTION_TOGGLE_INVERT_PADDLES},
            {"Toggle Slow Paddles", FUNCTION_TOGGLE_SLOW_PADDLES},
            {"Toggle Emulation Speed", FUNCTION_TOGGLE_LIMIT_SPEED},
            {NULL}}},
        {"F10 Function", get_func_f10, set_func_f10, {
            {"None", FUNCTION_NONE},
            {"Enter Debugger", FUNCTION_ENTER_DEBUGGER},
            {"Toggle Fast Disk Emul.", FUNCTION_TOGGLE_FAST_DISK_EMUL},
            {"Toggle Grab Mouse", FUNCTION_TOGGLE_WARP_POINTER},
            {"Toggle Display Size", FUNCTION_TOGGLE_VIDEOMODE},
            {"Toggle Fullscreen", FUNCTION_TOGGLE_FULLSCREEN},
            {"Toggle Swap Paddles", FUNCTION_TOGGLE_SWAP_PADDLES},
            {"Toggle Invert Paddles", FUNCTION_TOGGLE_INVERT_PADDLES},
            {"Toggle Slow Paddles", FUNCTION_TOGGLE_SLOW_PADDLES},
            {"Toggle Emulation Speed", FUNCTION_TOGGLE_LIMIT_SPEED},
            {NULL}}},
        {"F11 Function", get_func_f11, set_func_f11, {
            {"None", FUNCTION_NONE},
            {"Enter Debugger", FUNCTION_ENTER_DEBUGGER},
            {"Toggle Fast Disk Emul.", FUNCTION_TOGGLE_FAST_DISK_EMUL},
            {"Toggle Grab Mouse", FUNCTION_TOGGLE_WARP_POINTER},
            {"Toggle Display Size", FUNCTION_TOGGLE_VIDEOMODE},
            {"Toggle Fullscreen", FUNCTION_TOGGLE_FULLSCREEN},
            {"Toggle Swap Paddles", FUNCTION_TOGGLE_SWAP_PADDLES},
            {"Toggle Invert Paddles", FUNCTION_TOGGLE_INVERT_PADDLES},
            {"Toggle Slow Paddles", FUNCTION_TOGGLE_SLOW_PADDLES},
            {"Toggle Emulation Speed", FUNCTION_TOGGLE_LIMIT_SPEED},
            {NULL}}},
        {"F12 Function", get_func_f12, set_func_f12, {
            {"None", FUNCTION_NONE},
            {"Enter Debugger", FUNCTION_ENTER_DEBUGGER},
            {"Toggle Fast Disk Emul.", FUNCTION_TOGGLE_FAST_DISK_EMUL},
            {"Toggle Grab Mouse", FUNCTION_TOGGLE_WARP_POINTER},
            {"Toggle Display Size", FUNCTION_TOGGLE_VIDEOMODE},
            {"Toggle Fullscreen", FUNCTION_TOGGLE_FULLSCREEN},
            {"Toggle Swap Paddles", FUNCTION_TOGGLE_SWAP_PADDLES},
            {"Toggle Invert Paddles", FUNCTION_TOGGLE_INVERT_PADDLES},
            {"Toggle Slow Paddles", FUNCTION_TOGGLE_SLOW_PADDLES},
            {"Toggle Emulation Speed", FUNCTION_TOGGLE_LIMIT_SPEED},
            {NULL}}},
        {NULL}}},
    {"Joystick", {
        {"Swap Paddles", get_swap_paddles, set_swap_paddles, {
            {"No", 0},
            {"Yes", 1},
            {NULL}}},
        {"Invert Paddles", get_invert_paddles, set_invert_paddles, {
            {"No", 0},
            {"Yes", 1},
            {NULL}}},
        {"Slow Paddles", get_slow_paddles, set_slow_paddles, {
            {"No", 0},
            {"Yes", 1},
            {NULL}}},
        {"Type", get_joystick_type, set_joystick_type, {
            {"None", JOYSTICK_NONE},
            {"Mouse", JOYSTICK_MOUSE},
            {"SDL", JOYSTICK_SDL},
            {"Linux", JOYSTICK_LINUX},
            {"Win32", JOYSTICK_WIN32},
            /*{"Keypad", JOYSTICK_KEYPAD},*/
            {NULL}}},
        {NULL}}},
    {"Mouse", {
        {"Grab Mouse", get_warp_pointer, set_warp_pointer, {
            {"No", 0},
            {"Yes", 1},
            {NULL}}},
        {"Button 2 Function", get_button2_function, set_button2_function, {
            {"None", FUNCTION_NONE},
            {"Change Speed", FUNCTION_TOGGLE_LIMIT_SPEED},
            {"Enter Debugger", FUNCTION_ENTER_DEBUGGER},
            /*
            {"Paste Clipboard", FUNCTION_PASTE},
            {"Screen Dump", FUNCTION_SCREENDUMP},
            {"Shift+Click", FUNCTION_SHIFTCLICK},
            {"Control+Click", FUNCTION_CTRLCLICK},
            {"Option+Click", FUNCTION_OPTIONCLICK},
            */
            {NULL}}},
        {"Button 3 Function", get_button3_function, set_button3_function, {
            {"None", FUNCTION_NONE},
            {"Change Emulation Speed", FUNCTION_TOGGLE_LIMIT_SPEED},
            {"Enter Debugger", FUNCTION_ENTER_DEBUGGER},
            /*
            {"Paste Clipboard", FUNCTION_PASTE},
            {"Screen Dump", FUNCTION_SCREENDUMP},
            {"Shift+Click", FUNCTION_SHIFTCLICK},
            {"Control+Click", FUNCTION_CTRLCLICK},
            {"Option+Click", FUNCTION_OPTIONCLICK},
            */
            {NULL}}},
        {NULL}}},
    {"Disks", {
        {"Fast Disk Emulation", get_fast_disk_emul, set_fast_disk_emul, {
            {"No", 0},
            {"Yes", 1},
            {NULL}}},
        {NULL}}},
    {"Emulation", {
        {"Speed Limit", get_limit_speed, set_limit_speed, {
            {"None (Very fast)", SPEED_UNLIMITED},
            {"1.024MHz (Slow)", SPEED_1MHZ},
            {"2.8MHz (Normal)", SPEED_GS},
            {"8.0MHz (Zip Speed)", SPEED_ZIP},
            {NULL}}},
        {"Cold Start", get_no, cold_start, {
            {"No", 0},
            {"Yes", 1},
            {NULL}}},
        {"Configuration Font Size", get_font_size, set_font_size, {
            {"14x16", 0x22},
            {"7x16", 0x12},
            {"7x8", 0x11},
            {NULL}}},
        {"Enter Debugger", get_no, enter_debugger, {
            {"No", 0},
            {"Yes", 1},
            {NULL}}},
        {"Revert Configuration", get_no, configuration_load, {
            {"No", 0},
            {"Yes", 1},
            {NULL}}},
        {"Save Configuration", get_no, configuration_save, {
            {"No", 0},
            {"Yes", 1},
            {NULL}}},
        {NULL}}},
    {NULL}};


int g_font_size = 0x22;

int
get_no(void)
{
    return 0;
}

static int
get_font_size(void)
{
    return g_font_size;
}

static int
set_font_size(int val)
{

    g_font_size = val;
    return 1;
}

static int
cold_start(int val)
{
    assert(val == 1);
    return 0;
}

int
configuration_save(int val)
{
    const config_menu_t *menu;
    const config_item_t *item;
    const config_choice_t *choice;
    int value;
    
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s/.kegs/%s", getenv("HOME"), g_kegsrc_name);
    
    FILE *kegsrc = fopen(tmp_path,"w");

    assert(val == 1);
    if(kegsrc == NULL) {
        ki_printf("Can't open config file %s for writing\n",g_kegsrc_name);
        return 0;
    }
    for(menu = config_panel; menu->title != NULL; menu++) {
        for(item = menu->item; item->title != NULL; item++) {
            if(item->get != get_no) { /* ignore actions */
                value = item->get();
                fprintf(kegsrc,"%s/%s: ",menu->title,item->title);
                for(choice = item->choice; choice->name != NULL; choice++) {
                    if(value == choice->value)
                        fprintf(kegsrc,"%s\n", choice->name);
                }
            }
        }
    }
    fclose(kegsrc);
    return 1;
}


int
configuration_load(int val)
{
    const config_menu_t *menu;
    const config_item_t *item;
    const config_choice_t *choice;
    char line[256];
    char *c;
    
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s/.kegs/%s", getenv("HOME"), g_kegsrc_name);
    
    FILE *kegsrc = fopen(tmp_path,"r");

    assert(val == 1);
    if(kegsrc == NULL) {
        ki_printf("Can't open config file %s for reading\n",g_kegsrc_name);
        return 0;
    }
    while(!feof(kegsrc)) {
        fgets(line, 256, kegsrc);
        if(line[0] == '#')
            continue;           /* comment */
        for(menu = config_panel;
            menu->title != NULL && strncmp(menu->title, line, strlen(menu->title));
            menu++) {}
            
        if(menu->title == NULL) {
            ki_printf("Can't find configuration menu for %s\n", line);
            continue;
        }
        c = &line[strlen(menu->title)+1]; /* skip "/" */
        for(item = menu->item;
            item->title != NULL && strncmp(item->title, c, strlen(item->title));
            item++) {}
        if(item->title == NULL) {
            ki_printf("Can't find configuration item for %s\n", c);
            continue;
        }
        c = &c[strlen(item->title)+2]; /* skip ": " */
        for(choice = item->choice;
            choice->name != NULL && strncmp(choice->name, c, strlen(choice->name));
            choice++) {}
        if(choice->name == NULL) {
            ki_printf("Can't find configuration choice for %s\n", c);
            continue;
        }
        item->set(choice->value);
    }
    fclose(kegsrc);
    return 1;
}

