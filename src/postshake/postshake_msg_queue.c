/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>

/* Earthworm environment header include */
#include <earthworm.h>
#include <mem_circ_queue.h>

/* Define global variables */
static pthread_mutex_t QueueMutex;

static QUEUE     MsgQueue;                      /* from queue.h, queue.c; sets up linked */


/*********************************************************************
 *  MsgQueueInit( ) -- Initialization function of message queue and  *
 *                     mutex.                                        *
 *  Arguments:                                                       *
 *    queueSize   = Size of queue.                                   *
 *    elementSize = Size of each element in queue.                   *
 *  Returns:                                                         *
 *     0 = Normal.                                                   *
 *********************************************************************/
int MsgQueueInit( unsigned long queueSize, unsigned long elementSize )
{
/* Create a Mutex to control access to queue */
	CreateSpecificMutex(&QueueMutex);

/* Initialize the message queue */
	initqueue( &MsgQueue, queueSize, elementSize + 1 );

	return 0;
}

/********************************************************************
 *  MsgDequeue( ) -- Pop-out received message from main queue.      *
 *  Arguments:                                                      *
 *    packetOut = Pointer to output buffer.                         *
 *    msgSize   = Pointer of packet length.                         *
 *    msgLogo   = Pointer of message logo.                          *
 *  Returns:                                                        *
 *     0 = Normal, data pop-out success.                            *
 *    <0 = Normal, there is no data inside main queue.              *
 ********************************************************************/
int MsgDequeue( void *packetOut, long *msgSize, MSG_LOGO *msgLogo )
{
	int      ret;

	RequestSpecificMutex(&QueueMutex);
	ret = dequeue(&MsgQueue, (char *)packetOut, msgSize, msgLogo);
	ReleaseSpecificMutex(&QueueMutex);

	return ret;
}

/************************************************************************
 *  MsgEnqueue( ) -- Stack received message into queue of station       *
 *                   or main queue.                                     *
 *  Arguments:                                                          *
 *    packetIn = Pointer to received packet from Palert or server.      *
 *    msgSize  = Packet length.                                         *
 *    msgLogo  = Message logo.                                          *
 *  Returns:                                                            *
 *     0 = Normal, all data have been stacked into queue.               *
 *    -1 = Error, queue cannot allocate memory, lost message.           *
 *    -2 = Error, should not happen now.                                *
 *    -3 = Error, main queue is lapped.                                 *
 ************************************************************************/
int MsgEnqueue( void *packetIn, long msgSize, MSG_LOGO msgLogo )
{
	int ret;

/* put it into the main queue */
	RequestSpecificMutex(&QueueMutex);
	ret = enqueue(&MsgQueue, (char *)packetIn, msgSize, msgLogo);
	ReleaseSpecificMutex(&QueueMutex);

	return ret;
}

/************************************************
 *  MsgQueueEnd( ) -- End process of message    *
 *                    queue.                    *
 *  Arguments:                                  *
 *    None.                                     *
 *  Returns:                                    *
 *    None.                                     *
 ************************************************/
void MsgQueueEnd( void )
{
	freequeue(&MsgQueue);
	CloseSpecificMutex(&QueueMutex);

	return;
}
