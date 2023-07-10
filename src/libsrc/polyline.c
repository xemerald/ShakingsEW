/**
 *
 *
 */
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <float.h>
#include <math.h>
/* */
#include <dl_chain_list.h>
#include <polyline.h>

/* */
#define PI  3.141592653589793238462643383279f
#define PI2 6.283185307179586476925286766559f
#define RAD_TOLERANCE 1.25663706143591729539e-5 /* 4.0*eps*PI */

static void free_polyline_one( POLY_LINE_ENTITY * );

/**
 * @brief
 *
 * @param entry
 * @param path
 * @return int
 */
int polyline_read( void **entry, const char *path )
{
	int    i;
	int    pointcount = 0;
	int    maxpoint   = 65536;
	char   line[MAX_LINE_LENGTH];
	float *tmp_x = NULL;
	float *tmp_y = NULL;

	FILE             *fd;
	POLY_LINE_ENTITY *newpl = NULL;

/* Initialization */
	if ( (tmp_x = calloc(maxpoint, sizeof(float))) == NULL || (tmp_y = calloc(maxpoint, sizeof(float))) == NULL ) {
		fprintf(stderr, "polyline_read: Error allocate temporary memory for polygon line!\n");
		return -1;
	}
	memset(line, 0, sizeof(line));
/* Open the input file */
	if ( (fd = fopen(path, "r")) == NULL ) {
		fprintf(stderr, "polyline_read: Error opening polygon line file: %s!\n", path);
		return -1;
	}
/* */
	while ( fgets(line, sizeof(line) - 1, fd) != NULL ) {
		if ( !strlen(line) )
			continue;
	/* */
		for ( i = 0; i < MAX_LINE_LENGTH; i++ ) {
			if ( line[i] == '#' || line[i] == '\n' ) {
				break;
			}
			else if ( line[i] == '\t' || line[i] == ' ' ) {
				continue;
			}
			else if ( line[i] == '>' ) {
				if ( pointcount > 0 ) {
				/* */
					if ( (newpl = calloc(1, sizeof(POLY_LINE_ENTITY))) == NULL ) {
						fprintf(stderr, "polyline_read: Error allocate new memory for polygon line!\n");
						return -1;
					}
					newpl->points = pointcount;
					newpl->x      = calloc(pointcount + 1, sizeof(float));
					newpl->y      = calloc(pointcount + 1, sizeof(float));
					memcpy(newpl->x, tmp_x, pointcount * sizeof(float));
					memcpy(newpl->y, tmp_y, pointcount * sizeof(float));
				/* */
					dl_node_append( entry, newpl );
				}
				pointcount = 0;
			}
			else {
				if ( sscanf( line, "%f %f", tmp_x + pointcount, tmp_y + pointcount ) != 2 ) {
					fprintf(stderr, "polyline_read: Error reading boundary file: %s!\n", path);
					return -1;
				}
			/* */
				if ( ++pointcount > maxpoint ) {
					maxpoint *= 2;
					if ( (tmp_x = realloc(tmp_x, maxpoint * sizeof(float))) == NULL || (tmp_y = realloc(tmp_y, maxpoint * sizeof(float))) == NULL ) {
						fprintf(stderr, "polyline_read: Error re-allocate temporary memory for polygon line!\n");
						return -1;
					}
				}
			}
			break;
		}
	}

	fclose(fd);
	free(tmp_x);
	free(tmp_y);

	return 0;
}

/**
 * @brief Checks the position is whether inside the polygonal path or not
 *
 * @param ex
 * @param ey
 * @param entity
 * @param m
 * @return int
 */
int polyline_locpt_one( const float ex, const float ey, const POLY_LINE_ENTITY *entity, int *m )
{
	int          n = entity->points;
	const float *x = entity->x;
	const float *y = entity->y;
	const float *lastx = x + n - 1;
	const float *lasty = y + n - 1;

	double angle;
	double sum = 0.0;
	double theta0, thetap, thetai;
	double u = *x - ex;
	double v = *y - ey;

	if ( fabs(u) < DBL_EPSILON && fabs(v) < DBL_EPSILON ) return 0;

/* If the first point of path is equal to the last point of path, we should just skip the last point */
	if ( fabs(*x - *lastx) < DBL_EPSILON && fabs(*y - *lasty) < DBL_EPSILON ) {
		n--;
		lastx--;
		lasty--;
	}
/* The point of the path is less than 2, can't become a polygonal path */
	if ( n < 2 )
		return -2;

	*m = 0;
	theta0 = thetap = atan2(v, u);

	while( ++x <= lastx && ++y <= lasty ) {
		u = *x - ex;
		v = *y - ey;
	/* */
		if ( fabs(u) < DBL_EPSILON && fabs(v) < DBL_EPSILON )
			return 0;
	/* */
		thetai = atan2(v, u);
		angle  = fabs(thetai - thetap);
	/* */
		if ( fabs(angle - PI) < RAD_TOLERANCE )
			return 0;
		if ( angle > PI )
			angle -= PI2;
		if ( thetap > thetai )
			angle = -angle;
	/* */
		sum   += angle;
		thetap = thetai;
	}
/* */
	angle = fabs(theta0 - thetap);
/* */
	if ( fabs(angle - PI) < RAD_TOLERANCE )
		return 0;
	if ( angle > PI )
		angle -= PI2;
	if ( thetap > theta0 )
		angle = -angle;

	sum += angle;
/* SUM = 2*PI*m where m is the winding number */
	*m = (int)(fabs(sum)/PI2 + 0.2);
/* When winding number is zero here means the point is outside the path */
	if ( *m == 0 )
		return -1;
	if ( sum < 0.0 )
		*m = -(*m);

/* Return 1 here means the point is inside the path */
	return 1;
}

/**
 * @brief
 *
 * @param ex
 * @param ey
 * @param entry
 * @param m
 * @return int
 */
int polyline_locpt_all( const float ex, const float ey, const void *entry, int *m )
{
	int               result = -1;
	DL_NODE          *current;
	POLY_LINE_ENTITY *entity;

/* */
	DL_LIST_FOR_EACH_DATA(entry, current, entity) {
		if ( (result = polyline_locpt( ex, ey, entity, m )) > -1 )
			return result;
	}

	return -1;
}

/**
 * @brief
 *
 * @param entry
 * @param func
 * @return int
 */
int polyline_walk_all( const void *entry, void (*func)( POLY_LINE_ENTITY * ) )
{
	DL_NODE          *current;
	POLY_LINE_ENTITY *entity;

/* */
	DL_LIST_FOR_EACH_DATA(entry, current, entity) {
		func( entity );
	}

	return;
}

/**
 * @brief
 *
 * @param entry
 */
void polyline_free_all( void *entry )
{
	dl_list_destroy( entry, free_polyline_one );

	return;
}

/**
 * @brief
 *
 * @param entity
 */
static void free_polyline_one( POLY_LINE_ENTITY *entity )
{
	free(entity->x);
	free(entity->y);
	free(entity);

	return;
}
