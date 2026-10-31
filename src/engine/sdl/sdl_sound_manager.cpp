#include "sdl_sound_manager.h"
#include "../../appconfig.h"
#include "../data/error.h"
#include <iostream>

#define MIX_CHANNEL_MONO 1
#define MIX_CHANNEL_STEREO 2
#define MIX_CHUNK_SIZE_2K 2048
#define MIX_CHUNK_SIZE_1K 1024

std::unordered_map<int, std::pair<Sound, Mix_Chunk *>> m_current_channel_chunk_cache;

void onChannelFinished(int channel)
{
    m_current_channel_chunk_cache[channel] = std::make_pair(Sound{}, nullptr);
}

SDLSoundManager::SDLSoundManager()
{
    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_CHANNEL_MONO, MIX_CHUNK_SIZE_1K) < 0)
    {
        throw Error("SDL_mixer could not initialize!", Mix_GetError());
    }

    Mix_ChannelFinished(onChannelFinished);
}

SDLSoundManager::~SDLSoundManager()
{
    m_current_channel_chunk_cache.clear();

    for (auto &pair : m_sound_cache)
    {
        Mix_FreeChunk(pair.second);
    }
    m_sound_cache.clear();
    Mix_CloseAudio();
}

void SDLSoundManager::preload(const Sound &sound)
{
    loadSound(sound);
}

void SDLSoundManager::play(const Sound &sound)
{
    if (isMuted())
    {
        return;
    }

    if (!canPlay(sound))
    {
        return;
    }

    if (sound.highest_priority)
    {
        // Stop all currently playing sounds
        Mix_HaltChannel(-1);
        m_current_channel_chunk_cache.clear();
    }

    auto matching_sounds = findAllMatchingSounds(sound);

    double play_volume = sound.volume;

    if (!matching_sounds.empty())
    {
        // Find the lowest volume among matching sounds to play a quieter version to prevent clipping
        double min_volume = 100.0;
        for (const auto &matching_sound : matching_sounds)
        {
            if (matching_sound.volume < min_volume)
            {
                min_volume = matching_sound.volume;
            }
        }

        play_volume = min_volume * sound.overlapping_factor;
    }

    Mix_Chunk *chunk = loadSound(sound);

    int loops = 0;
    int channel = Mix_PlayChannel(-1, chunk, loops);
    if (channel == -1)
    {
        std::cerr << "Warning: Unable to play sound: " << sound.file_path << " Error: " << Mix_GetError() << std::endl;
        return;
    }

    int sdl_volume = static_cast<int>(MIX_MAX_VOLUME * (play_volume / 100.0));
    Mix_Volume(channel, sdl_volume);

    Sound sound_state = sound;
    sound_state.volume = play_volume;
    m_current_channel_chunk_cache[channel] = std::make_pair(sound_state, chunk);
}

void SDLSoundManager::stop(const Sound &sound)
{
    for (const auto &channel_to_sound : m_current_channel_chunk_cache)
    {
        auto current_sound = channel_to_sound.second.first;
        if (current_sound.file_path == sound.file_path)
        {
            // BE AWARE: Mix_ChannelFinished callback will be called after this and can modify m_current_channel_chunk_cache
            Mix_HaltChannel(channel_to_sound.first);
        }
    }
}

void SDLSoundManager::stopAll()
{
    Mix_HaltChannel(-1);
    m_current_channel_chunk_cache.clear();
}

Mix_Chunk *SDLSoundManager::loadSound(const Sound &sound)
{
    const std::string &file_path = sound.file_path;
    auto it = m_sound_cache.find(file_path);
    if (it != m_sound_cache.end())
    {
        return it->second;
    }

    Mix_Chunk *chunk = Mix_LoadWAV((AppConfig::assets_dir + file_path).c_str());
    if (chunk == nullptr)
    {
        throw Error("Failed to load sound: " + file_path, Mix_GetError());
    }

    m_sound_cache[file_path] = chunk;
    return chunk;
}

std::vector<Sound> SDLSoundManager::findAllMatchingSounds(const Sound &sound)
{
    std::vector<Sound> matching_sounds;
    for (const auto &channel_to_sound : m_current_channel_chunk_cache)
    {
        auto current_sound = channel_to_sound.second.first;
        if (sound.file_path == current_sound.file_path)
        {
            matching_sounds.push_back(current_sound);
        }
    }
    return matching_sounds;
}

bool SDLSoundManager::canPlay(const Sound &sound)
{
    if (!sound.multi_instance)
    {
        // Check if this Sound is already playing
        for (const auto &channel_to_sound : m_current_channel_chunk_cache)
        {
            auto current_sound = channel_to_sound.second.first;
            if (sound.file_path == current_sound.file_path)
            {
                return false;
            }
        }
    }

    if (!sound.highest_priority)
    {
        // If there is a highest priority sound playing, do not play this sound
        for (const auto &channel_to_sound : m_current_channel_chunk_cache)
        {
            auto current_sound = channel_to_sound.second.first;
            if (current_sound.highest_priority)
            {
                return false; // Do not play this sound
            }
        }
    }
    return true;
}