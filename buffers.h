#ifndef _BUFFERS_H
#define _BUFFERS_H


#define BUFFER_SIZE 8192
#define WAVE_AMPLITUDE 6000

typedef   short wave_buffer_t[BUFFER_SIZE];

typedef struct
{
    int comp_index;
    int misc_int_param;
	char connecting_mq_name[20];
} comp_thrd_arg_t;


#endif
