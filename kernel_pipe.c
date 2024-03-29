
#include "tinyos.h"
#include "kernel_pipe.h"
#include "kernel_cc.h"
#include "kernel_dev.h"
#include "kernel_streams.h"

static file_ops reader_operations = {
	.Open = (void*) return_error, /* This can be replaced with NULL or be put to comments*/
	.Read = pipe_read,
	.Write = return_error_const,
	.Close = pipe_reader_close
};

static file_ops writer_operations = {
	.Open = (void*) return_error, /* This can be replaced with NULL or be put to comments*/
	.Read = return_error,
	.Write = pipe_write,
	.Close = pipe_writer_close
};


/**
 * @brief Initialize a pipe. 
 * 
 * This function initializes a new pipe from the given FCB, 
 * assuming an FCB has been already reserved.
 * 
 * @param fcb 
 * @return pipe_cb* 
 */
pipe_cb* init_pipe(FCB* fcb[2]){
	pipe_cb* pipe = (pipe_cb*)xmalloc(sizeof(pipe_cb));

	pipe->reader = fcb[0];
	pipe->writer = fcb[1];
	
	pipe->has_space = COND_INIT;
	pipe->has_data = COND_INIT;
	
	pipe->w_position = 0;
	pipe->r_position = 0;

	return pipe;
}

/**
	@brief Construct and return a pipe.


	@param pipe a pointer to a pipe_t structure for storing the file ids
	@returns 0 on success, -1 on error.
*/
int sys_Pipe(pipe_t* pipe)
{
	Fid_t fid[2];
	FCB* fcb[2];

	int isReserved = FCB_reserve(2,fid,fcb);

	if(!isReserved) return -1;

	pipe->read = fid[0];
	pipe->write = fid[1];

	/*Pipe Control Block initialization*/
	pipe_cb* pipeCB = init_pipe(fcb);

	fcb[0]->streamobj = pipeCB;
	fcb[1]->streamobj = pipeCB;
	fcb[0]->streamfunc = &reader_operations;
	fcb[1]->streamfunc = &writer_operations;

	return 0;
}


/** @brief Write operation.

Write up to 'size' bytes from 'buf' to the stream 'this'.
If it is not possible to write any data (e.g., a buffer is full),
the thread will block. 
*/
int pipe_write(void* pipecb_t, const char* buf, unsigned int n){
	
	pipe_cb* pipe = (pipe_cb*) pipecb_t;
	
	if(pipe == NULL || pipe->reader == NULL || pipe->writer == NULL)
		return -1;

	int position = 0;
	
	
	while(check_condition(pipe)) 
		kernel_wait(&(pipe->has_space), SCHED_PIPE);

	
	while(position != n && position < PIPE_BUFFER_SIZE) {
		if(check_condition(pipe)) break;

		pipe->BUFFER[pipe->w_position] = buf[position];
		pipe->w_position = (pipe->w_position + 1)% PIPE_BUFFER_SIZE;
		position++;
	}

	kernel_broadcast(&(pipe->has_data));
	return position;
}

/**
 * @brief if the writing position is equal to the reading position, and
 * the reader is still open, return 1 else return 0.
 * 
 * @param pipe 
 * @return 0 or 1
 */
int check_condition(pipe_cb* pipe){
	return (pipe->w_position + 1)% PIPE_BUFFER_SIZE == pipe->r_position && pipe->reader != NULL;
}

/** @brief Read operation.

Read up to 'size' bytes from stream 'this' into buffer 'buf'. 
If no data is available, the thread will block, to wait for data.
*/
int pipe_read(void* pipecb_t, char* buf, unsigned int n){
	
	pipe_cb* pipe = (pipe_cb*) pipecb_t;

	if(pipe == NULL || pipe->reader == NULL) return -1;

	int position = 0;
	if(pipe->writer == NULL){
		while(pipe->r_position < pipe->w_position){
			if(position == n) return position;

			buf[position] = pipe->BUFFER[pipe->r_position];
			pipe->r_position = (pipe->r_position + 1) % PIPE_BUFFER_SIZE;
			position++;
		}

		return position;
	}

	while(pipe->r_position == pipe->w_position && pipe->writer != NULL)
		kernel_wait(&(pipe->has_data), SCHED_PIPE);

	while(position != n && position < PIPE_BUFFER_SIZE){
		if(pipe->r_position == pipe->w_position) break;

		buf[position] = pipe->BUFFER[pipe->r_position];
		pipe->r_position = (pipe->r_position + 1) % PIPE_BUFFER_SIZE;
		position++;
	}


	kernel_broadcast(&(pipe->has_space));
	return position;
}


/** @brief Close writer operation.

Close the stream object, deallocating any resources held by it.
This function returns 0 is it was successful and -1 if not.
	@param _pipecb pointer to void.
	@return 0 is it was successful and -1 if not.
*/
int pipe_writer_close(void* _pipecb){
	pipe_cb* pipe = (pipe_cb*) _pipecb;

	if(pipe == NULL) return -1;

	pipe->writer = NULL;

	if(pipe->reader == NULL){
		pipe = NULL;
		free(pipe);
		return 0;
	}
	
	kernel_broadcast(&(pipe->has_data));
	return 0;
}

/** @brief Close reader operation.

Close the stream object, deallocating any resources held by it.
This function returns 0 is it was successful and -1 if not.
	@param _pipecb pointer to void.
	returns 0 is it was successful and -1 if not.
*/
int pipe_reader_close(void* _pipecb){
	pipe_cb* pipe = (pipe_cb*) _pipecb;

	if(pipe == NULL) return -1;

	pipe->reader = NULL;

	if(pipe->writer == NULL){
		pipe = NULL;
		free(pipe);
		return 0;
	}
	

	kernel_broadcast(&(pipe->has_space));
	return 0;
}


int return_error(void* pipe_t, char *buf, unsigned int n){
	return -1;
}

int return_error_const(void* pipe_t, const char *buf, unsigned int n){
	return -1;
}
