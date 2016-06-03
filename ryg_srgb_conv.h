// float->sRGB8 conversions - two variants.
// by Fabian "ryg" Giesen
//
// I hereby place this code in the public domain.
//
// Both variants come with absolute error bounds and a reversibility and monotonicity
// guarantee (see test driver code below). They should pass D3D10 conformance testing
// (not that you can verify this, but still). They are verified against a clean reference
// implementation provided below, and the test driver checks all floats exhaustively.
//
// Variant 1 uses a smaller table (256 bytes) but a bit more code; variant 2 uses
// a 416-byte table and has simpler dataflow, so in terms of raw cycle count it should
// be faster, at the cost of a few more cache lines polluting L1. Both come in scalar
// and SSE2 variants. There's no total SSE2-isms there, so it should be reasonably easy
// to port to a different architecture. The biggest single part that would have to be
// replaced is the (admittedly weird) usage of PMADDWD (_mm_madd_epi16). The scale/bias
// computation can be done in other ways, but this one happened to map quite nicely to
// my requirements, so I used it.
//
// Generators for the tables are also included, for the curious. (Nothing up my sleeve!)
#ifndef RYGI_INCLUDE_RYG_SRGB_CONV_H
#define RYGI_INCLUDE_RYG_SRGB_CONV_H

unsigned char ryg_float_to_srgb8(float);
float ryg_srgb8_to_float(unsigned char);
void ryg_float_to_srgb8_SIMD(unsigned char out[4], float in[4]);

#endif // RYGI_INCLUDE_RYG_SRGB_CONV_H

#ifdef RYG_SRGB_CONV_IMPLEMENTATION

//#define RYGI_NO_SIMD

#include <stdio.h>
#include <math.h>

// x86/x64 detection
#if defined(__x86_64__) || defined(_M_X64)
#define RYGI__X64_TARGET
#elif defined(__i386) || defined(_M_IX86)
#define RYGI__X86_TARGET
#endif

#ifndef RYGI_NO_SIMD
#include <emmintrin.h>

#if defined(__GNUC__) && defined(RYGI__X86_TARGET) && !defined(RYGI_NO_SIMD)
#define SSE2FUNC __attribute__((__target__("sse2"), force_align_arg_pointer))
#else
#define SSE2FUNC
#endif
#endif

#if defined (_MSC_VER)
#define ALIGN16 __declspec(align(16))
#else
#define ALIGN16 __attribute__ ((aligned (16)))
#endif

typedef unsigned char uint8;
typedef unsigned int uint;

#if defined(_MSC_VER)
#define GET_128(x) (x)
#else
#include <stdint.h>

typedef union ALIGN16 {
    __m128i m128i;
    int8_t m128i_i8[16];
    int16_t m128i_i16[8];
    int32_t m128i_i32[4];
    int64_t m128i_i64[2];
    uint8_t m128i_u8[16];
    uint16_t m128i_u16[8];
    uint32_t m128i_u32[4];
    uint64_t m128i_u64[2];

    __m128 m128;
    int8_t m128_i8[16];
    int16_t m128_i16[8];
    int32_t m128_i32[4];
    int64_t m128_i64[2];
    uint8_t m128_u8[16];
    uint16_t m128_u16[8];
    uint32_t m128_u32[4];
    uint64_t m128_u64[2];
} get_128_u;

#define GET_128(x) (*(get_128_u *)&(x))
#endif

typedef union
{
    uint u;
    float f;
    struct
    {
        uint Mantissa : 23;
        uint Exponent : 8;
        uint Sign : 1;
    };
} FP32;

#define AS_UINT(x) (((FP32 *)&(x))->u)
#define AS_FLOAT(x) (((FP32 *)&(x))->f)

// Returns "exact" float value. Round to nearest integer for conversion.
// Done this way so that conversion error can be estimated properly.
static float float_to_srgb8_ref(float f)
{
    float s;

    if (!(f > 0.0f)) // also covers NaNs
        s = 0.0f;
    else if (f <= 0.0031308f)
        s = 12.92f * f;
    else if (f < 1.0f)
        s = 1.055f * pow(f, 1.0f / 2.4f) - 0.055f;
    else
        s = 1.0f;

    return s * 255.0f;
}

static uint8 float_to_srgb8_ref_int(float f)
{
    return (uint8) (float_to_srgb8_ref(f) + 0.5f); // round, then float->int
}

