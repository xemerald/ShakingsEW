/**
 * @file iirfilter.c
 * @author Benjamin Yang @ National Taiwan University (b98204032@gmail.com)
 * @brief Main source code for IIR filter related functions.
 * @version 1.0.1
 * @date 2024-04-05
 *
 * @copyright Copyright (c) 2019-now
 *
 */
#include <stdlib.h>
#include <string.h>
#include <complex.h>
#include <math.h>
#include <float.h>
/* */
#include <iirfilter.h>
/* */
#define PI  3.141592653589793238462643383279f
#define PI2 6.283185307179586476925286766559f

/* */
static int lowpass(
	const double _Complex *, const double _Complex *, const int *, const double, IIR_FILTER *
);
static int highpass(
	const double _Complex *, const double _Complex *, const int *, const double, IIR_FILTER *
);
static int bandpass(
	const double _Complex *, const double _Complex *, const int *, const double, const double, const double, IIR_FILTER *
);
static int bandreject(
	const double _Complex *, const double _Complex *, const int *, const double, const double, const double, IIR_FILTER *
);
/* */
static int bilinear2( IIR_FILTER * );
static int beroots( const int, double _Complex *, int *, double * );
static int buroots( const int, double _Complex *, int *, double * );
static int cutoff( double, IIR_FILTER * );
static double warp( const double, const double );

/**
 * @brief
 *
 * @param sample
 * @param filter
 * @param stage
 * @return double
 */
double iirfilter_apply( const double sample, const IIR_FILTER *filter, IIR_STAGE *stage )
{
	double output = 0;
	double b0, b1, b2;
	double a1, a2;

	for ( int i = 0; i < filter->nsects; i++ ) {
	/* */
		b0 = filter->sections[i].numerator[0] * sample;
		b1 = filter->sections[i].numerator[1] * stage[i].x1;
		b2 = filter->sections[i].numerator[2] * stage[i].x2;
		a1 = filter->sections[i].denominator[1] * stage[i].y1;
		a2 = filter->sections[i].denominator[2] * stage[i].y2;
	/* */
		output = (b0 + b1 + b2) - (a1 + a2);
	/* */
		stage[i].y2 = stage[i].y1;
		stage[i].y1 = output;
		stage[i].x2 = stage[i].x1;
		stage[i].x1 = sample;
	}

	return output;
}

/**
 * @brief
 *
 * @param order
 * @param filtertype
 * @param anproto
 * @param freql
 * @param freqh
 * @param delta
 * @return IIR_FILTER
 */
IIR_FILTER iirfilter_design( const int order, const int filtertype, const int anproto, const double freql, const double freqh, const double delta )
{
	const double freqhw = warp( freqh * delta / 2.0, 2.0 );
	const double freqlw = warp( freql * delta / 2.0, 2.0 );

	int             stype[MAX_NUM_SECTIONS];
	double _Complex poles[MAX_NUM_SECTIONS];
	double _Complex zeros[MAX_NUM_SECTIONS];

	double     dcvalue;
	IIR_FILTER result;

/* */
	memset(&result, 0, sizeof(IIR_FILTER));
/* Initialize the zeros complex array, it might be the most compatible way... */
	for ( int i = 0; i < MAX_NUM_SECTIONS; i++ )
		zeros[i] = 0.0 + 0.0 * _Complex_I;
/* */
	switch ( anproto ) {
	case IIR_BUTTERWORTH:
		result.nsects = buroots( order, poles, stype, &dcvalue );
		break;
	case IIR_BESSEL:
		result.nsects = beroots( order, poles, stype, &dcvalue );
		break;
	default:
		result.nsects = 0;
		return result;
	}
/* */
	switch ( filtertype ) {
	case IIR_BANDPASS_FILTER:
		bandpass( poles, zeros, stype, dcvalue, freqlw, freqhw, &result );
		break;
	case IIR_BANDREJECT_FILTER:
		bandreject( poles, zeros, stype, dcvalue, freqlw, freqhw, &result );
		break;
	case IIR_LOWPASS_FILTER:
		lowpass( poles, zeros, stype, dcvalue, &result );
		cutoff( freqhw, &result );
		break;
	case IIR_HIGHPASS_FILTER:
		highpass( poles, zeros, stype, dcvalue, &result );
		cutoff( freqlw, &result );
		break;
	default:
		return result;
	}
/* */
	bilinear2( &result );

	return result;
}

