#include <math.h>
#include <assert.h>
#include <sys/errno.h>

/* Partial C Math library implementation, cobbled together
 * from the Freely Distributable LibM (FDLIBM) at netlib.org.
 * All operations assume IEEE 754 compliant double-precision
 * floating point numbers.
 *
 * ====================================================
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/*
 * Constants:
 * The hexadecimal values are the intended ones for the following 
 * constants. The decimal values may be used, provided that the 
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */
static const double
zero = 0.0,
one	=  1.0,
two	=  2.0,
halF[2]	= {0.5,-0.5,},
huge	= 1.0e+300,
tiny   = 1.0e-300,
twom1000= 9.33263618503218878990e-302,     /* 2**-1000=0x01700000,0*/
o_threshold=  7.09782712893383973096e+02,  /* 0x40862E42, 0xFEFA39EF */
u_threshold= -7.45133219101941108420e+02,  /* 0xc0874910, 0xD52D3051 */
ln2HI[2]   ={ 6.93147180369123816490e-01,  /* 0x3fe62e42, 0xfee00000 */
	     -6.93147180369123816490e-01,},/* 0xbfe62e42, 0xfee00000 */
ln2LO[2]   ={ 1.90821492927058770002e-10,  /* 0x3dea39ef, 0x35793c76 */
	     -1.90821492927058770002e-10,},/* 0xbdea39ef, 0x35793c76 */
invln2 =  1.44269504088896338700e+00, /* 0x3ff71547, 0x652b82fe */
two54   =  1.80143985094819840000e+16, /* 0x43500000, 0x00000000 */
twom54  =  5.55111512312578270212e-17, /* 0x3C900000, 0x00000000 */
ln2_hi  =  6.93147180369123816490e-01,	/* 3fe62e42 fee00000 */
ln2_lo  =  1.90821492927058770002e-10,	/* 3dea39ef 35793c76 */
Lg1 = 6.666666666666735130e-01,  /* 3FE55555 55555593 */
Lg2 = 3.999999999940941908e-01,  /* 3FD99999 9997FA04 */
Lg3 = 2.857142874366239149e-01,  /* 3FD24924 94229359 */
Lg4 = 2.222219843214978396e-01,  /* 3FCC71C5 1D8E78AF */
Lg5 = 1.818357216161805012e-01,  /* 3FC74664 96CB03DE */
Lg6 = 1.531383769920937332e-01,  /* 3FC39A09 D078C69F */
Lg7 = 1.479819860511658591e-01,  /* 3FC2F112 DF3E5244 */
ivln10     =  4.34294481903251816668e-01, /* 0x3FDBCB7B, 0x1526E50E */
log10_2hi  =  3.01029995663611771306e-01, /* 0x3FD34413, 0x509F6000 */
log10_2lo  =  3.69423907715893078616e-13, /* 0x3D59FEF3, 0x11F12B36 */
C =  5.42857142857142815906e-01, /* 19/35     = 0x3FE15F15, 0xF15F15F1 */
D = -7.05306122448979611050e-01, /* -864/1225 = 0xBFE691DE, 0x2532C834 */
E =  1.41428571428571436819e+00, /* 99/70     = 0x3FF6A0EA, 0x0EA0EA0F */
F =  1.60714285714285720630e+00, /* 45/28     = 0x3FF9B6DB, 0x6DB6DB6E */
G =  3.57142857142857150787e-01, /* 5/14      = 0x3FD6DB6D, 0xB6DB6DB7 */
TWO52[2]={
  4.50359962737049600000e+15, /* 0x43300000, 0x00000000 */
 -4.50359962737049600000e+15, /* 0xC3300000, 0x00000000 */
},
bp[] = {1.0, 1.5,},
dp_h[] = { 0.0, 5.84962487220764160156e-01,}, /* 0x3FE2B803, 0x40000000 */
dp_l[] = { 0.0, 1.35003920212974897128e-08,}, /* 0x3E4CFDEB, 0x43CFD006 */
two53	=  9007199254740992.0,	/* 0x43400000, 0x00000000 */
	/* poly coefs for (3/2)*(log(x)-2s-2/3*s**3 */
L1  =  5.99999999999994648725e-01, /* 0x3FE33333, 0x33333303 */
L2  =  4.28571428578550184252e-01, /* 0x3FDB6DB6, 0xDB6FABFF */
L3  =  3.33333329818377432918e-01, /* 0x3FD55555, 0x518F264D */
L4  =  2.72728123808534006489e-01, /* 0x3FD17460, 0xA91D4101 */
L5  =  2.30660745775561754067e-01, /* 0x3FCD864A, 0x93C9DB65 */
L6  =  2.06975017800338417784e-01, /* 0x3FCA7E28, 0x4A454EEF */
P1   =  1.66666666666666019037e-01, /* 0x3FC55555, 0x5555553E */
P2   = -2.77777777770155933842e-03, /* 0xBF66C16C, 0x16BEBD93 */
P3   =  6.61375632143793436117e-05, /* 0x3F11566A, 0xAF25DE2C */
P4   = -1.65339022054652515390e-06, /* 0xBEBBBD41, 0xC5D26BF1 */
P5   =  4.13813679705723846039e-08, /* 0x3E663769, 0x72BEA4D0 */
lg2  =  6.93147180559945286227e-01, /* 0x3FE62E42, 0xFEFA39EF */
lg2_h  =  6.93147182464599609375e-01, /* 0x3FE62E43, 0x00000000 */
lg2_l  = -1.90465429995776804525e-09, /* 0xBE205C61, 0x0CA86C39 */
ovt =  8.0085662595372944372e-0017, /* -(1024-log2(ovfl+.5ulp)) */
cp    =  9.61796693925975554329e-01, /* 0x3FEEC709, 0xDC3A03FD =2/(3ln2) */
cp_h  =  9.61796700954437255859e-01, /* 0x3FEEC709, 0xE0000000 =(float)cp */
cp_l  = -7.02846165095275826516e-09, /* 0xBE3E2FE0, 0x145B01F5 =tail of cp_h*/
ivln2    =  1.44269504088896338700e+00, /* 0x3FF71547, 0x652B82FE =1/ln2 */
ivln2_h  =  1.44269502162933349609e+00, /* 0x3FF71547, 0x60000000 =24b 1/ln2*/
ivln2_l  =  1.92596299112661746887e-08; /* 0x3E54AE0B, 0xF85DDF44 =1/ln2 tail*/


