#include "fpcrtl.h"

#include "uMisc.h"
#include "uVariables.h"
#include "uUtils.h"
#include "uLandUtils.h"
static const string255 __str4 = STRINIT("snapshot");
static const string255 __str3 = STRINIT("_land");
static const string255 __str2 = STRINIT("_landpixels");
static const string255 __str1 = STRINIT("Error: Could not allocate memory for screenshot.");
static const string255 __str0 = STRINIT(".bmp");
typedef struct __TScreenshot * PScreenshot;
typedef struct __TScreenshot {
            PByte buffer;
            string255 filename;
            LongInt width;
            LongInt height;
            QWord size;
        } TScreenshot;
static PSDL_PixelFormat conversionFormat;
LongInt umisc_SaveScreenshot(pointer screenshot)
{
    LongInt savescreenshot_result;
    File f;
    typedef Byte head_tt[(53 + 1)];
    head_tt head = {0x42,
                    0x4d,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    54,
                    0,
                    0,
                    0,
                    40,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    1,
                    0,
                    32,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    96,
                    0,
                    0,
                    0,
                    96,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0};
    PScreenshot image;
    QWord size;
    LongInt writeResult;
    image = ((PScreenshot)screenshot);
    size = (image->width * image->height) * 4;
    head[0x02] = (size + 54) & 0xff;
    head[0x03] = ((size + 54) >> 8) & 0xff;
    head[0x04] = ((size + 54) >> 16) & 0xff;
    head[0x05] = ((size + 54) >> 24) & 0xff;
    head[0x12] = image->width & 0xff;
    head[0x13] = (image->width >> 8) & 0xff;
    head[0x14] = (image->width >> 16) & 0xff;
    head[0x15] = (image->width >> 24) & 0xff;
    head[0x16] = image->height & 0xff;
    head[0x17] = (image->height >> 8) & 0xff;
    head[0x18] = (image->height >> 16) & 0xff;
    head[0x19] = (image->height >> 24) & 0xff;
    head[0x22] = size & 0xff;
    head[0x23] = (size >> 8) & 0xff;
    head[0x24] = (size >> 16) & 0xff;
    head[0x25] = (size >> 24) & 0xff;
    fpcrtl_assign(f, image->filename);
    fpcrtl_rewrite(f, 1);
    if(IOResult == 0)
    {
        writeResult = 0;
        fpcrtl_BlockWrite(f, head, sizeof(head), writeResult);
        fpcrtl_BlockWrite(f, (*image->buffer), size, writeResult);
        fpcrtl_close(f);
    }
    else
    {
    }
    fpcrtl_FreeMem(image->buffer, image->size);
    fpcrtl_dispose(image);
    savescreenshot_result = 0;
    return savescreenshot_result;
};
void umisc_ReduceImage(PByteArray img,LongInt width,LongInt height,LongInt k)
{
    LongInt i;
    LongInt j;
    LongInt i0;
    LongInt j0;
    LongInt w;
    LongInt h;
    LongInt r;
    LongInt g;
    LongInt b;
    w = width / k;
    h = height / k;
    if(k != 1)
    {
        {i = 0;
         LongInt i__end__ = h - 1;
         if (i <= i__end__) do {
                                   {j = 0;
                                    LongInt j__end__ = w - 1;
                                    if (j <= j__end__) do {
                                                              r = 0;
                                                              g = 0;
                                                              b = 0;
                                                              {i0 = 0;
                                                               LongInt i0__end__ = k - 1;
                                                               if (i0 <= i0__end__) do {
                                                                                           {j0 = 0;
                                                                                            LongInt j0__end__ = k - 1;
                                                                                            if (j0 <= j0__end__) do {
                                                                                                                        r += (*img)[(4 * (((width * ((i * k) + i0)) + (j * k)) + j0)) + 0];
                                                                                                                        g += (*img)[(4 * (((width * ((i * k) + i0)) + (j * k)) + j0)) + 1];
                                                                                                                        b += (*img)[(4 * (((width * ((i * k) + i0)) + (j * k)) + j0)) + 2];
                                                                                                                    } while(j0++ != j0__end__);}
                                                                                       } while(i0++ != i0__end__);}
                                                              (*img)[(4 * ((w * i) + j)) + 0] = r / (k * k);
                                                              (*img)[(4 * ((w * i) + j)) + 1] = g / (k * k);
                                                              (*img)[(4 * ((w * i) + j)) + 2] = b / (k * k);
                                                              (*img)[(4 * ((w * i) + j)) + 3] = 255;
                                                          } while(j++ != j__end__);}
                               } while(i++ != i__end__);}
    }
};
boolean umisc_MakeScreenshot(string255 filename,LongInt k,LongWord dump)
{
    boolean makescreenshot_result;
    pointer p;
    QWord size;
    PScreenshot image;
    GLenum format;
    string255 ext;
    LongWord x;
    LongWord y;
    format = GL_BGRA;
    ext = __str0;
    if(dump > 0)
    {
        size = (LAND_WIDTH * LAND_HEIGHT) * 4;
    }
    else
    {
        size = (uutils_toPowerOf2(cScreenWidth) * uutils_toPowerOf2(cScreenHeight)) * 4;
    }
    p = fpcrtl_GetMem(size);
    if(p == NULL)
    {
        uutils_AddFileLog(__str1);
        makescreenshot_result = false;
        return makescreenshot_result;
    }
    if(dump > 0)
    {
        {y = 0;
         LongWord y__end__ = LAND_HEIGHT - 1;
         if (y <= y__end__) do {
                                   {x = 0;
                                    LongWord x__end__ = LAND_WIDTH - 1;
                                    if (x <= x__end__) do {
                                                              if(dump == 2)
                                                              {
                                                                  (*((PLongWordArray)p))[(y * LAND_WIDTH) + x] = ulandutils_LandPixelGet((LAND_HEIGHT - 1) - y, x);
                                                              }
                                                              else
                                                              {
                                                                  if((ulandutils_LandGet((LAND_HEIGHT - 1) - y, x) & lfIndestructible) == lfIndestructible)
                                                                  {
                                                                      (*((PLongWordArray)p))[(y * LAND_WIDTH) + x] = AMask | RMask;
                                                                  }
                                                                  else
                                                                  {
                                                                      if((ulandutils_LandGet((LAND_HEIGHT - 1) - y, x) & lfIce) == lfIce)
                                                                      {
                                                                          (*((PLongWordArray)p))[(y * LAND_WIDTH) + x] = AMask | BMask;
                                                                      }
                                                                      else
                                                                      {
                                                                          if((ulandutils_LandGet((LAND_HEIGHT - 1) - y, x) & lfBouncy) == lfBouncy)
                                                                          {
                                                                              (*((PLongWordArray)p))[(y * LAND_WIDTH) + x] = AMask | GMask;
                                                                          }
                                                                          else
                                                                          {
                                                                              if((ulandutils_LandGet((LAND_HEIGHT - 1) - y, x) & lfObject) == lfObject)
                                                                              {
                                                                                  (*((PLongWordArray)p))[(y * LAND_WIDTH) + x] = 0xffffffff;
                                                                              }
                                                                              else
                                                                              {
                                                                                  if((ulandutils_LandGet((LAND_HEIGHT - 1) - y, x) & lfBasic) == lfBasic)
                                                                                  {
                                                                                      (*((PLongWordArray)p))[(y * LAND_WIDTH) + x] = AMask;
                                                                                  }
                                                                                  else
                                                                                  {
                                                                                      (*((PLongWordArray)p))[(y * LAND_WIDTH) + x] = 0;
                                                                                  }
                                                                              }
                                                                          }
                                                                      }
                                                                  }
                                                              }
                                                          } while(x++ != x__end__);}
                               } while(y++ != y__end__);}
    }
    else
    {
        glReadPixels(0, 0, cScreenWidth, cScreenHeight, format, GL_UNSIGNED_BYTE, p);
        umisc_ReduceImage(p, cScreenWidth, cScreenHeight, k);
    }
    fpcrtl_new(image);
    if(dump == 2)
    {
        image->filename = _strconcat(_strconcat(_strconcat(fpcrtl_astr2str(UserPathPrefix), filename), __str2), ext);
    }
    else
    {
        if(dump == 1)
        {
            image->filename = _strconcat(_strconcat(_strconcat(fpcrtl_astr2str(UserPathPrefix), filename), __str3), ext);
        }
        else
        {
            image->filename = _strconcat(_strconcat(fpcrtl_astr2str(UserPathPrefix), filename), ext);
        }
    }
    if(dump != 0)
    {
        image->width = LAND_WIDTH;
        image->height = LAND_HEIGHT;
    }
    else
    {
        image->width = cScreenWidth / k;
        image->height = cScreenHeight / k;
    }
    image->size = size;
    image->buffer = p;
    SDL_CreateThread(&(umisc_SaveScreenshot), fpcrtl__pchar(__str4), image);
    makescreenshot_result = true;
    return makescreenshot_result;
};
PSDL_Surface umisc_doSurfaceConversion(PSDL_Surface tmpsurf)
{
    PSDL_Surface dosurfaceconversion_result;
    PSDL_Surface convertedSurf;
    dosurfaceconversion_result = tmpsurf;
    if((((*tmpsurf->format).BitsPerPixel == 32) && ((*tmpsurf->format).Rshift > (*tmpsurf->format).Bshift)) || ((*tmpsurf->format).BitsPerPixel == 24))
    {
        convertedSurf = SDL_ConvertSurface(tmpsurf, conversionFormat, SDL_SWSURFACE);
        SDL_FreeSurface(tmpsurf);
        dosurfaceconversion_result = convertedSurf;
    }
    return dosurfaceconversion_result;
};
TSDL_Rect umisc_SDL_RectMake(LongInt x,LongInt y,LongInt width,LongInt height)
{
    TSDL_Rect sdl_rectmake_result;
    sdl_rectmake_result.x = x;
    sdl_rectmake_result.y = y;
    sdl_rectmake_result.w = width;
    sdl_rectmake_result.h = height;
    return sdl_rectmake_result;
};
string255 umisc_GetTeamStatString(PTeam p)
{
    string255 getteamstatstring_result;
    string255 s;
    s = _strappend(_strconcat(_strappend(p->TeamName, 0x3a), uutils_IntToStr(p->TeamHealth)), 0x3a);
    getteamstatstring_result = s;
    return getteamstatstring_result;
};
void umisc_initModule()
{
    enum{SDL_PIXELFORMAT_ABGR8888 = ((((((1 << 28) | (6 << 24)) | (7 << 20)) | (6 << 16)) | (32 << 8)) | 4)};
    conversionFormat = SDL_AllocFormat(SDL_PIXELFORMAT_ABGR8888);
};
void umisc_freeModule()
{
    SDL_FreeFormat(conversionFormat);
};
