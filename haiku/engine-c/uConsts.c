#include "fpcrtl.h"

#include "uConsts.h"
;
;
;
;
cFontsPaths_tt cFontsPaths = {NULL,
                              NULL};
;
;
;
;
;
;
;
;
;
;
;
;
;
LongInt cameraKeyboardSpeed = 10;
LongInt cameraKeyboardSpeedSlow = 3;
TSDL_Color cWhiteColorChannels = {
                                     .r = 0xff,
                                     .g = 0xff,
                                     .b = 0xff,
                                     .a = 0xff
                                 };
TSDL_Color cNearBlackColorChannels = {
                                         .r = 0x00,
                                         .g = 0x00,
                                         .b = 0x10,
                                         .a = 0xff
                                     };
Byte cInvertTextColorAt = 64;
LongWord cWhiteColor = 0xffffffff;
LongWord cNearBlackColor = 0xff000010;
LongWord capcolDefault = 0xffffffff;
LongWord capcolSetting = 0xffcccccc;
LongWord capcolDefaultLua = 0xffffffff;
LongWord capcolSettingLua = 0xccccccff;
LongWord cCentralMessageColor = 0xffffff00;
LongInt cVisibleWater = 128;
LongInt cTeamHealthWidth = 128;
LongInt cGearContourThreshold = 179;
LongInt JETPACK_FUEL_INFINITE = -2147483648;
LongInt BIRDY_ENERGY_INFINITE = -2147483648;
probabilityLevels_tt probabilityLevels = {0,
                                          20,
                                          30,
                                          60,
                                          100,
                                          200,
                                          400,
                                          600,
                                          800};
bouncinessLevels_tt bouncinessLevels = {350,
                                        700,
                                        defaultBounciness,
                                        2000,
                                        4000};
TPoint cTargetPointRef = {
                             .x = NoPointX,
                             .y = 0
                         };
