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

const char rcsid_sound_driver_c[] = "@(#)$Header: /cvsroot/kegs-sdl/kegs/src/sound_driver.c,v 1.3 2005/09/23 12:37:09 fredyd Exp $";

#include <assert.h>

#include "sim65816.h"
#include "sound.h"
#include "sounddriver.h"
#include "dis.h"

static long sound_init_device_alib();
static long sound_init_device_native();
#ifdef HAVE_SOUND_NATIVE
static long sound_init_device_fork(int devtype);
static long parent_sound_get_sample_rate(int read_fd);
#endif
static void reliable_buf_write(word32 *shm_addr, int pos, int size);
static void reliable_zero_write(int amt);
static void child_sound_loop(int devtype, int read_fd, int write_fd, word32 *shm_addr);
#ifdef HAVE_SOUND_HPUX
static void child_sound_init_hpdev(void);
static void child_sound_init_alib(void);
#endif
#ifdef HAVE_SOUND_LINUX
static void child_sound_init_linux(void);
#endif
#ifdef HAVE_SOUND_WIN32
static long sound_init_device_win32();
static void CheckWaveError(char *,int);
static void child_sound_init_win32(void *);
static void child_sound_shut_win32() ;
static unsigned int __stdcall child_sound_loop_win32(void *);
#endif
#ifdef HAVE_SOUND_SDL
static void _snd_callback(void*, Uint8 *stream, int len);
#endif
static long sound_init_device_sdl();
static void sound_shutdown_native();
static void sound_shutdown_sdl();
static void sound_shutdown_alib();
static void sound_write_native(int real_samps, int size);
static void sound_write_sdl(int real_samps, int size);
#ifdef HAVE_SOUND_LINUX
static int	g_pipe_fd[2] = { -1, -1 };
static int	g_pipe2_fd[2] = { -1, -1 };
#endif

#ifdef HAVE_SOUND_HPUX
static Audio	*g_audio = 0;
static ATransID g_xid;
static int	g_pipe_fd[2] = { -1, -1 };
#endif /* HAVE_SOUND_HPUX */

#ifdef HAVE_SOUND_WIN32
static HWAVEOUT WaveHandle;
static WAVEHDR WaveHeader[NUM_BUFFERS];
static int	g_pipe_fd[2] = { -1, -1 };
#endif /* HAVE_SOUND_WIN32 */

#ifdef HAVE_SOUND_SDL
static byte *playbuf = 0;
static int g_playbuf_buffered = 0;
static SDL_AudioSpec spec;
static int snd_buf;
static int snd_write;           /* write position into playbuf */
static /* volatile */ int snd_read = 0;
static int g_sound_paused;
static int g_zeroes_buffered;
static int g_zeroes_seen;
#endif

int	g_preferred_rate = 48000;
static int g_audio_devtype = SOUND_NATIVE;
#if defined(HAVE_SOUND_LINUX) || defined(HAVE_SOUND_HPUX)
static int g_audio_socket = -1;
#endif
static int	g_bytes_written = 0;

#define ZERO_BUF_SIZE		2048

#ifdef HAVE_SOUND_NATIVE
static word32 zero_buf2[ZERO_BUF_SIZE];
#endif

#define ZERO_PAUSE_SAFETY_SAMPS		(g_audio_rate >> 5)
#define ZERO_PAUSE_NUM_SAMPS		(4*g_audio_rate)


static void
reliable_buf_write(word32 *shm_addr, int pos, int size)
{
	byte	*ptr;
#ifdef HAVE_SOUND_NATIVE
	int	ret;
#endif
#ifdef HAVE_SOUND_WIN32
    int i, found, wave_buf;
#endif

#ifndef NDEBUG
	if(size < 1 || pos < 0 || pos > SOUND_SHM_SAMP_SIZE ||
				size > SOUND_SHM_SAMP_SIZE ||
				(pos + size) > SOUND_SHM_SAMP_SIZE) {
		ki_printf("reliable_buf_write: pos: %04x, size: %04x\n",
			pos, size);
		my_exit(1);
	}
#endif

	ptr = (byte *)&(shm_addr[pos]);
	size = size * 4;

#if defined(HAVE_SOUND_LINUX) || defined(HAVE_SOUND_HPUX)
	while(size > 0) {
		ret = write(g_audio_socket, ptr, size);

		if(ret < 0) {
			ki_printf("audio write, errno: %d\n", errno);
			my_exit(1);
		}
		size = size - ret;
		ptr += ret;
		g_bytes_written += ret;
	}
#endif /* HAVE_SOUND_HPUX || HAVE_SOUND_LINUX */
#ifdef HAVE_SOUND_WIN32
    i=0;
    found=0;
    while ((i < NUM_BUFFERS)) {
		if (WaveHeader[i].dwUser == FALSE) {
            wave_buf=i;
            found=1;
            break;
        }
        i++;
    }

    if (!found) {
        return ;
    }

    memcpy(WaveHeader[wave_buf].lpData,ptr,size);
    WaveHeader[wave_buf].dwBufferLength=size;
    WaveHeader[wave_buf].dwUser=TRUE;

    ret=waveOutWrite(WaveHandle,&WaveHeader[wave_buf],
                     sizeof(WaveHeader));
    CheckWaveError("Writing wave out",ret);

    g_bytes_written += (size);
#endif /* HAVE_SOUND_WIN32 */

}