static const unsigned
B1 = 715094163, /* B1 = (682-0.03306235651)*2**20 */
B2 = 696219795; /* B2 = (664-0.03306235651)*2**20 */

static const double Zero[] = {0.0, -0.0,};

double exp(double x) {

	double y,hi,lo,c,t;
	int k, xsb;
	unsigned hx;

	hx  = __HI(x); 			/* high word of x */
	xsb = (hx>>31)&1;		/* sign bit of x */
	hx &= 0x7fffffff;		/* high word of |x| */

	/* filter out non-finite argument */
	if(hx >= 0x40862E42) {			/* if |x|>=709.78... */
		if(hx >= 0x7ff00000) {
			if(((hx & 0xfffff) | __LO(x))!=0)
				return x+x; 		/* NaN */
			else return (xsb == 0) ? x : 0.0;	/* exp(+-inf)={inf,0} */
		}
		if(x > o_threshold) return huge*huge; /* overflow */
		if(x < u_threshold) return twom1000*twom1000; /* underflow */
	}

	/* argument reduction */
	if(hx > 0x3fd62e42) {		/* if  |x| > 0.5 ln2 */ 
		if(hx < 0x3FF0A2B2) {	/* and |x| < 1.5 ln2 */
			hi = x-ln2HI[xsb]; lo=ln2LO[xsb]; k = 1-xsb-xsb;
		} else {
			k  = (int)(invln2 * x + halF[xsb]);
			t  = k;
			hi = x - t * ln2HI[0];	/* t*ln2HI is exact here */
			lo = t * ln2LO[0];
		}
		x  = hi - lo;
	} 
	else if(hx < 0x3e300000)  {	/* when |x|<2**-28 */
		if(huge + x > one) return one + x; /* trigger inexact */
        k = 0;
	}
	else k = 0;

	/* x is now in primary range */
	t  = x*x;
	c  = x - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))));
	if(k==0) 	return one-((x*c)/(c-2.0)-x); 
	else 		y = one-((lo-(x*c)/(2.0-c))-hi);
	if(k >= -1021) {
		__HI(y) += (k<<20);	/* add k to y's exponent */
		return y;
	} else {
		__HI(y) += ((k+1000)<<20);/* add k to y's exponent */
		return y * twom1000;
	}
}
double ldexp(double value, int exp){
	assert(exp >= 0);
	if(!isfinite(value) || value==0.0) return value;
	value = scalbn(value,exp);
	if(!isfinite(value) || value==0.0) errno = ERANGE;
	return value;
}

