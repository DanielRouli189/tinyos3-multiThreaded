#ifndef __KERNEL_SOCKET_H__
#define __KERNEL_SOCKET_H__


#include "tinyos.h"
#include "util.h"
#include "kernel_dev.h"
#include "kernel_pipe.h"

typedef enum socket_type {
    SOCKET_LISTENER,
    SOCKET_UNBOUND,
    SOCKET_PEER
} socket_type;


typedef struct socket_listener {
    rlnode queue;
    CondVar req_available;
} socket_listener;

typedef struct socket_unbound {
    rlnode unbound_socket;
} socket_unbound;

typedef struct socket_peer {
    //socket_cb* peer;
    pipe_cb* read_pipe;
    pipe_cb* write_pipe;
} socket_peer;


typedef struct socket_control_block {
    unsigned int refcount;
    FCB* fcb;
    socket_type type;
    port_t port;

    union{
        socket_listener listener;
        socket_unbound unbound;
        socket_peer peer;
    };
} socket_cb;


typedef struct request_connection {
    int admitted; 
    socket_cb* peer;

    CondVar connected_cv;
    rlnode queue_node;
} request_connection;


socket_cb* PORTMAP[MAX_PORT + 1];


/**
    @brief Return a new socket bound on a port.

    This function returns a file descriptor for a new
    socket object.	If the @c port argument is NOPORT, then the 
    socket will not be bound to a port. Else, the socket
    will be bound to the specified port. 

    @param port the port the new socket will be bound to
    @returns a file id for the new socket, or NOFILE on error. Possible
        reasons for error:
        - the port is iilegal
        - the available file ids for the process are exhausted
*/
Fid_t sys_Socket(port_t port);


/**
	@brief Initialize a socket as a listening socket.

	A listening socket is one which can be passed as an argument to
	@c Accept. Once a socket becomes a listening socket, it is not
	possible to call any other functions on it except @c Accept, @Close
	and @c Dup2().

	The socket must be bound to a port, as a result of calling @c Socket.
	On each port there must be a unique listening socket (although any number
	of non-listening sockets are allowed).

	@param sock the socket to initialize as a listening socket
	@returns 0 on success, -1 on error. Possible reasons for error:
		- the file id is not legal
		- the socket is not bound to a port
		- the port bound to the socket is occupied by another listener
		- the socket has already been initialized
	@see Socket
 */
int sys_Listen(Fid_t sock);



/**
	@brief Wait for a connection.

	With a listening socket as its sole argument, this call will block waiting
	for a single @c Connect() request on the socket's port. 
	one which can be passed as an argument to @c Accept. 

	It is possible (and desirable) to re-use the listening socket in multiple successive
	calls to Accept. This is a typical pattern: a thread blocks at Accept in a tight
	loop, where each iteration creates new a connection, 
	and then some thread takes over the connection for communication with the client.

	@param sock the socket to initialize as a listening socket
	@returns a new socket file id on success, @c NOFILE on error. Possible reasons 
	    for error:
		- the file id is not legal
		- the file id is not initialized by @c Listen()
		- the available file ids for the process are exhausted
		- while waiting, the listening socket @c lsock was closed

	@see Connect
	@see Listen
 */
Fid_t sys_Accept(Fid_t lsock);


/**
	@brief Create a connection to a listener at a specific port.

	Given a socket @c sock and @c port, this call will attempt to establish
	a connection to a listening socket on that port. If sucessful, the
	@c sock stream is connected to the new stream created by the listener.

	The two connected sockets communicate by virtue of two pipes of opposite directions, 
	but with one file descriptor servicing both pipes at each end.

	The connect call will block for approximately the specified amount of time.
	The resolution of this timeout is implementation specific, but should be
	in the order of 100's of msec. Therefore, a timeout of at least 500 msec is
	reasonable. If a negative timeout is given, it means, "infinite timeout".

	@params sock the socket to connect to the other end
	@params port the port on which to seek a listening socket
	@params timeout the approximate amount of time to wait for a
	        connection.
	@returns 0 on success and -1 on error. Possible reasons for error:
	   - the file id @c sock is not legal (i.e., an unconnected, non-listening socket)
	   - the given port is illegal.
	   - the port does not have a listening socket bound to it by @c Listen.
	   - the timeout has expired without a successful connection.
*/
int sys_Connect(Fid_t sock, port_t port, timeout_t timeout);



/**
   @brief Shut down one direction of socket communication.

   With a socket which is connected to another socket, this call will 
   shut down one or the other direction of communication. The shut down
   of a direction has implications similar to those of a pipe's end shutdown.
   More specifically, assume that this end is socket A, connected to socket
   B at the other end. Then,

   - if `ShutDown(A, SHUTDOWN_READ)` is called, any attempt to call `Write(B,...)`
     will fail with a code of -1.
   - if ShutDown(A, SHUTDOWN_WRITE)` is called, any attempt to call `Read(B,...)`
     will first exhaust the buffered data and then will return 0.
   - if ShutDown(A, SHUTDOWN_BOTH)` is called, it is equivalent to shutting down
     both read and write.

   After shutdown of socket A, the corresponding operation `Read(A,...)` or `Write(A,...)`
   will return -1.

   Shutting down multiple times is not an error.
   
   @param sock the file ID of the socket to shut down.
   @param how the type of shutdown requested
   @returns 0 on success and -1 on error. Possible reasons for error:
       - the file id @c sock is not legal (a connected socket stream).
*/
int sys_ShutDown(Fid_t sock, shutdown_mode how);

int socket_read(void* socketcb_t, char* buf, unsigned int len); /**< @brief Forward declaration*/

int socket_write(void* socketcb_t, const char* buf, unsigned int len); /**< @brief Forward declaration*/

int socket_close(void* socketcb_t); /**< @brief Forward declaration */

#endif