// There's just 256 different input valus - just use a table.
static const uint srgb8_to_fp32_tab[256] = {
    0x00000000, 0x399f22b4, 0x3a1f22b4, 0x3a6eb40f, 0x3a9f22b4, 0x3ac6eb61, 0x3aeeb40f, 0x3b0b3e5e,
    0x3b1f22b4, 0x3b33070b, 0x3b46eb61, 0x3b5b518d, 0x3b70f18d, 0x3b83e1c6, 0x3b8fe616, 0x3b9c87fd,
    0x3ba9c9b7, 0x3bb7ad6f, 0x3bc63549, 0x3bd56361, 0x3be539c1, 0x3bf5ba70, 0x3c0373b5, 0x3c0c6152,
    0x3c15a703, 0x3c1f45be, 0x3c293e6b, 0x3c3391f7, 0x3c3e4149, 0x3c494d43, 0x3c54b6c7, 0x3c607eb1,
    0x3c6ca5df, 0x3c792d22, 0x3c830aa8, 0x3c89af9f, 0x3c9085db, 0x3c978dc5, 0x3c9ec7c2, 0x3ca63433,
    0x3cadd37d, 0x3cb5a601, 0x3cbdac20, 0x3cc5e639, 0x3cce54ab, 0x3cd6f7d5, 0x3cdfd010, 0x3ce8ddb9,
    0x3cf22131, 0x3cfb9ac6, 0x3d02a56c, 0x3d0798df, 0x3d0ca7e7, 0x3d11d2b2, 0x3d171965, 0x3d1c7c31,
    0x3d21fb3f, 0x3d2796b5, 0x3d2d4ebe, 0x3d332384, 0x3d39152e, 0x3d3f23e6, 0x3d454fd4, 0x3d4b991f,
    0x3d51ffef, 0x3d58846a, 0x3d5f26b7, 0x3d65e6fe, 0x3d6cc564, 0x3d73c20f, 0x3d7add29, 0x3d810b67,
    0x3d84b795, 0x3d887330, 0x3d8c3e4a, 0x3d9018f6, 0x3d940345, 0x3d97fd4a, 0x3d9c0716, 0x3da020bb,
    0x3da44a4b, 0x3da883d7, 0x3daccd70, 0x3db12728, 0x3db59112, 0x3dba0b3b, 0x3dbe95b5, 0x3dc33092,
    0x3dc7dbe2, 0x3dcc97b6, 0x3dd1641f, 0x3dd6412c, 0x3ddb2eef, 0x3de02d77, 0x3de53cd5, 0x3dea5d19,
    0x3def8e55, 0x3df4d093, 0x3dfa23ea, 0x3dff8864, 0x3e027f09, 0x3e054282, 0x3e080ea5, 0x3e0ae379,
    0x3e0dc107, 0x3e10a755, 0x3e13966c, 0x3e168e53, 0x3e198f11, 0x3e1c98ae, 0x3e1fab32, 0x3e22c6a3,
    0x3e25eb0b, 0x3e29186d, 0x3e2c4ed4, 0x3e2f8e45, 0x3e32d6c8, 0x3e362865, 0x3e398322, 0x3e3ce706,
    0x3e405419, 0x3e43ca62, 0x3e4749e8, 0x3e4ad2b1, 0x3e4e64c6, 0x3e52002b, 0x3e55a4e9, 0x3e595307,
    0x3e5d0a8b, 0x3e60cb7c, 0x3e6495e0, 0x3e6869bf, 0x3e6c4720, 0x3e702e0c, 0x3e741e84, 0x3e781890,
    0x3e7c1c38, 0x3e8014c2, 0x3e82203c, 0x3e84308d, 0x3e8645ba, 0x3e885fc5, 0x3e8a7eb2, 0x3e8ca283,
    0x3e8ecb3d, 0x3e90f8e1, 0x3e932b74, 0x3e9562f8, 0x3e979f71, 0x3e99e0e2, 0x3e9c274e, 0x3e9e72b7,
    0x3ea0c322, 0x3ea31892, 0x3ea57308, 0x3ea7d289, 0x3eaa3718, 0x3eaca0b7, 0x3eaf0f69, 0x3eb18333,
    0x3eb3fc18, 0x3eb67a18, 0x3eb8fd37, 0x3ebb8579, 0x3ebe12e1, 0x3ec0a571, 0x3ec33d2d, 0x3ec5da17,
    0x3ec87c33, 0x3ecb2383, 0x3ecdd00b, 0x3ed081cd, 0x3ed338cc, 0x3ed5f50b, 0x3ed8b68d, 0x3edb7d54,
    0x3ede4965, 0x3ee11ac1, 0x3ee3f16b, 0x3ee6cd67, 0x3ee9aeb6, 0x3eec955d, 0x3eef815d, 0x3ef272ba,
    0x3ef56976, 0x3ef86594, 0x3efb6717, 0x3efe6e02, 0x3f00bd2d, 0x3f02460e, 0x3f03d1a7, 0x3f055ff9,
    0x3f06f108, 0x3f0884d1, 0x3f0a1b57, 0x3f0bb49d, 0x3f0d50a2, 0x3f0eef69, 0x3f1090f2, 0x3f123540,
    0x3f13dc53, 0x3f15862d, 0x3f1732cf, 0x3f18e23b, 0x3f1a9471, 0x3f1c4973, 0x3f1e0143, 0x3f1fbbe1,
    0x3f217950, 0x3f23398f, 0x3f24fca2, 0x3f26c288, 0x3f288b43, 0x3f2a56d5, 0x3f2c253f, 0x3f2df681,
    0x3f2fca9e, 0x3f31a199, 0x3f337b6e, 0x3f355822, 0x3f3737b5, 0x3f391a28, 0x3f3aff7e, 0x3f3ce7b7,
    0x3f3ed2d4, 0x3f40c0d6, 0x3f42b1c0, 0x3f44a592, 0x3f469c4d, 0x3f4895f3, 0x3f4a9284, 0x3f4c9203,
    0x3f4e9470, 0x3f5099cd, 0x3f52a21a, 0x3f54ad59, 0x3f56bb8c, 0x3f58ccb3, 0x3f5ae0cf, 0x3f5cf7e2,
    0x3f5f11ee, 0x3f612ef2, 0x3f634eef, 0x3f6571ec, 0x3f6797e3, 0x3f69c0db, 0x3f6beccd, 0x3f6e1bc4,
    0x3f704db8, 0x3f7282b4, 0x3f74baae, 0x3f76f5b3, 0x3f7933b9, 0x3f7b74cb, 0x3f7db8e0, 0x3f800000,
};

