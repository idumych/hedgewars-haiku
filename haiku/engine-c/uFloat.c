#include "fpcrtl.h"

#include "uFloat.h"
boolean ufloat_HWFloat_HWFloat_op_eq(HWFloat z1,HWFloat z2);
boolean ufloat_HWFloat_HWFloat_op_neq(HWFloat z1,HWFloat z2);
HWFloat ufloat_HWFloat_HWFloat_op_add(HWFloat z1,HWFloat z2);
HWFloat ufloat_HWFloat_HWFloat_op_sub(HWFloat z1,HWFloat z2);
HWFloat ufloat_HWFloat_op_sub(HWFloat z1);
HWFloat ufloat_HWFloat_HWFloat_op_mul(HWFloat z1,HWFloat z2);
HWFloat ufloat_HWFloat_LongInt_op_mul(HWFloat z1,LongInt z2);
HWFloat ufloat_HWFloat_HWFloat_op_div(HWFloat z1,HWFloat z2);
HWFloat ufloat_HWFloat_LongInt_op_div(HWFloat z1,LongInt z2);
boolean ufloat_HWFloat_HWFloat_op_lt(HWFloat z1,HWFloat z2);
boolean ufloat_HWFloat_HWFloat_op_gt(HWFloat z1,HWFloat z2);
#include "uRust.h"
#include "uSinTable.h"
#include "uConsts.h"
HWFloat ufloat_int2hwFloat(LongInt i)
{
    HWFloat int2hwfloat_result;
    int2hwfloat_result = hwf_new(i, 1);
    return int2hwfloat_result;
};
double ufloat_hwFloat2Float(HWFloat i)
{
    double hwfloat2float_result;
    hwfloat2float_result = hwf_to_f64(i);
    return hwfloat2float_result;
};
boolean ufloat_HWFloat_HWFloat_op_eq(HWFloat z1,HWFloat z2)
{
    boolean z_result;
    z_result = hwf_op_eq(z1, z2);
    return z_result;
};
boolean ufloat_HWFloat_HWFloat_op_neq(HWFloat z1,HWFloat z2)
{
    boolean z_result;
    z_result = !hwf_op_eq(z1, z2);
    return z_result;
};
HWFloat ufloat_HWFloat_HWFloat_op_add(HWFloat z1,HWFloat z2)
{
    HWFloat z_result;
    z_result = hwf_op_plus(z1, z2);
    return z_result;
};
HWFloat ufloat_HWFloat_HWFloat_op_sub(HWFloat z1,HWFloat z2)
{
    HWFloat z_result;
    z_result = hwf_op_minus(z1, z2);
    return z_result;
};
boolean ufloat_isZero(HWFloat z)
{
    boolean iszero_result;
    iszero_result = hwf_is_zero(z);
    return iszero_result;
};
boolean ufloat_isNegative(HWFloat z)
{
    boolean isnegative_result;
    isnegative_result = hwf_is_negative(z);
    return isnegative_result;
};
boolean ufloat_HWFloat_HWFloat_op_lt(HWFloat z1,HWFloat z2)
{
    boolean b_result;
    b_result = hwf_op_lt(z1, z2);
    return b_result;
};
boolean ufloat_HWFloat_HWFloat_op_gt(HWFloat z1,HWFloat z2)
{
    boolean b_result;
    b_result = hwf_op_gt(z1, z2);
    return b_result;
};
HWFloat ufloat_HWFloat_op_sub(HWFloat z1)
{
    HWFloat z_result;
    z_result = hwf_op_neg(z1);
    return z_result;
};
HWFloat ufloat_HWFloat_HWFloat_op_mul(HWFloat z1,HWFloat z2)
{
    HWFloat z_result;
    z_result = hwf_op_mul(z1, z2);
    return z_result;
};
HWFloat ufloat_HWFloat_LongInt_op_mul(HWFloat z1,LongInt z2)
{
    HWFloat z_result;
    z_result = hwf_op_mul_int(z1, z2);
    return z_result;
};
HWFloat ufloat_HWFloat_HWFloat_op_div(HWFloat z1,HWFloat z2)
{
    HWFloat z_result;
    z_result = hwf_op_div(z1, z2);
    return z_result;
};
HWFloat ufloat_HWFloat_LongInt_op_div(HWFloat z1,LongInt z2)
{
    HWFloat z_result;
    z_result = hwf_op_div_int(z1, z2);
    return z_result;
};
string255 ufloat_cstr(HWFloat z)
{
    string255 cstr_result;
    cstr_result = hwf_to_str(z);
    return cstr_result;
};
LongInt ufloat_hwRound(HWFloat t)
{
    LongInt hwround_result;
    hwround_result = hwf_round(t);
    return hwround_result;
};
HWFloat ufloat_hwAbs(HWFloat t)
{
    HWFloat hwabs_result;
    hwabs_result = hwf_abs(t);
    return hwabs_result;
};
HWFloat ufloat_hwSqr(HWFloat t)
{
    HWFloat hwsqr_result;
    hwsqr_result = hwf_sqr(t);
    return hwsqr_result;
};
HWFloat ufloat_hwSqrt(HWFloat x)
{
    HWFloat hwsqrt_result;
    hwsqrt_result = hwf_sqrt(x);
    return hwsqrt_result;
};
HWFloat ufloat_Distance(HWFloat dx,HWFloat dy)
{
    HWFloat distance_result;
    distance_result = hwf_distance(dx, dy);
    return distance_result;
};
HWFloat ufloat_SqrDistance(HWFloat dx,HWFloat dy)
{
    HWFloat sqrdistance_result;
    sqrdistance_result = hwf_sqr_distance(dx, dy);
    return sqrdistance_result;
};
HWFloat ufloat_DistanceI(LongInt dx,LongInt dy)
{
    HWFloat distancei_result;
    distancei_result = hwf_sqrt(hwf_new(fpcrtl_sqr(dx) + fpcrtl_sqr(dy), 1));
    return distancei_result;
};
HWFloat ufloat_SignAs(HWFloat num,HWFloat signum)
{
    HWFloat signas_result;
    signas_result = hwf_sign_as(num, signum);
    return signas_result;
};
HWFloat ufloat_WithSign(HWFloat num,boolean is_negative)
{
    HWFloat withsign_result;
    withsign_result = hwf_with_sign(num, is_negative);
    return withsign_result;
};
LongInt ufloat_hwSign(HWFloat r)
{
    LongInt hwsign_result;
    hwsign_result = hwf_signum(r);
    return hwsign_result;
};
HWFloat ufloat_AngleSin(LongWord Angle)
{
    HWFloat anglesin_result;
    if(Angle < 1024)
    {
        anglesin_result = hwf_raw(false, SinTable[Angle]);
    }
    else
    {
        anglesin_result = hwf_raw(false, SinTable[2048 - Angle]);
    }
    return anglesin_result;
};
HWFloat ufloat_AngleCos(LongWord Angle)
{
    HWFloat anglecos_result;
    if(Angle < 1024)
    {
        anglecos_result = hwf_raw(false, SinTable[1024 - Angle]);
    }
    else
    {
        anglecos_result = hwf_raw(Angle > 1024, SinTable[Angle - 1024]);
    }
    return anglecos_result;
};
LongInt ufloat_vector2Angle(HWFloat x,HWFloat y)
{
    LongInt vector2angle_result;
    HWFloat d;
    HWFloat nf;
    LongWord l;
    LongWord r;
    LongWord c;
    LongWord oc;
    d = ufloat_HWFloat_HWFloat_op_div(_1, hwf_distance(x, y));
    nf = ufloat_hwAbs(ufloat_HWFloat_HWFloat_op_mul(y, d));
    l = 0;
    r = 1024;
    c = 0;
    do {
           oc = c;
           c = (l + r) >> 1;
           if(ufloat_HWFloat_HWFloat_op_lt(nf, hwf_raw(false, SinTable[c])))
           {
               r = c;
           }
           else
           {
               l = c;
           }
       } while(!(oc == c));
    if(hwf_is_negative(x))
    {
        c = 2048 - c;
    }
    if(hwf_is_negative(y))
    {
        c = -c;
    }
    vector2angle_result = c;
    return vector2angle_result;
};
