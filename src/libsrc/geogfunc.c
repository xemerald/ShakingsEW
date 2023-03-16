/* Standard C header include */
#include <math.h>
#include <float.h>


#define PI  3.141592653589793238462643383279f
#define PI2 6.283185307179586476925286766559f

/***********************************************************************************
 * coor2distf() Transforms the coordinate(latitude & longitude) into distance(unit: km) *
 ***********************************************************************************/
double coor2distf( const double elat, const double elon, const double slat, const double slon )
{
	const double avlat = (elat + slat)*0.5;

	double a = 1.840708 + avlat*(.0015269 + avlat*(-.00034 + avlat*(1.02337e-6)));
	double b = 1.843404 + avlat*(-6.93799e-5 + avlat*(8.79993e-6 + avlat*(-6.47527e-8)));

	a *= (slon - elon) * 60.0;
	b *= (slat - elat) * 60.0;

	return sqrt(a*a + b*b);
}


#define RAD_TOLERANCE 1.25663706143591729539e-5 /* 4.0*eps*PI */

/****************************************************************************
 * locpt() Checks the position is whether inside the polygonal path or not  *
 ****************************************************************************/
int locpt( const float ex, const float ey, const float *x, const float *y, int n, int *m )
{
	double angle;
	double sum = 0.0;
	double theta0, thetap, thetai;
	double u = *x - ex;
	double v = *y - ey;

	const float *lastx = x + n - 1;
	const float *lasty = y + n - 1;

	if ( fabs(u) < DBL_EPSILON && fabs(v) < DBL_EPSILON ) return 0;

/* If the first point of path is equal to the last point of path, we should just skip the last point */
	if ( fabs(*x - *lastx) < DBL_EPSILON && fabs(*y - *lasty) < DBL_EPSILON ) {
		n--;
		lastx--;
		lasty--;
	}

/* The point of the path is less than 2, can't become a polygonal path */
	if ( n < 2 ) return -1;

	*m = 0;
	theta0 = thetap = atan2(v, u);

	while( ++x <= lastx && ++y <= lasty ) {
		u = *x - ex;
		v = *y - ey;

		if ( fabs(u) < DBL_EPSILON && fabs(v) < DBL_EPSILON ) return 0;

		thetai = atan2(v, u);
		angle  = fabs(thetai - thetap);

		if ( fabs(angle - PI) < RAD_TOLERANCE ) return 0;
		if ( angle > PI ) angle -= PI2;
		if ( thetap > thetai ) angle = -angle;

		sum   += angle;
		thetap = thetai;
	}

	angle = fabs(theta0 - thetap);

	if ( fabs(angle - PI) < RAD_TOLERANCE ) return 0;
	if ( angle > PI ) angle -= PI2;
	if ( thetap > theta0 ) angle = -angle;

	sum += angle;

/* SUM = 2*PI*m where m is the winding number */
	*m = (int)(fabs(sum)/PI2 + 0.2);

/* When winding number is zero here means the point is outside the path */
	if ( *m == 0 ) return -1;
	if ( sum < 0.0 ) *m = -(*m);

/* Return 1 here means the point is inside the path */
	return 1;
}
