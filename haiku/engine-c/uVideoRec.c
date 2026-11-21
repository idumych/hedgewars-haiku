#include "fpcrtl.h"

#include "uVideoRec.h"
boolean flagPrerecording = false;
#include "uVariables.h"
#include "GL.h"
#include "SDLh.h"
#include "SysUtils.h"
#include "uUtils.h"
#include "uSound.h"
#include "uChat.h"
#include "uIO.h"
#include "uMisc.h"
#include "uTypes.h"
#include "uDebug.h"
static const string255 __str33 = STRINIT("StopPreRecording");
static const string255 __str32 = STRINIT(".txtout");
static const string255 __str31 = STRINIT("Error: Unexpected audio format ");
static const string255 __str30 = STRINIT(", channels = ");
static const string255 __str29 = STRINIT(", format = ");
static const string255 __str28 = STRINIT("sound: frequency = ");
static const string255 __str27 = STRINIT(".hwd");
static const string255 __str26 = STRINIT("YYYY-MM-DD_HH-mm-ss-z");
static const string255 __str25 = STRINIT("hw-");
static const string255 __str24 = STRINIT("Pre-recording prevented; /lua command was used before");
static const string255 __str23 = STRINIT("BeginPreRecording");
static const string255 __str22 = STRINIT("Error: Could not write to ");
static const string255 __str21 = STRINIT("Saving thumbnail ");
static const string255 __str20 = STRINIT("/VideoThumbnails/");
static const string255 __str19 = STRINIT("AVWrapper_WriteFrame(RGB_Buffer) has failed.");
static const string255 __str18 = STRINIT("v");
static const string255 __str17 = STRINIT("Warning: Tried to delete the cameraFile but it was already deleted");
static const string255 __str16 = STRINIT("AVWrapper_Close() has failed.");
static const string255 __str15 = STRINIT("StopVideoRecording");
static const string255 __str14 = STRINIT("Error: Could not allocate memory for video recording (RGB buffer).");
static const string255 __str13 = STRINIT("AVWrapper_Init failed");
static const string255 __str12 = STRINIT(".sw");
static const string255 __str11 = STRINIT("no");
static const string255 __str10 = STRINIT("]prefix");
static const string255 __str9 = STRINIT("prefix[");
static const string255 __str8 = STRINIT("Theme: ");
static const string255 __str7 = STRINIT("Map: ");
static const string255 __str6 = STRINIT("Record: ");
static const string255 __str5 = STRINIT("Player: ");
static const string255 __str4 = STRINIT("");
static const string255 __str3 = STRINIT("Error: Could not read from ");
static const string255 __str2 = STRINIT(".txtin");
static const string255 __str1 = STRINIT("/VideoTemp/");
static const string255 __str0 = STRINIT("BeginVideoRecording");
typedef void(*uvideorec_TAddFileLogRaw)(PChar s);
static const string255 AvwrapperLibName = STRINIT("libavwrapper");
LongInt AVWrapper_Init(uvideorec_TAddFileLogRaw uvideorec_AddLog,PChar filename,PChar desc,PChar soundFile,PChar format,PChar vcodec,PChar acodec,LongInt width,LongInt height,LongInt framerateNum,LongInt framerateDen,LongInt vquality);
LongInt AVWrapper_Close();
LongInt AVWrapper_WriteFrame(PByte rgb);
typedef struct __TFrame {
            LongWord realTicks;
            LongWord gameTicks;
            LongInt CamX;
            LongInt CamY;
            single zoom;
        } TFrame;