/**
 * @brief
 *
 * @param poles
 * @param zeros
 * @param rtype
 * @param dcvalue
 * @param filter
 * @return int
 */
static int lowpass(
	const double _Complex *poles, const double _Complex *zeros, const int *rtype, const double dcvalue, IIR_FILTER *filter
) {
	double scale;
	double tmppole;
	double tmpzero;

/* */
	for ( int i = 0; i < filter->nsects; i++ ) {
		switch ( rtype[i] ) {
		case IIR_COMPLEX_CONJ_PZPAIR:
			tmppole = creal(poles[i] * conj(poles[i]));
			tmpzero = creal(zeros[i] * conj(zeros[i]));
			scale   = tmppole / tmpzero;

			filter->sections[i].numerator[0] = tmpzero * scale;
			filter->sections[i].numerator[1] = -2.0 * creal(zeros[i]) * scale;
			filter->sections[i].numerator[2] = 1.0 * scale;
			filter->sections[i].denominator[0] = tmppole;
			filter->sections[i].denominator[1] = -2.0 * creal(poles[i]);
			filter->sections[i].denominator[2] = 1.0;
			break;
		case IIR_COMPLEX_CONJ_POLEPAIR:
			tmppole = creal(poles[i] * conj(poles[i]));

			filter->sections[i].numerator[0] = tmppole;
			filter->sections[i].numerator[1] = 0.0;
			filter->sections[i].numerator[2] = 0.0;
			filter->sections[i].denominator[0] = tmppole;
			filter->sections[i].denominator[1] = -2.0 * creal(poles[i]);
			filter->sections[i].denominator[2] = 1.0;
			break;
		case IIR_SINGLE_REALPOLE:
			tmppole = creal(poles[i]) * -1.0;

			filter->sections[i].numerator[0] = tmppole;
			filter->sections[i].numerator[1] = 0.0;
			filter->sections[i].numerator[2] = 0.0;
			filter->sections[i].denominator[0] = tmppole;
			filter->sections[i].denominator[1] = 1.0;
			filter->sections[i].denominator[2] = 0.0;
			break;
		default:
			continue;
		}
	}
/* */
	filter->sections[0].numerator[0] *= dcvalue;
	filter->sections[0].numerator[1] *= dcvalue;
	filter->sections[0].numerator[2] *= dcvalue;

	return 0;
}

/**
 * @brief
 *
 * @param poles
 * @param zeros
 * @param rtype
 * @param dcvalue
 * @param filter
 * @return int
 */
static int highpass(
	const double _Complex *poles, const double _Complex *zeros, const int *rtype, const double dcvalue, IIR_FILTER *filter
) {
	double scale;
	double tmppole;
	double tmpzero;

/* */
	for ( int i = 0; i < filter->nsects; i++ ) {
		switch ( rtype[i] ) {
		case IIR_COMPLEX_CONJ_PZPAIR:
			tmppole = creal(poles[i] * conj(poles[i]));
			tmpzero = creal(zeros[i] * conj(zeros[i]));
			scale   = tmppole / tmpzero;

			filter->sections[i].numerator[0] = 1.0 * scale;
			filter->sections[i].numerator[1] = -2.0 * creal(zeros[i]) * scale;
			filter->sections[i].numerator[2] = tmpzero * scale;
			filter->sections[i].denominator[0] = 1.0;
			filter->sections[i].denominator[1] = -2.0 * creal(poles[i]);
			filter->sections[i].denominator[2] = tmppole;
			break;
		case IIR_COMPLEX_CONJ_POLEPAIR:
			tmppole = creal(poles[i] * conj(poles[i]));

			filter->sections[i].numerator[0] = 0.0;
			filter->sections[i].numerator[1] = 0.0;
			filter->sections[i].numerator[2] = tmppole;
			filter->sections[i].denominator[0] = 1.0;
			filter->sections[i].denominator[1] = -2.0 * creal(poles[i]);
			filter->sections[i].denominator[2] = tmppole;
			break;
		case IIR_SINGLE_REALPOLE:
			tmppole = creal(poles[i]) * -1.0;

			filter->sections[i].numerator[0] = 0.0;
			filter->sections[i].numerator[1] = tmppole;
			filter->sections[i].numerator[2] = 0.0;
			filter->sections[i].denominator[0] = 1.0;
			filter->sections[i].denominator[1] = tmppole;
			filter->sections[i].denominator[2] = 0.0;
			break;
		default:
			continue;
		}
	}
/* */
	filter->sections[0].numerator[0] *= dcvalue;
	filter->sections[0].numerator[1] *= dcvalue;
	filter->sections[0].numerator[2] *= dcvalue;

	return 0;
}

