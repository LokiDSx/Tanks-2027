#include "profile.h"
#include "appconfig.h"
#include "high_score.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

#ifdef VITA_PORT
#include <psp2/io/stat.h>
#else
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#endif

namespace
{
    // All existing profiles, sorted; the index file is the source of truth
    std::vector<std::string> s_profiles;
    // Active profile; empty while none is active
    std::string s_current;
    // Records of an older build were loaded into memory and wait to be
    // seeded into the first created profile
    bool s_legacy_loaded = false;

    std::string baseDirPath()
    {
#ifdef VITA_PORT
        // The port stores its data at ux0:data/tanks/
        sceIoMkdir("ux0:data", 0777); // fails harmlessly if the dir exists
        sceIoMkdir("ux0:data/tanks", 0777);
        return "ux0:data/tanks/";
#else
        // SDL_GetPrefPath creates the directory if it does not exist
        char *dir = SDL_GetPrefPath("vita", "tanks");
        if (dir == nullptr)
            return "";
        std::string path(dir);
        SDL_free(dir);
        return path;
#endif
    }

    std::string profilesDirPath()
    {
        const std::string base = baseDirPath();
        if (base.empty())
            return "";
#ifdef VITA_PORT
        sceIoMkdir("ux0:data/tanks/profiles", 0777);
#else
#ifdef _WIN32
        _mkdir((base + "profiles").c_str());
#else
        mkdir((base + "profiles").c_str(), 0777);
#endif
#endif
        return base + "profiles/";
    }

    std::string indexPath()
    {
        return profilesDirPath() + "index.txt";
    }

    std::string currentPointerPath()
    {
        return baseDirPath() + "current_profile.txt";
    }

    std::string profilePath(const std::string &name)
    {
        return profilesDirPath() + name + ".txt";
    }

    std::string trim(const std::string &line)
    {
        std::string result = line;
        while (!result.empty() && (result.back() == '\r' || result.back() == '\n' || result.back() == ' '))
            result.pop_back();
        size_t start = result.find_first_not_of(" \t\r\n");
        return start == std::string::npos ? "" : result.substr(start);
    }

    std::string readFile(const std::string &path)
    {
        SDL_RWops *file = SDL_RWFromFile(path.c_str(), "rb");
        if (file == nullptr)
            return "";

        char buffer[1024];
        Sint64 size = SDL_RWread(file, buffer, 1, sizeof(buffer) - 1);
        SDL_RWclose(file);
        if (size <= 0)
            return "";
        buffer[size] = '\0';
        return std::string(buffer);
    }

    bool fileExists(const std::string &path)
    {
        SDL_RWops *file = SDL_RWFromFile(path.c_str(), "rb");
        if (file == nullptr)
            return false;
        SDL_RWclose(file);
        return true;
    }

    // Apply key=value content (settings + per-mode records) to the
    // in-memory AppConfig/HighScore values
    void applyContent(const std::string &content)
    {
        size_t pos = 0;
        while (pos < content.size())
        {
            size_t eol = content.find('\n', pos);
            if (eol == std::string::npos)
                eol = content.size();

            std::string line = content.substr(pos, eol - pos);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            size_t eq = line.find('=');
            if (eq != std::string::npos)
            {
                const std::string key = line.substr(0, eq);
                const int value = std::atoi(line.substr(eq + 1).c_str());

                if (key == "widescreen")
                    AppConfig::widescreen = (value != 0);
                else if (key == "sound")
                    AppConfig::sound_enabled = (value != 0);
                else if (key == "highscore_classic")
                    HighScore::scores[0] = value;
                else if (key == "highscore_generated")
                    HighScore::scores[1] = value;
                else if (key == "highscore_survival")
                    HighScore::scores[2] = value;
                else if (key == "highscore_deathmatch")
                    HighScore::scores[3] = value;
                else if (key == "survival_tanks")
                    HighScore::survival_tanks = value;
            }

            pos = eol + 1;
        }
    }

    // Write the current settings + records to the given profile file
    bool writeCurrentTo(const std::string &path)
    {
        SDL_RWops *file = SDL_RWFromFile(path.c_str(), "wb");
        if (file == nullptr)
            return false;

        char buffer[320];
        int len = std::snprintf(buffer, sizeof(buffer),
                                "widescreen=%d\n"
                                "sound=%d\n"
                                "highscore_classic=%u\n"
                                "highscore_generated=%u\n"
                                "highscore_survival=%u\n"
                                "highscore_deathmatch=%u\n"
                                "survival_tanks=%u\n",
                                AppConfig::widescreen ? 1 : 0,
                                AppConfig::sound_enabled ? 1 : 0,
                                HighScore::scores[0], HighScore::scores[1], HighScore::scores[2], HighScore::scores[3],
                                HighScore::survival_tanks);
        if (len > 0)
            SDL_RWwrite(file, buffer, 1, len);
        SDL_RWclose(file);
        return true;
    }

    void resetToDefaults()
    {
        for (int i = 0; i < 4; i++)
            HighScore::scores[i] = 0;
        HighScore::survival_tanks = 0;
        AppConfig::widescreen = false;
        AppConfig::sound_enabled = true;
    }

    void writeIndex()
    {
        const std::string path = indexPath();
        if (path.empty())
            return;

        SDL_RWops *file = SDL_RWFromFile(path.c_str(), "wb");
        if (file == nullptr)
            return;
        for (const auto &name : s_profiles)
        {
            const std::string line = name + "\n";
            SDL_RWwrite(file, line.c_str(), 1, line.size());
        }
        SDL_RWclose(file);
    }

