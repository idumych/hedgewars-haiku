#pragma once

#include "pas2c.h"

extern boolean flagPrerecording;
boolean uvideorec_BeginVideoRecording();
boolean uvideorec_LoadNextCameraPosition(LongInt (*newRealTicks),LongInt (*newGameTicks));
void uvideorec_EncodeFrame();
void uvideorec_StopVideoRecording();
void uvideorec_BeginPreRecording();
void uvideorec_StopPreRecording();
void uvideorec_SaveCameraPosition();
void uvideorec_initModule();
void uvideorec_freeModule();

