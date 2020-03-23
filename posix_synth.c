#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <mqueue.h>
//#include <time.h>
#include <semaphore.h>
#include <signal.h>
#include <alsa/asoundlib.h>

#include  "audio.h"
#include "components.h"


#define dbg 0
void *controller(void *arg);


static pthread_t cntr_thread;
static pthread_t osc_thread[2];
static pthread_t mixer_thread;
static pthread_t filter_thread;
static pthread_t audio_thread;

static sem_t controller_exit_sem;

const char osc1_mq_name[] = "/osc1_mq";
const char osc2_mq_name[] = "/osc2_mq";
const char mixer_mq_name[] = "/mixer_mq";
const char filter_mq_name[] = "/filter_mq";
const char volume_mq_name[] = "/vol_mq";
unsigned int wave_type;
unsigned int num_synth_cycles;


comp_thrd_arg_t osc_args[2];
comp_thrd_arg_t mixer_args;
comp_thrd_arg_t filter_args;
comp_thrd_arg_t volume_args;
comp_thrd_arg_t audio_args;

void create_threads(void)
{
	int td_stat;

	td_stat = pthread_create(&osc_thread[0], NULL, oscillator, &osc_args[0]);
    if (td_stat != 0)
    {
        printf("Error creating osc0_thread: %s\n", strerror(td_stat));
    }

    td_stat = pthread_create(&osc_thread[1], NULL, oscillator, &osc_args[1]);
    if (td_stat != 0)
    {
        printf("Error creating osc1_thread: %s\n", strerror(td_stat));
    }

    td_stat = pthread_create(&filter_thread, NULL, filter, &filter_args);
    if (td_stat != 0)
    {
        printf("Error creating filter_thread: %s\n", strerror(td_stat));
    }

    td_stat = pthread_create(&mixer_thread, NULL, mixer, &mixer_args);
    if (td_stat != 0)
    {
        printf("Error creating mixer_thread: %s\n", strerror(td_stat));
    }


	td_stat = pthread_create(&audio_thread, NULL, audio_playback, &audio_args);
    if (td_stat != 0)
    {
        printf("Error creating audio_thread: %s\n", strerror(td_stat));
    }


    //create controller thread
    td_stat = pthread_create(&cntr_thread, NULL, controller, NULL);
    if (td_stat != 0)
    {
        printf("Error creating cntr_thread: %s\n", strerror(td_stat));
    }



}

void cancel_threads(void)
{
	int td_stat;

	td_stat =pthread_cancel(osc_thread[0]);
	if(td_stat != 0)
	{
		printf("canceling osc1 thread: %s\n", strerror(td_stat));
	}

	td_stat =pthread_cancel(osc_thread[1]);
	if(td_stat != 0)
	{
		printf("canceling osc2 thread: %s\n", strerror(td_stat));
	}

	td_stat =pthread_cancel(mixer_thread);
	if(td_stat != 0)
	{
		printf("canceling mixer thread: %s\n", strerror(td_stat));
	}

	td_stat =pthread_cancel(filter_thread);
	if(td_stat != 0)
	{
		printf("canceling filter thread: %s\n", strerror(td_stat));
	}


	td_stat = pthread_cancel(audio_thread);
	if(td_stat != 0)
	{
		printf("canceling audio thread: %s\n", strerror(td_stat));
	}

}
static void signal_handler(int sig)
{
	 printf("signal received: %d\n", sig);

	cancel_threads();
	thread_ipc_close();
	sleep(1);
	audio_close();

	signal(sig, SIG_DFL);
	kill(getpid(),sig);
}

void create_controller_exit_semaphore()
{
	sem_t *p_sem;
	errno = 0;
	p_sem = sem_open("/controller_exit_sem", O_CREAT | O_EXCL, 0666, 0);
	if( p_sem == SEM_FAILED)
	{
		printf("sem_open for /controller_exit_sem failed: %s\n", strerror(errno));
	}
	else
	{
		controller_exit_sem = *p_sem;
	}
}

void close_controller_exit_semaphore()
{
	sem_close(&controller_exit_sem);
	sem_unlink("/controller_exit_sem");

}


