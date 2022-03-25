#pragma once

/* Function prototype */
int  shakemap_msgqueue_init( unsigned long, unsigned long );    /* Initialization function of message queue and mutex */
int  shakemap_msgqueue_reinit( unsigned long, unsigned long );  /* Re-Initialization function of message queue and mutex */
int  shakemap_msgqueue_dequeue( void *, long * );               /* Pop-out received message from main queue */
int  shakemap_msgqueue_enqueue( void *, long );                 /* Stack received message into queue of station or main queue */
void shakemap_msgqueue_end( void );                             /* End process of message queue */