/**
 * @brief
 *
 * @param poles
 * @param zeros
 * @param rtype
 * @param dcvalue
 * @param freql
 * @param freqh
 * @param filter
 * @return int
 */
static int bandpass(
	const double _Complex *poles, const double _Complex *zeros, const int *rtype, const double dcvalue, const double freql, const double freqh, IIR_FILTER *filter
) {
	const double a = PI2 * PI2 * freql * freqh;
	const double b = PI2 * (freqh - freql);

	int    nsects;
	double scale;

	double _Complex ctemp;
	double _Complex p1, p2;
	double _Complex z1, z2;
	double _Complex s, h;

	IIR_SECTION *sectptr = NULL;

/* */
	nsects  = filter->nsects;
	sectptr = filter->sections;
	for ( int i = 0; i < nsects; i++ ) {
		switch ( rtype[i] ) {
		case IIR_COMPLEX_CONJ_PZPAIR:
			ctemp  = b * zeros[i];
			ctemp *= ctemp;
			ctemp -= 4.0 * a;
			ctemp  = csqrt(ctemp);
			z1 = 0.5 * (b * zeros[i] + ctemp);
			z2 = 0.5 * (b * zeros[i] - ctemp);

			ctemp  = b * poles[i];
			ctemp *= ctemp;
			ctemp -= 4.0 * a;
			ctemp  = csqrt(ctemp);
			p1 = 0.5 * (b * poles[i] + ctemp);
			p2 = 0.5 * (b * poles[i] - ctemp);

			sectptr->numerator[0]   = creal(z1 * conj(z1));
			sectptr->numerator[1]   = -2.0 * creal(z1);
			sectptr->numerator[2]   = 1.0;
			sectptr->denominator[0] = creal(p1 * conj(p1));
			sectptr->denominator[1] = -2.0 * creal(p1);
			sectptr->denominator[2] = 1.0;
			sectptr++;

			sectptr->numerator[0]   = creal(z2 * conj(z2));
			sectptr->numerator[1]   = -2.0 * creal(z2);
			sectptr->numerator[2]   = 1.0;
			sectptr->denominator[0] = creal(p2 * conj(p2));
			sectptr->denominator[1] = -2.0 * creal(p2);
			sectptr->denominator[2] = 1.0;
			sectptr++;

			filter->nsects++;
			break;
		case IIR_COMPLEX_CONJ_POLEPAIR:
			ctemp  = b * poles[i];
			ctemp *= ctemp;
			ctemp -= 4.0 * a;
			ctemp  = csqrt(ctemp);
			p1 = 0.5 * (b * poles[i] + ctemp);
			p2 = 0.5 * (b * poles[i] - ctemp);

			sectptr->numerator[0]   = 0.0;
			sectptr->numerator[1]   = b;
			sectptr->numerator[2]   = 0.0;
			sectptr->denominator[0] = creal(p1 * conj(p1));
			sectptr->denominator[1] = -2.0 * creal(p1);
			sectptr->denominator[2] = 1.0;
			sectptr++;

			sectptr->numerator[0]   = 0.0;
			sectptr->numerator[1]   = b;
			sectptr->numerator[2]   = 0.0;
			sectptr->denominator[0] = creal(p2 * conj(p2));
			sectptr->denominator[1] = -2.0 * creal(p2);
			sectptr->denominator[2] = 1.0;
			sectptr++;

			filter->nsects++;
			break;
		case IIR_SINGLE_REALPOLE:
			sectptr->numerator[0]   = 0.0;
			sectptr->numerator[1]   = b;
			sectptr->numerator[2]   = 0.0;
			sectptr->denominator[0] = a;
			sectptr->denominator[1] = -b * creal(poles[i]);
			sectptr->denominator[2] = 1.0;
			sectptr++;
			break;
		default:
			continue;
		}
	}

/* */
	s = 0.0 + sqrt(a) * _Complex_I;
	h = 1.0 + 0.0 * _Complex_I;
/* */
	nsects = filter->nsects;
	sectptr = filter->sections;
	for ( int i = 0; i < nsects; i++ ) {
		ctemp  = (sectptr->numerator[2] * s + sectptr->numerator[1]) * s + sectptr->numerator[0];
		ctemp /= (sectptr->denominator[2] * s + sectptr->denominator[1]) * s + sectptr->denominator[0];
		h *= ctemp;
		sectptr++;
	}
/* */
	scale = dcvalue / sqrt(creal(h * conj(h)));
	filter->sections[0].numerator[0] *= scale;
	filter->sections[0].numerator[1] *= scale;
	filter->sections[0].numerator[2] *= scale;

	return 0;
}

