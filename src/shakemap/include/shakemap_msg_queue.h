#pragma once

/* Function prototype */
int  MsgQueueInit( unsigned long, unsigned long );    /* Initialization function of message queue and mutex */
int  MsgQueueReInit( unsigned long, unsigned long );  /* Re-Initialization function of message queue and mutex */
int  MsgDequeue( void *, long * );                    /* Pop-out received message from main queue */
int  MsgEnqueue( void *, long );                      /* Stack received message into queue of station or main queue */
void MsgQueueEnd( void );                             /* End process of message queue */
