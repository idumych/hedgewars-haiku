#pragma once

#include "pas2c.h"

#include "uFloat.h"
void urandom_SetRandomSeed(string255 Seed,boolean dropAdditionalPart);
HWFloat urandom_GetRandomf();
LongWord urandom_GetRandom(LongWord m);
void urandom_AddRandomness(LongWord r);
HWFloat urandom_rndSign(HWFloat num);

