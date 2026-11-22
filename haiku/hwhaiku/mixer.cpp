// SDL_mixer on the Media Kit: sounds and music are decoded with BMediaFile
// (whatever codecs the system has: Ogg Vorbis, Opus, WAV...) into 16-bit
// stereo at the output rate, and mixed in a BSoundPlayer callback.
#include "hwhaiku.h"

#include <DataIO.h>
#include <MediaFile.h>
#include <MediaTrack.h>
#include <SoundPlayer.h>
#ifdef __VOS__
#include <Entry.h>
#include <unistd.h>
#endif

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <vector>

struct Mix_Chunk {	// layout as SDL_mixer's
	int allocated;
	Uint8* abuf;
	Uint32 alen;
	Uint8 volume;
};

struct Mix_Music {
	int16_t* samples;	// interleaved stereo
	size_t frames;
};

struct Channel {
	Mix_Chunk* chunk = NULL;
	size_t pos = 0;			// in frames
	int loops = 0;			// -1 forever
	int volume = 128;		// 0..128
	float fade = 1, fadeStep = 0;	// fade gain and its change per frame
	bool haltAfterFade = false;
	int64_t framesLeft = -1;	// -1: no time limit
};

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static BSoundPlayer* sPlayer;
static int sRate = 44100;
static std::vector<Channel> sChannels(8);
static Mix_Music* sMusic;
static size_t sMusicPos;
static int sMusicLoops;
static bool sMusicPaused;
static int sMusicVolume = 128;
static float sMusicFade = 1, sMusicFadeStep;
static pthread_mutex_t sPostLock = PTHREAD_MUTEX_INITIALIZER;
static void (*volatile sPostMix)(void*, Uint8*, int);
static void* volatile sPostData;

static inline int16_t clamp16(int v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : v; }

static void mix(void*, void* buffer, size_t size, const media_raw_audio_format& format)
{
	// 16-bit as asked for; VitruvianOS's media2 may hand over a float buffer instead
	bool isFloat = format.format == media_raw_audio_format::B_AUDIO_FLOAT;
	size_t frames = size / (isFloat ? 8 : 4);
	static std::vector<int16_t> pcm;
	pcm.resize(frames * 2);
	int16_t* out = isFloat ? pcm.data() : (int16_t*)buffer;
	static std::vector<int> acc;	// kept between calls: no allocation on the audio thread
	acc.assign(frames * 2, 0);
	pthread_mutex_lock(&sLock);
	for (Channel& c : sChannels) {
		if (!c.chunk)
			continue;
		const int16_t* src = (const int16_t*)c.chunk->abuf;
		size_t total = c.chunk->alen / 4;
		for (size_t f = 0; f < frames && c.chunk; f++) {
			if (c.pos >= total) {
				if (c.loops == 0) { c.chunk = NULL; break; }
				if (c.loops > 0) c.loops--;
				c.pos = 0;
				if (total == 0) { c.chunk = NULL; break; }
			}
			float gain = c.volume / 128.f * c.chunk->volume / 128.f * c.fade;
			acc[f * 2] += (int)(src[c.pos * 2] * gain);
			acc[f * 2 + 1] += (int)(src[c.pos * 2 + 1] * gain);
			c.pos++;
			if (c.fadeStep != 0) {
				c.fade += c.fadeStep;
				if (c.fade >= 1) { c.fade = 1; c.fadeStep = 0; }
				if (c.fade <= 0) { c.fade = 0; c.fadeStep = 0; if (c.haltAfterFade) c.chunk = NULL; }
			}
			if (c.framesLeft > 0 && --c.framesLeft == 0)
				c.chunk = NULL;
		}
	}
	if (sMusic && !sMusicPaused) {
		for (size_t f = 0; f < frames && sMusic; f++) {
			if (sMusicPos >= sMusic->frames) {
				if (sMusicLoops == 0) { sMusic = NULL; break; }
				if (sMusicLoops > 0) sMusicLoops--;
				sMusicPos = 0;
			}
			float gain = sMusicVolume / 128.f * sMusicFade;
			acc[f * 2] += (int)(sMusic->samples[sMusicPos * 2] * gain);
			acc[f * 2 + 1] += (int)(sMusic->samples[sMusicPos * 2 + 1] * gain);
			sMusicPos++;
			if (sMusicFadeStep != 0) {
				sMusicFade += sMusicFadeStep;
				if (sMusicFade >= 1) { sMusicFade = 1; sMusicFadeStep = 0; }
				if (sMusicFade <= 0) { sMusicFade = 0; sMusicFadeStep = 0; sMusic = NULL; }
			}
		}
	}
	for (size_t i = 0; i < frames * 2; i++)
		out[i] = clamp16(acc[i]);
	pthread_mutex_unlock(&sLock);
	if (isFloat)
		for (size_t i = 0; i < frames * 2; i++)
			((float*)buffer)[i] = out[i] / 32768.f;
	pthread_mutex_lock(&sPostLock);
	if (sPostMix)	// the video recorder wants 16-bit samples
		sPostMix(sPostData, (Uint8*)out, (int)(frames * 4));
	pthread_mutex_unlock(&sPostLock);
}

