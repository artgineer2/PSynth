
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <mqueue.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <time.h>
#include <errno.h>

#include "audio.h"
#include "components.h"
#include "component_value_arrays.h"

#define dbg 0

wave_buffer_t volume_out;

wave_buffer_t osc_out[2];
wave_buffer_t mixer_out;
wave_buffer_t filter_out;



 mqd_t osc_param_q[2];
 mqd_t mixer_param_q;
 mqd_t filter_param_q;
static unsigned int mq_prio = 0;


sem_t start_cycle_sem[2];
sem_t osc_sem[2];
sem_t mixer_sem;
sem_t filter_sem;
sem_t cycle_done_sem;


wave_buffer_t ramp;
wave_buffer_t sine;
wave_buffer_t pulse;

static void make_param_queue(const char *name, mqd_t *p_queue, int param_count)
{
	mqd_t temp_mqd;
	struct mq_attr attr, *p_attr;
	attr.mq_maxmsg = 1;
	attr.mq_msgsize = param_count;
	p_attr = &attr;
	errno = 0;
	temp_mqd = mq_open(name, O_CREAT | O_EXCL | O_RDWR | O_NONBLOCK, 0666, p_attr);
	if( temp_mqd == (mqd_t) -1)
	{
		printf("mq_open for /osc1_mq_name failed: %s\n", strerror(errno));
	}
	else
	{
		*p_queue = temp_mqd;
	}

}



void wave_buffer_and_thread_ipc_init(void)
{
	int i = 0;
	// initialize message queues
	make_param_queue("/osc1_mq", &osc_param_q[0],3);
	make_param_queue("/osc2_mq", &osc_param_q[1],3);
	make_param_queue("/mixer_mq", &mixer_param_q,3);
	make_param_queue("/filter_mq", &filter_param_q,3);

	struct mq_attr attr;
	mq_getattr(osc_param_q[0],&attr);
	printf("osc1_mq attrs: %ld,%ld\n", attr.mq_maxmsg, attr.mq_msgsize);
	mq_getattr(osc_param_q[1],&attr);
	printf("osc2_mq attrs: %ld,%ld\n", attr.mq_maxmsg, attr.mq_msgsize);
	mq_getattr(mixer_param_q,&attr);
	printf("mixer_mq attrs: %ld,%ld\n", attr.mq_maxmsg, attr.mq_msgsize);
	mq_getattr(filter_param_q,&attr);
	printf("filter_mq attrs: %ld,%ld\n", attr.mq_maxmsg, attr.mq_msgsize);



	// initialize output buffers
	for(i = 0; i < period_size; i++)
	{
		osc_out[0][i] = 0;
		osc_out[1][i] = 0;
		mixer_out[i] = 0;
		filter_out[i] = 0;
	}

	// put waves in wave table buffers

	for(int i = 0; i < period_size; i++)
	{
		ramp[i] = ((WAVE_AMPLITUDE*i)/period_size)-(WAVE_AMPLITUDE/2);
	}

	for(int i = 0; i < period_size; i++)
	{
		if(i < period_size/10)
		{
			pulse[i] = WAVE_AMPLITUDE;
		}
		else
		{
			pulse[i] = 0;
		}

	}

    for(int i = 0; i < period_size; i++)
    {
        sine[i] = (WAVE_AMPLITUDE/2)*sin((6.283*i)/period_size);
    }




	sem_t *p_sem;
	p_sem = sem_open("/start_cycle_sem0", O_CREAT | O_EXCL, 0666, 0);
	if( p_sem == SEM_FAILED)
	{
		printf("sem_open for /start_cycle_sem0 failed: %s\n", strerror(errno));
	}
	else
	{
		start_cycle_sem[0] = *p_sem;
	}
	p_sem = sem_open("/start_cycle_sem1", O_CREAT | O_EXCL, 0666, 0);
	if( p_sem == SEM_FAILED)
	{
		printf("sem_open for /start_cycle_sem1 failed: %s\n", strerror(errno));
	}
	else
	{
		start_cycle_sem[1] = *p_sem;
	}

	p_sem = sem_open("/osc_sem0", O_CREAT | O_EXCL, 0666, 0);
	if( p_sem == SEM_FAILED)
	{
		printf("sem_open for /osc_sem[0] failed: %s\n", strerror(errno));
	}
	else
	{
		osc_sem[0] = *p_sem;
	}

	errno = 0;
	p_sem = sem_open("/osc_sem1", O_CREAT | O_EXCL, 0666, 0);
	if( p_sem == SEM_FAILED)
	{
		printf("sem_open for /osc2_sem[1] failed: %s\n", strerror(errno));
	}
	else
	{
		osc_sem[1] = *p_sem;
	}
	errno = 0;
	p_sem = sem_open("/mixer_sem", O_CREAT | O_EXCL, 0666, 0);
	if( p_sem == SEM_FAILED)
	{
		printf("sem_open for /mixer_sem failed: %s\n", strerror(errno));
	}
	else
	{
		mixer_sem = *p_sem;

	}


	errno = 0;
	p_sem = sem_open("/filter_sem", O_CREAT | O_EXCL, 0666, 0);
	if( p_sem == SEM_FAILED)
	{
		printf("sem_open for /filter_sem failed: %s\n", strerror(errno));
	}
	else
	{
		filter_sem = *p_sem;
	}

	errno = 0;
	p_sem = sem_open("/cycle_done_sem", O_CREAT | O_EXCL, 0666, 0);
	if( p_sem == SEM_FAILED)
	{
		printf("sem_open for /cycle_done_sem failed: %s\n", strerror(errno));
	}
	else
	{
		cycle_done_sem = *p_sem;
	}

}