static void
reliable_zero_write(int amt)
{
#if defined(HAVE_SOUND_LINUX) || defined(HAVE_SOUND_HPUX)
	int	len;

	while(amt > 0) {
		len = MIN(amt, ZERO_BUF_SIZE);
		reliable_buf_write(zero_buf2, 0, len);
		amt -= len;
	}
#endif /* HAVE_SOUND_HPUX || HAVE_SOUND_LINUX */
#ifdef HAVE_SOUND_WIN32
    reliable_buf_write(zero_buf2, 0, amt);
#endif /* HAVE_SOUND_WIN32 */
}


static void
child_sound_loop(int devtype, int read_fd, int write_fd, word32 *shm_addr)
{
	word32	tmp;
	int	zeroes_buffered;
	int	zeroes_seen;
	int	sound_paused;
	int	vbl;
	int	size;
	int	pos;
	int	ret;

	doc_printf("Child pipe fd: %d\n", read_fd);

	g_audio_rate = g_preferred_rate;

	switch(devtype) {
    case SOUND_NATIVE:
#if defined(HAVE_SOUND_HPUX)
		child_sound_init_hpdev();
#elif defined(HAVE_SOUND_LINUX)
        child_sound_init_linux();
#elif defined(HAVE_SOUND_WIN32)
        child_sound_init_win32(shm_addr);
#endif
        break;
    case SOUND_ALIB:
#ifdef HAVE_SOUND_ALIB
		child_sound_init_alib();
#endif
        break;
	}

	tmp = g_audio_rate;
	ret = write(write_fd, &tmp, 4);
	if(ret != 4) {
		ki_printf("Unable to send back audio rate to parent\n");
		my_exit(1);
	}

#if defined(HAVE_SOUND_HPUX) || defined(HAVE_SOUND_LINUX)
    close(write_fd);
#endif

	/*
	OG g_audio_devtype not intialized yet
	if (g_audio_devtype == SOUND_NONE) {
		return;
	}
	*/

	zeroes_buffered = 0;
	zeroes_seen = 0;
	sound_paused = 0;

	pos = 0;
	vbl = 0;
	while(1) {
		errno = 0;

		ret = read(read_fd, &tmp, 4);

		if(ret <= 0) {
			ki_printf("child dying from ret: %d, errno: %d\n",
				ret, errno);
			break;
		}

		size = tmp & 0xffffff;

		if((tmp >> 24) == 0xa2) {
			/* real_samps */
            /* play sound here */

			if(sound_paused) {
				ki_printf("Unpausing sound, zb: %d\n",
					zeroes_buffered);
				sound_paused = 0;
			}


#if 0
			pos += zeroes_buffered;
			while(pos >= SOUND_SHM_SAMP_SIZE) {
				pos -= SOUND_SHM_SAMP_SIZE;
			}
#endif

			if(zeroes_buffered) {
				reliable_zero_write(zeroes_buffered);
			}

			zeroes_buffered = 0;
			zeroes_seen = 0;


			if((size + pos) > SOUND_SHM_SAMP_SIZE) {
				reliable_buf_write(shm_addr, pos,
						SOUND_SHM_SAMP_SIZE - pos);
				size = (pos + size) - SOUND_SHM_SAMP_SIZE;
				pos = 0;
			}

			reliable_buf_write(shm_addr, pos, size);


		} else if((tmp >> 24) == 0xa1) {
            /* !real_samps */
			if(sound_paused) {
				if(zeroes_buffered < ZERO_PAUSE_SAFETY_SAMPS) {
					zeroes_buffered += size;
				}
			} else {
				/* not paused, send it through */
				zeroes_seen += size;

				reliable_zero_write(size);

				if(zeroes_seen >= ZERO_PAUSE_NUM_SAMPS) {
					ki_printf("Pausing sound\n");
					sound_paused = 1;
				}
			}
		} else {
			ki_printf("tmp received bad: %08x\n", tmp);
			my_exit(3);
		}

		pos = pos + size;
		while(pos >= SOUND_SHM_SAMP_SIZE) {
			pos -= SOUND_SHM_SAMP_SIZE;
		}

		vbl++;
		if(vbl >= 60) {
			vbl = 0;
#if 0
			ki_printf("sound bytes written: %06x\n", g_bytes_written);
			ki_printf("Sample samples[0]: %08x %08x %08x %08x\n",
				shm_addr[0], shm_addr[1],
				shm_addr[2], shm_addr[3]);
			ki_printf("Sample samples[100]: %08x %08x %08x %08x\n",
				shm_addr[100], shm_addr[101],
				shm_addr[102], shm_addr[103]);
#endif
			g_bytes_written = 0;
		}
	}


#ifdef HAVE_SOUND_HPUX
    switch(g_audio_devtype) {
    case SOUND_NATIVE:
		ioctl(g_audio_socket, AUDIO_DRAIN, 0);
        break;
    case SOUND_ALIB:
		ACloseAudio(g_audio, &status_return);
        break;
	}
	close(g_audio_socket);
#endif

#ifdef HAVE_SOUND_WIN32
	child_sound_shut_win32();
#endif
}


