/*
 * palert2ew_msg_queue.h
 *
 * Header file for construct main messages queue.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * August, 2020
 *
 */
#pragma once
/* */
#include <mem_circ_queue.h>
/* */
#define SKREP_GEN_MSG_LOGO_BY_SRC(PA2EW_MSG_SRC) \
		((MSG_LOGO){ (PA2EW_MSG_SRC), (PA2EW_MSG_SRC), (PA2EW_MSG_SRC) })

/* Function prototype */
int  skrep_msgqueue_init( const unsigned long, const unsigned long );  /* Initialization function of message queue and mutex */
void skrep_msgqueue_end( void );                                  /* End process of message queue */
int  skrep_msgqueue_dequeue( void *, size_t *, MSG_LOGO * );    /* Pop-out received message from main queue */
int  skrep_msgqueue_enqueue( void *, size_t, MSG_LOGO );        /* Put the compelete packet into the main queue. */
