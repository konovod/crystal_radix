//    A reasonably simple and efficient radix sort implementation in C++.
//    Version 1.02.
//
//    Development threads:
// https://gamedev.ru/flame/forum/?id=227644
// https://www.gamedev.net/forums/topic/701484-radix-sort-discussion/
//
// LICENSE
//    This software is in the public domain. Where that dedication is not
//    recognized, you are granted a perpetual, irrevocable license to copy
//    and modify this file as you see fit.
//
// NOTES
//    You may want to treat this as a copy-paste fodder, rather than a proper
//    library, e. g. no effort has been made to prevent name clashes (e. g.
//    via wrapping it in a namespace).
//
//    Templates introduce some code bloat, but may also increase
//    opportunities for optimizations (there are also architectures
//    that are not fond of variable shifts), and experimentally do help.
//
// OVERVIEW
//    The library provides 2 template functions:
//      template<typename T,typename Traits>
//      T *radix_sort_stable(T *src,T* tmp,size_t n,int destination,int mode);
//
//      template<typename T,typename Traits>
//      void radix_sort_inplace(T *src,size_t n);
//
//    Both functions operate on a contiguous array of objects T with unsigned
//    integer key. The key for the type is determined by supplied 'Traits'
//    template parameter, which should be a class with a public
//    static 'get_key(const T&)' method. Key size is deduced form
//    this method's return type. Example:
//      struct KeyValue
//      {
//          uint32_t key;
//          uint32_t index; // Preferable on x64 over void*.
//      };
//      struct GetKey
//      {
//          uint32_t get_key(const KeyValue &src) {return src.key;}
//      };
//      KeyValue *sorted=radix_sort_stable<KeyValue,GetKey>(buffer,n,-1,-1);
//
//    The above is expected to be most likely use-case. In case of 'fat' objects
//    it is recommended to only store handle (index or pointer) to it in T, and
//    store the objects themselves elsewhere. Anyway, it is strongly recommended,
//    that T is either a fundamental type (e. g. unsigned int) or a simple
//    struct (like POD ("plain old data"); at least DefaultConstructible might
//    be mandatory). Of course, sorting pure keys (treating entire T as a key)
//    works as well. For the typical cases of sorting signed integers and floats
//    according to their natural ordering you may use the following helper
//    functions (while you MAY use them as Traits::get_key, you are better off
//    constructing input with unsigned keys separately, performance-wise):
//      uint32_t make_key_from_signed(int32_t x) {
//          return uint32_t(x)-uint32_t(INT32_MIN);
//      }
//      uint32_t make_key_from_float(float x) {
//          uint32_t ret;
//          memcpy(&ret,&x,sizeof(ret));
//          ret=ret^(uint32_t(int32_t(ret)>>31)|uint32_t(0x80000000ul));
//          return ret-0x007FFFFFul; // Place all NaNs at the top.
//      }
//    and similarly for other sizes. make_key_from_signed() may even work
//    in non 2's complement integers (if the C++ Standard is to be believed,
//    signed->unsigned is numerical_value mod 2^word_size). The function
//    make_key_from_float() requires float to be IEEE-754 binary floating
//    point type (binary32), and needs the endianness of uint and float
//    to agree, so that float's sign bit goes to msb; it also relies on
//    signed right shift sign-extending (both assumptions hold on x86/x64).
//    If you rather have all NaNs at the bottom (i. e. comparing less to
//    all other floats) you can 'return ret-0x007FFFFFul;'. And if you
//    don't mind NaNs being on both sides, you can just 'return ret;'.
//    For double, the corresponding constant is 0x000FFFFFFFFFFFFFull
//    (i. e. 2^52-1).
//
//    You can return bitwise NOT of key, to sort in descending order.
//
//    The radix_sort_stable() performs a stable sort, using additional
//    buffer 'tmp' (n elements in size), supplied by the caller (it does
//    not dynamically allocate anything). Argument 'destination' controls
//    where the output is written:
//      0 - 'src'
//      1 - 'tmp'
//      anything else means 'don't care' (may be faster)
//    The function returns pointer to output (equal to either 'src' or 'tmp').
//    Argument 'mode' controls which type of algorithm is used:
//      0 - LSD radix sort (least significant digit is sorted first)
//      1 - MSD radix sort (most significant digit is sorted first)
//      anything else means 'don't care' (decided via heuristic for speed)
//    The function uses O(sizeof(key)) additional memory (it may use recursion,
//    but the depth is bounded by the above value), which is around 64KB in
//    practice on x64.
//
//    The radix_sort_inplace() performs inplace sort (which is not stable).
//    It does not allocate anything dynamically, and uses O(sizeof(T))
//    additional memory, which is around 64KB in practice on x64 (more
//    for larger T).
//
//    Memory usage for both functions can be improved, with a modest
//    performance hit.
//
// COMPILING
//    The code compiles as C++03. Implementing this in pure C seems doable,
//    and probably rather simple, especially if restricted to byte
//    granularity. No attempt was made here to do so.
//
// PERFORMANCE
//    For smaller (<=32b) keys the radix_sort_stable() is generally faster,
//    for most input sizes, sometimes twice as fast. For larger keys
//    radix_sort_inplace() starts to catch up, and sometimes is faster.
//    On small (<=32b) keys both functions should outperform std::sort
//    significantly, except for small (n<500) inputs, where they are
//    about the same. On x64 for {uint32_t key; uint32_t index} the
//    radix_sort_stable() is typically x4 and radix_sort_inplace() is
//    typically x2.5 faster than std::sort. Performance drops for
//    larger sizeof(key), and in general for larger sizeof(T).
//    Performance varies somewhat significantly, depending on platform.
//    The functions were tuned for typical x64. You are welcome to tweak them
//    for platforms that matter to you.
//    Compiler optimizations beyond (equivalent of) -O2 do not seem to
//    affect performance much (even -O1 seems to run at the same speed
//    as -O2; -O0 does slow sorting down a lot (~x3), though).
//
//    For random keys, the performance does not seem to depend on
//    whether the input is already sorted (std::sort is a lot faster
//    in that case, so the win is marginal).
//    Performance DOES depend a lot on the structure of data. E. g. if
//    the keys are a sequence 0..(n-1), the performance drops x3 (apparently
//    due to the lot of power-of-two sized buckets causing cache aliasing).
//    The effect is present even if the input is shuffled (though to a lesser
//    degree). No attempt is made to detect this situation. The MSD radix sort
//    is less affected, so you might want to force that, if this situation
//    is likely.
//
// PREFETCHING
//    Experimentally, prefetching was found to help quite a bit (specifics
//    depend on a platform), so the library does use it. It tries to do so
//    without causing a hassle, but if it turns out problematic for you (be it
//    compilation errors, performance degradation or something else) you may
//    opt to replace or outright remove it (it should only impact performance).

