#pragma once

#include <earthworm.h>

/* Function prototype */
int  psk_msgqueue_init( unsigned long, unsigned long );    /* Initialization function of message queue and mutex */
int  psk_msgqueue_dequeue( void *, long *, MSG_LOGO * );   /* Pop-out received message from main queue */
int  psk_msgqueue_enqueue( void *, long, MSG_LOGO );       /* Stack received message into queue of station or main queue */
void psk_msgqueue_end( void );                             /* End process of message queue */