// the video recorder taps the mixed output; SDL_LockAudio keeps it from running meanwhile
HW_API void Mix_SetPostMix(void (*fn)(void*, Uint8*, int), void* data)
{
	sPostData = data;
	sPostMix = fn;
}
HW_API void SDL_LockAudio() { pthread_mutex_lock(&sPostLock); }
HW_API void SDL_UnlockAudio() { pthread_mutex_unlock(&sPostLock); }

HW_API int Mix_QuerySpec(int* frequency, Uint16* format, int* channels)
{
	if (frequency) *frequency = sRate;
	if (format) *format = 0x8010;	// AUDIO_S16LSB
	if (channels) *channels = 2;
	return sPlayer ? 1 : 0;
}

HW_API int Mix_Init(int flags) { return flags & (0x10 | 0x40); }	// OGG | OPUS
HW_API void Mix_Quit() {}

HW_API int Mix_OpenAudio(int frequency, Uint16, int, int chunksize)
{
	if (sPlayer)
		return 0;
	sRate = frequency;
	media_raw_audio_format fmt = media_raw_audio_format::wildcard;
	fmt.frame_rate = frequency;
	fmt.channel_count = 2;
	fmt.format = media_raw_audio_format::B_AUDIO_SHORT;
	fmt.byte_order = B_MEDIA_HOST_ENDIAN;
	fmt.buffer_size = chunksize * 4;
#ifdef __VOS__
	BMediaFormat format(fmt);	// VitruvianOS's media2 takes a BMediaFormat
	sPlayer = new BSoundPlayer(&format, "Hedgewars", mix);
#else
	sPlayer = new BSoundPlayer(&fmt, "Hedgewars", mix);
#endif
	if (sPlayer->InitCheck() != B_OK) {
		delete sPlayer;
		sPlayer = NULL;
		SDL_SetError("can't open the sound output");
		return -1;
	}
	sPlayer->SetHasData(true);
	sPlayer->Start();
	return 0;
}

HW_API void Mix_CloseAudio()
{
	if (!sPlayer)
		return;
	sPlayer->Stop();
	delete sPlayer;
	sPlayer = NULL;
}

