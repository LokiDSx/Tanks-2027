#ifndef SDL_SOUND_MANAGER_H
#define SDL_SOUND_MANAGER_H

#include "../sound_manager.h"

#include <SDL2/SDL_mixer.h>
#include <string>
#include <unordered_map>
#include <vector>

class SDLSoundManager : public SoundManager
{
public:
    SDLSoundManager();
    ~SDLSoundManager() override;
    void preload(const Sound &sound) override;
    void play(const Sound &sound) override;
    void stop(const Sound &sound) override;
    void stopAll() override;

private:
    Mix_Chunk *loadSound(const Sound &sound);
    std::vector<Sound> findAllMatchingSounds(const Sound &sound);

    bool canPlay(const Sound &sound);

    std::unordered_map<std::string, Mix_Chunk *> m_sound_cache;
};

#endif // SDL_SOUND_MANAGER_H