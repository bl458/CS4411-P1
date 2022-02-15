#ifndef EGOS_MATH_H
#define EGOS_MATH_H

#if defined(x86_32) || defined(x86_64)
#define __LITTLE_ENDIAN
#endif

#ifdef __LITTLE_ENDIAN
#define __HI(x) *(1+(int*)&x)
#define __LO(x) *(int*)&x
#define __HIp(x) *(1+(int*)x)
#define __LOp(x) *(int*)x
#else
#define __HI(x) *(int*)&x
#define __LO(x) *(1+(int*)&x)
#define __HIp(x) *(int*)x
#define __LOp(x) *(1+(int*)x)
#endif

#define	MAXFLOAT	((float)3.40282346638528860e+38)

/* isnan(x) returns 1 if x is NaN, else 0
 */
int isnan(double x);

/*
 * isfinite(x) returns 1 if is x is finite, else 0
 */
int isfinite(double x);
/*
 * copysign(x,y) returns a value with the magnitude of x and
 * with the sign bit of y.
 */
double copysign(double x, double y);

/*
 * exp(x) returns e^x (the exponential function of x)
 */
double exp(double x);
/*
 * ldexp(x, exp) multiplies x by 2^exp
 */
double ldexp(double x, int exp);

/*
 * Return x^y nearly rounded. In particular, pow(integer, integer)
 * always returns the correct integer provided it is representable.
 */
double pow(double x, double y);
/*
 * Return x mod y in exact arithmetic
 * Method: shift and subtract
 */
double fmod(double x, double y);
/*
 * modf(double x, double *iptr) 
 * return fraction part of x, and return x's integral part in *iptr.
 * Method:
 *	Bit twiddling.
 */
double modf(double x, double *iptr);
/*
 * scalbn (double x, int n)
 * scalbn(x,n) returns x* 2**n  computed by  exponent
 * manipulation rather than by actually performing an
 * exponentiation or a multiplication.
 */
double scalbn (double x, int n);

/*
 * Return correctly rounded square root of x.
 * Use the hardware sqrt if you have one.
 */
double sqrt(double x);

/*
 * cbrt(x) returns the cube root of x.
 */
double cbrt(double x);

/*
 * Computes sqrt(x^2 + y^2)
 */
double hypot(double x, double y);

/* remainder(x,p)
 * Return :
 * 	returns  x REM p  =  x - [x/p]*p as if in infinite
 * 	precise arithmetic, where [x/p] is the (infinite bit)
 *	integer nearest x/p (in half way case choose the even one).
 * Method :
 *	Based on fmod() return x-[x/p]chopped*p exactlp.
 */
double remainder(double x, double p);

/*
 * Return the logarithm of x
 */
double log(double x);

/*
 * Return the base 10 logarithm of x
 */
double log10(double x);

/*
 * ceil(x)
 * Return x rounded toward -inf to integral value
 * Method:
 *	Bit twiddling.
 * Exception:
 *	Inexact flag raised if x not equal to ceil(x).
 */
double ceil(double x);

/*
 * floor(x)
 * Return x rounded toward -inf to integral value
 * Method:
 *	Bit twiddling.
 * Exception:
 *	Inexact flag raised if x not equal to floor(x).
 */
double floor(double x);

/*
 * fabs(x) returns the absolute value of x.
 */
double fabs(double x);
/*
 * rint(x)
 * Return x rounded to integral value according to the prevailing
 * rounding mode.
 * Method:
 *	Using floating addition.
 * Exception:
 *	Inexact flag raised if x not equal to rint(x).
 */
double rint(double x);

/*
 * nextafter(x,y)
 * return the next machine floating-point number of x in the
 * direction toward y.
 */
double nextafter(double x, double y);

#endif // EGOS_MATH_H
