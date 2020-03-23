#ifndef _AUDIO_H
#define _AUDIO_H

#include "buffers.h"


extern comp_thrd_arg_t audio_args;
extern sem_t audio_sem;

extern void audio_init ();
extern void *audio_playback(void *arg);
extern void audio_close(void);

extern wave_buffer_t volume_out;
extern pthread_mutex_t volume_out_mtx;

extern snd_pcm_sframes_t buffer_size;
extern snd_pcm_sframes_t period_size;

#endif
