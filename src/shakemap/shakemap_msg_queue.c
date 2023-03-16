/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <mem_circ_queue.h>

/* Define global variables */
static pthread_mutex_t QueueMutex;
static QUEUE           MsgQueue;                      /* from queue.h, queue.c; sets up linked */


/*
 * shakemap_msgqueue_init() - Initialization function of message queue and
 *                            mutex.
 * Arguments:
 *   queueSize   = Size of queue.
 *   elementSize = Size of each element in queue.
 * Returns:
 *    0 = Normal.
 */
int shakemap_msgqueue_init( unsigned long queueSize, unsigned long elementSize )
{
/* Create a Mutex to control access to queue */
	CreateSpecificMutex(&QueueMutex);
/* Initialize the message queue */
	initqueue(&MsgQueue, queueSize, elementSize + 1);

	return 0;
}

/*
 * shakemap_msgqueue_reinit() - Re-Initialization function of message queue and
 *                              mutex.
 * Arguments:
 *   queueSize   = Size of queue.
 *   elementSize = Size of each element in queue.
 * Returns:
 *    0 = Normal.
 */
int shakemap_msgqueue_reinit( unsigned long queueSize, unsigned long elementSize )
{
/* Free the old queue */
	RequestSpecificMutex(&QueueMutex);
	freequeue(&MsgQueue);
/* Re-Initialize the message queue */
	initqueue(&MsgQueue, queueSize, elementSize + 1);
	ReleaseSpecificMutex(&QueueMutex);

	return 0;
}

/*
 * shakemap_msgqueue_dequeue() - Pop-out received message from main queue.
 * Arguments:
 *   packetOut = Pointer to output buffer.
 *   msgSize   = Pointer of packet length.
 * Returns:
 *    0 = Normal, data pop-out success.
 *   <0 = Normal, there is no data inside main queue.
 */
int shakemap_msgqueue_dequeue( void *packetOut, long *msgSize )
{
	int      ret;
	MSG_LOGO tmplogo;

	RequestSpecificMutex(&QueueMutex);
	ret = dequeue(&MsgQueue, (char *)packetOut, msgSize, &tmplogo);
	ReleaseSpecificMutex(&QueueMutex);

	return ret;
}

/*
 * shakemap_msgqueue_enqueue() - Stack received message into queue of station
 *                               or main queue.
 * Arguments:
 *   packetIn = Pointer to received packet from Palert or server.
 * Returns:
 *    0 = Normal, all data have been stacked into queue.
 *   -1 = Error, queue cannot allocate memory, lost message.
 *   -2 = Error, should not happen now.
 *   -3 = Error, main queue is lapped.
 */
int shakemap_msgqueue_enqueue( void *packetIn, long msgSize )
{
	int ret;

/* put it into the main queue */
	RequestSpecificMutex(&QueueMutex);
	ret = enqueue(&MsgQueue, (char *)packetIn, msgSize, (MSG_LOGO){ 0 });
	ReleaseSpecificMutex(&QueueMutex);

	return ret;
}

/*
 * shakemap_msgqueue_end( ) -- End process of message queue.                    
 * Arguments:
 *   None.
 * Returns:
 *   None.
 */
void shakemap_msgqueue_end( void )
{
	freequeue(&MsgQueue);
	CloseSpecificMutex(&QueueMutex);

	return;
}