/**
 * @brief
 *
 * @param poles
 * @param zeros
 * @param rtype
 * @param dcvalue
 * @param freql
 * @param freqh
 * @param filter
 * @return int
 */
static int bandreject(
	const double _Complex *poles, const double _Complex *zeros, const int *rtype, const double dcvalue, const double freql, const double freqh, IIR_FILTER *filter
) {
	const double a = PI2 * PI2 * freql * freqh;
	const double b = PI2 * (freqh - freql);

	int    nsects;
	double scale;

	double _Complex cinv;
	double _Complex ctemp;
	double _Complex p1, p2;
	double _Complex z1, z2;

	IIR_SECTION *sectptr = NULL;

/* */
	nsects  = filter->nsects;
	sectptr = filter->sections;
	for ( int i = 0; i < nsects; i++ ) {
		switch ( rtype[i] ) {
		case IIR_COMPLEX_CONJ_PZPAIR:
			cinv   = 1.0 / zeros[i];
			ctemp  = b * cinv;
			ctemp *= ctemp;
			ctemp -= 4.0 * a;
			ctemp  = csqrt(ctemp);
			z1 = 0.5 * (b * cinv + ctemp);
			z2 = 0.5 * (b * cinv - ctemp);

			cinv   = 1.0 / poles[i];
			ctemp  = b * cinv;
			ctemp *= ctemp;
			ctemp -= 4.0 * a;
			ctemp  = csqrt(ctemp);
			p1 = 0.5 * (b * cinv + ctemp);
			p2 = 0.5 * (b * cinv - ctemp);

			sectptr->numerator[0]   = creal(z1 * conj(z1));
			sectptr->numerator[1]   = -2.0 * creal(z1);
			sectptr->numerator[2]   = 1.0;
			sectptr->denominator[0] = creal(p1 * conj(p1));
			sectptr->denominator[1] = -2.0 * creal(p1);
			sectptr->denominator[2] = 1.0;
			sectptr++;

			sectptr->numerator[0]   = creal(z2 * conj(z2));
			sectptr->numerator[1]   = -2.0 * creal(z2);
			sectptr->numerator[2]   = 1.0;
			sectptr->denominator[0] = creal(p2 * conj(p2));
			sectptr->denominator[1] = -2.0 * creal(p2);
			sectptr->denominator[2] = 1.0;
			sectptr++;

			filter->nsects++;
			break;
		case IIR_COMPLEX_CONJ_POLEPAIR:
			cinv   = 1.0 / poles[i];
			ctemp  = b * cinv;
			ctemp *= ctemp;
			ctemp -= 4.0 * a;
			ctemp  = csqrt(ctemp);
			p1 = 0.5 * (b * cinv + ctemp);
			p2 = 0.5 * (b * cinv - ctemp);

			sectptr->numerator[0]   = a;
			sectptr->numerator[1]   = 0.0;
			sectptr->numerator[2]   = 1.0;
			sectptr->denominator[0] = creal(p1 * conj(p1));
			sectptr->denominator[1] = -2.0 * creal(p1);
			sectptr->denominator[2] = 1.0;
			sectptr++;

			sectptr->numerator[0]   = a;
			sectptr->numerator[1]   = 0.0;
			sectptr->numerator[2]   = 1.0;
			sectptr->denominator[0] = creal(p2 * conj(p2));
			sectptr->denominator[1] = -2.0 * creal(p2);
			sectptr->denominator[2] = 1.0;
			sectptr++;

			filter->nsects++;
			break;
		case IIR_SINGLE_REALPOLE:
			sectptr->numerator[0]   = a;
			sectptr->numerator[1]   = 0.0;
			sectptr->numerator[2]   = 1.0;
			sectptr->denominator[0] = -a * creal(poles[i]);
			sectptr->denominator[1] = b;
			sectptr->denominator[2] = -creal(poles[i]);
			sectptr++;
			break;
		default:
			continue;
		}
	}

/* */
	scale   = 1.0;
	nsects  = filter->nsects;
	sectptr = filter->sections;
	for ( int i = 0; i < nsects; i++ ) {
		scale *= sectptr->numerator[0] / sectptr->denominator[0];
		sectptr++;
	}
/* */
	scale = dcvalue / fabs(scale);
	filter->sections[0].numerator[0] *= scale;
	filter->sections[0].numerator[1] *= scale;
	filter->sections[0].numerator[2] *= scale;

	return 0;
}