double pow(double x, double y) {
	double z,ax,z_h,z_l,p_h,p_l;
	double y1,t1,t2,r,s,t,u,v,w;
	int i,j,k,yisint,n;
	int hx,hy,ix,iy;
	unsigned lx,ly;

	/* i0 = ((*(int*)&one)>>29)^1;  i1=1-i0; */
	hx = __HI(x); lx = __LO(x);
	hy = __HI(y); ly = __LO(y);
	ix = hx&0x7fffffff;  iy = hy&0x7fffffff;

	/* y==zero: x**0 = 1 */
	if((iy|ly)==0) return one;

	/* +-NaN return x+y */
	if(ix > 0x7ff00000 || ((ix==0x7ff00000)&&(lx!=0)) ||
			iy > 0x7ff00000 || ((iy==0x7ff00000)&&(ly!=0)))
		return x+y;

	/* determine if y is an odd int when x < 0
	 * yisint = 0	... y is not an integer
	 * yisint = 1	... y is an odd int
	 * yisint = 2	... y is an even int
	 */
	yisint  = 0;
	if(hx<0) {
		if(iy>=0x43400000) yisint = 2; /* even integer y */
		else if(iy>=0x3ff00000) {
			k = (iy>>20)-0x3ff;	   /* exponent */
			if(k>20) {
				j = ly>>(52-k);
				if((j<<(52-k))==ly) yisint = 2-(j&1);
			} else if(ly==0) {
				j = iy>>(20-k);
				if((j<<(20-k))==iy) yisint = 2-(j&1);
			}
		}
	}

	/* special value of y */
	if(ly==0) {
		if (iy==0x7ff00000) {	/* y is +-inf */
			if(((ix-0x3ff00000)|lx)==0)
				return  y - y;	/* inf**+-1 is NaN */
			else if (ix >= 0x3ff00000)/* (|x|>1)**+-inf = inf,0 */
				return (hy>=0)? y: zero;
			else			/* (|x|<1)**-,+inf = inf,0 */
				return (hy<0)?-y: zero;
		}
		if(iy==0x3ff00000) {	/* y is  +-1 */
			if(hy<0) return one/x; else return x;
		}
		if(hy==0x40000000) return x*x; /* y is  2 */
		if(hy==0x3fe00000) {	/* y is  0.5 */
			if(hx>=0)	/* x >= +0 */
				return sqrt(x);
		}
	}

	ax   = fabs(x);
	/* special value of x */
	if(lx==0) {
		if(ix==0x7ff00000||ix==0||ix==0x3ff00000){
			z = ax;			/*x is +-0,+-inf,+-1*/
			if(hy<0) z = one/z;	/* z = (1/|x|) */
			if(hx<0) {
				if(((ix-0x3ff00000)|yisint)==0) {
					z = (z-z)/(z-z); /* (-1)**non-int is NaN */
				} else if(yisint==1)
					z = -z;		/* (x<0)**odd = -(|x|**odd) */
			}
			return z;
		}
	}

	n = (hx>>31)+1;

	/* (x<0)**(non-int) is NaN */
	if((n|yisint)==0) return (x-x)/(x-x);

	s = one; /* s (sign of result -ve**odd) = -1 else = 1 */
	if((n|(yisint-1))==0) s = -one;/* (-ve)**(odd int) */

	/* |y| is huge */
	if(iy>0x41e00000) { /* if |y| > 2**31 */
		if(iy>0x43f00000){	/* if |y| > 2**64, must o/uflow */
			if(ix<=0x3fefffff) return (hy<0)? huge*huge:tiny*tiny;
			if(ix>=0x3ff00000) return (hy>0)? huge*huge:tiny*tiny;
		}
		/* over/underflow if x is not close to one */
		if(ix<0x3fefffff) return (hy<0)? s*huge*huge:s*tiny*tiny;
		if(ix>0x3ff00000) return (hy>0)? s*huge*huge:s*tiny*tiny;
		/* now |1-x| is tiny <= 2**-20, suffice to compute
	   log(x) by x-x^2/2+x^3/3-x^4/4 */
		t = ax-one;		/* t has 20 trailing zeros */
		w = (t*t)*(0.5-t*(0.3333333333333333333333-t*0.25));
		u = ivln2_h*t;	/* ivln2_h has 21 sig. bits */
		v = t*ivln2_l-w*ivln2;
		t1 = u+v;
		__LO(t1) = 0;
		t2 = v-(t1-u);
	} else {
		double ss,s2,s_h,s_l,t_h,t_l;
		n = 0;
		/* take care subnormal number */
		if(ix<0x00100000)
		{ax *= two53; n -= 53; ix = __HI(ax); }
		n  += ((ix)>>20)-0x3ff;
		j  = ix&0x000fffff;
		/* determine interval */
		ix = j|0x3ff00000;		/* normalize ix */
		if(j<=0x3988E) k=0;		/* |x|<sqrt(3/2) */
		else if(j<0xBB67A) k=1;	/* |x|<sqrt(3)   */
		else {k=0;n+=1;ix -= 0x00100000;}
		__HI(ax) = ix;

		/* compute ss = s_h+s_l = (x-1)/(x+1) or (x-1.5)/(x+1.5) */
		u = ax-bp[k];		/* bp[0]=1.0, bp[1]=1.5 */
		v = one/(ax+bp[k]);
		ss = u*v;
		s_h = ss;
		__LO(s_h) = 0;
		/* t_h=ax+bp[k] High */
		t_h = zero;
		__HI(t_h)=((ix>>1)|0x20000000)+0x00080000+(k<<18);
		t_l = ax - (t_h-bp[k]);
		s_l = v*((u-s_h*t_h)-s_h*t_l);
		/* compute log(ax) */
		s2 = ss*ss;
		r = s2*s2*(L1+s2*(L2+s2*(L3+s2*(L4+s2*(L5+s2*L6)))));
		r += s_l*(s_h+ss);
		s2  = s_h*s_h;
		t_h = 3.0+s2+r;
		__LO(t_h) = 0;
		t_l = r-((t_h-3.0)-s2);
		/* u+v = ss*(1+...) */
		u = s_h*t_h;
		v = s_l*t_h+t_l*ss;
		/* 2/(3log2)*(ss+...) */
		p_h = u+v;
		__LO(p_h) = 0;
		p_l = v-(p_h-u);
		z_h = cp_h*p_h;		/* cp_h+cp_l = 2/(3*log2) */
		z_l = cp_l*p_h+p_l*cp+dp_l[k];
		/* log2(ax) = (ss+..)*2/(3*log2) = n + dp_h + z_h + z_l */
		t = (double)n;
		t1 = (((z_h+z_l)+dp_h[k])+t);
		__LO(t1) = 0;
		t2 = z_l-(((t1-t)-dp_h[k])-z_h);
	}

	/* split up y into y1+y2 and compute (y1+y2)*(t1+t2) */
	y1  = y;
	__LO(y1) = 0;
	p_l = (y-y1)*t1+y*t2;
	p_h = y1*t1;
	z = p_l+p_h;
	j = __HI(z);
	i = __LO(z);
	if (j>=0x40900000) {				/* z >= 1024 */
		if(((j-0x40900000)|i)!=0)			/* if z > 1024 */
			return s*huge*huge;			/* overflow */
		else {
			if(p_l+ovt>z-p_h) return s*huge*huge;	/* overflow */
		}
	} else if((j&0x7fffffff)>=0x4090cc00 ) {	/* z <= -1075 */
		if(((j-0xc090cc00)|i)!=0) 		/* z < -1075 */
			return s*tiny*tiny;		/* underflow */
		else {
			if(p_l<=z-p_h) return s*tiny*tiny;	/* underflow */
		}
	}
	/*
	 * compute 2**(p_h+p_l)
	 */
	i = j&0x7fffffff;
	k = (i>>20)-0x3ff;
	n = 0;
	if(i>0x3fe00000) {		/* if |z| > 0.5, set n = [z+0.5] */
		n = j+(0x00100000>>(k+1));
		k = ((n&0x7fffffff)>>20)-0x3ff;	/* new k for n */
		t = zero;
		__HI(t) = (n&~(0x000fffff>>k));
		n = ((n&0x000fffff)|0x00100000)>>(20-k);
		if(j<0) n = -n;
		p_h -= t;
	}
	t = p_l+p_h;
	__LO(t) = 0;
	u = t*lg2_h;
	v = (p_l-(t-p_h))*lg2+t*lg2_l;
	z = u+v;
	w = v-(z-u);
	t  = z*z;
	t1  = z - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))));
	r  = (z*t1)/(t1-two)-(w+z*w);
	z  = one-(r-z);
	j  = __HI(z);
	j += (n<<20);
	if((j>>20)<=0) z = scalbn(z,n);	/* subnormal output */
	else __HI(z) += (n<<20);
	return s*z;
}