#ifdef HAVE_SOUND_HPUX
static void
child_sound_init_hpdev()
{
	struct audio_describe audio_descr;
	int	output_channel;
	char	*str;
	int	speaker;
	int	ret;
	int	i;

	g_audio_socket = open("/dev/audio", O_WRONLY, 0);
	if(g_audio_socket < 0) {
		ki_printf("open /dev/audio failed, ret: %d, errno:%d\n",
			g_audio_socket, errno);
		my_exit(1);
	}

	ret = ioctl(g_audio_socket, AUDIO_DESCRIBE, &audio_descr);
	if(ret < 0) {
		ki_printf("ioctl AUDIO_DESCRIBE failed, ret:%d, errno:%d\n",
			ret, errno);
		my_exit(1);
	}

	for(i = 0; i < audio_descr.nrates; i++) {
		ki_printf("Audio rate[%d] = %d\n", i,
			audio_descr.sample_rate[i]);
	}

	ret = ioctl(g_audio_socket, AUDIO_SET_DATA_FORMAT,
			AUDIO_FORMAT_LINEAR16BIT);
	if(ret < 0) {
		ki_printf("ioctl AUDIO_SET_DATA_FORMAT failed, ret:%d, errno:%d\n",
			ret, errno);
		my_exit(1);
	}

	ret = ioctl(g_audio_socket, AUDIO_SET_CHANNELS, NUM_CHANNELS);
	if(ret < 0) {
		ki_printf("ioctl AUDIO_SET_CHANNELS failed, ret:%d, errno:%d\n",
			ret, errno);
		my_exit(1);
	}

	ret = ioctl(g_audio_socket, AUDIO_SET_TXBUFSIZE, 16*1024);
	if(ret < 0) {
		ki_printf("ioctl AUDIO_SET_TXBUFSIZE failed, ret:%d, errno:%d\n",
			ret, errno);
		my_exit(1);
	}

	ret = ioctl(g_audio_socket, AUDIO_SET_SAMPLE_RATE, g_audio_rate);
	if(ret < 0) {
		ki_printf("ioctl AUDIO_SET_SAMPLE_RATE failed, ret:%d, errno:%d\n",
			ret, errno);
		my_exit(1);
	}

	ret = ioctl(g_audio_socket, AUDIO_GET_OUTPUT, &output_channel);
	if(ret < 0) {
		ki_printf("ioctl AUDIO_GET_OUTPUT failed, ret:%d, errno:%d\n",
			ret, errno);
		my_exit(1);
	}


	speaker = 1;
	str = getenv("SPEAKER");
	if(str) {
		if(str[0] != 'i' && str[0] != 'I') {
			speaker = 0;
		}
	}

	if(speaker) {
		ki_printf("Sending sound to internal speaker\n");
		output_channel |= AUDIO_OUT_SPEAKER;
	} else {
		ki_printf("Sending sound to external jack\n");
		output_channel &= (~AUDIO_OUT_SPEAKER);
		output_channel |= AUDIO_OUT_HEADPHONE;
	}

	ret = ioctl(g_audio_socket, AUDIO_SET_OUTPUT, output_channel);
	if(ret < 0) {
		ki_printf("ioctl AUDIO_SET_OUTPUT failed, ret:%d, errno:%d\n",
			ret, errno);
		my_exit(1);
	}
}


