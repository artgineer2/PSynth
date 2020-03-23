#ifndef _COMPONENTS_H
#define _COMPONENTS_H


#include "buffers.h"


extern comp_thrd_arg_t osc_args[2];
extern comp_thrd_arg_t mixer_args;
extern comp_thrd_arg_t filter_args;


extern  mqd_t osc_param_q[2];
extern  mqd_t mixer_param_q;
extern  mqd_t filter_param_q;


extern sem_t start_cycle_sem[2];
extern sem_t osc_sem[2];
extern sem_t mixer_sem;
extern sem_t filter_sem;
extern sem_t cycle_done_sem;

extern void wave_buffer_and_thread_ipc_init(void);
extern void thread_ipc_close(void);
extern void *oscillator(void *thrd_arg);
extern void *mixer(void *thrd_arg);
extern void *filter(void *thrd_arg);

extern wave_buffer_t osc_out[2];
extern wave_buffer_t mixer_out;
extern wave_buffer_t filter_out;
extern wave_buffer_t sine;
extern wave_buffer_t ramp;
extern wave_buffer_t pulse;



#endif