static float srgb8_to_float(uint8 val)
{
    return AS_FLOAT(srgb8_to_fp32_tab[val]);
}

// This is the version that tries to use a small table (4 cache lines at 64 bytes/line)
// at the expense of a few extra instructions. Use "var2" below for a version with
// less instructions that uses a somewhat larger table.
//
// Float is semi-logarithmic.
// Linear x->sRGB for x >= 0.0031308 is (mostly) a power function which we
// approximate with a bunch of linear segments based on exponent and 3 highest
// bits of mantissa (2 was too inaccurate).
//
// Which exponents do we care about?
// Exponent >= 0: value was >=1, so we return 255 (in fact, we min with 1.0f-eps, so this never happens anyway).
// Exponent < -9: x < 1/512 which is well into the linear part of the sRGB mapping function.
// So the interesting exponent range is [-9,-1].
//
// To get a pow2-sized range, we cheat a bit and only store anchors for linear segments in
// the exponent range [-8,-1], using linear sRGB part of the formula until 1/256.
// This means that we treat a small part of the nonlinear range (namely, the interval
// [0.0031308,0.00390625]) as linear. Our linear scale value needs to be adjusted for this.
// This is done simply by starting from the "correct" scale value (255*12.92, 0x454de99a)
// and doing a binary search for the value that gives the best results (=lowest max error
// in this case) across the range we care about.
//
// The table itself has a bias in the top 16 bits and a scale factor for the linear function
// (based on the next 8 mantissa bits after the 3 we already used). Both are scaled to make
// good use of the available bits. The format was chosen this way so the linear function
// can be evaluated with a single PMADDWD after the mantissa bits were extracted - okay, we do
// need to insert one more set bit in the high half for the bias part to work.
// These coefficients were determined simply by doing a least-squares fit of a linear function
// f(x) = a+b*x inside each "bucket" (see table-making functions below).
//
// Max error for whole function (integer-rounded result minus "exact" value, as computed in
// floats using the official formula): 0.573277 at 0x3b7a88c6
static const uint fp32_to_srgb8_tab3[64] = {
    0x0b0f01cb, 0x0bf401ae, 0x0ccb0195, 0x0d950180, 0x0e56016e, 0x0f0d015e, 0x0fbc0150, 0x10630143,
    0x11070264, 0x1238023e, 0x1357021d, 0x14660201, 0x156601e9, 0x165a01d3, 0x174401c0, 0x182401af,
    0x18fe0331, 0x1a9602fe, 0x1c1502d2, 0x1d7e02ad, 0x1ed4028d, 0x201a0270, 0x21520256, 0x227d0240,
    0x239f0443, 0x25c003fe, 0x27bf03c4, 0x29a10392, 0x2b6a0367, 0x2d1d0341, 0x2ebe031f, 0x304d0300,
    0x31d105b0, 0x34a80555, 0x37520507, 0x39d504c5, 0x3c37048b, 0x3e7c0458, 0x40a8042a, 0x42bd0401,
    0x44c20798, 0x488e071e, 0x4c1c06b6, 0x4f76065d, 0x52a50610, 0x55ac05cc, 0x5892058f, 0x5b590559,
    0x5e0c0a23, 0x631c0980, 0x67db08f6, 0x6c55087f, 0x70940818, 0x74a007bd, 0x787d076c, 0x7c330723,
    0x06970158, 0x07420142, 0x07e30130, 0x087b0120, 0x090b0112, 0x09940106, 0x0a1700fc, 0x0a9500f2,
};