double scalbn (double x, int n) {
	int  k,hx,lx;
	hx = __HI(x);
	lx = __LO(x);
	k = (hx&0x7ff00000)>>20;		/* extract exponent */
	if (k==0) {				/* 0 or subnormal x */
		if ((lx|(hx&0x7fffffff))==0) return x; /* +-0 */
		x *= two54;
		hx = __HI(x);
		k = ((hx&0x7ff00000)>>20) - 54;
		if (n< -50000) return tiny*x; 	/*underflow*/
	}
	if (k==0x7ff) return x+x;		/* NaN or Inf */
	k = k+n;
	if (k >  0x7fe) return huge*copysign(huge,x); /* overflow  */
	if (k > 0) 				/* normal result */
	{
		__HI(x) = (hx&0x800fffff)|(k<<20);
		return x;
	}
	if (k <= -54) {
		if (n > 50000) 	/* in case integer overflow in n+k */
			return huge*copysign(huge,x);	/*overflow*/
		else return tiny*copysign(tiny,x); 	/*underflow*/
	}
	k += 54;				/* subnormal result */
	__HI(x) = (hx&0x800fffff)|(k<<20);
	return x*twom54;
}

int isfinite(double x) {
	int hx; 
	hx = __HI(x);
	return  (unsigned)((hx&0x7fffffff) - 0x7ff00000) >> 31;
}
int isnan(double x) {
	int hx,lx;
	hx = (__HI(x) & 0x7fffffff);
	lx = __LO(x);
	hx |= (unsigned)(lx | (-lx)) >> 31;	
	hx = 0x7ff00000 - hx;
	return ((unsigned)(hx)) >> 31;
}

double copysign(double x, double y) {
	__HI(x) = (__HI(x) & 0x7fffffff) | (__HI(y) & 0x80000000);
	return x;
}



double fmod(double x, double y) {
	int n,hx,hy,hz,ix,iy,sx,i;
	unsigned lx,ly,lz;

	hx = __HI(x);		/* high word of x */
	lx = __LO(x);		/* low  word of x */
	hy = __HI(y);		/* high word of y */
	ly = __LO(y);		/* low  word of y */
	sx = hx&0x80000000;		/* sign of x */
	hx ^=sx;		/* |x| */
	hy &= 0x7fffffff;	/* |y| */

    /* purge off exception values */
	if((hy|ly)==0||(hx>=0x7ff00000)||	/* y=0,or x not finite */
	  ((hy|((ly|-ly)>>31))>0x7ff00000))	/* or y is NaN */
		return (x*y)/(x*y);
	if(hx<=hy) {
		if((hx<hy)||(lx<ly)) return x;	/* |x|<|y| return x */
		if(lx==ly) 
			return Zero[(unsigned)sx>>31];	/* |x|=|y| return x*0*/
	}

    /* determine ix = ilogb(x) */
	if(hx<0x00100000) {	/* subnormal x */
		if(hx==0) {
			for (ix = -1043, i=lx; i>0; i<<=1) ix -=1;
		} else {
			for (ix = -1022,i=(hx<<11); i>0; i<<=1) ix -=1;
		}
	} else ix = (hx>>20)-1023;

    /* determine iy = ilogb(y) */
	if(hy<0x00100000) {	/* subnormal y */
		if(hy==0) {
			for (iy = -1043, i=ly; i>0; i<<=1) iy -=1;
		} else {
			for (iy = -1022,i=(hy<<11); i>0; i<<=1) iy -=1;
		}
	} else iy = (hy>>20)-1023;

    /* set up {hx,lx}, {hy,ly} and align y to x */
	if(ix >= -1022) 
		hx = 0x00100000|(0x000fffff&hx);
	else {		/* subnormal x, shift x to normal */
		n = -1022-ix;
		if(n<=31) {
			hx = (hx<<n)|(lx>>(32-n));
			lx <<= n;
		} else {
			hx = lx<<(n-32);
			lx = 0;
		}
	}
	if(iy >= -1022) 
	    hy = 0x00100000|(0x000fffff&hy);
	else {		/* subnormal y, shift y to normal */
		n = -1022-iy;
		if(n<=31) {
			hy = (hy<<n)|(ly>>(32-n));
			ly <<= n;
		} else {
			hy = ly<<(n-32);
			ly = 0;
		}
	}

    /* fix point fmod */
	n = ix - iy;
	while(n--) {
		hz=hx-hy; lz=lx-ly; if(lx<ly) hz -= 1;
		if(hz<0){hx = hx+hx+(lx>>31); lx = lx+lx;}
		else {
			if((hz|lz)==0) 		/* return sign(x)*0 */
				return Zero[(unsigned)sx>>31];
			hx = hz+hz+(lz>>31); lx = lz+lz;
		}
	}
	hz=hx-hy; lz=lx-ly; if(lx<ly) hz -= 1;
	if(hz>=0) {hx=hz; lx=lz;}

    /* convert back to floating value and restore the sign */
	if((hx|lx)==0) 			/* return sign(x)*0 */
		return Zero[(unsigned)sx>>31];	
	while(hx<0x00100000) {		/* normalize x */
		hx = hx+hx+(lx>>31); lx = lx+lx;
		iy -= 1;
	}
	if(iy>= -1022) {	/* normalize output */
		hx = ((hx-0x00100000)|((iy+1023)<<20));
		__HI(x) = hx|sx;
		__LO(x) = lx;
	} else {		/* subnormal output */
		n = -1022 - iy;
		if(n<=20) {
			lx = (lx>>n)|((unsigned)hx<<(32-n));
			hx >>= n;
		} else if (n<=31) {
			lx = (hx<<(32-n))|(lx>>n); hx = sx;
		} else {
			lx = hx>>(n-32); hx = sx;
		}
		__HI(x) = hx|sx;
		__LO(x) = lx;
		x *= one;		/* create necessary signal */
	}
	return x;		/* exact output */
}