static void
child_sound_init_alib()
{
	AOutputChMask	chan_mask;
	AOutputDstMask	dest_mask;
	AudioAttributes attr;
	AudioAttributes *attr_ptr;
	SSPlayParams	play_params;
	SStream		audio_stream;
	AGainEntry	gain_entries[2];
	long		status_return;
	char	*str;
	int	speaker;
	int	ret;

	status_return = 0;
	g_audio = AOpenAudio((char *)0, &status_return);
	if(g_audio == (Audio *)0) {
		ki_printf("AopenAudio failed, ret: %ld\n", status_return);
		my_exit(1);
	}

	chan_mask = AOutputChannels(g_audio);
	doc_printf("Output channel mask: %08x\n", (word32)chan_mask);

	dest_mask = AOutputDestinations(g_audio);
	doc_printf("Output destinations mask: %08x\n", (word32)dest_mask);

	attr_ptr = ABestAudioAttributes(g_audio);
	attr = *attr_ptr;

	attr.attr.sampled_attr.data_format = ADFLin16;
	attr.attr.sampled_attr.bits_per_sample = 8*SAMPLE_SIZE;
	attr.attr.sampled_attr.sampling_rate = g_audio_rate;
	attr.attr.sampled_attr.channels = NUM_CHANNELS;
	attr.attr.sampled_attr.interleave = 1;
	attr.attr.sampled_attr.duration.type = ATTFullLength;

	gain_entries[0].u.o.out_ch = AOCTLeft;
	gain_entries[1].u.o.out_ch = AOCTLeft;
	gain_entries[0].gain = AUnityGain;
	gain_entries[1].gain = AUnityGain;

	speaker = 1;
	str = getenv("SPEAKER");
	if(str) {
		if(str[0] != 'i' && str[0] != 'I') {
			speaker = 0;
		}
	}

	if(speaker) {
		ki_printf("Sending sound to internal speaker\n");
		gain_entries[0].u.o.out_dst = AODTLeftIntSpeaker;
		gain_entries[1].u.o.out_dst = AODTRightIntSpeaker;
	} else {
		ki_printf("Sending sound to external jack\n");
		gain_entries[0].u.o.out_dst = AODTLeftJack;
		gain_entries[1].u.o.out_dst = AODTRightJack;
	}

	play_params.gain_matrix.type = AGMTOutput;
	play_params.gain_matrix.num_entries = NUM_CHANNELS;
	play_params.gain_matrix.gain_entries = &(gain_entries[0]);
	play_params.play_volume = AUnityGain;
	play_params.priority = APriorityNormal;
	play_params.event_mask = 0;

	g_xid = APlaySStream(g_audio, -1, &attr, &play_params, &audio_stream,
		&status_return);

	doc_printf("g_xid: %ld, status_return: %ld\n", (long)g_xid,
		status_return);

	g_audio_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(g_audio_socket < 0) {
		ki_printf("socket create, errno: %d\n", errno);
		my_exit(1);
	}

	doc_printf("g_audio_socket: %d\n", g_audio_socket);

	ret = connect(g_audio_socket,
			(struct sockaddr *)&audio_stream.tcp_sockaddr,
			sizeof(struct sockaddr_in) );

	if(ret < 0) {
		ki_printf("connect failed, errno: %d\n", errno);
		my_exit(1);
	}
}
#endif /* HAVE_SOUND_HPUX */

#ifdef HAVE_SOUND_LINUX
static void
child_sound_init_linux()
{
	int	stereo;
	int	sample_size;
	int	rate;
	int	fmt;
	int	ret;

	g_audio_socket = open("/dev/dsp", O_WRONLY, 0);
	if(g_audio_socket < 0) {
		ki_printf("open /dev/audio failed, ret: %d, errno:%d\n",
			g_audio_socket, errno);
		my_exit(1);
	}

#if 0
	fragment = 0x00200009;
	ret = ioctl(g_audio_socket, SNDCTL_DSP_SETFRAGMENT, &fragment);
	if(ret < 0) {
		ki_printf("ioctl SETFRAGEMNT failed, ret:%d, errno:%d\n",
			ret, errno);
		my_exit(1);
	}
#endif

	sample_size = 16;
	ret = ioctl(g_audio_socket, SNDCTL_DSP_SAMPLESIZE, &sample_size);
	if(ret < 0) {
		ki_printf("ioctl SNDCTL_DSP_SAMPLESIZE failed, ret:%d, errno:%d\n",
			ret, errno);
		my_exit(1);
	}

#ifdef KEGS_LITTLE_ENDIAN
	fmt = AFMT_S16_LE;
#else
	fmt = AFMT_S16_BE;
#endif
	ret = ioctl(g_audio_socket, SNDCTL_DSP_SETFMT, &fmt);
	if(ret < 0) {
		ki_printf("ioctl SNDCTL_DSP_SETFMT failed, ret:%d, errno:%d\n",
			ret, errno);
		my_exit(1);
	}

	stereo = 1;
	ret = ioctl(g_audio_socket, SNDCTL_DSP_STEREO, &stereo);
	if(ret < 0) {
		ki_printf("ioctl SNDCTL_DSP_STEREO failed, ret:%d, errno:%d\n",
			ret, errno);
		my_exit(1);
	}

	rate = g_audio_rate;
	ret = ioctl(g_audio_socket, SNDCTL_DSP_SPEED, &rate);
	if(ret < 0) {
		ki_printf("ioctl SNDCTL_DSP_SPEED failed, ret:%d, errno:%d\n",
			ret, errno);
		my_exit(1);
	}
	if(ret > 0) {
		rate = ret;	/* rate is returned value */
	}
	if(rate < 8000) {
		ki_printf("Audio rate of %d which is < 8000!\n", rate);
		my_exit(1);
	}
	
	g_audio_rate = rate;

	ki_printf("Sound initialized\n");
}
#endif /* HAVE_SOUND_LINUX */

#ifdef HAVE_SOUND_WIN32
static void CheckWaveError(char *s, int res) {
    char message[256];
    if (res == MMSYSERR_NOERROR) {
        return;
    }
    waveOutGetErrorText(res,message,sizeof(message));
    ki_printf ("%s: %s\n",s,message);
    my_exit(1);
}