/**
 * @brief
 *
 * @param filter
 * @return int
 */
static int bilinear2( IIR_FILTER *filter )
{
	double a0, a1, a2;
	double scale;

	for ( int i = 0; i < filter->nsects; i++ ) {
		a0 = filter->sections[i].denominator[0];
		a1 = filter->sections[i].denominator[1];
		a2 = filter->sections[i].denominator[2];

		scale = a2 + a1 + a0;
		filter->sections[i].denominator[0] = 1.0;
		filter->sections[i].denominator[1] = (2.0 * (a0 - a2)) / scale;
		filter->sections[i].denominator[2] = (a2 - a1 + a0) / scale;

		a0 = filter->sections[i].numerator[0];
		a1 = filter->sections[i].numerator[1];
		a2 = filter->sections[i].numerator[2];

		filter->sections[i].numerator[0] = (a2 + a1 + a0) / scale;
		filter->sections[i].numerator[1] = (2.0 * (a0 - a2)) / scale;
		filter->sections[i].numerator[2] = (a2 - a1 + a0) / scale;
	}

	return 0;
}

/**
 * @brief
 *
 * @param order
 * @param poles
 * @param rtype
 * @param dcvalue
 * @return int
 */
static int beroots( const int order, double _Complex *poles, int *rtype, double *dcvalue )
{
	int nsects;

	switch ( order ) {
		case 1:
			poles[0] = -1.0 + 0.0 * _Complex_I;
			rtype[0] = IIR_SINGLE_REALPOLE;
			break;
		case 2:
			poles[0] = -1.1016013 + 0.6360098 * _Complex_I;
			rtype[0] = IIR_COMPLEX_CONJ_POLEPAIR;
			break;
		case 3:
			poles[0] = -1.0474091 + 0.9992645 * _Complex_I;
			rtype[0] = IIR_COMPLEX_CONJ_POLEPAIR;
			poles[1] = -1.3226758 + 0.0 * _Complex_I;
			rtype[1] = IIR_SINGLE_REALPOLE;
			break;
		case 4:
			poles[0] = -0.9952088 + 1.2571058 * _Complex_I;
			rtype[0] = IIR_COMPLEX_CONJ_POLEPAIR;
			poles[1] = -1.3700679 + 0.4102497 * _Complex_I;
			rtype[1] = IIR_COMPLEX_CONJ_POLEPAIR;
			break;
		case 5:
			poles[0] = -0.9576766 + 1.4711244 * _Complex_I;
			rtype[0] = IIR_COMPLEX_CONJ_POLEPAIR;
			poles[1] = -1.3808774 + 0.7179096 * _Complex_I;
			rtype[1] = IIR_COMPLEX_CONJ_POLEPAIR;
			poles[2] = -1.5023160 + 0.0 * _Complex_I;
			rtype[2] = IIR_SINGLE_REALPOLE;
			break;
		case 6:
			poles[0] = -0.9306565 + 1.6618633 * _Complex_I;
			rtype[0] = IIR_COMPLEX_CONJ_POLEPAIR;
			poles[1] = -1.3818581 + 0.9714719 * _Complex_I;
			rtype[1] = IIR_COMPLEX_CONJ_POLEPAIR;
			poles[2] = -1.5714904 + 0.3208964 * _Complex_I;
			rtype[2] = IIR_COMPLEX_CONJ_POLEPAIR;
			break;
		case 7:
			poles[0] = -0.9098678 + 1.8364514 * _Complex_I;
			rtype[0] = IIR_COMPLEX_CONJ_POLEPAIR;
			poles[1] = -1.3789032 + 1.1915667 * _Complex_I;
			rtype[1] = IIR_COMPLEX_CONJ_POLEPAIR;
			poles[2] = -1.6120388 + 0.5892445 * _Complex_I;
			rtype[2] = IIR_COMPLEX_CONJ_POLEPAIR;
			poles[3] = -1.6843682 + 0.0 * _Complex_I;
			rtype[3] = IIR_SINGLE_REALPOLE;
			break;
		case 8:
			poles[0] = -0.8928710 + 1.9983286 * _Complex_I;
			rtype[0] = IIR_COMPLEX_CONJ_POLEPAIR;
			poles[1] = -1.3738431 + 1.3883585 * _Complex_I;
			rtype[1] = IIR_COMPLEX_CONJ_POLEPAIR;
			poles[2] = -1.6369417 + 0.8227968 * _Complex_I;
			rtype[2] = IIR_COMPLEX_CONJ_POLEPAIR;
			poles[3] = -1.7574108 + 0.2728679 * _Complex_I;
			rtype[3] = IIR_COMPLEX_CONJ_POLEPAIR;
			break;
		default:
			return -1;
	}
/* */
	nsects = order - order / 2;
	*dcvalue = 1.0;

	return nsects;
}