static PByte RGB_Buffer;
static File cameraFile;
static string255 cameraFileName;
static File audioFile;
static LongWord numPixels;
static LongWord startTime;
static LongWord numFrames;
static LongWord curTime;
static LongWord progress;
static LongWord maxProgress;
static string255 soundFilePath;
static boolean thumbnailSaved;
static boolean recordAudio;
boolean uvideorec_BeginVideoRecording()
{
    boolean beginvideorecording_result;
    string255 filename;
    string255 desc;
    astring filenameA;
    astring descA;
    astring soundFilePathA;
    astring cAVFormatA;
    astring cVideoCodecA;
    astring cAudioCodecA;
    uutils_AddFileLog(__str0);
    cameraFileName = _strconcat(_strconcat(_strconcat(fpcrtl_astr2str(UserPathPrefix), __str1), ((shortstring)RecPrefix)), __str2);
    fpcrtl_assign(cameraFile, cameraFileName);
    fpcrtl_reset(cameraFile, sizeof(TFrame));
    maxProgress = fpcrtl_FileSize(cameraFile);
    if(IOResult != 0)
    {
        uutils_AddFileLog(_strconcat(__str3, cameraFileName));
        return false;
    }
    desc = __str4;
    if(_strncompare(UserNick, __str4))
    {
        desc = _strappend(_strconcat(_strconcat(desc, __str5), UserNick), 0xa);
    }
    if(_strncompare(recordFileName, __str4))
    {
        desc = _strappend(_strconcat(_strconcat(desc, __str6), recordFileName), 0xa);
    }
    if(_strncompare(cMapName, __str4))
    {
        desc = _strappend(_strconcat(_strconcat(desc, __str7), cMapName), 0xa);
    }
    if(_strncompare(Theme, __str4))
    {
        desc = _strappend(_strconcat(_strconcat(desc, __str8), Theme), 0xa);
    }
    desc = _strconcat(_strconcat(_strconcat(desc, __str9), RecPrefix), __str10);
    filename = _strconcat(_strconcat(fpcrtl_astr2str(UserPathPrefix), __str1), ((shortstring)RecPrefix));
    recordAudio = _strncompare(cAudioCodec, __str11);
    if(recordAudio)
    {
        soundFilePath = _strconcat(_strconcat(_strconcat(fpcrtl_astr2str(UserPathPrefix), __str1), ((shortstring)RecPrefix)), __str12);
    }
    else
    {
        soundFilePath = __str4;
    }
    filenameA = fpcrtl_str2astr(filename);
    descA = fpcrtl_str2astr(desc);
    soundFilePathA = fpcrtl_str2astr(soundFilePath);
    cAVFormatA = fpcrtl_str2astr(cAVFormat);
    cVideoCodecA = fpcrtl_str2astr(cVideoCodec);
    cAudioCodecA = fpcrtl_str2astr(cAudioCodec);
    if(udebug_checkFails(AVWrapper_Init(&(uutils_AddFileLogRaw), fpcrtl__pcharA(filenameA), fpcrtl__pcharA(descA), fpcrtl__pcharA(soundFilePathA), fpcrtl__pcharA(cAVFormatA), fpcrtl__pcharA(cVideoCodecA), fpcrtl__pcharA(cAudioCodecA), cScreenWidth, cScreenHeight, cVideoFramerateNum, cVideoFramerateDen, cVideoQuality) >= 0, __str13, true))
    {
        return false;
    }
    numPixels = cScreenWidth * cScreenHeight;
    RGB_Buffer = fpcrtl_GetMem(4 * numPixels);
    if(RGB_Buffer == NULL)
    {
        uutils_AddFileLog(__str14);
        return false;
    }
    curTime = 0;
    numFrames = 0;
    progress = 0;
    beginvideorecording_result = true;
    return beginvideorecording_result;
};
void uvideorec_StopVideoRecording()
{
    uutils_AddFileLog(__str15);
    fpcrtl_FreeMem(RGB_Buffer, 4 * numPixels);
    fpcrtl_close(cameraFile);
    if(AVWrapper_Close() < 0)
    {
        udebug_OutError(__str16, true);
    }
    if(fpcrtl_FileExists(cameraFileName))
    {
        fpcrtl_DeleteFile(cameraFileName);
    }
    else
    {
        uutils_AddFileLog(__str17);
    }
    if(recordAudio && fpcrtl_FileExists(soundFilePath))
    {
        fpcrtl_DeleteFile(soundFilePath);
    }
    uio_SendIPC(__str18);
};
void uvideorec_EncodeFrame()
{
    string255 s;
    glReadPixels(0, 0, cScreenWidth, cScreenHeight, GL_RGBA, GL_UNSIGNED_BYTE, RGB_Buffer);
    if(AVWrapper_WriteFrame(RGB_Buffer) < 0)
    {
        udebug_OutError(__str19, true);
    }
    s.s[0] = 0x3;
    s.s[1] = 0x70;
    sdlh_SDLNet_Write16((progress * 10000) / maxProgress, &(s.s[2]));
    uio_SendIPC(s);
    ++numFrames;
};
boolean uvideorec_LoadNextCameraPosition(LongInt (*newRealTicks),LongInt (*newGameTicks))
{
    boolean loadnextcameraposition_result;
    TFrame frame = {
                       .realTicks = 0,
                       .gameTicks = 0,
                       .CamX = 0,
                       .CamY = 0,
                       .zoom = 0
                   };
    LongInt res;
    while((((Int64)curTime) * cVideoFramerateNum) <= ((((Int64)numFrames) * cVideoFramerateDen) * 1000))
    {
        res = 0;
        if(fpcrtl_eof(cameraFile))
        {
            return false;
        }
        fpcrtl_BlockRead(cameraFile, frame, 1, res);
        curTime = frame.realTicks;
        WorldDx = frame.CamX;
        WorldDy = frame.CamY + (cScreenHeight / 2);
        zoom = frame.zoom * cScreenWidth;
        ZoomValue = zoom;
        ++progress;
        (*newRealTicks) = frame.realTicks;
        (*newGameTicks) = frame.gameTicks;
    }
    loadnextcameraposition_result = true;
    return loadnextcameraposition_result;
};
void uvideorec_RecordPostMix(pointer udata,PByte stream,LongInt len)
{
    LongInt result;
    result = 0;
    UNUSED (udata);
    fpcrtl_BlockWrite(audioFile, (*stream), len, result);
};
void uvideorec_SaveThumbnail()
{
    string255 thumbpath;
    LongInt k;
    thumbpath = _strconcat(__str20, RecPrefix);
    uutils_AddFileLog(_strconcat(__str21, thumbpath));
    k = uutils_Max(uutils_Max(cScreenWidth, cScreenHeight) / 400, 1);
    umisc_MakeScreenshot(thumbpath, k, 0);
    thumbnailSaved = true;
};
void uvideorec_CopyFile(string255 src,string255 dest)
{
    File inF;
    File outF;
    typedef Byte buffer_tt[(1023 + 1)];
    buffer_tt buffer;
    LongInt result;
    LongInt result2;
    Integer i;
    result = 0;
    result2 = 0;
    {i = 0;
     Integer i__end__ = 1023;
     if (i <= i__end__) do {
                               buffer[i] = 0;
                           } while(i++ != i__end__);}
    fpcrtl_assign(inF, src);
    fpcrtl_reset(inF, 1);
    if(IOResult != 0)
    {
        uutils_AddFileLog(_strconcat(__str3, src));
        return;
    }
    fpcrtl_assign(outF, dest);
    fpcrtl_rewrite(outF, 1);
    if(IOResult != 0)
    {
        uutils_AddFileLog(_strconcat(__str22, dest));
        return;
    }
    do {
           fpcrtl_BlockRead(inF, buffer, 1024, result);
           fpcrtl_BlockWrite(outF, buffer, result, result2);
       } while(!(result < 1024));
};
void uvideorec_BeginPreRecording()
{
    Word format;
    string255 filename;
    LongInt frequency;
    LongInt channels;
    LongInt result;
    result = 0;
    uutils_AddFileLog(__str23);
    if(LuaCmdUsed)
    {
        usound_PlaySound_1(sndDenied);
        uchat_AddChatString(_strprepend(0x0, fpcrtl_astr2str(trmsg[sidVideoRecLuaFail])));
        uutils_AddFileLog(__str24);
        return;
    }
    thumbnailSaved = false;
    RecPrefix = _strconcat(__str25, fpcrtl_FormatDateTime(__str26, ((TDateTime)fpcrtl_Now())));
    if(_strncompare(recordFileName, __str4))
    {
        if(GameType != gmtDemo)
        {
            return;
        }
        uvideorec_CopyFile(recordFileName, _strconcat(_strconcat(_strconcat(fpcrtl_astr2str(UserPathPrefix), __str1), ((shortstring)RecPrefix)), __str27));
    }
    if(cIsSoundEnabled)
    {
        Mix_QuerySpec(&(frequency), &(format), &(channels));
        uutils_AddFileLog(_strconcat(_strconcat(_strconcat(_strconcat(_strconcat(__str28, uutils_IntToStr(frequency)), __str29), uutils_IntToStr(format)), __str30), uutils_IntToStr(channels)));
        if(format != 0x8010)
        {
            uutils_AddFileLog(_strconcat(__str31, uutils_IntToStr(format)));
            return;
        }
        filename = _strconcat(_strconcat(_strconcat(fpcrtl_astr2str(UserPathPrefix), __str1), ((shortstring)RecPrefix)), __str12);
        fpcrtl_assign(audioFile, filename);
        fpcrtl_rewrite(audioFile, 1);
        if(IOResult != 0)
        {
            uutils_AddFileLog(_strconcat(__str22, filename));
            return;
        }
    }
    filename = _strconcat(_strconcat(_strconcat(fpcrtl_astr2str(UserPathPrefix), __str1), ((shortstring)RecPrefix)), __str32);
    fpcrtl_assign(cameraFile, filename);
    fpcrtl_rewrite(cameraFile, sizeof(TFrame));
    if(IOResult != 0)
    {
        uutils_AddFileLog(_strconcat(__str22, filename));
        return;
    }
    if(cIsSoundEnabled)
    {
        fpcrtl_BlockWrite(audioFile, frequency, 4, result);
        fpcrtl_BlockWrite(audioFile, channels, 4, result);
        Mix_SetPostMix(&(uvideorec_RecordPostMix), NULL);
    }
    startTime = SDL_GetTicks();
    flagPrerecording = true;
};
void uvideorec_StopPreRecording()
{
    uutils_AddFileLog(__str33);
    flagPrerecording = false;
    if(cIsSoundEnabled)
    {
        SDL_LockAudio();
        fpcrtl_close(audioFile);
    }
    fpcrtl_close(cameraFile);
    if(cIsSoundEnabled)
    {
        Mix_SetPostMix(NULL, NULL);
        SDL_UnlockAudio();
    }
    if(!thumbnailSaved)
    {
        uvideorec_SaveThumbnail();
    }
};
void uvideorec_SaveCameraPosition()
{
    TFrame frame;
    LongInt result;
    result = 0;
    if(!thumbnailSaved && (ScreenFade == sfNone))
    {
        uvideorec_SaveThumbnail();
    }
    frame.realTicks = ((int64_t) (SDL_GetTicks())) - ((int64_t) (startTime));
    frame.gameTicks = GameTicks;
    frame.CamX = WorldDx;
    frame.CamY = WorldDy - (cScreenHeight / 2);
    frame.zoom = zoom /(float) cScreenWidth;
    fpcrtl_BlockWrite(cameraFile, frame, 1, result);
};
void uvideorec_initModule()
{
    cScreenWidth = uutils_Max(cWindowedWidth, 640);
    cScreenHeight = uutils_Max(cWindowedHeight, 480);
};
void uvideorec_freeModule()
{
    if(flagPrerecording)
    {
        uvideorec_StopPreRecording();
    }
};
