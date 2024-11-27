/**
 * @file shakereport_msg_queue.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-03-25
 *
 * @copyright Copyright (c) 2024
 *
 */
/* Standard C header include */
#include <stdint.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <mem_circ_queue.h>

/* Define global variables */
static mutex_t QueueMutex;
static QUEUE   MsgQueue;         /* from queue.h, queue.c; sets up linked */

/*
 * skrep_msgqueue_init() - Initialization function of message queue and mutex.
 */
int skrep_msgqueue_init( const unsigned long queue_size, const unsigned long element_size )
{
/* Create a Mutex to control access to queue */
	CreateSpecificMutex(&QueueMutex);
/* Initialize the message queue */
	initqueue( &MsgQueue, queue_size, element_size + 1 );

	return 0;
}

/*
 * skrep_msgqueue_end() - End process of message queue.
 */
void skrep_msgqueue_end( void )
{
	RequestSpecificMutex(&QueueMutex);
	freequeue(&MsgQueue);
	ReleaseSpecificMutex(&QueueMutex);
	CloseSpecificMutex(&QueueMutex);

	return;
}

/*
 * skrep_msgqueue_dequeue() - Pop-out received message from main queue.
 */
int skrep_msgqueue_dequeue( void *buffer, size_t *size, MSG_LOGO *logo )
{
	int      result;
	long int _size;

	RequestSpecificMutex(&QueueMutex);
	result = dequeue(&MsgQueue, (char *)buffer, &_size, logo);
	ReleaseSpecificMutex(&QueueMutex);
	*size = _size;

	return result;
}

/*
 * skrep_msgqueue_enqueue() - Put the compelete packet into the main queue.
 */
int skrep_msgqueue_enqueue( void *buffer, size_t size, MSG_LOGO logo )
{
	int result = 0;

/* put it into the main queue */
	RequestSpecificMutex(&QueueMutex);
	result = enqueue(&MsgQueue, (char *)buffer, size, logo);
	ReleaseSpecificMutex(&QueueMutex);

	if ( result != 0 ) {
		if ( result == -1 )
			logit("et", "shakereport: Main queue cannot allocate memory, lost message!\n");
		else if ( result == -2 )
			logit("et", "shakereport: Unknown error happened to main queue!\n");
		else if ( result == -3 )
			logit("et", "shakereport: Main queue has lapped, please check it!\n");
	}

	return result;
}