static uint8 float_to_srgb8(float in)
{
    static const FP32 almostone = { 0x3f7fffff }; // 1-eps
    static const FP32 lutthresh = { 0x3b800000 }; // 2^(-8)
    static const FP32 linearsc  = { 0x454c5d00 };
    static const FP32 float2int = { (127 + 23) << 23 };
    FP32 f;

    // Clamp to [0, 1-eps]; these two values map to 0 and 1, respectively.
    // The tests are carefully written so that NaNs map to 0, same as in the reference
    // implementation.
    if (!(in > 0.0f)) // written this way to catch NaNs
        in = 0.0f;
    if (in > almostone.f)
        in = almostone.f;

    // Check which region this value falls into
    f.f = in;
    if (f.f < lutthresh.f) // linear region
    {
        f.f *= linearsc.f;
        f.f += float2int.f; // use "magic value" to get float->int with rounding.
        return (uint8) (f.u & 255);
    }
    else // non-linear region
    {
        // Unpack bias, scale from table
        uint tab = fp32_to_srgb8_tab3[(f.u >> 20) & 63];
        uint bias = (tab >> 16) << 9;
        uint scale = tab & 0xffff;

        // Grab next-highest mantissa bits and perform linear interpolation
        uint t = (f.u >> 12) & 0xff;
        return (uint8) ((bias + scale*t) >> 16);
    }
}

#ifndef RYGI_NO_SIMD
SSE2FUNC static __m128i float_to_srgb8_SSE2(__m128 f)
{
#define SSE_CONST4(name, val) static const ALIGN16 uint name[4] = { (val), (val), (val), (val) }
#define CONST(name) *(const __m128i *)&name
#define CONSTF(name) *(const __m128 *)&name

    SSE_CONST4(c_almostone, 0x3f7fffff);
    SSE_CONST4(c_lutthresh, 0x3b800000);
    SSE_CONST4(c_tabmask,   63);
    SSE_CONST4(c_linearsc,  0x454c5d00);
    SSE_CONST4(c_mantmask,  0xff);
    SSE_CONST4(c_topscale,  0x02000000);

    __m128i temp; // temp value (on stack)

    // Initial clamp
    __m128  zero        = _mm_setzero_ps();
    __m128  clamp1      = _mm_max_ps(f, zero); // limit to [0,1-eps] - also nukes NaNs
    __m128  clamp2      = _mm_min_ps(clamp1, CONSTF(c_almostone));

    // Table index
    __m128i tabidx1     = _mm_srli_epi32(_mm_castps_si128(clamp2), 20);
    __m128i tabidx2     = _mm_and_si128(tabidx1, CONST(c_tabmask));
    _mm_store_si128(&temp, tabidx2);

    // Table lookup
    GET_128(temp).m128i_u32[0] = fp32_to_srgb8_tab3[GET_128(temp).m128i_u32[0]];
    GET_128(temp).m128i_u32[1] = fp32_to_srgb8_tab3[GET_128(temp).m128i_u32[1]];
    GET_128(temp).m128i_u32[2] = fp32_to_srgb8_tab3[GET_128(temp).m128i_u32[2]];
    GET_128(temp).m128i_u32[3] = fp32_to_srgb8_tab3[GET_128(temp).m128i_u32[3]];

    // Linear part of ramp
    __m128  linear1     = _mm_mul_ps(clamp2, CONSTF(c_linearsc));
    __m128i linear2     = _mm_cvtps_epi32(linear1);

    // Table finisher
    __m128i tabval      = _mm_load_si128(&temp);
    __m128i tabmult1    = _mm_srli_epi32(_mm_castps_si128(clamp2), 12);
    __m128i tabmult2    = _mm_and_si128(tabmult1, CONST(c_mantmask));
    __m128i tabmult3    = _mm_or_si128(tabmult2, CONST(c_topscale));
    __m128i tabprod     = _mm_madd_epi16(tabval, tabmult3);
    __m128i tabshifted  = _mm_srli_epi32(tabprod, 16);

    // Combine linear+table
    __m128  b_uselin    = _mm_cmplt_ps(clamp2, CONSTF(c_lutthresh)); // use linear results
    __m128i merge1      = _mm_and_si128(linear2, _mm_castps_si128(b_uselin));
    __m128i merge2      = _mm_andnot_si128(_mm_castps_si128(b_uselin), tabshifted);
    __m128i result      = _mm_or_si128(merge1, merge2);

    return result;

#undef SSE_CONST4
#undef CONST
#undef CONSTF
}
#endif // RYGI_NO_SIMD


