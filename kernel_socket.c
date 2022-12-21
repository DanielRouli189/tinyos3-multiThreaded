
#include "tinyos.h"
#include "kernel_cc.h"
#include "kernel_streams.h"
#include "kernel_socket.h"
#include "kernel_pipe.h"


file_ops socket_operations = {
	.Open = (void*)return_error,
	.Read = socket_read,
	.Write = socket_write,
	.Close = socket_close
};


socket_cb* init_socket(port_t port, FCB* fcb[1]){
	socket_cb* scb = (socket_cb*)xmalloc(sizeof(socket_cb));

	scb->refcount = 1;
	scb->port = port;
	scb->fcb = fcb[0];
	scb->type = SOCKET_UNBOUND;

	fcb[0]->streamobj = scb;
	fcb[0]->streamfunc = & socket_operations;

	return scb;
}

request_connection* init_request_connection(socket_cb* peer) {
	request_connection* rc = (request_connection*)xmalloc(sizeof(request_connection));

	rc->admitted = 0;
	rc->peer = peer;
	rc->connected_cv = COND_INIT;
	rlnode_init(&rc->queue_node, rc);

	return rc;
}


Fid_t sys_Socket(port_t port)
{	
	if(port < NOPORT || port > MAX_PORT) return	-1;

	Fid_t fid[1];
	FCB* fcb[1];

	int isReserved = FCB_reserve(1, fid, fcb);

	if(!isReserved) return -1;

	socket_cb* scb = init_socket(port, fcb);


	if(PORTMAP[port] == NULL) PORTMAP[port] = scb;

	return fid[0];
}