static unsigned int __stdcall child_sound_loop_win32(void *param) {
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
	
	soundActive=1;
	ki_printf("win32 sound thread activated\n");
	child_sound_loop(SOUND_NATIVE,g_pipe_fd[0],g_pipe_fd[1],param);
	soundActive=0;	
	ki_printf("win32 sound thread terminated\n");
	return 0;
}

static void CALLBACK handle_wav_snd(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance,
                             DWORD dwParam1, DWORD dwParam2)
{
    /* Only service "buffer done playing" messages */
    if ( uMsg == WOM_DONE ) {
		if (((LPWAVEHDR) dwParam1)->dwFlags == (WHDR_DONE | WHDR_PREPARED)) {
			((LPWAVEHDR)dwParam1)->dwUser=FALSE;
		}
        return;
    }
}

byte *waveBuffer=NULL;

static void
child_sound_shut_win32() 
{
	MMRESULT res;
	int i;

	for (i=0;i<NUM_BUFFERS;i++) 
	{
		while (1)
		{
			res = waveOutUnprepareHeader(WaveHandle,&WaveHeader[i],
									sizeof(WAVEHDR));
			if (res == MMSYSERR_NOERROR )
				break;
			else 
			if (res != WAVERR_STILLPLAYING )
				CheckWaveError("WaveOutUnPrepareHeader()",res);
		}
    }
	waveOutClose(WaveHandle);

	WaveHandle=NULL;
	free(waveBuffer);
	waveBuffer=NULL;
}


static void
child_sound_init_win32(void *shmaddr) {
    int res,i;
    WAVEFORMATEX WaveFmt;
    WAVEOUTCAPS caps;

    int blen;

    memset(&WaveFmt, 0, sizeof(WAVEFORMATEX));
    WaveFmt.wFormatTag = WAVE_FORMAT_PCM;
    WaveFmt.wBitsPerSample = 16;
    WaveFmt.nChannels = 2;
    WaveFmt.nSamplesPerSec = g_audio_rate;
    WaveFmt.nBlockAlign = 
        WaveFmt.nChannels * (WaveFmt.wBitsPerSample/8);
    WaveFmt.nAvgBytesPerSec = 
        WaveFmt.nSamplesPerSec * WaveFmt.nBlockAlign;

    res=waveOutOpen(&WaveHandle,WAVE_MAPPER,&WaveFmt,0,0,WAVE_FORMAT_QUERY);

	if (res != MMSYSERR_NOERROR) {
		ki_printf ("Cannot open audio device\n");
		g_audio_devtype = SOUND_NONE;
		return;
	}

    res=waveOutOpen(&WaveHandle,WAVE_MAPPER,&WaveFmt,(DWORD)handle_wav_snd,
                    0,CALLBACK_FUNCTION | WAVE_ALLOWSYNC );

	if (res != MMSYSERR_NOERROR) {
		ki_printf ("Cannot open audio device\n");
		g_audio_devtype = SOUND_NONE;
		return;
	}

    g_audio_rate= WaveFmt.nSamplesPerSec;

    blen=SOUND_SHM_SAMP_SIZE;
	waveBuffer=malloc(blen*NUM_BUFFERS);
    if (waveBuffer==NULL) {
        ki_printf ("Unable to allocate sound buffer\n");
        my_exit(1);
    }
    for (i=0;i<NUM_BUFFERS;i++) {
        memset(&WaveHeader[i],0,sizeof(WAVEHDR)); 
		/* dwUser contains the busy state */
        WaveHeader[i].dwUser=FALSE;
        WaveHeader[i].lpData=waveBuffer+i*blen;
        WaveHeader[i].dwBufferLength=blen;
        WaveHeader[i].dwFlags=0L;
        WaveHeader[i].dwLoops=0L;
        res = waveOutPrepareHeader(WaveHandle,&WaveHeader[i],
                                   sizeof(WAVEHDR));
        CheckWaveError("WaveOutPrepareHeader()",res);
    }

    res = waveOutGetDevCaps((UINT)WaveHandle, &caps, sizeof(caps));
    CheckWaveError("WaveOutGetDevCaps()",res);
    ki_printf("Using %s\n", caps.szPname);
    ki_printf ("--Bits Per Sample = %d\n",WaveFmt.wBitsPerSample);
    ki_printf ("--Channel = %d\n",WaveFmt.nChannels);
    ki_printf ("--Sampling Rate = %ld\n",WaveFmt.nSamplesPerSec);
    ki_printf ("--Average Bytes Per Second = %ld\n",WaveFmt.nAvgBytesPerSec);
    
}
#endif /* HAVE_SOUND_WIN32 */

