/**
 * \mainpage
 * \par Tanks
 * Battle City / Tank 1990 C++ implementation
 * \author Krystian Kałużny
 * \date 12.05.2015
 * \version 1.0
 * \par Kontakt:
 * \a k.kaluzny141@gmail.com
 */

#include "app.h"

#ifdef ANDROID_PORT
// SDLActivity.java starts the game by calling SDL_main() inside libmain.so;
// SDL_main.h (pulled in by SDL.h) renames main() to SDL_main and marks it
// for export so dlsym() can find it.
#include <SDL2/SDL.h>
#endif

#ifdef PSP_PORT
#include "appconfig.h"

#include <SDL2/SDL.h>
#include <exception>
#include <fstream>

// Note: on PSP <SDL2/SDL.h> renames main() to SDL_main; the real main()
// entry point and the .sceModuleInfo section both come from libSDL2main,
// which is part of the link line produced by psp-pkg-config.

namespace
{
    // The log is written next to EBOOT.PBP, so it works no matter how the
    // install folder is named (SDL_GetBasePath returns the EBOOT directory).
    std::string bootLogPath()
    {
        std::string dir = "ms0:/PSP/GAME/TANKS/";
        if (char *base = SDL_GetBasePath())
        {
            dir = base;
            SDL_free(base);
        }
        return dir + "boot.log";
    }

    void logBoot(const std::string &line, bool fresh = false)
    {
        std::ofstream out(bootLogPath().c_str(), fresh ? std::ios::trunc : std::ios::app);
        if (out)
            out << line << "\n";
    }

    bool assetsPresent()
    {
        std::ifstream in(AppConfig::texture_path.c_str(), std::ios::binary);
        return in.good();
    }
} // namespace
#endif

int main(int argc, char* args[] )
{
#ifdef PSP_PORT
    logBoot("boot: start", true);
    if (!assetsPresent())
        logBoot("fatal: assets are missing (" + AppConfig::assets_dir +
                ") - copy the assets/ folder from build-psp/ next to EBOOT.PBP "
                "and keep the install folder named TANKS");
    try
    {
#endif
    App app;
#ifdef PSP_PORT
        logBoot("boot: app constructed");
#endif
    app.run();
#ifdef PSP_PORT
        logBoot("boot: clean exit");
    }
    catch (const std::exception &e)
    {
        logBoot(std::string("fatal: uncaught exception: ") + e.what());
        return 1;
    }
    catch (...)
    {
        logBoot("fatal: unknown non-std exception");
        return 1;
    }
#endif

    return 0;
}