void thread_ipc_close(void)
{

	mq_close(osc_param_q[0]);
	mq_close(osc_param_q[1]);
	mq_close(mixer_param_q);
	mq_close(filter_param_q);

	mq_unlink("/osc1_mq");
	mq_unlink("/osc2_mq");
	mq_unlink("/mixer_mq");
	mq_unlink("/filter_mq");

	sem_close(&start_cycle_sem[0]);
	sem_close(&start_cycle_sem[1]);
	sem_close(&osc_sem[0]);
	sem_close(&osc_sem[1]);
	sem_close(&mixer_sem);
	sem_close(&filter_sem);
	sem_close(&cycle_done_sem);

	sem_unlink("/start_cycle_sem0");
	sem_unlink("/start_cycle_sem1");
	sem_unlink("/osc_sem0");
	sem_unlink("/osc_sem1");
	sem_unlink("/mixer_sem");
	sem_unlink("/filter_sem");
	sem_unlink("/cycle_done_sem");

}

void *oscillator(void *thrd_arg)
{
	// initialize task
	comp_thrd_arg_t *osc_data = (comp_thrd_arg_t *) thrd_arg;
	int osc_index = (*osc_data).comp_index;
	int wave_type = (*osc_data).misc_int_param;
	char param_str[4];

	wave_buffer_t wave;
	unsigned int tuning_divider = 9912;

	unsigned char scale_index ;

    unsigned int frequency;
    unsigned int j_int = 0;
	unsigned int wave_index = 0;

	if(wave_type == 0)
	{
		for(int i = 0; i < period_size; i++)
		{
			wave[i] = ramp[i];
		}
	}
	else
	{
		for(int i = 0; i < period_size; i++)
		{
			wave[i] = pulse[i];
		}

	}
	// main loop
	for(;;)
	{
        errno = 0;
        if(sem_wait(&start_cycle_sem[osc_index]) == -1)
        {
            printf("sem_wait for /start_cycle_sem failed: %s\n", strerror(errno));
        }

		/***************************** PROCESS START ******************************/
		mq_receive(osc_param_q[osc_index], param_str,3,&mq_prio);

		scale_index = (unsigned char) param_str[0];
		if(57 < scale_index )
        {
			scale_index  = 57;
        }
		frequency=pitch[scale[scale_index]-2]; // transpose C down to Bflat (-2 half-tones)
#if(dbg)
		printf("osc_out[%d] %s:%d\n",osc_index, (*osc_data).connecting_mq_name, frequency);
#endif

		for(int i = 0; i < period_size; i++)
		{
			osc_out[osc_index][i] = 2*wave[wave_index];
			j_int += 100*frequency;
			wave_index = j_int/tuning_divider;
			if(wave_index >= period_size)
			{
				j_int = 0;
				wave_index = 0;
			}
		}



		/****************************** PROCESS END  ***************************/

		errno = 0;
		if(sem_post(&osc_sem[osc_index]) == -1)
        {
            printf("sem_post for /osc_sem[%d] failed: %s\n",osc_index, strerror(errno));
        }
		else
		{
#if(dbg)
			printf("/osc_sems posted by oscillator\n");
#endif
		}

	}
	return 0;
}