LongInt cTeamHealthHeight;
LongInt cFontBorder;
LongInt cFontPadding;
HWFloat cAirPlaneSpeed;
HWFloat cBombsSpeed;
HWFloat _1div1024;
HWFloat _1div10000;
HWFloat _1div50000;
HWFloat _1div100000;
HWFloat _1div3;
HWFloat hwPi;
HWFloat _0_000004;
HWFloat _0_000064;
HWFloat _0_0002;
HWFloat _0_0005;
HWFloat _0_001;
HWFloat _0_003;
HWFloat _0_0032;
HWFloat _0_004;
HWFloat _0_005;
HWFloat _0_008;
HWFloat _0_01;
HWFloat _0_0128;
HWFloat _0_02;
HWFloat _0_03;
HWFloat _0_05;
HWFloat _0_07;
HWFloat _0_08;
HWFloat _0_1;
HWFloat _0_15;
HWFloat _0_17;
HWFloat _0_2;
HWFloat _0_25;
HWFloat _0_3;
HWFloat _0_35;
HWFloat _0_375;
HWFloat _0_39;
HWFloat _0_4;
HWFloat _0_45;
HWFloat _0_5;
HWFloat _0_55;
HWFloat _0_6;
HWFloat _0_64;
HWFloat _0_7;
HWFloat _0_8;
HWFloat _0_84;
HWFloat _0_87;
HWFloat _0_9;
HWFloat _0_93;
HWFloat _0_96;
HWFloat _0_995;
HWFloat _0_999;
HWFloat _0;
HWFloat _1;
HWFloat _1_2;
HWFloat _1_5;
HWFloat _1_6;
HWFloat _1_9;
HWFloat _2;
HWFloat _2_4;
HWFloat _3;
HWFloat _3_2;
HWFloat _PI;
HWFloat _4;
HWFloat _4_5;
HWFloat _5;
HWFloat _6;
HWFloat _6_4;
HWFloat _7;
HWFloat _10;
HWFloat _12;
HWFloat _16;
HWFloat _19;
HWFloat _20;
HWFloat _24;
HWFloat _25;
HWFloat _30;
HWFloat _40;
HWFloat _41;
HWFloat _49;
HWFloat _50;
HWFloat _70;
HWFloat _90;
HWFloat _128;
HWFloat _180;
HWFloat _250;
HWFloat _256;
HWFloat _300;
HWFloat _360;
HWFloat _450;
HWFloat _500;
HWFloat _900;
HWFloat _1000;
HWFloat _1024;
HWFloat _2048;
HWFloat _4096;
HWFloat _10000;
HWFloat cLittle;
HWFloat cHHKick;
#include "uRust.h"
void uconsts_initModule()
{
    cTeamHealthHeight = fpcrtl_round(19 * HDPIScaleFactor);
    cFontBorder = fpcrtl_round(2 * HDPIScaleFactor);
    cFontPadding = fpcrtl_round(2 * HDPIScaleFactor);
    cAirPlaneSpeed = hwf_raw(false, 3006477107);
    cBombsSpeed = hwf_raw(false, 429496729);
    _1div1024 = hwf_new(1, 1024);
    _1div10000 = hwf_new(1, 10000);
    _1div50000 = hwf_new(1, 50000);
    _1div100000 = hwf_new(1, 100000);
    _1div3 = hwf_new(1, 3);
    hwPi = hwf_raw(false, 13493037704);
    _0_000004 = hwf_new(1, 250000);
    _0_000064 = hwf_new(1, 15625);
    _0_0002 = hwf_new(1, 5000);
    _0_0005 = hwf_new(1, 2000);
    _0_001 = hwf_new(1, 1000);
    _0_003 = hwf_new(3, 1000);
    _0_0032 = hwf_new(32, 10000);
    _0_004 = hwf_new(4, 1000);
    _0_005 = hwf_new(5, 1000);
    _0_008 = hwf_new(8, 1000);
    _0_01 = hwf_new(1, 100);
    _0_0128 = hwf_new(128, 10000);
    _0_02 = hwf_new(2, 100);
    _0_03 = hwf_new(3, 100);
    _0_05 = hwf_new(5, 100);
    _0_07 = hwf_new(7, 100);
    _0_08 = hwf_new(8, 100);
    _0_1 = hwf_new(1, 10);
    _0_15 = hwf_new(15, 100);
    _0_17 = hwf_new(17, 100);
    _0_2 = hwf_new(2, 10);
    _0_25 = hwf_new(1, 4);
    _0_3 = hwf_new(3, 10);
    _0_35 = hwf_new(35, 100);
    _0_375 = hwf_new(3, 8);
    _0_39 = hwf_new(39, 100);
    _0_4 = hwf_new(4, 10);
    _0_45 = hwf_new(45, 100);
    _0_5 = hwf_new(1, 2);
    _0_55 = hwf_new(55, 100);
    _0_6 = hwf_new(6, 10);
    _0_64 = hwf_new(64, 100);
    _0_7 = hwf_new(7, 10);
    _0_8 = hwf_new(8, 10);
    _0_84 = hwf_new(84, 100);
    _0_87 = hwf_new(87, 100);
    _0_9 = hwf_new(9, 10);
    _0_93 = hwf_new(93, 100);
    _0_96 = hwf_new(96, 100);
    _0_995 = hwf_new(995, 1000);
    _0_999 = hwf_new(999, 1000);
    _0 = hwf_raw(false, 0);
    _1 = hwf_new(1, 1);
    _1_2 = hwf_new(12, 10);
    _1_5 = hwf_new(15, 10);
    _1_6 = hwf_new(16, 10);
    _1_9 = hwf_new(19, 10);
    _2 = hwf_new(2, 1);
    _2_4 = hwf_new(24, 10);
    _3 = hwf_new(3, 1);
    _3_2 = hwf_new(32, 10);
    _PI = hwf_raw(false, 13493037704);
    _4 = hwf_new(4, 1);
    _4_5 = hwf_new(45, 10);
    _5 = hwf_new(5, 1);
    _6 = hwf_new(6, 1);
    _6_4 = hwf_new(64, 10);
    _7 = hwf_new(7, 1);
    _10 = ufloat_int2hwFloat(10);
    _12 = ufloat_int2hwFloat(12);
    _16 = ufloat_int2hwFloat(16);
    _19 = ufloat_int2hwFloat(19);
    _20 = ufloat_int2hwFloat(20);
    _24 = ufloat_int2hwFloat(24);
    _25 = ufloat_int2hwFloat(25);
    _30 = ufloat_int2hwFloat(30);
    _40 = ufloat_int2hwFloat(40);
    _41 = ufloat_int2hwFloat(41);
    _49 = ufloat_int2hwFloat(49);
    _50 = ufloat_int2hwFloat(50);
    _70 = ufloat_int2hwFloat(70);
    _90 = ufloat_int2hwFloat(90);
    _128 = ufloat_int2hwFloat(128);
    _180 = ufloat_int2hwFloat(180);
    _250 = ufloat_int2hwFloat(250);
    _256 = ufloat_int2hwFloat(256);
    _300 = ufloat_int2hwFloat(300);
    _360 = ufloat_int2hwFloat(360);
    _450 = ufloat_int2hwFloat(450);
    _500 = ufloat_int2hwFloat(500);
    _900 = ufloat_int2hwFloat(900);
    _1000 = ufloat_int2hwFloat(1000);
    _1024 = ufloat_int2hwFloat(1024);
    _2048 = ufloat_int2hwFloat(2048);
    _4096 = ufloat_int2hwFloat(4096);
    _10000 = ufloat_int2hwFloat(10000);
    cLittle = hwf_min_positive();
    cHHKick = hwf_new(1, 100);
};