double modf(double x, double *iptr) {
	int i0,i1,j0;
	unsigned i;
	i0 =  __HI(x);		/* high x */
	i1 =  __LO(x);		/* low  x */
	j0 = ((i0>>20)&0x7ff)-0x3ff;	/* exponent of x */
	if(j0<20) {			/* integer part in high x */
		if(j0<0) {			/* |x|<1 */
			__HIp(iptr) = i0&0x80000000;
			__LOp(iptr) = 0;		/* *iptr = +-0 */
			return x;
		} else {
			i = (0x000fffff)>>j0;
			if(((i0&i)|i1)==0) {		/* x is integral */
				*iptr = x;
				__HI(x) &= 0x80000000;
				__LO(x)  = 0;	/* return +-0 */
				return x;
			} else {
				__HIp(iptr) = i0&(~i);
				__LOp(iptr) = 0;
				return x - *iptr;
			}
		}
	} else if (j0>51) {		/* no fraction part */
		*iptr = x*one;
		__HI(x) &= 0x80000000;
		__LO(x)  = 0;	/* return +-0 */
		return x;
	} else {			/* fraction part in low x */
		i = ((unsigned)(0xffffffff))>>(j0-20);
		if((i1&i)==0) { 		/* x is integral */
			*iptr = x;
			__HI(x) &= 0x80000000;
			__LO(x)  = 0;	/* return +-0 */
			return x;
		} else {
			__HIp(iptr) = i0;
			__LOp(iptr) = i1&(~i);
			return x - *iptr;
		}
	}
}

double sqrt(double x) {
	double z;
	int 	sign = (int)0x80000000;
	unsigned r,t1,s1,ix1,q1;
	int ix0,s0,q,m,t,i;

	ix0 = __HI(x);			/* high word of x */
	ix1 = __LO(x);		/* low word of x */

	/* take care of Inf and NaN */
	if((ix0&0x7ff00000)==0x7ff00000) {
		return x*x+x;		/* sqrt(NaN)=NaN, sqrt(+inf)=+inf
					   sqrt(-inf)=sNaN */
	}
	/* take care of zero */
	if(ix0<=0) {
		if(((ix0&(~sign))|ix1)==0) return x;/* sqrt(+-0) = +-0 */
		else if(ix0<0)
			return (x-x)/(x-x);		/* sqrt(-ve) = sNaN */
	}
	/* normalize x */
	m = (ix0>>20);
	if(m==0) {				/* subnormal x */
		while(ix0==0) {
			m -= 21;
			ix0 |= (ix1>>11); ix1 <<= 21;
		}
		for(i=0;(ix0&0x00100000)==0;i++) ix0<<=1;
		m -= i-1;
		ix0 |= (ix1>>(32-i));
		ix1 <<= i;
	}
	m -= 1023;	/* unbias exponent */
	ix0 = (ix0&0x000fffff)|0x00100000;
	if(m&1){	/* odd m, double x to make it even */
		ix0 += ix0 + ((ix1&sign)>>31);
		ix1 += ix1;
	}
	m >>= 1;	/* m = [m/2] */

	/* generate sqrt(x) bit by bit */
	ix0 += ix0 + ((ix1&sign)>>31);
	ix1 += ix1;
	q = q1 = s0 = s1 = 0;	/* [q,q1] = sqrt(x) */
	r = 0x00200000;		/* r = moving bit from right to left */

	while(r!=0) {
		t = s0+r;
		if(t<=ix0) {
			s0   = t+r;
			ix0 -= t;
			q   += r;
		}
		ix0 += ix0 + ((ix1&sign)>>31);
		ix1 += ix1;
		r>>=1;
	}

	r = sign;
	while(r!=0) {
		t1 = s1+r;
		t  = s0;
		if((t<ix0)||((t==ix0)&&(t1<=ix1))) {
			s1  = t1+r;
			if(((t1&sign)==sign)&&(s1&sign)==0) s0 += 1;
			ix0 -= t;
			if (ix1 < t1) ix0 -= 1;
			ix1 -= t1;
			q1  += r;
		}
		ix0 += ix0 + ((ix1&sign)>>31);
		ix1 += ix1;
		r>>=1;
	}

	/* use floating add to find out rounding direction */
	if((ix0|ix1)!=0) {
		z = one-tiny; /* trigger inexact flag */
		if (z>=one) {
			z = one+tiny;
			if (q1==(unsigned)0xffffffff) { q1=0; q += 1;}
			else if (z>one) {
				if (q1==(unsigned)0xfffffffe) q+=1;
				q1+=2;
			} else
				q1 += (q1&1);
		}
	}
	ix0 = (q>>1)+0x3fe00000;
	ix1 =  q1>>1;
	if ((q&1)==1) ix1 |= sign;
	ix0 += (m <<20);
	__HI(z) = ix0;
	__LO(z) = ix1;
	return z;
}

