#pragma once

#include <earthworm.h>

/* Function prototype */
int  MsgQueueInit( unsigned long, unsigned long );    /* Initialization function of message queue and mutex */
int  MsgDequeue( void *, long *, MSG_LOGO * );        /* Pop-out received message from main queue */
int  MsgEnqueue( void *, long, MSG_LOGO );            /* Stack received message into queue of station or main queue */
void MsgQueueEnd( void );                             /* End process of message queue */
