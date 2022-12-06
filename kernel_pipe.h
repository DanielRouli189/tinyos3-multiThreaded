#ifndef __KERNEL_PIPE_H
#define __KERNEL_PIPE_H


/**
  @file kernel_pipe.h
  @brief TinyOS kernel: 

  @defgroup 
  @ingroup kernel
  @brief 

  

  @{
*/ 

#include "tinyos.h"
#include "kernel_proc.h"
#include "kernel_sched.h"
#include "kernel_streams.h"


/*******************************************
 *
 * Pipes
 *
 *******************************************/



typedef struct pipe_control_block {
    FCB* reader;
    FCB* writer;

    CondVar has_space;
    CondVar has_data;

    int w_position;
    int r_position;

    char BUFFER[PIPE_BUFFER_SIZE];
} pipe_cb;


/**
	@brief Construct and return a pipe.

	A pipe is a one-directional buffer accessed via two file ids,
	one for each end of the buffer. The size of the buffer is 
	implementation-specific, but can be assumed to be between 4 and 16 
	kbytes. 

	Once a pipe is constructed, it remains operational as long as both
	ends are open. If the read end is closed, the write end becomes 
	unusable: calls on @c Write to it return error. On the other hand,
	if the write end is closed, the read end continues to operate until
	the buffer is empty, at which point calls to @c Read return 0.

	@param pipe a pointer to a pipe_t structure for storing the file ids.
	@returns 0 on success, or -1 on error. Possible reasons for error:
		- the available file ids for the process are exhausted.
*/
int sys_Pipe(pipe_t* pipe);

/** @} */

#endif 