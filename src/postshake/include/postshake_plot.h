#pragma once


#define PLOT_NORMAL_POLY   0
#define PLOT_STRONG_POLY   1

typedef struct poly_line {
	long   points;
	float *x;
	float *y;
	struct poly_line *next;
} POLY_LINE;

/* Function prototype */
int PlotInit( const float, const float, const float, const float );
int PlotShakemap( void *, const char *, const char * );
int PlotReadPolyLine( const char *, int );

void PlotEnd( void );