int sys_Listen(Fid_t sock)
{
	FCB* fcb  = get_fcb(sock);

	if(fcb == NULL || fcb->streamfunc != &socket_operations) return -1;

	/** illegal file id */
	socket_cb* scb = fcb->streamobj;
	if(scb == NULL) return -1;

	/** socket is not bound on a port */
	if(scb->port <= NOPORT || scb->port > MAX_PORT) return -1;

	/** port bound on the socket is occupied by another listener */
	if(PORTMAP[scb->port]->type == SOCKET_LISTENER) return -1;

	/** socket is already initialized */
	if(scb->type != SOCKET_UNBOUND) return -1;

	scb->type = SOCKET_LISTENER;
	rlnode_init(&scb->listener.queue, NULL);
	scb->listener.req_available = COND_INIT;
	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{
	FCB* fcb = get_fcb(lsock);

	if(fcb == NULL || fcb->streamfunc != &socket_operations) return NOFILE;

	socket_cb* scb = fcb->streamobj;

	/** illegal file id*/
	if(scb == NULL) return -1;

	/** socket is not bound on a port */
	if(scb->port <= NOPORT || scb->port > MAX_PORT) return -1;

	/** socket type is peer*/
	if(scb->type == SOCKET_PEER) return -1;

	/** port does not have listening socket bound to it */
	if(PORTMAP[scb->port]->type != SOCKET_LISTENER) return -1;
	
	scb->refcount++;
	socket_cb* port = PORTMAP[scb->port];

	while(is_rlist_empty(&PORTMAP[scb->port]->listener.queue)){
		kernel_wait(&PORTMAP[scb->port]->listener.req_available, SCHED_IO);
		if(PORTMAP[scb->port] == NULL) return -1;
	}

	Fid_t peerFid = sys_Socket(scb->port);
	if(peerFid == NOFILE) return -1;

	FCB* peerFcb = get_fcb(peerFid);
	if(peerFcb == NULL) return -1;

	socket_cb* peer = peerFcb->streamobj;
	if(peer == NULL) return -1;

	rlnode* requestNode = rlist_pop_front(& port->listener.queue);

	request_connection* reqConn = requestNode->obj;

	socket_cb* reqPeer = reqConn->peer;
	if(reqPeer == NULL) return -1;

	FCB* fcb_1[2] = {reqPeer->fcb, peerFcb};
	pipe_cb* pipe_1 = init_pipe(fcb_1);
	
	FCB* fcb_2[2] = {peerFcb, reqPeer->fcb};
	pipe_cb* pipe_2 = init_pipe(fcb_2);

	if(pipe_1 != NULL && pipe_2 != NULL) {
		peer->type = SOCKET_PEER;
		peer->peer.read_pipe = pipe_2;
		peer->peer.write_pipe = pipe_1;

		reqPeer->type = SOCKET_PEER;
		reqPeer->peer.read_pipe = pipe_1;
		reqPeer->peer.write_pipe = pipe_2;
	}

	reqConn->admitted = 1;
	kernel_signal(&reqConn->connected_cv);
	if(scb->refcount > 0) scb->refcount--;

	return peerFid;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	FCB* fcb = get_fcb(sock);
	int timeOut;

	if(fcb == NULL || fcb->streamfunc != &socket_operations) return NOFILE;

	if(port <= NOPORT || port > MAX_PORT) return -1;

	socket_cb* peer = fcb->streamobj;

	/** illegal file id*/
	if(peer == NULL) return -1;

	socket_cb* listener = PORTMAP[port];

	if(peer->type != SOCKET_UNBOUND) return -1;

	if(listener == NULL) return -1;

	if(listener->type != SOCKET_LISTENER) return -1;

	peer->refcount++;

	request_connection* rc = init_request_connection(peer);

	rlist_push_back(&PORTMAP[port]->listener.queue, &rc->queue_node);

	kernel_signal(&PORTMAP[port]->listener.req_available);

	while(rc->admitted == 0)
	{
		timeOut = kernel_timedwait(&rc->connected_cv, SCHED_IO, timeout);
		if(!timeOut) return -1; 
	}
	
	if(peer->refcount > 0)
		peer->refcount--;	

	rc = NULL;
	free(rc);
	return 0;	


}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{	
	int read, write;
	FCB* fcb = get_fcb(sock);

	if(fcb == NULL || fcb->streamfunc != &socket_operations) return NOFILE;

	if(how <1 || how > 3) return -1;

	socket_cb* scb = fcb->streamobj;

	if(scb == NULL || scb->type != SOCKET_PEER) return -1;

	switch(how) {
		case SHUTDOWN_READ: 
			return pipe_reader_close(scb->peer.read_pipe);
		case SHUTDOWN_WRITE:
			return pipe_writer_close(scb->peer.write_pipe);
		case SHUTDOWN_BOTH:
			read = pipe_reader_close(scb->peer.read_pipe);
			write = pipe_writer_close(scb->peer.write_pipe);
			if(read != 0 || write != 0) return -1;

			return 0;
		default:	
			return -1;
	}
}



int socket_read(void* socketcb_t, char* buf, unsigned int len){
	socket_cb* scb = (socket_cb*) socketcb_t;

	if(scb == NULL) return -1;

	if(scb->type == SOCKET_PEER && scb->peer.read_pipe != NULL)
		return pipe_read(scb->peer.read_pipe, buf, len);

	return -1;
}


int socket_write(void* socketcb_t, const char* buf, unsigned int len){

	socket_cb* scb = (socket_cb*) socketcb_t;
	if(scb == NULL) return -1;

	if(scb->type == SOCKET_PEER && scb->peer.write_pipe != NULL)
		return pipe_write(scb->peer.write_pipe, buf, len);

	return -1;
}

int socket_close(void* socketcb_t){
	int read, write;
	socket_cb* scb = (socket_cb*) socketcb_t;

	if(scb == NULL) return -1;
	
	if(scb->type == SOCKET_PEER) {
		read = pipe_reader_close(scb->peer.read_pipe);
		write = pipe_writer_close(scb->peer.write_pipe);
		if(read != 0 || write != 0) return -1;
	}

	if(scb->type == SOCKET_LISTENER) {
		PORTMAP[scb->port] = NULL;
		kernel_signal(& scb->listener.req_available);
	}

	scb = NULL;
	return 0;
}