double cbrt(double x) {
	int	hx;
	double r,s,t=0.0,w;
	unsigned sign;


	hx = __HI(x);		/* high word of x */
	sign=hx&0x80000000; 		/* sign= sign(x) */
	hx  ^=sign;
	if(hx>=0x7ff00000) return(x+x); /* cbrt(NaN,INF) is itself */
	if((hx|__LO(x))==0)
		return(x);		/* cbrt(0) is itself */

	__HI(x) = hx;	/* x <- |x| */
	/* rough cbrt to 5 bits */
	if(hx<0x00100000) 		/* subnormal number */
		{__HI(t)=0x43500000; 		/* set t= 2**54 */
		t*=x; __HI(t)=__HI(t)/3+B2;
		}
	else
		__HI(t)=hx/3+B1;


	/* new cbrt to 23 bits, may be implemented in single precision */
	r=t*t/x;
	s=C+r*t;
	t*=G+F/(s+E+D/s);

	/* chopped to 20 bits and make it larger than cbrt(x) */
	__LO(t)=0; __HI(t)+=0x00000001;


	/* one step newton iteration to 53 bits with error less than 0.667 ulps */
	s=t*t;		/* t*t is exact */
	r=x/s;
	w=t+t;
	r=(r-t)/(w+r);	/* r-s is exact */
	t=t+t*r;

	/* retore the sign bit */
	__HI(t) |= sign;
	return(t);
}

double hypot(double x, double y) {
	double a=x,b=y,t1,t2,y1,y2,w;
	int j,k,ha,hb;

	ha = __HI(x)&0x7fffffff;	/* high word of  x */
	hb = __HI(y)&0x7fffffff;	/* high word of  y */
	if(hb > ha) {a=y;b=x;j=ha; ha=hb;hb=j;} else {a=x;b=y;}
	__HI(a) = ha;	/* a <- |a| */
	__HI(b) = hb;	/* b <- |b| */
	if((ha-hb)>0x3c00000) {return a+b;} /* x/y > 2**60 */
	k=0;
	if(ha > 0x5f300000) {	/* a>2**500 */
		if(ha >= 0x7ff00000) {	/* Inf or NaN */
			w = a+b;			/* for sNaN */
			if(((ha&0xfffff)|__LO(a))==0) w = a;
			if(((hb^0x7ff00000)|__LO(b))==0) w = b;
			return w;
		}
		/* scale a and b by 2**-600 */
		ha -= 0x25800000; hb -= 0x25800000;	k += 600;
		__HI(a) = ha;
		__HI(b) = hb;
	}
	if(hb < 0x20b00000) {	/* b < 2**-500 */
		if(hb <= 0x000fffff) {	/* subnormal b or 0 */
			if((hb|(__LO(b)))==0) return a;
			t1=0;
			__HI(t1) = 0x7fd00000;	/* t1=2^1022 */
			b *= t1;
			a *= t1;
			k -= 1022;
		} else {		/* scale a and b by 2^600 */
			ha += 0x25800000; 	/* a *= 2^600 */
			hb += 0x25800000;	/* b *= 2^600 */
			k -= 600;
			__HI(a) = ha;
			__HI(b) = hb;
		}
	}
	/* medium size a and b */
	w = a-b;
	if (w>b) {
		t1 = 0;
		__HI(t1) = ha;
		t2 = a-t1;
		w  = sqrt(t1*t1-(b*(-b)-t2*(a+t1)));
	} else {
		a  = a+a;
		y1 = 0;
		__HI(y1) = hb;
		y2 = b - y1;
		t1 = 0;
		__HI(t1) = ha+0x00100000;
		t2 = a - t1;
		w  = sqrt(t1*y1-(w*(-w)-(t1*y2+t2*b)));
	}
	if(k!=0) {
		t1 = 1.0;
		__HI(t1) += (k<<20);
		return t1*w;
	} else return w;
}