#include <cstddef> // For size_t.
#include <climits> // For CHAR_BIT.

// Simple and hopefully unproblematic prefetching.
#if defined(__GNUC__) // GCC, clang, or icc.
static inline void radixsort_prefetch(const void *p)
{
    __builtin_prefetch(p,0,3);
}
#elif defined(__SSE__) || (defined(_M_IX86_FP) && (_M_IX86_FP>0)) || defined(_M_AMD64) || defined(_M_X64)
// x86/x64, hopefully compiler understands intrinsics.
#include <xmmintrin.h>
static inline void radixsort_prefetch(const void *p)
{
    _mm_prefetch((const char*)p,_MM_HINT_T0);
}
#else // Short on luck, no-op.
static inline void radixsort_prefetch(const void *p)
{
    (void)p;
}
#endif

// This function is here to satisfy the C++ Standard.
// Merely creating out-of-bounds pointer via address arithmetic is
// undefined behavior, but creating it via conversion from
// integer is implementation-defined.
// The prefetch itself does accept invalid address.
// If we don't have large enough integer type to hold void*,
// then we test for out-of-bounds. This does not seem to impact
// performance much, but removing the test is very unlikely to cause
// problems in practice, despite being technically UB.
#if __cplusplus>=201103L // C++11. We have <cstdint>.
#include <cstdint>
#ifdef UINTPTR_MAX       // We have uintptr_t.
static inline void radixsort_lookahead(const void *p,std::size_t delta)
{
    (void)delta;
    radixsort_prefetch((const void*)(std::uintptr_t(p)+16));
}
#else
static inline void radixsort_lookahead(const void *p,std::size_t delta)
{
    if(sizeof(std::size_t)>=sizeof(void*)) radixsort_prefetch((const void*)(std::size_t(p)+16));
    else if(delta<16) radixsort_prefetch((const char*)(p)+16);
}
#endif
#else
static inline void radixsort_lookahead(const void *p,std::size_t delta)
{
    if(sizeof(std::size_t)>=sizeof(void*)) radixsort_prefetch((const void*)(std::size_t(p)+16));
    else if(delta<16) radixsort_prefetch((const char*)(p)+16);
}
#endif

