#include <stdint.h>

#define I32_AS_F32(vi) ((union { int32_t i; float f; }){.i = vi}.f)
#define I64_AS_F64(vi) ((union { int64_t i; double d; }){.i = vi}.d)
#define F32_AS_I32(v) ((union { int32_t i; float f; }){.f = v}.i)
#define F64_AS_I64(v) ((union { int64_t i; double d; }){.d = v}.i)
#define EXP_F32(vi) ((((vi) >> 23) & 0xff) - 0x7f)
#define EXP_F64(vi) ((((vi) >> 52) & 0x7ff) - 0x3ff)

float floorf(float v)
{
    int32_t vi = F32_AS_I32(v);
    int exp = EXP_F32(vi);
    if (exp < 0)
    { // less than one
        // < 0 -> -1, else sign(v) * 0
        return vi == INT32_MIN ? -0.0f : vi < 0 ? -1.0f
                                                : 0.0f;
    }
    else if (exp < 23)
    {
        int32_t msk = 0x7fffff >> exp;
        return I32_AS_F32((vi + (vi < 0 ? msk : 0)) & ~msk);
    }
    else
    { // integral, Inf, or NaN
        return v;
    }
}

double
floor(double v)
{
    int64_t vi = F64_AS_I64(v);
    int exp = EXP_F64(vi);
    if (exp < 0)
    { // less than one
        // < 0 -> -1, else sign(v) * 0
        return vi == INT64_MIN ? -0.0 : vi < 0 ? -1.0
                                               : 0.0;
    }
    else if (exp < 52)
    {
        int64_t msk = 0xfffffffffffff >> exp;
        return I64_AS_F64((vi + (vi < 0 ? msk : 0)) & ~msk);
    }
    else
    { // integral, Inf, or NaN
        return v;
    }
}

float ceilf(float v)
{
    int32_t vi = F32_AS_I32(v);
    int exp = EXP_F32(vi);
    if (exp < 0)
    { // less than one
        // <= 0 -> sign(v) * 0, else 1
        return vi < 0 ? -0.0f : vi == 0 ? 0.0f
                                        : 1.0f;
    }
    else if (exp < 23)
    {
        int32_t msk = 0x7fffff >> exp;
        return I32_AS_F32((vi + (vi >= 0 ? msk : 0)) & ~msk);
    }
    else
    { // integral, Inf, or NaN
        return v;
    }
}

double
ceil(double v)
{
    int64_t vi = F64_AS_I64(v);
    int exp = EXP_F64(vi);
    if (exp < 0)
    { // less than one
        // <= 0 -> sign(v) * 0, else 1
        return vi < 0 ? -0.0 : vi == 0 ? 0.0
                                       : 1.0;
    }
    else if (exp < 52)
    {
        int64_t msk = 0xfffffffffffff >> exp;
        return I64_AS_F64((vi + (vi >= 0 ? msk : 0)) & ~msk);
    }
    else
    { // integral, Inf, or NaN
        return v;
    }
}

float roundf(float v)
{
    int32_t vi = F32_AS_I32(v);
    int exp = EXP_F32(vi);
    if (exp < -1)
    { // less than 0.5
        return I32_AS_F32(vi & 0x80000000);
    }
    else if (exp == -1)
    { // between 0.5 and 1
        return vi < 0 ? -1.0f : 1.0f;
    }
    else if (exp < 23)
    {
        int32_t msk = 0x7fffff >> exp;
        if (vi & msk) // v is not integral
            return I32_AS_F32((vi + (1 + (msk >> 1))) & ~msk);
        return v;
    }
    else
    { // integral, Inf, or NaN
        return v;
    }
}

double
round(double v)
{
    int64_t vi = F64_AS_I64(v);
    int exp = EXP_F64(vi);
    if (exp < -1)
    { // less than 0.5
        return I64_AS_F64(vi & 0x8000000000000000);
    }
    else if (exp == -1)
    { // between 0.5 and 1
        return vi < 0 ? -1.0 : 1.0;
    }
    else if (exp < 52)
    {
        int64_t msk = 0xfffffffffffff >> exp;
        if (vi & msk) // v is not integral
            return I64_AS_F64((vi + (1 + (msk >> 1))) & ~msk);
        return v;
    }
    else
    { // integral, Inf, or NaN
        return v;
    }
}

float truncf(float v)
{
    int32_t vi = F32_AS_I32(v);
    int exp = EXP_F32(vi);
    if (exp < 0)
    { // less than one
        // < 0 -> -1, else sign(v) * 0
        return vi < 0 ? -0.0f : 0.0f;
    }
    else if (exp < 23)
    {
        int32_t msk = 0x7fffff >> exp;
        return I32_AS_F32(vi & ~msk);
    }
    else
    { // integral, Inf, or NaN
        return v;
    }
}

double
trunc(double v)
{
    int64_t vi = F64_AS_I64(v);
    int exp = EXP_F64(vi);
    if (exp < 0)
    { // less than one
        // < 0 -> -1, else sign(v) * 0
        return vi < 0 ? -0.0 : 0.0;
    }
    else if (exp < 52)
    {
        int64_t msk = 0xfffffffffffff >> exp;
        return I64_AS_F64(vi & ~msk);
    }
    else
    { // integral, Inf, or NaN
        return v;
    }
}

float fmaf(float x, float y, float z)
{
    return (x * y) + z; // TODO: actually perform fused multiply-add
}

double
fma(double x, double y, double z)
{
    return (x * y) + z; // TODO: actually perform fused multiply-add
}