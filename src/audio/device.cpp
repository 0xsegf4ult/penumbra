#include <penumbra/audio.hpp>
#include <penumbra/cvar.hpp>
#include <penumbra/log.hpp>
#include <penumbra/types.hpp>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_audio.h>

namespace penumbra
{

static SDL_AudioDeviceID playback_device;

static SDL_AudioStream* streams[32];

static void set_mvol_cv(cvar_t* cvar)
{
	if(cvar->float_v < 0.0f)
		cvar->float_v = 0.0f;
	if(cvar->float_v > 1.0f)
		cvar->float_v = 1.0f;

	SDL_SetAudioDeviceGain(playback_device, cvar->float_v);
}

static cvar_t mvol_cv
{
	.name = "audio.master",
	.type = CVAR_TYPE_FLOAT,
	.float_defv = 0.1f,
	.callback = set_mvol_cv
};

void audio_init()
{
	cvar_register(&mvol_cv);

	SDL_InitSubSystem(SDL_INIT_AUDIO);

	playback_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
	set_mvol_cv(&mvol_cv);

	log::info("audio: playback device: {} - {}", SDL_GetCurrentAudioDriver(), SDL_GetAudioDeviceName(playback_device));

	for(int i = 0; i < 32; i++)
	{
		streams[i] = SDL_CreateAudioStream(nullptr, nullptr);
		SDL_BindAudioStreams(playback_device, &streams[0], 32);
	}
}

void audio_shutdown()
{
	for(int i = 0; i < 32; i++)
		SDL_DestroyAudioStream(streams[i]);

	SDL_CloseAudioDevice(playback_device);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void audio_play(std::span<u8> data, void* spec)
{
	SDL_AudioStream* stream = nullptr;
	for(int i = 0; i < 32; i++)
	{
		if(SDL_GetAudioStreamQueued(streams[i]) == 0)
		{
			stream = streams[i];
			break;
		}
	}

	if(!stream)
		return;

	SDL_SetAudioStreamFormat(stream, (const SDL_AudioSpec*)spec, nullptr);	
	SDL_ClearAudioStream(stream);
	SDL_PutAudioStreamData(stream, data.data(), data.size());
	SDL_FlushAudioStream(stream);
}

}
