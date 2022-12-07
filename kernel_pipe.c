
#include "tinyos.h"
#include "kernel_pipe.h"


int sys_Pipe(pipe_t* pipe)
{
	return -1;
}


/** @brief Write operation.

Write up to 'size' bytes from 'buf' to the stream 'this'.
If it is not possible to write any data (e.g., a buffer is full),
the thread will block. 
*/
int pipe_write(void* pipecb_t, const char* buf, unsigned int n){
	return -1;
}

/** @brief Read operation.

Read up to 'size' bytes from stream 'this' into buffer 'buf'. 
If no data is available, the thread will block, to wait for data.
*/
int pipe_read(void* pipecb_t, char* buf, unsigned int n){
	return -1;
}


/** @brief Close operation.

Close the stream object, deallocating any resources held by it.
This function returns 0 is it was successful and -1 if not.

*/
int pipe_writer_close(void* _pipecb){
	return -1;
}

int pipe_reader_close(void* _pipecb){
	return -1;
}