void *filter(void *thrd_arg)
{
	// initialize task
	//comp_thrd_arg_t *filter_data = (comp_thrd_arg_t *) thrd_arg;
	double lp_b[3], lp_a[3];
	double lp_y[3], lp_x[3];

	char param_str[4];

	unsigned char filter_index;
	double osc_out1_double;
	// main loop
	for(;;)
	{

		errno = 0;
		if(sem_wait(&osc_sem[1]) == -1)
		{
			printf("sem_wait by filter for /mixer_sem failed: %s\n", strerror(errno));
		}



		/***************************** PROCESS START ******************************/

		mq_receive(filter_param_q, param_str,3,&mq_prio);
		filter_index = (unsigned char) param_str[0];
		if(filter_index > 99)
		{
			filter_index = 99;
		}
#if(dbg)
		printf("filter loop: %s: %d\n", (*filter_data).connecting_mq_name,filter_index);
#endif
#if(dbg)
#endif
		lp_b[0] = lp[filter_index][0];
		lp_b[1] = lp[filter_index][1];
		lp_b[2] = lp[filter_index][2];
		lp_a[1] = lp[filter_index][4];
		lp_a[2] = lp[filter_index][5];

		for(int i = 0; i < period_size; i++)
		{
			osc_out1_double = (double)osc_out[1][i];
			lp_x[0] = osc_out1_double;
			lp_y[0] = lp_b[0]*lp_x[0] + lp_b[1]*lp_x[1] + lp_b[2]*lp_x[2] - lp_a[1]*lp_y[1] - lp_a[2]*lp_y[2];
			lp_x[2] = lp_x[1];
			lp_x[1] = lp_x[0];

			lp_y[2] = lp_y[1];
			lp_y[1] = lp_y[0];

			filter_out[i] = (short)lp_y[0];
		}


		/****************************** PROCESS END  ***************************/
		errno = 0;
		if(sem_post(&filter_sem) == -1)
        {
            printf("sem_post for /filter_sem failed: %s\n", strerror(errno));
        }
		else
		{
#if(dbg)
			printf("/filter_sem posted by filter\n");
#endif
		}
	}
	return 0;
}


void *mixer(void *thrd_arg)
{
	// initialize task
	//comp_thrd_arg_t *mixer_data = (comp_thrd_arg_t *) thrd_arg;
	char param_str[4];

	unsigned char input1_level;
	unsigned char input2_level;
	unsigned char output_level;

	unsigned char temp_byte ;

	// main loop
	for(;;)
	{
		errno = 0;
		if(sem_wait(&osc_sem[0]) == -1)
		{
			printf("sem_wait by mixer for /osc_sem failed: %s\n", strerror(errno));
		}

		errno = 0;
		if(sem_wait(&filter_sem) == -1)
		{
			printf("sem_wait by mixer for /filter failed: %s\n", strerror(errno));
		}

		/***************************** PROCESS START ******************************/

		mq_receive(mixer_param_q, param_str,3,&mq_prio);
		temp_byte = (unsigned char) param_str[0];
		input1_level = temp_byte;
		temp_byte = (unsigned char) param_str[1];
		input2_level = temp_byte;
		temp_byte = (unsigned char) param_str[2];
		output_level = temp_byte;
#if(dbg)
		printf("mixer loop: %s:%d:%d:%d\n", (*mixer_data).connecting_mq_name,input1_level,input2_level,output_level);
#endif

		for(int i = 0; i < period_size; i++)
		{
			mixer_out[i] = (output_level*(input1_level*osc_out[0][i] + input2_level*filter_out[i]))/100;
		}

		/****************************** PROCESS END  ***************************/

		errno = 0;
		if(sem_post(&mixer_sem) == -1)
        {
#if(dbg)
            printf("sem_post for /mixer_sem failed: %s\n", strerror(errno));
#endif
        }
	}
	return 0;
}