double remainder(double x, double p) {
	int hx,hp;
	unsigned sx,lx,lp;
	double p_half;

	hx = __HI(x);		/* high word of x */
	lx = __LO(x);		/* low  word of x */
	hp = __HI(p);		/* high word of p */
	lp = __LO(p);		/* low  word of p */
	sx = hx&0x80000000;
	hp &= 0x7fffffff;
	hx &= 0x7fffffff;

	/* purge off exception values */
	if((hp|lp)==0) return (x*p)/(x*p); 	/* p = 0 */
	if((hx>=0x7ff00000)||			/* x not finite */
	   ((hp>=0x7ff00000)&&			/* p is NaN */
	   (((hp-0x7ff00000)|lp)!=0)))
		return (x*p)/(x*p);


	if (hp<=0x7fdfffff) x = fmod(x,p+p);	/* now x < 2p */
	if (((hx-hp)|(lx-lp))==0) return zero*x;
	x  = fabs(x);
	p  = fabs(p);
	if (hp<0x00200000) {
		if(x+x>p) {
			x-=p;
			if(x+x>=p) x -= p;
		}
	} else {
		p_half = 0.5*p;
		if(x>p_half) {
			x-=p;
			if(x>=p_half) x -= p;
		}
	}
	__HI(x) ^= sx;
	return x;
}

double log(double x) {
	double hfsq,f,s,z,R,w,t1,t2,dk;
	int k,hx,i,j;
	unsigned lx;

	hx = __HI(x);		/* high word of x */
	lx = __LO(x);		/* low  word of x */

	k=0;
	if (hx < 0x00100000) {			/* x < 2**-1022  */
		if (((hx&0x7fffffff)|lx)==0)
			return -two54/zero;		/* log(+-0)=-inf */
		if (hx<0) return (x-x)/zero;	/* log(-#) = NaN */
		k -= 54; x *= two54; /* subnormal number, scale up x */
		hx = __HI(x);		/* high word of x */
	}
	if (hx >= 0x7ff00000) return x+x;
	k += (hx>>20)-1023;
	hx &= 0x000fffff;
	i = (hx+0x95f64)&0x100000;
	__HI(x) = hx|(i^0x3ff00000);	/* normalize x or x/2 */
	k += (i>>20);
	f = x-1.0;
	if((0x000fffff&(2+hx))<3) {	/* |f| < 2**-20 */
		if(f==zero) { if(k==0) return zero; else {dk=(double)k; return dk*ln2_hi+dk*ln2_lo;} }
		R = f*f*(0.5-0.33333333333333333*f);
		if(k==0) return f-R; else {dk=(double)k; return dk*ln2_hi-((R-dk*ln2_lo)-f);}
	}
	s = f/(2.0+f);
	dk = (double)k;
	z = s*s;
	i = hx-0x6147a;
	w = z*z;
	j = 0x6b851-hx;
	t1= w*(Lg2+w*(Lg4+w*Lg6));
	t2= z*(Lg1+w*(Lg3+w*(Lg5+w*Lg7)));
	i |= j;
	R = t2+t1;
	if(i>0) {
		hfsq=0.5*f*f;
		if(k==0) return f-(hfsq-s*(hfsq+R)); else
			return dk*ln2_hi-((hfsq-(s*(hfsq+R)+dk*ln2_lo))-f);
	} else {
		if(k==0) return f-s*(f-R); else
			return dk*ln2_hi-((s*(f-R)-dk*ln2_lo)-f);
	}
}

double log10(double x) {
	double y,z;
	int i,k,hx;
	unsigned lx;

	hx = __HI(x);	/* high word of x */
	lx = __LO(x);	/* low word of x */

	k=0;
	if (hx < 0x00100000) {                  /* x < 2**-1022  */
		if (((hx&0x7fffffff)|lx)==0)
			return -two54/zero;             /* log(+-0)=-inf */
		if (hx<0) return (x-x)/zero;        /* log(-#) = NaN */
		k -= 54; x *= two54; /* subnormal number, scale up x */
		hx = __HI(x);                /* high word of x */
	}
	if (hx >= 0x7ff00000) return x+x;
	k += (hx>>20)-1023;
	i  = ((unsigned)k&0x80000000)>>31;
	hx = (hx&0x000fffff)|((0x3ff-i)<<20);
	y  = (double)(k+i);
	__HI(x) = hx;
	z  = y*log10_2lo + ivln10*log(x);
	return  z+y*log10_2hi;
}

double ceil(double x) {
	int i0,i1,j0;
	unsigned i,j;
	i0 =  __HI(x);
	i1 =  __LO(x);
	j0 = ((i0>>20)&0x7ff)-0x3ff;
	if(j0<20) {
		if(j0<0) { 	/* raise inexact if x != 0 */
			if(huge+x>0.0) {/* return 0*sign(x) if |x|<1 */
				if(i0<0) {i0=0x80000000;i1=0;}
				else if((i0|i1)!=0) { i0=0x3ff00000;i1=0;}
			}
		} else {
			i = (0x000fffff)>>j0;
			if(((i0&i)|i1)==0) return x; /* x is integral */
			if(huge+x>0.0) {	/* raise inexact flag */
				if(i0>0) i0 += (0x00100000)>>j0;
				i0 &= (~i); i1=0;
			}
		}
	} else if (j0>51) {
		if(j0==0x400) return x+x;	/* inf or NaN */
		else return x;		/* x is integral */
	} else {
		i = ((unsigned)(0xffffffff))>>(j0-20);
		if((i1&i)==0) return x;	/* x is integral */
		if(huge+x>0.0) { 		/* raise inexact flag */
			if(i0>0) {
				if(j0==20) i0+=1;
				else {
					j = i1 + (1<<(52-j0));
					if(j<i1) i0+=1;	/* got a carry */
					i1 = j;
				}
			}
			i1 &= (~i);
		}
	}
	__HI(x) = i0;
	__LO(x) = i1;
	return x;
}