/**
 * @brief
 *
 * @param order
 * @param poles
 * @param rtype
 * @param dcvalue
 * @return int
 */
static int buroots( const int order, double _Complex *poles, int *rtype, double *dcvalue )
{
	const int half = order / 2;
	int    nsects = 0;
	double angle;

/* Test for odd order, and add pole at -1 */
	if ( half * 2 < order ) {
		poles[0] = -1.0 + 0.0 * _Complex_I;
		rtype[0] = IIR_SINGLE_REALPOLE;
		nsects   = 1;
	}

	for ( int i = 0; i < half; i++ ) {
		angle = PI * (0.5 + (2.0 * i + 1.0) / (2.0 * order));
		poles[nsects] = cos(angle) + sin(angle) * _Complex_I;
		rtype[nsects] = IIR_COMPLEX_CONJ_POLEPAIR;
		nsects++;
	}
/* */
	*dcvalue = 1.0;

	return nsects;
}

/**
 * @brief
 *
 * @param nfreq
 * @param filter
 * @return int
 */
static int cutoff( double nfreq, IIR_FILTER *filter )
{
	const double scale = PI2 * nfreq;
	const double sqscale = scale * scale;

/* */
	for ( int i = 0; i < filter->nsects; i++ ) {
		filter->sections[i].numerator[1]   /= scale;
		filter->sections[i].denominator[1] /= scale;
		filter->sections[i].numerator[2]   /= sqscale;
		filter->sections[i].denominator[2] /= sqscale;
	}

	return filter->nsects;
}

/**
 * @brief
 *
 * @param freq
 * @param delta
 * @return double
 */
static double warp( const double freq, const double delta )
{
	double result = PI * freq * delta;  /* Orig: PI2 * freq * delta * 0.5 */

	result  = 2.0 * tan(result) / delta;
	result /= PI2;

	return result;
}