// This version uses a larger table than the code above (104 entries at 4 bytes each,
// or 6.5 cache lines at 64b/line) but is conceptually simpler and needs less instructions.
//
// The basic ideas are still the same, only this time, we squeeze everything into the
// table, even the linear part of the range; since we are approximating the function as
// piecewise linear anyway, this is fairly easy.
//
// In the exact version of the conversion, any value that produces an output float less
// than 0.5 will be rounded to an integer of zero. Inverting the linear part of the transform,
// we get:
//
//   log2(0.5 / (255 * 12.92)) =~ -12.686
//
// which in turn means that any value smaller than about 2^(-12.687) will return 0.
// What this means is that we can adapt the clamping code to just clamp to
// [2^(-13), 1-eps] and we're covered. This means our table needs to cover a range of
// 13 different exponents from -13 to -1.
//
// The table lookup, storage and interpolation works exactly the same way as in the code
// above.
//
// Max error for the whole function (integer-rounded result minus "exact" value, as computed in
// floats using the official formula): 0.544403 at 0x3e9f8000
static const uint fp32_to_srgb8_tab4[104] = {
    0x0073000d, 0x007a000d, 0x0080000d, 0x0087000d, 0x008d000d, 0x0094000d, 0x009a000d, 0x00a1000d,
    0x00a7001a, 0x00b4001a, 0x00c1001a, 0x00ce001a, 0x00da001a, 0x00e7001a, 0x00f4001a, 0x0101001a,
    0x010e0033, 0x01280033, 0x01410033, 0x015b0033, 0x01750033, 0x018f0033, 0x01a80033, 0x01c20033,
    0x01dc0067, 0x020f0067, 0x02430067, 0x02760067, 0x02aa0067, 0x02dd0067, 0x03110067, 0x03440067,
    0x037800ce, 0x03df00ce, 0x044600ce, 0x04ad00ce, 0x051400ce, 0x057b00c5, 0x05dd00bc, 0x063b00b5,
    0x06970158, 0x07420142, 0x07e30130, 0x087b0120, 0x090b0112, 0x09940106, 0x0a1700fc, 0x0a9500f2,
    0x0b0f01cb, 0x0bf401ae, 0x0ccb0195, 0x0d950180, 0x0e56016e, 0x0f0d015e, 0x0fbc0150, 0x10630143,
    0x11070264, 0x1238023e, 0x1357021d, 0x14660201, 0x156601e9, 0x165a01d3, 0x174401c0, 0x182401af,
    0x18fe0331, 0x1a9602fe, 0x1c1502d2, 0x1d7e02ad, 0x1ed4028d, 0x201a0270, 0x21520256, 0x227d0240,
    0x239f0443, 0x25c003fe, 0x27bf03c4, 0x29a10392, 0x2b6a0367, 0x2d1d0341, 0x2ebe031f, 0x304d0300,
    0x31d105b0, 0x34a80555, 0x37520507, 0x39d504c5, 0x3c37048b, 0x3e7c0458, 0x40a8042a, 0x42bd0401,
    0x44c20798, 0x488e071e, 0x4c1c06b6, 0x4f76065d, 0x52a50610, 0x55ac05cc, 0x5892058f, 0x5b590559,
    0x5e0c0a23, 0x631c0980, 0x67db08f6, 0x6c55087f, 0x70940818, 0x74a007bd, 0x787d076c, 0x7c330723,
};

