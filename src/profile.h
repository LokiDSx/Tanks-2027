#ifndef PROFILE_H
#define PROFILE_H

#include <string>
#include <vector>

/**
 * @brief
 * Player profiles: each profile keeps its own settings (widescreen, sound)
 * and per-mode high scores in a separate file under
 * ux0:data/tanks/profiles/ (the app data directory on desktop systems).
 * The name of the active profile is remembered in current_profile.txt, so
 * the next launch can offer to continue where the player left off.
 *
 * On the very first launch (no profiles yet) the data saved by older
 * builds (settings.txt) is kept in memory and seeded into the first
 * created profile, so existing records are not lost.
 */
namespace Profile
{
    /**
     * Create the storage directories, read the profile list and activate
     * the profile used on the previous launch (if it still exists).
     * Legacy settings.txt data (older builds) is loaded when there are no
     * profiles at all, ready to be seeded into the first created profile.
     */
    void init();

    /** Names of all existing profiles (sorted alphabetically). */
    const std::vector<std::string> &list();

    /** The active profile name; empty while no profile is active. */
    const std::string &current();

    bool exists(const std::string &name);

    /**
     * Create a new profile, make it active and remember it as the last
     * used one. Fails (returns false) when the name is invalid or already
     * taken. The very first profile inherits the records of an older
     * build when they are present.
     */
    bool create(const std::string &name);

    /**
     * Delete a profile. When the active profile is removed, another one
     * becomes active (or none, if it was the last one).
     */
    void remove(const std::string &name);

    /**
     * Make the given profile active: its settings and records replace the
     * current in-memory values, and it becomes the last used profile.
     */
    void activate(const std::string &name);

    /** Write the active profile (settings + records) to disk. No-op without a profile. */
    void save();

    /**
     * The best score across all modes stored in the given profile file.
     * Reads the file directly and does not touch the in-memory values of
     * the active profile (used to show records in the profile list).
     */
    unsigned bestScore(const std::string &name);

    /**
     * Turn raw input into a valid profile name: uppercase, only letters,
     * digits and underscore, at most 6 characters. May return an empty
     * string when nothing usable is left.
     */
    std::string sanitize(const std::string &raw);
}

#endif // PROFILE_H