// Decode a whole sound file to 16-bit stereo at sRate. Caller frees.
static int16_t* decode(SDL_RWops* rw, bool freesrc, size_t* outFrames)
{
	if (!rw)
		return NULL;
	size_t size;
	void* data = hw_read_all(rw, &size, freesrc);
#ifdef __VOS__
	// media2 opens files only, not memory: the sound goes through a temporary file
	struct TempFile {
		char path[32] = "/tmp/hw-sound-XXXXXX";
		~TempFile() { unlink(path); }
	} temp;
	int fd = mkstemp(temp.path);
	bool written = fd >= 0 && write(fd, data, size) == (ssize_t)size;
	if (fd >= 0)
		close(fd);
	entry_ref ref;
	if (!written || get_ref_for_path(temp.path, &ref) != B_OK) {
		free(data);
		SDL_SetError("can't decode sound");
		return NULL;
	}
	BMediaFile file(&ref);
#else
	BMemoryIO io(data, size);
	BMediaFile file(&io);
#endif
	BMediaTrack* track = NULL;
	media_format fmt;
	for (int32 i = 0; file.InitCheck() == B_OK && i < file.CountTracks(); i++) {
		BMediaTrack* t = file.TrackAt(i);
		fmt.Clear();
		fmt.type = B_MEDIA_RAW_AUDIO;
		fmt.u.raw_audio.format = media_raw_audio_format::B_AUDIO_SHORT;
		fmt.u.raw_audio.byte_order = B_MEDIA_HOST_ENDIAN;
#ifdef __VOS__
		BMediaFormat wanted;
		wanted.format = fmt;
		bool decoded = t->DecodedFormat(&wanted) == B_OK;
		fmt = wanted.format;
#else
		bool decoded = t->DecodedFormat(&fmt) == B_OK;
#endif
		if (decoded && fmt.type == B_MEDIA_RAW_AUDIO
			&& (fmt.u.raw_audio.format == media_raw_audio_format::B_AUDIO_SHORT
				|| fmt.u.raw_audio.format == media_raw_audio_format::B_AUDIO_FLOAT)) {
			track = t;
			break;
		}
		file.ReleaseTrack(t);
	}
	if (!track) {
		free(data);
		SDL_SetError("can't decode sound");
		return NULL;
	}
	int channels = fmt.u.raw_audio.channel_count;
	float rate = fmt.u.raw_audio.frame_rate;
	std::vector<int16_t> pcm;	// interleaved, source channels and rate
	std::vector<char> buf(std::max<size_t>(fmt.u.raw_audio.buffer_size, 1 << 20));	// room for any one read
	int64 frames;
	media_header mh;
	bool isFloat = fmt.u.raw_audio.format == media_raw_audio_format::B_AUDIO_FLOAT;	// media2 decodes to float
	while (track->ReadFrames(buf.data(), &frames, &mh) == B_OK && frames > 0) {
		if (!isFloat) {
			pcm.insert(pcm.end(), (int16_t*)buf.data(), (int16_t*)buf.data() + frames * channels);
			continue;
		}
		const float* f = (const float*)buf.data();
		for (int64 i = 0; i < frames * channels; i++)
			pcm.push_back(clamp16((int)(f[i] * 32767.f)));
	}
	file.ReleaseTrack(track);
	free(data);

	size_t inFrames = pcm.size() / (channels ? channels : 1);
	size_t n = (size_t)((double)inFrames * sRate / rate);
	int16_t* out = (int16_t*)malloc(n * 4 + 4);
	for (size_t i = 0; i < n; i++) {	// linear resample, mono -> stereo, extra channels dropped
		double p = (double)i * rate / sRate;
		size_t i0 = (size_t)p;
		size_t i1 = i0 + 1 < inFrames ? i0 + 1 : i0;
		float t = (float)(p - i0);
		for (int c = 0; c < 2; c++) {
			int sc = channels > 1 ? c : 0;
			out[i * 2 + c] = (int16_t)(pcm[i0 * channels + sc] * (1 - t) + pcm[i1 * channels + sc] * t);
		}
	}
	*outFrames = n;
	return out;
}

HW_API Mix_Chunk* Mix_LoadWAV_RW(SDL_RWops* rw, int freesrc)
{
	size_t frames;
	int16_t* pcm = decode(rw, freesrc, &frames);
	if (!pcm)
		return NULL;
	Mix_Chunk* c = (Mix_Chunk*)malloc(sizeof(Mix_Chunk));
	*c = {1, (Uint8*)pcm, (Uint32)(frames * 4), 128};
	return c;
}

HW_API void Mix_FreeChunk(Mix_Chunk* chunk)
{
	if (!chunk)
		return;
	pthread_mutex_lock(&sLock);
	for (Channel& c : sChannels)
		if (c.chunk == chunk)
			c.chunk = NULL;
	pthread_mutex_unlock(&sLock);
	free(chunk->abuf);
	free(chunk);
}

HW_API int Mix_AllocateChannels(int n)
{
	if (n < 0)
		return (int)sChannels.size();
	pthread_mutex_lock(&sLock);
	sChannels.resize(n);
	pthread_mutex_unlock(&sLock);
	return n;
}

static bool valid(int ch) { return ch >= 0 && ch < (int)sChannels.size(); }

HW_API int Mix_FadeInChannelTimed(int ch, Mix_Chunk* chunk, int loops, int ms, int ticks)
{
	if (!chunk)
		return -1;
	pthread_mutex_lock(&sLock);
	if (ch < 0)
		for (int i = 0; i < (int)sChannels.size() && ch < 0; i++)
			if (!sChannels[i].chunk)
				ch = i;
	if (!valid(ch)) {
		pthread_mutex_unlock(&sLock);
		SDL_SetError("no free channels");
		return -1;
	}
	Channel& c = sChannels[ch];
	c.chunk = chunk;
	c.pos = 0;
	c.loops = loops;
	c.fade = ms > 0 ? 0 : 1;
	c.fadeStep = ms > 0 ? 1000.f / (ms * (float)sRate) : 0;
	c.haltAfterFade = false;
	c.framesLeft = ticks >= 0 ? (int64_t)ticks * sRate / 1000 : -1;
	pthread_mutex_unlock(&sLock);
	return ch;
}