static uint8 float_to_srgb8_var2(float in)
{
    static const FP32 almostone = { 0x3f7fffff }; // 1-eps
    static const FP32 minval    = { (127-13) << 23 };
    FP32 f;

    // Clamp to [2^(-13), 1-eps]; these two values map to 0 and 1, respectively.
    // The tests are carefully written so that NaNs map to 0, same as in the reference
    // implementation.
    if (!(in > minval.f)) // written this way to catch NaNs
        in = minval.f;
    if (in > almostone.f)
        in = almostone.f;

    // Do the table lookup and unpack bias, scale
    f.f = in;
    uint tab = fp32_to_srgb8_tab4[(f.u - minval.u) >> 20];
    uint bias = (tab >> 16) << 9;
    uint scale = tab & 0xffff;

    // Grab next-highest mantissa bits and perform linear interpolation
    uint t = (f.u >> 12) & 0xff;
    return (uint8) ((bias + scale*t) >> 16);
}

#ifndef RYGI_NO_SIMD
SSE2FUNC static __m128i float_to_srgb8_var2_SSE2(__m128 f)
{
#define SSE_CONST4(name, val) static const ALIGN16 uint name[4] = { (val), (val), (val), (val) }
#define CONST(name) *(const __m128i *)&name
#define CONSTF(name) *(const __m128 *)&name

    SSE_CONST4(c_clampmin,  (127 - 13) << 23);
    SSE_CONST4(c_almostone, 0x3f7fffff);
    SSE_CONST4(c_mantmask,  0xff);
    SSE_CONST4(c_topscale,  0x02000000);

    __m128i temp; // temp value (on stack)

    // Initial clamp
    __m128  clamp1      = _mm_max_ps(f, CONSTF(c_clampmin)); // limit to [clampmin,1-eps] - also nuke NaNs
    __m128  clamp2      = _mm_min_ps(clamp1, CONSTF(c_almostone));

    // Table index
    __m128i tabidx      = _mm_srli_epi32(_mm_castps_si128(clamp2), 20);
    _mm_store_si128(&temp, tabidx);

    // Table lookup
    GET_128(temp).m128i_u32[0] = fp32_to_srgb8_tab4[GET_128(temp).m128i_i32[0] - (127-13)*8];
    GET_128(temp).m128i_u32[1] = fp32_to_srgb8_tab4[GET_128(temp).m128i_i32[1] - (127-13)*8];
    GET_128(temp).m128i_u32[2] = fp32_to_srgb8_tab4[GET_128(temp).m128i_i32[2] - (127-13)*8];
    GET_128(temp).m128i_u32[3] = fp32_to_srgb8_tab4[GET_128(temp).m128i_i32[3] - (127-13)*8];

    // Finisher
    __m128i tabval      = _mm_load_si128(&temp);
    __m128i tabmult1    = _mm_srli_epi32(_mm_castps_si128(clamp2), 12);
    __m128i tabmult2    = _mm_and_si128(tabmult1, CONST(c_mantmask));
    __m128i tabmult3    = _mm_or_si128(tabmult2, CONST(c_topscale));
    __m128i tabprod     = _mm_madd_epi16(tabval, tabmult3);
    __m128i result      = _mm_srli_epi32(tabprod, 16);

    return result;

#undef SSE_CONST4
#undef CONST
#undef CONSTF
}
#endif // RYGI_NO_SIMD


// ----
//
// Table generation functions. These are not required to run the code; they're just
// here to show how the tables were computed.

//#define GENTABLES

#ifdef GENTABLES

static void print_table(const char *filename, const char *varname, const uint *table, int nelem)
{
    FILE *file = fopen(filename, "w");
    fprintf(file, "static const uint %s[%d] = {\n", varname, nelem);
    for (int i=0; i < nelem; i++)
    {
        if ((i & 7) == 0)
            fprintf(file, "   ");
        fprintf(file, " 0x%08x,", table[i]);
        if ((i & 7) == 7)
            fprintf(file, "\n");
    }

    if ((nelem & 7) != 0)
        fprintf(file, "\n");

    fprintf(file, "};\n");
    fclose(file);
}

// Table-generation function for srgb8 to float.
static void make_tab1()
{
    static const int nelem = 256;
    uint table[nelem];
    
    for (int i=0; i < nelem; i++)
    {
        float c = (float) i * (1.0f / 255.0f);
        if (c <= 0.04045f)
            AS_FLOAT(table[i]) = c / 12.92f;
        else
            AS_FLOAT(table[i]) = pow((c + 0.055f) / 1.055f, 2.4f);
    }

    print_table("tab1.txt", "srgb8_to_fp32_tab", table, nelem);
}

