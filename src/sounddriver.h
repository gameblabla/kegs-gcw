#ifndef KEGS_SOUNDDRIVER_H
#define KEGS_SOUNDDRIVER_H

#ifdef HAVE_SDL
#ifndef DISABLE_SOUND_SDL
#define HAVE_SOUND_SDL
#endif
#endif

#ifndef DISABLE_SOUND_NATIVE
#if defined(HPUX)
#define HAVE_SOUND_HPUX
#define HAVE_SOUND_NATIVE
#define HAVE_SOUND_ALIB
#elif defined(__linux__) || defined(OSS)
#define HAVE_SOUND_LINUX
#define HAVE_SOUND_NATIVE
#elif defined(WIN32)
#define HAVE_SOUND_WIN32
#define HAVE_SOUND_NATIVE
#endif
#endif

#ifdef HAVE_SOUND_NATIVE
#define SOUND_DEFAULT SOUND_NATIVE
#elif defined(HAVE_SOUND_SDL)
#define SOUND_DEFAULT SOUND_SDL
#else
#define SOUND_DEFAULT SOUND_NONE
#endif

#ifdef HAVE_SOUND_SDL
#include <SDL/SDL.h>
#endif /* HAVE_SOUND_SDL */

#ifdef HAVE_SOUND_HPUX
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/audio.h>
#include "Alib.h"
extern int errno;

#endif /* HAVE_SOUND_HPUX */

#ifdef HAVE_SOUND_LINUX
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
/* default to off for now */

#endif /* HAVE_SOUND_LINUX */

#ifdef HAVE_SOUND_WIN32
#include <windows.h>
#include <mmsystem.h>
#include <process.h>
#define NUM_BUFFERS 10 

#endif /* HAVE_SOUND_WIN32 */

/* sound_driver.c */
long sound_init_device(int devtype);
void sound_shutdown();
void sound_write(int real_samps, int size);
int sound_enabled();

extern int g_preferred_rate;

int get_preferred_rate(void);
int set_preferred_rate(int);
int get_audio_devtype(void);
int set_audio_devtype(int);

#endif