long
sound_init_device(int devtype)
{
    g_audio_devtype = SOUND_NONE;
    switch(devtype) {
    case SOUND_NONE:
        return g_preferred_rate;
        break;
    case SOUND_NATIVE:          /* native */
        return sound_init_device_native();
        break;
    case SOUND_SDL:             /* SDL audio */
        return sound_init_device_sdl();
        break;
    case SOUND_ALIB:            /* Alib */
        return sound_init_device_alib();
        break;
    default:
        ki_printf("sound_init_device: unknown device type %d\n", devtype);
        break;
    }
    return 0;
}

static long
sound_init_device_native()
{
#if defined(HAVE_SOUND_HPUX) || defined(HAVE_SOUND_LINUX)
    return sound_init_device_fork(SOUND_NATIVE);
#elif defined(HAVE_SOUND_WIN32)
    return sound_init_device_win32();
#else
    return 0;
#endif
}

static long
sound_init_device_alib()
{
#if defined(HAVE_SOUND_ALIB)
    return sound_init_device_fork(SOUND_ALIB);
#else
    return 0;
#endif
}

#if defined(HAVE_SOUND_HPUX) || defined(HAVE_SOUND_LINUX)
static long
sound_init_device_fork(int devtype)
{
	int	shmid;
	int	ret;
	int	pid;
    int i;

	shmid = shmget(IPC_PRIVATE, SOUND_SHM_SAMP_SIZE * SAMPLE_CHAN_SIZE,
							IPC_CREAT | 0777);
	if(shmid < 0) {
		ki_printf("sound_init: shmget ret: %d, errno: %d\n", shmid,
			errno);
		return 0;
	}

	g_sound_shm_addr = shmat(shmid, 0, 0);
	if((int)PTR2WORD(g_sound_shm_addr) == -1) {
		ki_printf("sound_init: shmat ret: %p, errno: %d\n", g_sound_shm_addr,
			errno);
		return 0;
	}

	ret = shmctl(shmid, IPC_RMID, 0);
	if(ret < 0) {
		ki_printf("sound_init: shmctl ret: %d, errno: %d\n", ret, errno);
		return 0;
	}

	/* prepare pipe so parent can signal child each other */
	/*  pipe[0] = read side, pipe[1] = write end */
	ret = pipe(&g_pipe_fd[0]);
	if(ret < 0) {
		ki_printf("sound_init: pipe ret: %d, errno: %d\n", ret, errno);
		return 0;;
	}
	ret = pipe(&g_pipe2_fd[0]);
	if(ret < 0) {
		ki_printf("sound_init: pipe ret: %d, errno: %d\n", ret, errno);
		return 0;
	}

    g_audio_devtype = devtype;
	pid = fork();
	switch(pid) {
	case 0:
		/* child */
		/* close stdin and write-side of pipe */
		close(0);
		/* Close other fds to make sure X window fd is closed */
		for(i = 3; i < 100; i++) {
			if((i != g_pipe_fd[0]) && (i != g_pipe2_fd[1])) {
				close(i);
			}
		}
		close(g_pipe_fd[1]);		/*make sure write pipe closed*/
		close(g_pipe2_fd[0]);		/*make sure read pipe closed*/
		child_sound_loop(devtype, g_pipe_fd[0], g_pipe2_fd[1], g_sound_shm_addr);
		my_exit(0);
	case -1:
		/* error */
		ki_printf("sound_init: fork ret: -1, errno: %d\n", errno);
		return 0;
	default:
		/* parent */
		/* close read-side of pipe1, and the write side of pipe2 */
		close(g_pipe_fd[0]);
		close(g_pipe2_fd[1]);
		doc_printf("Child is pid: %d\n", pid);
		return parent_sound_get_sample_rate(g_pipe2_fd[0]);
	}
}
#endif /* HAVE_SOUND_HPUX || HAVE_SOUND_LINUX */

#ifdef HAVE_SOUND_WIN32
static long
sound_init_device_win32()
{
	int	tid;
    long rate;
    
    ki_printf ("Sound shared memory size=%d\n", 
            SOUND_SHM_SAMP_SIZE * SAMPLE_CHAN_SIZE);
    
	g_sound_shm_addr = malloc(SOUND_SHM_SAMP_SIZE * SAMPLE_CHAN_SIZE);
    memset(g_sound_shm_addr,0,SOUND_SHM_SAMP_SIZE * SAMPLE_CHAN_SIZE);
    
    if (_pipe(g_pipe_fd,0,_O_BINARY) <0) {
        ki_printf ("Unable to create sound pipes\n");
        return 0;
    }
    _beginthreadex(NULL,0,child_sound_loop_win32,g_sound_shm_addr,0,(void *)&tid);
    rate = parent_sound_get_sample_rate(g_pipe_fd[0]);
    g_audio_devtype = SOUND_NATIVE;
    return rate;
}
#endif

#ifdef HAVE_SOUND_NATIVE
static long
parent_sound_get_sample_rate(int read_fd)
{
    word32	tmp;
	int	ret;

	ret = read(read_fd, &tmp, 4);
	if(ret != 4) {
		ki_printf("parent dying, could not get sample rate from child\n");
		my_exit(1);
    }
    if ((g_audio_devtype == SOUND_NATIVE) || (g_audio_devtype == SOUND_ALIB)) {
        close(read_fd);
    }
    return tmp;
}
#endif