// Table-generation function for variant 1 above.
static void make_tab3()
{
    static const int nbuckets = 64;
    static const int bucketsize = 1 << 20;
    static const int mantshift = 12;

    FP32 f;
    uint table[nbuckets];

    double sum_aa = bucketsize;
    double sum_ab = 0.0, sum_bb = 0.0;
    for (int i=0; i < bucketsize; i++)
    {
        int j = i >> mantshift;

        sum_ab += j;
        sum_bb += j*j;
    }

    double inv_det = 1.0 / (sum_aa * sum_bb - sum_ab * sum_ab);

    for (int bucket=0; bucket < nbuckets; bucket++)
    {
        int start = 0x3b800000 + bucket*bucketsize;
        double sum_a = 0.0;
        double sum_b = 0.0;

        // model: a+b*i
        for (int i=0; i<bucketsize; i++)
        {
            int j = i >> mantshift;

            f.u = start + i;
            float val = float_to_srgb8_ref(f.f) + 0.5f;

            sum_a += val;
            sum_b += j*val;
        }

        // solve
        double solved_a = inv_det * (sum_bb*sum_a - sum_ab*sum_b);
        double solved_b = inv_det * (sum_aa*sum_b - sum_ab*sum_a);

        double scaled_a = solved_a * 65536.0 / 512.0;
        double scaled_b = solved_b * 65536.0;

        int int_a = (int) (scaled_a + 0.5f);
        int int_b = (int) (scaled_b + 0.5f);
        table[(start / bucketsize) & (nbuckets - 1)] = (int_a << 16) + int_b;
        printf("%d\n", bucket);
    }

    print_table("tab.txt", "fp32_to_srgb8_tab3", table, nbuckets);
}

// Table-generation function for variant 2 above.
static void make_tab4()
{
    static const int numexp = 13;
    static const int mantissa_msb = 3;
    static const int nbuckets = numexp << mantissa_msb;
    static const int bucketsize = 1 << (23 - mantissa_msb);
    static const int mantshift = 12;

    FP32 f;
    uint table[nbuckets];

    double sum_aa = bucketsize;
    double sum_ab = 0.0, sum_bb = 0.0;
    for (int i=0; i < bucketsize; i++)
    {
        int j = i >> mantshift;

        sum_ab += j;
        sum_bb += j*j;
    }

    double inv_det = 1.0 / (sum_aa * sum_bb - sum_ab * sum_ab);

    for (int bucket=0; bucket < nbuckets; bucket++)
    {
        int start = ((127 - numexp) << 23) + bucket*bucketsize;
        double sum_a = 0.0;
        double sum_b = 0.0;

        // model: a+b*i
        for (int i=0; i<bucketsize; i++)
        {
            int j = i >> mantshift;

            f.u = start + i;
            float val = float_to_srgb8_ref(f.f) + 0.5f;

            sum_a += val;
            sum_b += j*val;
        }

        // solve
        double solved_a = inv_det * (sum_bb*sum_a - sum_ab*sum_b);
        double solved_b = inv_det * (sum_aa*sum_b - sum_ab*sum_a);

        double scaled_a = solved_a * 65536.0 / 512.0;
        double scaled_b = solved_b * 65536.0;

        int int_a = (int) (scaled_a + 0.5f);
        int int_b = (int) (scaled_b + 0.5f);
        table[bucket] = (int_a << 16) + int_b;
        printf("%d\n", bucket);
    }

    print_table("tab4.txt", "fp32_to_srgb8_tab4", table, nbuckets);
}

#endif

unsigned char ryg_float_to_srgb8(float in)
{
    return float_to_srgb8(in);
}

float ryg_srgb8_to_float(unsigned char in)
{
    return srgb8_to_float(in);
}

SSE2FUNC void ryg_float_to_srgb8_SIMD(unsigned char out[4], float in[4])
{
#ifdef RYGI_NO_SIMD
    out[0] = float_to_srgb8(in[0]);
    out[1] = float_to_srgb8(in[1]);
    out[2] = float_to_srgb8(in[2]);
    out[3] = float_to_srgb8(in[3]);
#else
    __m128 ssein;
    __m128i sseout;

    GET_128(ssein).m128_u32[0] = AS_UINT(in[0]);
    GET_128(ssein).m128_u32[1] = AS_UINT(in[1]);
    GET_128(ssein).m128_u32[2] = AS_UINT(in[2]);
    GET_128(ssein).m128_u32[3] = AS_UINT(in[3]);

    sseout = float_to_srgb8_SSE2(ssein);

    out[0] = GET_128(sseout).m128i_u32[0];
    out[1] = GET_128(sseout).m128i_u32[1];
    out[2] = GET_128(sseout).m128i_u32[2];
    out[3] = GET_128(sseout).m128i_u32[3];
#endif
}

