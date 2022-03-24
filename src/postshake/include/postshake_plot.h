/*
 *
 */
#pragma once

#define PLOT_NORMAL_POLY   0
#define PLOT_STRONG_POLY   1
/* */
typedef struct poly_line {
	long   points;
	float *x;
	float *y;
	struct poly_line *next;
} POLY_LINE;

/* Function prototype */
int  psk_plot_init( const float, const float, const float, const float );
int  psk_plot_sm_plot( void *, const char *, const char * );
int  psk_plot_polyline_read( const char *, int );
void psk_plot_end( void );