int main(int argc, char *argv[])
{
	if(argc > 1)
	{
		if(strcmp(argv[1],"ramp") == 0)
		{
			wave_type = 0;
		}
		else if(strcmp(argv[1],"pulse") == 0)
		{
			wave_type = 1;
		}
		num_synth_cycles = (unsigned int)strtol(argv[2],NULL,10);
	}
	else
	{
		wave_type = 0;
		num_synth_cycles = 1;
	}



    osc_args[0].comp_index = 0;
    osc_args[0].misc_int_param = wave_type;
    strcpy(osc_args[0].connecting_mq_name,osc1_mq_name);
    osc_args[1].comp_index = 1;
    osc_args[1].misc_int_param = wave_type;
    strcpy(osc_args[1].connecting_mq_name,osc2_mq_name);
    mixer_args.comp_index = 0;
    strcpy(mixer_args.connecting_mq_name,mixer_mq_name);
    filter_args.comp_index = 0;
    strcpy(filter_args.connecting_mq_name,filter_mq_name);
    volume_args.comp_index = 0;
    strcpy(volume_args.connecting_mq_name,volume_mq_name);

    signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGABRT, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGKILL, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGPIPE, signal_handler);
	signal(SIGALRM, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGSTKFLT, signal_handler);
	signal(SIGSTOP, signal_handler);
	signal(SIGXCPU, signal_handler);
	signal(SIGXFSZ, signal_handler);
	signal(SIGVTALRM, signal_handler);
	signal(SIGPROF, signal_handler);


	audio_init();

	create_controller_exit_semaphore();

	wave_buffer_and_thread_ipc_init();

    //create component threads
	create_threads();


	errno = 0;
    if(sem_wait(&controller_exit_sem) == -1)
    {
    	printf("sem_wait in main for /controller_exit_sem failed: %s\n", strerror(errno));
    }

	cancel_threads();
	close_controller_exit_semaphore();
	thread_ipc_close();
	sleep(1);
	audio_close();
	printf("Exiting posix_synth.\n");
    return 0;
}


void *controller(void *arg)
{
    // initialize task
    printf("controller loop\n");
	unsigned char osc_test_array[2][3]={{7,10,10},{5,10,10}};
	unsigned char mixer_test_array[] = {10,10,10};
	unsigned char filter_test_array[] = {10,10,10};
	unsigned int timer;
	unsigned int sequence[18] = {37,40,41,42,44,42,41,37,40,41,42,41,40,41,40,35,40,33};
	unsigned int sequencer_index = 0;


	// main loop
	int exit_count;
	for(exit_count = num_synth_cycles; exit_count > 0; exit_count--)
	{
		for(timer = 0; timer < 100; timer++)
		{

			/*********** COMPONENT PARAMETER CONTROL SECTION ***************/
			// Filter sweep
			if(timer < 50)
			{
				filter_test_array[0] = (unsigned char)(2*(timer));
			}
			else
			{
				filter_test_array[0] = (unsigned char)(2*(100-timer));
			}

			if(99 < filter_test_array[0])
			{
				filter_test_array[0] = 99;
			}
			else if(1 > filter_test_array[0])
			{
				filter_test_array[0] = 1;
			}
			// Sequencer

			if(timer%2)
			{
				if(sequencer_index < 17)
				{
					sequencer_index++;
				}
				else
				{
					sequencer_index = 0;
				}
			}
			osc_test_array[0][0] = (unsigned char)(sequence[sequencer_index]-14);

			/*********** START SYNTHESIS CYCLE *************/
			usleep(1000);
			errno = 0;
			mq_send(osc_param_q[0],(const char*)osc_test_array[0],3,0);
#if(dbg)
			printf("osc_param_q[0]: %s\n", strerror(errno));
#endif
			errno = 0;
			mq_send(filter_param_q,(const char*)filter_test_array,3,0);
#if(dbg)
			printf("filter_param_q: %s\n", strerror(errno));
#endif
			errno = 0;
		    mq_send(osc_param_q[1],(const char*)osc_test_array[1],3,0);
#if(dbg)
		    printf("osc_param_q[1]: %s\n", strerror(errno));
#endif
			errno = 0;
			mq_send(mixer_param_q,(const char*)mixer_test_array,3,0);
#if(dbg)
			printf("mixer_param_q: %s\n", strerror(errno));
#endif
			usleep(1000);
			errno = 0;
			if((sem_post(&start_cycle_sem[0]) == -1)||(sem_post(&start_cycle_sem[1]) == -1))
			{
				printf("sem_post by controller for /start_cycle_sem failed: %s\n", strerror(errno));
			}

			usleep(50);
			if((sem_wait(&cycle_done_sem) == -1))
			{
				printf("sem_wait by controller for /cycle_done_sem failed: %s\n", strerror(errno));
			}


		}
	}
	if(sem_post(&controller_exit_sem) == -1)
	{
		printf("sem_post by controller for /controller_exit_sem failed: %s\n", strerror(errno));
	}
	return 0;
}
