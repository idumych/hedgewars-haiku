#pragma once

#include "pas2c.h"

typedef struct __HWFloat {
            LongWord data[(3 + 1)];
        } HWFloat;
HWFloat ufloat_int2hwFloat(LongInt i);
double ufloat_hwFloat2Float(HWFloat i);
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
string255 ufloat_cstr(HWFloat z);
LongInt ufloat_hwRound(HWFloat t);
HWFloat ufloat_hwAbs(HWFloat t);
HWFloat ufloat_hwSqr(HWFloat t);
HWFloat ufloat_hwSqrt(HWFloat x);
HWFloat ufloat_Distance(HWFloat dx,HWFloat dy);
HWFloat ufloat_SqrDistance(HWFloat dx,HWFloat dy);
HWFloat ufloat_DistanceI(LongInt dx,LongInt dy);
HWFloat ufloat_AngleSin(LongWord Angle);
HWFloat ufloat_AngleCos(LongWord Angle);
LongInt ufloat_vector2Angle(HWFloat x,HWFloat y);
HWFloat ufloat_SignAs(HWFloat num,HWFloat signum);
HWFloat ufloat_WithSign(HWFloat num,boolean is_negative);
LongInt ufloat_hwSign(HWFloat r);
boolean ufloat_isZero(HWFloat z);
boolean ufloat_isNegative(HWFloat z);