#if 0

// number of variants we're testing
#define NUM_VARIANTS 2

SSE2FUNC int main()
{
#ifdef GENTABLES
    // generate the tables
    make_tab3();
    make_tab4();
    make_tab1();
#endif

    // First, verify that conversion round-trip works. This is an
    // obvious and important constraint.
    for (int i=0; i < 256; i++)
    {
        float f = srgb8_to_float(i);
        int ref = float_to_srgb8_ref_int(f);
        int var1 = float_to_srgb8(f);
        int var2 = float_to_srgb8_var2(f);

        if (ref != i || var1 != i || var2 != i)
        {
            printf("invertability broken! i=%d ref=%d var1=%d var2=%d\n", i, ref, var1, var2);
            return 1;
        }
    }

    // Loop over whole 32-bit range, checking whether error is within allowed bounds.
    // At the same time, we also test whether float->sRGB8 conversion is monotonic.
    // To make the latter easy, we traverse the range starting from the first "positive"
    // NaN (which maps to 0), then going over all negative values, finally looping back
    // to 0 and positive values. That way, we see the whole range in increasing order of
    // return values.
    static const float max_abs_err = 0.6f;
    float maxerr[NUM_VARIANTS] = { 0 };
    uint maxerrat[NUM_VARIANTS] = { 0 };
    int prev[NUM_VARIANTS] = { 0 };

    uint start = (255 << 23) + 1; // first NaN
    uint u = start;

    printf("Scalar\n");

    do
    {
        FP32 f;
        int res[NUM_VARIANTS];

        f.u = u;
        float ref_val = float_to_srgb8_ref(f.f);

        res[0] = float_to_srgb8(f.f);
        res[1] = float_to_srgb8_var2(f.f);

        for (int i=0; i < NUM_VARIANTS; i++)
        {
            float err = fabs(res[i] - ref_val);
            if (err >= max_abs_err)
            {
                printf("err=%f at u=%08x for variant %d, must be less than %f!\n", err, u, i + 1, max_abs_err);
                return 1;
            }

            if (err >= maxerr[i])
            {
                maxerr[i] = err;
                maxerrat[i] = u;
            }

            if (res[i] < prev[i])
            {
                printf("monotonicity violated at u=%08x for variant %d! result=%d prev=%d\n", u, i + 1, res[i], prev[i]);
                return 1;
            }
            prev[i] = res[i];
        }

        u++;
        if ((u & 0xffffff) == 1)
            printf("  %02x\n", u >> 24);
    } while (u != start);

#ifndef RYGI_NO_SIMD
    printf("SIMD\n");

    do
    {
        __m128 ssein;
        __m128i sseout[2];
        __m128i ref[2];

        for (uint j=0; j < 4; j++)
        {
            GET_128(ssein).m128_u32[j] = u + j;

            FP32 f;
            f.u = u + j;
            GET_128(ref[0]).m128i_u32[j] = float_to_srgb8(f.f);
            GET_128(ref[1]).m128i_u32[j] = float_to_srgb8_var2(f.f);
        }

        sseout[0] = float_to_srgb8_SSE2(ssein);
        sseout[1] = float_to_srgb8_var2_SSE2(ssein);

        for (int i=0; i < NUM_VARIANTS; i++)
        {
            for (uint j=0; j < 4; j++)
            {
                uint simd = GET_128(sseout[i]).m128i_u32[j];
                uint scalar = GET_128(ref[i]).m128i_u32[j];
                if (simd != scalar)
                {
                    printf("SIMD/scalar mismatch at u=%08x for variant %d: scalar=%d, SIMD=%d\n", u + j, i + 1, scalar, simd);
                    return 1;
                }
            }
        }

        u += 4;
        if ((u & 0xffffff) == 1)
            printf("  %02x\n", u >> 24);
    } while (u != start);
#endif

    printf("\nAll done!\n\n");

    for (int i=0; i < NUM_VARIANTS; i++)
        printf("variant %d: max error %f at 0x%08x\n", i+1, maxerr[i], maxerrat[i]);

    return 0;
}
#endif 

#endif // RYG_SRGB_CONV_IMPLEMENTATION