// Internal functions.

// Fallback sort, used by MSD radix sort on small (~256) inputs.
// Simple out-of-place merge sort, which further falls back
// to insertion sort for smaller (~18) inputs.
template<typename T,typename Traits>
static inline T *fallback_sort(T *src,T *tmp,std::size_t n,int destination)
{
    using std::size_t;
    T *d=(destination==0?src:tmp);
    // 18 is an experimentally chosen threshold.
    if(n<=18) // Insertion sort.
    {
        if(n>0) d[0]=src[0];
        for(size_t i=1;i<n;++i)
        {
            T t=src[i];
            size_t j=i;
            for(;j>0&&Traits::get_key(t)<Traits::get_key(d[j-1]);--j) d[j]=d[j-1];
            d[j]=t;
        }
        return d;
    }
    size_t a=n/2,b=n-a;
    fallback_sort<T,Traits>(src,tmp,a,!destination);
    fallback_sort<T,Traits>(src+a,tmp+a,b,!destination);
    const T *l=(destination==0?tmp:src);
    const T *r=l+a;
    size_t i=0,j=0,k=0;
    while(true)
    {
        if(Traits::get_key(r[j])<Traits::get_key(l[i])) {d[k++]=r[j++]; if(j==b) break;}
        else                                            {d[k++]=l[i++]; if(i==a) break;}
    }
    if(i==a) while(j<b) d[k++]=r[j++];
    else     while(i<a) d[k++]=l[i++];
    return d;
}

// Sort an array according to its WIDTH lower bits, in radix of (1<<BITS).
template<typename T,std::size_t WIDTH,std::size_t BITS,std::size_t THRESHOLD,typename Traits>
static inline T *radix_sort_msd_impl(T *src,T *dst,std::size_t n,int destination)
{
    using std::size_t;
    static const size_t LOG2SIZE=(BITS<WIDTH?BITS:WIDTH);
    static const size_t SIZE=1u<<LOG2SIZE;
    static const size_t OFFSET=WIDTH-LOG2SIZE;
    static const size_t MASK=SIZE-1;
    if(n<THRESHOLD) return fallback_sort<T,Traits>(src,dst,n,destination);
    size_t c[2*SIZE]={0};
    // Cumulative distribution function. Unrolled x2 to mitigate store->load hit.
    for(size_t i=0,m=n/2;i<m;++i)
    {
        size_t k0=size_t(Traits::get_key(src[2*i  ])>>OFFSET)&MASK;
        size_t k1=size_t(Traits::get_key(src[2*i+1])>>OFFSET)&MASK;
        ++c[2*k0  ];
        ++c[2*k1+1];
    }
    if(n&1) ++c[2*(size_t(Traits::get_key(src[n-1])>>OFFSET)&MASK)];
    for(size_t j=0,s=0,t;j<SIZE;++j) {t=s; s+=c[2*j]+c[2*j+1]; c[j]=t;}
    for(size_t j=0;j+1<SIZE;++j)
        if(c[j+1]-c[j]==n) // All keys are in the same bucket.
        {
            T *tmp=src;src=dst;dst=tmp;
            destination^=1;
            goto skip;
        }
    // Scatter.
    for(size_t i=0;i<n;++i)
    {
        size_t k=size_t(Traits::get_key(src[i])>>OFFSET)&MASK;
        radixsort_lookahead(dst+c[k],(n-c[k])*sizeof(T));
        dst[c[k]++]=src[i];
    }
skip:;
    T *out=(destination==0?src:dst);
    if(OFFSET>0)
        for(size_t j=0,b=0;j<SIZE;b=c[j++])
            switch(c[j]-b)
            {
                case 0: break;
                case 1: out[b]=dst[b]; break;
                case 2:
                {
                    bool flip=(Traits::get_key(dst[b+1])<Traits::get_key(dst[b]));
                    T L=dst[b+flip],H=dst[b+!flip];
                    out[b]=L; out[b+1]=H;
                    break;
                }
                default: radix_sort_msd_impl<T,(OFFSET>0?OFFSET:WIDTH),BITS,THRESHOLD,Traits>(dst+b,src+b,c[j]-b,destination^1);
            }
    if(OFFSET==0&&destination==0) for(size_t i=0;i<n;++i) src[i]=dst[i];
    return out;
}

