#ifndef __KERNEL_PIPE_H
#define __KERNEL_PIPE_h


/**
  @file kernel_pipe.h
  @brief TinyOS kernel: 

  @defgroup 
  @ingroup kernel
  @brief 


  @{
*/ 

#include "tinyos.h"
#include "kernel_dev.h"
#include "util.h"

/*******************************************
 *
 * Pipes
 *
 *******************************************/

#define PIPE_BUFFER_SIZE 8192

/**
  @brief The pipe control block

  An object of this type is associated to every thread. In this object
  are stored all the metadata that relate to the thread.
*/
typedef struct pipe_control_block {
  FCB* reader;
  FCB* writer;

  CondVar has_space; /**< @brief blocking writer if no space is available */
  CondVar has_data;  /**< @brief blocking reader until data are available */
  
  int w_position; /**< @brief write position in buffer */
  int r_position; /**< @brief read position in buffer */

  char BUFFER[PIPE_BUFFER_SIZE]; /**< @brief bounded (cyclic) byte buffer */
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


/** @brief Write operation.

Write up to 'size' bytes from 'buf' to the stream 'this'.
If it is not possible to write any data (e.g., a buffer is full),
the thread will block. 
The write function should return the number of bytes copied from buf, 
or -1 on error. 

Possible errors are:
- There was a I/O runtime problem.
*/
int pipe_write(void* pipecb_t, const char* buf, unsigned int n);

/** @brief Read operation.

Read up to 'size' bytes from stream 'this' into buffer 'buf'. 
If no data is available, the thread will block, to wait for data.
The Read function should return the number of bytes copied into buf, 
or -1 on error. The call may return fewer bytes than 'size', 
but at least 1. A value of 0 indicates "end of data".

Possible errors are:
- There was a I/O runtime problem.
*/
int pipe_read(void* pipecb_t, char* buf, unsigned int n);


/** @brief Close operation.

Close the stream object, deallocating any resources held by it.
This function returns 0 is it was successful and -1 if not.
Although the value in case of failure is passed to the calling process,
the stream should still be destroyed.

Possible errors are:
- There was a I/O runtime problem.
*/
int pipe_writer_close(void* _pipecb);

int pipe_reader_close(void* _pipecb);

#endif