HW_API int Mix_PlayChannelTimed(int ch, Mix_Chunk* chunk, int loops, int ticks)
{
	return Mix_FadeInChannelTimed(ch, chunk, loops, 0, ticks);
}

HW_API int Mix_HaltChannel(int ch)
{
	pthread_mutex_lock(&sLock);
	for (int i = 0; i < (int)sChannels.size(); i++)
		if (ch < 0 || i == ch)
			sChannels[i].chunk = NULL;
	pthread_mutex_unlock(&sLock);
	return 0;
}

HW_API int Mix_FadeOutChannel(int ch, int ms)
{
	int n = 0;
	pthread_mutex_lock(&sLock);
	for (int i = 0; i < (int)sChannels.size(); i++) {
		Channel& c = sChannels[i];
		if ((ch < 0 || i == ch) && c.chunk) {
			if (ms <= 0)
				c.chunk = NULL;
			else {
				c.fadeStep = -1000.f / (ms * (float)sRate);
				c.haltAfterFade = true;
			}
			n++;
		}
	}
	pthread_mutex_unlock(&sLock);
	return n;
}

HW_API int Mix_Playing(int ch)
{
	int n = 0;
	pthread_mutex_lock(&sLock);
	for (int i = 0; i < (int)sChannels.size(); i++)
		if ((ch < 0 || i == ch) && sChannels[i].chunk)
			n++;
	pthread_mutex_unlock(&sLock);
	return n;
}

HW_API int Mix_Volume(int ch, int volume)
{
	pthread_mutex_lock(&sLock);
	int prev = 0, count = 0;
	for (int i = 0; i < (int)sChannels.size(); i++) {
		if (ch >= 0 && i != ch)
			continue;
		prev += sChannels[i].volume;
		count++;
		if (volume >= 0)
			sChannels[i].volume = volume > 128 ? 128 : volume;
	}
	pthread_mutex_unlock(&sLock);
	return count ? prev / count : 0;
}

// shortcut: music is decoded whole into memory (~10 MB per track); stream it if memory gets tight
HW_API Mix_Music* Mix_LoadMUS_RW(SDL_RWops* rw)
{
	size_t frames;
	int16_t* pcm = decode(rw, true, &frames);	// SDL_mixer 2 frees it too
	if (!pcm)
		return NULL;
	return new Mix_Music{pcm, frames};
}

HW_API void Mix_FreeMusic(Mix_Music* m)
{
	if (!m)
		return;
	pthread_mutex_lock(&sLock);
	if (sMusic == m)
		sMusic = NULL;
	pthread_mutex_unlock(&sLock);
	free(m->samples);
	delete m;
}

HW_API int Mix_FadeInMusic(Mix_Music* m, int loops, int ms)
{
	if (!m)
		return -1;
	pthread_mutex_lock(&sLock);
	sMusic = m;
	sMusicPos = 0;
	sMusicLoops = loops;	// SDL_mixer: -1 forever, 0 and 1 both play once
	if (sMusicLoops > 0)
		sMusicLoops--;
	sMusicPaused = false;
	sMusicFade = ms > 0 ? 0 : 1;
	sMusicFadeStep = ms > 0 ? 1000.f / (ms * (float)sRate) : 0;
	pthread_mutex_unlock(&sLock);
	return 0;
}

HW_API int Mix_HaltMusic() { pthread_mutex_lock(&sLock); sMusic = NULL; pthread_mutex_unlock(&sLock); return 0; }
// SDLh.pas declares these with an (ignored) argument; SDL_mixer takes none
HW_API int Mix_PauseMusic(Mix_Music*) { sMusicPaused = true; return 0; }
HW_API int Mix_ResumeMusic(Mix_Music*) { sMusicPaused = false; return 0; }

HW_API int Mix_VolumeMusic(int volume)
{
	int prev = sMusicVolume;
	if (volume >= 0)
		sMusicVolume = volume > 128 ? 128 : volume;
	return prev;
}