    void writeCurrentPointer()
    {
        const std::string path = currentPointerPath();
        if (path.empty())
            return;

        SDL_RWops *file = SDL_RWFromFile(path.c_str(), "wb");
        if (file == nullptr)
            return;
        const std::string line = s_current + "\n";
        SDL_RWwrite(file, line.c_str(), 1, line.size());
        SDL_RWclose(file);
    }

    // Locations used by older builds; the first readable one seeds the
    // records of the first created profile
    bool tryLoadLegacy()
    {
#ifdef VITA_PORT
        const std::string candidates[] = {
            "ux0:data/tanks2027/settings.txt", // previous data location
        };
#else
        const std::string candidates[] = {
            baseDirPath() + "settings.txt", // same dir, plain file
        };
#endif
        for (const auto &candidate : candidates)
        {
            if (fileExists(candidate))
            {
                applyContent(readFile(candidate));
                return true;
            }
        }

#ifndef VITA_PORT
        return false;
#else
        // The oldest location (SDL pref path) used by the very first builds
        char *dir = SDL_GetPrefPath("vita", "tanks");
        if (dir != nullptr)
        {
            std::string path(dir);
            SDL_free(dir);
            path += "settings.txt";
            if (fileExists(path))
            {
                applyContent(readFile(path));
                return true;
            }
        }
        return false;
#endif
    }
}

void Profile::init()
{
    profilesDirPath(); // make sure the directories exist
    if (profilesDirPath().empty())
        return;

    s_profiles.clear();
    const std::string index = readFile(indexPath());
    size_t pos = 0;
    while (pos < index.size())
    {
        size_t eol = index.find('\n', pos);
        if (eol == std::string::npos)
            eol = index.size();
        const std::string name = trim(index.substr(pos, eol - pos));
        if (!name.empty() && fileExists(profilePath(name)))
            s_profiles.push_back(name);
        pos = eol + 1;
    }
    std::sort(s_profiles.begin(), s_profiles.end());

    s_current = trim(readFile(currentPointerPath()));
    if (std::find(s_profiles.begin(), s_profiles.end(), s_current) == s_profiles.end())
        s_current.clear();

    s_legacy_loaded = false;
    if (s_profiles.empty())
        s_legacy_loaded = tryLoadLegacy();
}

const std::vector<std::string> &Profile::list()
{
    return s_profiles;
}

const std::string &Profile::current()
{
    return s_current;
}

bool Profile::exists(const std::string &name)
{
    return std::find(s_profiles.begin(), s_profiles.end(), name) != s_profiles.end();
}

bool Profile::create(const std::string &raw_name)
{
    const std::string name = sanitize(raw_name);
    if (name.empty() || exists(name))
        return false;

    // The very first profile inherits the records of an older build kept
    // in memory; every later profile starts from scratch
    if (!(s_profiles.empty() && s_legacy_loaded))
        resetToDefaults();

    s_profiles.push_back(name);
    std::sort(s_profiles.begin(), s_profiles.end());
    writeIndex();

    writeCurrentTo(profilePath(name));
    s_current = name;
    writeCurrentPointer();
    return true;
}

void Profile::remove(const std::string &name)
{
    auto it = std::find(s_profiles.begin(), s_profiles.end(), name);
    if (it == s_profiles.end())
        return;

    s_profiles.erase(it);
    writeIndex();
    ::remove(profilePath(name).c_str());

    if (s_current == name)
    {
        s_current.clear();
        writeCurrentPointer();
        if (!s_profiles.empty())
            activate(s_profiles.front());
    }
}

void Profile::activate(const std::string &name)
{
    if (!exists(name))
        return;

    resetToDefaults();
    applyContent(readFile(profilePath(name)));

    s_current = name;
    writeCurrentPointer();
}

void Profile::save()
{
    if (s_current.empty())
        return;
    writeCurrentTo(profilePath(s_current));
}

unsigned Profile::bestScore(const std::string &name)
{
    // Read the records straight from the profile file without touching
    // the in-memory values of the active profile
    const std::string content = readFile(profilePath(name));
    unsigned scores[4] = {0, 0, 0, 0};

    size_t pos = 0;
    while (pos < content.size())
    {
        size_t eol = content.find('\n', pos);
        if (eol == std::string::npos)
            eol = content.size();
        const std::string line = content.substr(pos, eol - pos);

        size_t eq = line.find('=');
        if (eq != std::string::npos)
        {
            const std::string key = line.substr(0, eq);
            const int value = std::atoi(line.substr(eq + 1).c_str());
            if (key == "highscore_classic")
                scores[0] = value;
            else if (key == "highscore_generated")
                scores[1] = value;
            else if (key == "highscore_survival")
                scores[2] = value;
            else if (key == "highscore_deathmatch")
                scores[3] = value;
        }
        pos = eol + 1;
    }

    unsigned best = scores[0];
    for (int i = 1; i < 4; i++)
        if (scores[i] > best)
            best = scores[i];
    return best;
}

std::string Profile::sanitize(const std::string &raw)
{
    std::string out;
    for (char c : raw)
    {
        if (out.size() >= 6)
            break;
        if (c >= 'a' && c <= 'z')
            c = c - 'a' + 'A';
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
            out.push_back(c);
    }
    return out;
}
