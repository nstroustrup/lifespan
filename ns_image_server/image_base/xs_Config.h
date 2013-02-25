// ====================================================================================================================
// ====================================================================================================================
//  xs_Config.h
// ====================================================================================================================
// ====================================================================================================================
#ifndef _xs_CONFIG_H_
#define _xs_CONFIG_H_

// ====================================================================================================================
// Types
// ====================================================================================================================
#ifndef _xs_Types_
#define _xs_Types_                  1
    typedef signed char             int8;
    typedef unsigned char           uint8;
    typedef signed short            int16;
    typedef unsigned short          uint16;
    typedef int			            int32;
    typedef unsigned int           uint32;
    typedef float                   real32;
    typedef double                  real64;
#endif //_xs_Types_



// ====================================================================================================================
// Basic stuff
// ====================================================================================================================
#ifndef _xs_BigEndian_
#define _xs_BigEndian_              0       //intel is little endian
#endif

#ifdef _MSC_VER
#define finline                     __forceinline
#else
#define finline						__attribute__((always_inline))
#endif
#define xs_MAXINT                   0x7ffffff
#define xs_NULL                     0
#define xs_Min(a,b)			        (((a)<(b))	? (a) : (b))
#define xs_Max(a,b)			        (((a)>(b))	? (a) : (b))
#define xs_Clamp(a,b,c)		         xs_Min(xs_Max(a,b), c)

#if _DEBUG
    #include <assert.h>
    #define xs_Verify(e)             assert(e)
    #define xs_Assert(e)             assert(e)
#else
    #define xs_Verify(e)             (e)
    #define xs_Assert(e)
#endif



// ====================================================================================================================
// Memory
// ====================================================================================================================
void* xs_PtrAlloc       (int32 count, int32 size=1);                      
void* xs_PtrRealloc     (void* p, int32 count, int32 size=1);           
bool  xs_PtrFree        (void* p);                                       
void* xs_Memmove        (void* p1, void* p2, int32 count, int32 size=1);  
void* xs_Memcpy         (void* p1, void* p2, int32 count, int32 size=1);   
void* xs_Memset         (void* p,  int32 data, int32 count, int32 size=1);   
void* xs_Memzero        (void* p,  int32 count, int32 size=1);   



// ====================================================================================================================
// ====================================================================================================================
//you can override the implementation by 
//defining _xs_Memory_ in your project
//and then implementing to the prototypes above
#ifndef _xs_MemoryAlloc_
#define _xs_MemoryAlloc_ 
#include <stdlib.h>
inline void* xs_PtrAlloc    (int32 count, int32 size)                    {return malloc(count*size);}
inline void* xs_PtrRealloc  (void* p, int32 count, int32 size)           {return realloc(p, count*size);}
inline bool  xs_PtrFree     (void* p)                                    {free(p); return true;}
#endif

#ifndef _xs_MemoryMove_
#define _xs_MemoryMove_ 
#include <string.h>
inline void* xs_Memmove (void* p1, void* p2, int32 count, int32 size)    {return memmove(p1,p2,count*size);}
inline void* xs_Memcpy  (void* p1, void* p2, int32 count, int32 size)    {return memcpy(p1,p2,count*size);}
inline void* xs_Memset  (void* p,  int32 data, int32 count, int32 size)  {return memset(p,data,count*size);}
inline void* xs_Memzero (void* p,  int32 count, int32 size)              {return memset(p,0,count*size);}
#endif


/*
    void*   blockalloc(long size);
    void*   blockrealloc(void* fp, long newsize);      
    void    blockfree(void* p);
    inline void* xs_PtrAlloc    (int32 count, int32 size)                    {return blockalloc(count*size);}
    inline void* xs_PtrRealloc  (void* p, int32 count, int32 size)           {return blockrealloc(p, count*size);}
    inline bool  xs_PtrFree     (void* p)                                    {blockfree(p); return true;}
*/
// ====================================================================================================================
// ====================================================================================================================
#endif // _xs_CONFIG_H_