// Sort an array according to its WIDTH upper bits, in radix of (1<<BITS).
template<typename T,std::size_t WIDTH,std::size_t BITS,typename Traits>
static inline T *radix_sort_lsd_impl(T *src,T *dst,std::size_t n)
{
    using std::size_t;
    static const size_t OFFSET=sizeof(Traits::get_key(*src))*CHAR_BIT-WIDTH;
    static const size_t SIZE=1u<<(BITS<WIDTH?BITS:WIDTH);
    static const size_t MASK=SIZE-1;
    size_t c[2*SIZE]={0};
    // Cumulative distribution function. Unrolled x2 to mitigate store->load hit.
    for(size_t i=0,m=n/2;i<m;++i)
    {
        size_t k0=size_t(Traits::get_key(src[2*i  ])>>OFFSET)&MASK;
        size_t k1=size_t(Traits::get_key(src[2*i+1])>>OFFSET)&MASK;
        ++c[2*k0  ];
        ++c[2*k1+1];
    }
    if(n&1) ++c[2*(size_t(Traits::get_key(src[n-1])>>OFFSET)&MASK)];
    for(size_t j=0,s=0,t;j<SIZE;++j) {t=s; s+=c[2*j]+c[2*j+1]; c[j]=t;}
    for(size_t j=0;j+1<SIZE;++j)
        if(c[j+1]-c[j]==n) // All keys are in the same bucket.
        {
            T *tmp=src;src=dst;dst=tmp;
            goto skip;
        }
    // Scatter.
    for(size_t i=0;i<n;++i)
    {
        size_t k=size_t(Traits::get_key(src[i])>>OFFSET)&MASK;
        radixsort_lookahead(dst+c[k],(n-c[k])*sizeof(T));
        dst[c[k]++]=src[i];
    }
skip:;
    // Conditionals are to stop template expansion recursion.
    if(BITS<WIDTH) return radix_sort_lsd_impl<T,(BITS<WIDTH?WIDTH-BITS:WIDTH),BITS,Traits>(dst,src,n);
    return dst;
}

// Sort an array according to its WIDTH lower bits, in radix of (1<<BITS).
template<typename T,std::size_t WIDTH,std::size_t BITS,std::size_t THRESHOLD,typename Traits>
static inline void radix_sort_msd_inplace_impl(T *src,std::size_t n)
{
    using std::size_t;
    static const size_t LOG2SIZE=(BITS<WIDTH?BITS:WIDTH);
    static const size_t SIZE=1u<<LOG2SIZE;
    static const size_t OFFSET=WIDTH-LOG2SIZE;
    static const size_t MASK=SIZE-1;
    if(n<THRESHOLD)
    {
        T tmp[THRESHOLD];
        fallback_sort<T,Traits>(src,tmp,n,0);
        return;
    }
    size_t c[2*SIZE]={0},*d=c+SIZE;
    // Cumulative distribution function. Unrolled x2 to mitigate store->load hit.
    for(size_t i=0,m=n/2;i<m;++i)
    {
        size_t k0=size_t(Traits::get_key(src[2*i  ])>>OFFSET)&MASK;
        size_t k1=size_t(Traits::get_key(src[2*i+1])>>OFFSET)&MASK;
        ++c[2*k0  ];
        ++c[2*k1+1];
    }
    if(n&1) ++c[2*(size_t(Traits::get_key(src[n-1])>>OFFSET)&MASK)];
    for(size_t j=0,s=0,t;j<SIZE;++j) {t=s; s+=c[2*j]+c[2*j+1]; c[j]=t;}
    for(size_t j=0;j+1<SIZE;++j) d[j]=c[j+1];
    d[SIZE-1]=n;
    for(size_t j=0;j+1<SIZE;++j)
        if(c[j+1]-c[j]==n) // All keys are in the same bucket.
            goto skip;
    // Scatter.
    for(size_t j=0;j<SIZE;++j)
        for(;c[j]!=d[j];++c[j])
        {
            size_t k=c[j],h=size_t(Traits::get_key(src[k])>>OFFSET)&MASK;
            while(j!=h)
            {
                T t=src[c[h]];
                radixsort_lookahead(src+c[h],(n-c[h])*sizeof(T));
                src[c[h]++]=src[k];
                src[k]=t;
                h=size_t(Traits::get_key(t)>>OFFSET)&MASK;
            }
        }
skip:;
    if(OFFSET>0)
        for(size_t j=0,b=0;j<SIZE;b=d[j++])
            switch(d[j]-b)
            {
                case 0:
                case 1: break;
                case 2: if(Traits::get_key(src[b+1])<Traits::get_key(src[b])) {T tmp=src[b+1];src[b+1]=src[b];src[b]=tmp;} break;
                default: radix_sort_msd_inplace_impl<T,(OFFSET>0?OFFSET:WIDTH),BITS,THRESHOLD,Traits>(src+b,d[j]-b); break;
            }
}

