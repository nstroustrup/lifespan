//downloaded from http://www.stereopsis.com/sree/fpu2006.html
// ====================================================================================================================
// ====================================================================================================================
//  xs_Float.h
// ====================================================================================================================
// ====================================================================================================================
#ifndef _xs_FLOAT_H_
#define _xs_FLOAT_H_

#include "xs_Config.h"

// ====================================================================================================================
//  Defines
// ====================================================================================================================
#ifndef _xs_DEFAULT_CONVERSION
#define _xs_DEFAULT_CONVERSION      0
#endif //_xs_DEFAULT_CONVERSION


#if _xs_BigEndian_
	#define _xs_iexp_				0
	#define _xs_iman_				1
#else
	#define _xs_iexp_				1       //intel is little endian
	#define _xs_iman_				0
#endif //BigEndian_


#define _xs_doublecopysgn(a,b)      (( xs_int32*)&a)[_xs_iexp_]&=~((( xs_int32*)&b)[_xs_iexp_]&0x80000000)
#define _xs_doubleisnegative(a)     (((( xs_int32*)&a)[_xs_iexp_])|0x80000000)

// ====================================================================================================================
//  Constants
// ====================================================================================================================
const  xs_real64 _xs_doublemagic			= xs_real64 (6755399441055744.0); 	    //2^52 * 1.5,  uses limited precisicion to floor
const  xs_real64 _xs_doublemagicdelta      	= (1.5e-8);                         //almost .5f = .5f + 1e^(number of exp bit)
const  xs_real64 _xs_doublemagicroundeps	= (.5f-_xs_doublemagicdelta);       //almost .5f = .5f - 1e^(number of exp bit)


// ====================================================================================================================
//  Prototypes
// ====================================================================================================================

xs_int32 xs_CRoundToInt      (xs_real64 val, xs_real64 dmr =  _xs_doublemagic);
xs_int32 xs_ToInt            (xs_real64 val, xs_real64 dme = -_xs_doublemagicroundeps);
xs_int32 xs_FloorToInt       (xs_real64 val, xs_real64 dme =  _xs_doublemagicroundeps);
xs_int32 xs_CeilToInt        (xs_real64 val, xs_real64 dme =  _xs_doublemagicroundeps);
xs_int32 xs_RoundToInt       (xs_real64 val);

//int32 versions
xs_int32 xs_CRoundToInt      (xs_int32 val);
xs_int32 xs_ToInt            (xs_int32 val);



// ====================================================================================================================
//  Fix Class
// ====================================================================================================================
template < xs_int32 N> class xs_Fix
{
public:
    typedef  xs_int32 Fix;

    // ====================================================================================================================
    //  Basic Conversion from Numbers
    // ====================================================================================================================
    finline static Fix       ToFix       (xs_int32 val)    {return val<<N;}
    finline static Fix       ToFix       (xs_real64 val)   {return xs_ConvertToFixed(val);}

    // ====================================================================================================================
    //  Basic Conversion to Numbers
    // ====================================================================================================================
    finline static  xs_real64    ToReal      (Fix f)        {return xs_real64(f)/xs_real64(1<<N);}
    finline static  xs_int32     ToInt       (Fix f)        {return f>>N;}



protected:
    // ====================================================================================================================
    // Helper function - mainly to preserve _xs_DEFAULT_CONVERSION
    // ====================================================================================================================
    finline static  xs_int32 xs_ConvertToFixed (xs_real64 val)
    {
    #if _xs_DEFAULT_CONVERSION==0
        return xs_CRoundToInt(val, _xs_doublemagic/(1<<N));
    #else
        return (long)((val)*(1<<N));
    #endif
    }
};



class xs_float{
public:
	// ====================================================================================================================
	// ====================================================================================================================
	//  Inline implementation
	// ====================================================================================================================
	// ====================================================================================================================
	static finline  xs_int32 xs_CRoundToInt(xs_real64 val, xs_real64 dmr =  _xs_doublemagic)
	{
	#if _xs_DEFAULT_CONVERSION==0
		val		= val + dmr;
		return ((xs_int32*)&val)[_xs_iman_];
		//return 0;
	#else
		return int32(floor(val+.5));
	#endif
	}

	// ====================================================================================================================
	static finline  xs_int32 xs_ToInt(xs_real64 val, xs_real64 dme = -_xs_doublemagicroundeps)
	{
		/* unused - something else I tried...
				_xs_doublecopysgn(dme,val);
				return xs_CRoundToInt(val+dme);
				return 0;
		*/

	#if _xs_DEFAULT_CONVERSION==0
		return (val<0) ?   xs_CRoundToInt(val-dme) :
						   xs_CRoundToInt(val+dme);
	#else
		return int32(val);
	#endif
	}


	// ====================================================================================================================
	static finline  xs_int32 xs_FloorToInt(xs_real64 val, xs_real64 dme =  _xs_doublemagicroundeps)
	{
	#if _xs_DEFAULT_CONVERSION==0
		return xs_CRoundToInt (val - dme);
	#else
		return floor(val);
	#endif
	}


	// ====================================================================================================================
	static finline  xs_int32 xs_CeilToInt(xs_real64 val, xs_real64 dme =  _xs_doublemagicroundeps)
	{
	#if _xs_DEFAULT_CONVERSION==0
		return xs_CRoundToInt (val + dme);
	#else
		return ceil(val);
	#endif
	}


	// ====================================================================================================================
	static finline  xs_int32 xs_RoundToInt(xs_real64 val)
	{
	#if _xs_DEFAULT_CONVERSION==0
		return xs_CRoundToInt (val + _xs_doublemagicdelta);
	#else
		return floor(val+.5);
	#endif
	}
};


// ====================================================================================================================
// ====================================================================================================================
#endif // _xs_FLOAT_H_