double floor(double x) {
	int i0,i1,j0;
	unsigned i,j;
	i0 =  __HI(x);
	i1 =  __LO(x);
	j0 = ((i0>>20)&0x7ff)-0x3ff;
	if(j0<20) {
		if(j0<0) { 	/* raise inexact if x != 0 */
			if(huge+x>0.0) {/* return 0*sign(x) if |x|<1 */
				if(i0>=0) {i0=i1=0;}
				else if(((i0&0x7fffffff)|i1)!=0)
				{ i0=0xbff00000;i1=0;}
			}
		} else {
			i = (0x000fffff)>>j0;
			if(((i0&i)|i1)==0) return x; /* x is integral */
			if(huge+x>0.0) {	/* raise inexact flag */
				if(i0<0) i0 += (0x00100000)>>j0;
				i0 &= (~i); i1=0;
			}
		}
	} else if (j0>51) {
		if(j0==0x400) return x+x;	/* inf or NaN */
		else return x;		/* x is integral */
	} else {
		i = ((unsigned)(0xffffffff))>>(j0-20);
		if((i1&i)==0) return x;	/* x is integral */
		if(huge+x>0.0) { 		/* raise inexact flag */
			if(i0<0) {
				if(j0==20) i0+=1;
				else {
					j = i1+(1<<(52-j0));
					if(j<i1) i0 +=1 ; 	/* got a carry */
					i1=j;
				}
			}
			i1 &= (~i);
		}
	}
	__HI(x) = i0;
	__LO(x) = i1;
	return x;
}

double fabs(double x) {
	__HI(x) &= 0x7fffffff;
	return x;
}

double rint(double x) {
	int i0,j0,sx;
	unsigned i,i1;
	double w,t;
	i0 =  __HI(x);
	sx = (i0>>31)&1;
	i1 =  __LO(x);
	j0 = ((i0>>20)&0x7ff)-0x3ff;
	if(j0<20) {
		if(j0<0) {
			if(((i0&0x7fffffff)|i1)==0) return x;
			i1 |= (i0&0x0fffff);
			i0 &= 0xfffe0000;
			i0 |= ((i1|-i1)>>12)&0x80000;
			__HI(x)=i0;
			w = TWO52[sx]+x;
			t =  w-TWO52[sx];
			i0 = __HI(t);
			__HI(t) = (i0&0x7fffffff)|(sx<<31);
			return t;
		} else {
			i = (0x000fffff)>>j0;
			if(((i0&i)|i1)==0) return x; /* x is integral */
			i>>=1;
			if(((i0&i)|i1)!=0) {
				if(j0==19) i1 = 0x40000000; else
					i0 = (i0&(~i))|((0x20000)>>j0);
			}
		}
	} else if (j0>51) {
		if(j0==0x400) return x+x;	/* inf or NaN */
		else return x;		/* x is integral */
	} else {
		i = ((unsigned)(0xffffffff))>>(j0-20);
		if((i1&i)==0) return x;	/* x is integral */
		i>>=1;
		if((i1&i)!=0) i1 = (i1&(~i))|((0x40000000)>>(j0-20));
	}
	__HI(x) = i0;
	__LO(x) = i1;
	w = TWO52[sx]+x;
	return w-TWO52[sx];
}

double nextafter(double x, double y) {
	int	hx,hy,ix,iy;
	unsigned lx,ly;

	hx = __HI(x);		/* high word of x */
	lx = __LO(x);		/* low  word of x */
	hy = __HI(y);		/* high word of y */
	ly = __LO(y);		/* low  word of y */
	ix = hx&0x7fffffff;		/* |x| */
	iy = hy&0x7fffffff;		/* |y| */

	if(((ix>=0x7ff00000)&&((ix-0x7ff00000)|lx)!=0) ||   /* x is nan */
			((iy>=0x7ff00000)&&((iy-0x7ff00000)|ly)!=0))     /* y is nan */
		return x+y;
	if(x==y) return x;		/* x=y, return x */
	if((ix|lx)==0) {			/* x == 0 */
		__HI(x) = hy&0x80000000;	/* return +-minsubnormal */
		__LO(x) = 1;
		y = x*x;
		if(y==x) return y; else return x;	/* raise underflow flag */
	}
	if(hx>=0) {				/* x > 0 */
		if(hx>hy||((hx==hy)&&(lx>ly))) {	/* x > y, x -= ulp */
			if(lx==0) hx -= 1;
			lx -= 1;
		} else {				/* x < y, x += ulp */
			lx += 1;
			if(lx==0) hx += 1;
		}
	} else {				/* x < 0 */
		if(hy>=0||hx>hy||((hx==hy)&&(lx>ly))){/* x < y, x -= ulp */
			if(lx==0) hx -= 1;
			lx -= 1;
		} else {				/* x > y, x += ulp */
			lx += 1;
			if(lx==0) hx += 1;
		}
	}
	hy = hx&0x7ff00000;
	if(hy>=0x7ff00000) return x+x;	/* overflow  */
	if(hy<0x00100000) {		/* underflow */
		y = x*x;
		if(y!=x) {		/* raise underflow flag */
			__HI(y) = hx; __LO(y) = lx;
			return y;
		}
	}
	__HI(x) = hx; __LO(x) = lx;
	return x;
}