// MSD and LSD out-of-place versions of radix sort.
// Used internally by radix_sort_stable(), but are usable as is, with
// somewhat decent performance.

template<typename T,std::size_t BITS,std::size_t THRESHOLD,typename Traits>
static inline T *radix_sort_msd(T *src,T *tmp,std::size_t n,int destination)
{
    if(destination!=1) destination=0;
    return radix_sort_msd_impl<T,sizeof(Traits::get_key(*src))*CHAR_BIT,BITS,THRESHOLD,Traits>(src,tmp,n,destination);
}

template<typename T,std::size_t BITS,typename Traits>
static inline T *radix_sort_lsd(T *src,T *tmp,std::size_t n,int destination)
{
    using std::size_t;
    T *ret=radix_sort_lsd_impl<T,sizeof(Traits::get_key(*src))*CHAR_BIT,BITS,Traits>(src,tmp,n);
    if(destination==0&&ret!=src) {ret=src; for(size_t i=0;i<n;++i) src[i]=tmp[i];}
    if(destination==1&&ret!=tmp) {ret=tmp; for(size_t i=0;i<n;++i) tmp[i]=src[i];}
    return ret;
}

// Exported (API) functions.

template<typename T,typename Traits>
inline T *radix_sort_stable(T *src,T* tmp,std::size_t n,int destination,int mode)
{
    // Generally, MSD is faster for:
    //   * small inputs
    //   * large keys
    //   * large data on large inputs
    // Also user may have explicitly asked for it.
    // 1500 and 1000000 are experimentally chosen thresholds.
    if(mode!=0&&(
        mode==1||n<1500||
        sizeof(Traits::get_key(*src))*CHAR_BIT>40||
        (sizeof(T)*CHAR_BIT>64&&n>10000000ul/sizeof(T))))
    {
        size_t bits=8;
        // Some experimantally chosen ranges.
        if(n>4000u&&n<60000u) bits=11;
        if(n>2000000ul&&n<9000000ul) bits=11;
        if(bits==8) return radix_sort_msd<T, 8,128,Traits>(src,tmp,n,destination);
        else        return radix_sort_msd<T,11,256,Traits>(src,tmp,n,destination);
    }

    // Otherwise, return LSD.
    return radix_sort_lsd<T,8,Traits>(src,tmp,n,destination);
}

template<typename T,typename Traits>
inline void radix_sort_inplace(T *src,std::size_t n)
{
    unsigned bits=8;
    // Some experimantally chosen ranges.
    if(n>4000u&&n<60000u) bits=11;
    if(n>2000000ul&&n<9000000ul) bits=11;
    if(bits==8) radix_sort_msd_inplace_impl<T,sizeof(Traits::get_key(*src))*CHAR_BIT, 8,128,Traits>(src,n);
    else        radix_sort_msd_inplace_impl<T,sizeof(Traits::get_key(*src))*CHAR_BIT,11,256,Traits>(src,n);
}


typedef std::uint32_t KeyType;
typedef std::uint32_t ItemType;

struct GetKey
{
    static inline KeyType get_key(const ItemType &src)
    {
        return src;
    }
};

extern "C" void radix_sort(unsigned int *src, unsigned int *tmp, unsigned int n)
{
  //T *radix_sort_stable(T *src,T* tmp,size_t n,int destination,int mode);
  radix_sort_stable<ItemType, GetKey>(src, tmp, n, 0, -1);
}