static long
sound_init_device_sdl()
{
#ifdef HAVE_SOUND_SDL
    long rate;
    SDL_AudioSpec wanted;

    if(SDL_InitSubSystem(SDL_INIT_AUDIO)) {
      fprintf(stderr, "sdl: Couldn't init SDL_Audio: %s!\n", SDL_GetError());
      return 0;
    }

    /* Set the desired format */
    wanted.freq = g_preferred_rate;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = NUM_CHANNELS;
    wanted.samples = 512;
    wanted.callback = _snd_callback;
    wanted.userdata = NULL;

    /* Open audio, and get the real spec */
    if(SDL_OpenAudio(&wanted, &spec) < 0) {
        fprintf(stderr, "sdl: Couldn't open audio: %s!\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return 0;
    }
    /* Check everything */
    if(spec.channels != wanted.channels) {
        fprintf(stderr, "sdl: Couldn't get stereo audio format!\n");
        goto snd_error;
    }
    if(spec.format != wanted.format) {
        fprintf(stderr, "sdl: Couldn't get a supported audio format!\n");
        fprintf(stderr, "sdl: wanted %X, got %X\n",wanted.format,spec.format);
        goto snd_error;
    }
    if(spec.freq != wanted.freq) {
        fprintf(stderr, "sdl: wanted rate = %d, got rate = %d\n", wanted.freq, spec.freq);
    }
    /* Set things as they really are */
    rate = spec.freq;
    
    snd_buf = SOUND_SHM_SAMP_SIZE*SAMPLE_CHAN_SIZE;
    playbuf = (byte*) malloc(snd_buf);
    if (!playbuf)
        goto snd_error;
    g_playbuf_buffered = 0;

    ki_printf ("Sound shared memory size=%d\n", 
            SOUND_SHM_SAMP_SIZE * SAMPLE_CHAN_SIZE);
    
	g_sound_shm_addr = malloc(SOUND_SHM_SAMP_SIZE * SAMPLE_CHAN_SIZE);
    memset(g_sound_shm_addr,0,SOUND_SHM_SAMP_SIZE * SAMPLE_CHAN_SIZE);

    /* It's all good! */
    g_audio_devtype = SOUND_SDL;
    g_zeroes_buffered = 0;
    g_zeroes_seen = 0;
    /* Let's start playing sound */
    g_sound_paused = 0;
    SDL_PauseAudio(0);
    return rate;
    
  snd_error:
    /* Oops! Something bad happened, cleanup. */
    SDL_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    if(playbuf)
        free((void*)playbuf);
    playbuf = 0;
    snd_buf = 0;
    return 0;
#else
    return 0;
#endif
}

void
sound_shutdown()
{
    switch(g_audio_devtype) {
    case SOUND_NONE:
        return;
        break;
    case SOUND_NATIVE:                     /* native */
        sound_shutdown_native();
        break;
    case SOUND_SDL:                     /* SDL audio */
        sound_shutdown_sdl();
        break;
    case SOUND_ALIB:
        sound_shutdown_alib();
        break;
    default:
        ki_printf("sound_shutdown: unknown device type %d\n", g_audio_devtype);
        break;
    }
	if (g_sound_shm_addr)
	{
		free(g_sound_shm_addr);
		g_sound_shm_addr=NULL;
	}
    g_audio_devtype = SOUND_NONE;
}

static void
sound_shutdown_native()
{
#ifdef HAVE_SOUND_NATIVE
    
	if(g_pipe_fd[1] != -1) 
	{
        close(g_pipe_fd[1]);
		g_pipe_fd[1]=-1;
    }
	if(g_pipe_fd[0] != -1) 
	{
        close(g_pipe_fd[0]);
		g_pipe_fd[0]=-1;
    }

	ki_printf("wait for sound termination...\n");
	while(soundActive) Sleep(10);

	
#endif
}

static void
sound_shutdown_sdl()
{
#ifdef HAVE_SOUND_SDL
    SDL_CloseAudio();
    if(playbuf)
        free((void*)playbuf);
    playbuf = 0;
#endif
}

static void
sound_shutdown_alib()
{
#ifdef HAVE_SOUND_ALIB
    sound_shutdown_native();
#endif
}

void
sound_write(int real_samps, int size)
{
	DOC_LOG("sound_write", -1, g_last_sound_play_dsamp,
						(real_samps << 30) + size);
    switch(g_audio_devtype) {
    case SOUND_NONE:
        return;
        break;
    case SOUND_NATIVE:                     /* native */
        sound_write_native(real_samps, size);
        break;
    case SOUND_SDL:                     /* SDL audio */
        sound_write_sdl(real_samps, size);
        break;
    case SOUND_ALIB:
        sound_write_native(real_samps, size);
        break;
    default:
        ki_printf("sound_write: unknown device type %d\n", g_audio_devtype);
        break;
    }
}

static void
sound_write_native(int real_samps, int size)
{
#ifdef HAVE_SOUND_NATIVE
	word32	tmp;
	int	ret;

	if(real_samps) {
		tmp = size + 0xa2000000;
	} else {
		tmp = size + 0xa1000000;
	}

	/* Although this looks like a big/little-endian issue, since the */
	/*  child is also reading an int, it just works with no byte swap */
	ret = write(g_pipe_fd[1], &tmp, 4);
	if(ret != 4) {
		halt_printf("send_sound, wr ret: %d, errno: %d\n", ret, errno);
	}
#endif
}

static void
sound_write_sdl(int real_samps, int size)
{
#ifdef HAVE_SOUND_SDL
    int shm_read;

    if (real_samps) {
        shm_read = (g_sound_shm_pos - size + SOUND_SHM_SAMP_SIZE)%SOUND_SHM_SAMP_SIZE;
        SDL_LockAudio();
        while(size > 0) {
            if(g_playbuf_buffered >= snd_buf) {
                ki_printf("sound_write_sdl failed @%d, %d buffered, %d samples skipped\n",snd_write,g_playbuf_buffered, size);
                shm_read += size;
                shm_read %= SOUND_SHM_SAMP_SIZE;
                size = 0;
            } else {
                ((word32*)playbuf)[snd_write/SAMPLE_CHAN_SIZE] = g_sound_shm_addr[shm_read];
                shm_read++;
                if (shm_read >= SOUND_SHM_SAMP_SIZE)
                    shm_read = 0;
                snd_write += SAMPLE_CHAN_SIZE;
                if (snd_write >= snd_buf)
                    snd_write = 0;
                size--;
                g_playbuf_buffered += SAMPLE_CHAN_SIZE;
            }
        }
        assert((snd_buf+snd_write - snd_read)%snd_buf == g_playbuf_buffered%snd_buf);
        assert(g_sound_shm_pos == shm_read);
        SDL_UnlockAudio();
    }
    if(g_sound_paused && (g_playbuf_buffered > 0)) {
        ki_printf("Unpausing sound, %d buffered\n",g_playbuf_buffered);
        g_sound_paused = 0;
        SDL_PauseAudio(0);
    }
    if(!g_sound_paused && (g_playbuf_buffered <= 0)) {
        ki_printf("Pausing sound\n");
        g_sound_paused = 1;
        SDL_PauseAudio(1);
    }
#endif
}

#ifdef HAVE_SOUND_SDL
/* Callback for sound */
static void _snd_callback(void* userdata, Uint8 *stream, int len)
{
    int i;
    /* Slurp off the play buffer */
    assert((snd_buf+snd_write - snd_read)%snd_buf == g_playbuf_buffered%snd_buf);
    /*ki_printf("slurp %d, %d buffered\n",len, g_playbuf_buffered);*/
    for(i = 0; i < len; ++i) {
        if(g_playbuf_buffered <= 0) {
            stream[i] = 0;
        } else {
            stream[i] = playbuf[snd_read++];
            if(snd_read == snd_buf)
                snd_read = 0;
            g_playbuf_buffered--;
        }
    }
#if 0
    if (g_playbuf_buffered <= 0) {
        ki_printf("snd_callback: buffer empty, Pausing sound\n");
        g_sound_paused = 1;
        SDL_PauseAudio(1);
    }
#endif
    //ki_printf("end slurp %d, %d buffered\n",len, g_playbuf_buffered);
}
#endif

int
sound_enabled()
{
    return (g_audio_devtype != SOUND_NONE);
}

int
get_preferred_rate()
{
    return g_preferred_rate;
}

int
set_preferred_rate(int rate)
{
    long audio_rate;
    int devtype = g_audio_devtype;

    sound_shutdown();
    g_preferred_rate = rate;
    audio_rate = sound_init_device(devtype);
    if(audio_rate) {
        set_audio_rate(audio_rate);
    }
    else {
        sound_shutdown();
        return 0;
    }
    return 1;
}

int
get_audio_devtype()
{
    return g_audio_devtype;
}

int
set_audio_devtype(int val)
{
    int oldval = g_audio_devtype;
    long audio_rate;

    sound_shutdown();
    audio_rate = sound_init_device(val);
    ki_printf("sound_init: audio_rate = %ld\n",audio_rate);
    if(audio_rate) {
        set_audio_rate(audio_rate);
    }
    else {
        ki_printf("sound_init: couldn't initialize driver %d\n", val);
        sound_shutdown();
        audio_rate = sound_init_device(oldval);
        ki_printf("sound_init: audio_rate = %ld\n",audio_rate);
        if(audio_rate) {
            set_audio_rate(audio_rate);
        }
        else {
            ki_printf("sound_init: couldn't initialize driver %d\n", oldval);
            ki_printf("sound_init: disabling audio\n");
            sound_shutdown();
            set_audio_rate(g_preferred_rate);
        }
        return 0;
    }
    return 1;
}
