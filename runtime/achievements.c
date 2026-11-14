// Recomp Achievements (third_party/rl_achievements, events mode): what the game does, as named events for
// RecompLauncher's achievement list for MUA3 (hosted by RecompLauncher; docs/achievements.json is the one we propose).
// Started outside RecompLauncher (no RL_SESSION) every call here does nothing.
//
// The events come from the story's movies (runtime/hle/movie.c): MUA3's are rom:/MOVIE/EV_<chapter><scene>_<cut>.mp4,
// chapters 01 to 11, and the last one of the story is EV_1105_01. Each one opened fires
//   movie_EV_0201_01       (every movie, by name: the list can tie an achievement to any of them)
//   chapter_02             (the first movie of a chapter this session: the story has reached that chapter)
// and the profile's "now playing" line says the chapter. A save loaded (a read of a slot file, save00 ...) is reported
// as such, so unlocks right after it count as Casual.
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "hle.h"
#include "rl_achievements.h"

void achievements_init(void) {
    extern const char *larecomp_version(void);  // runtime/hle/game_hooks.c
    rl_ach_set_info("mua3-recomp", larecomp_version());
    if (rl_ach_init("", NULL, NULL)) LOG("[achievements] connected to RecompLauncher\n");
}

void achievements_frame(void) { rl_ach_frame(); }

static const char *base_name(const char *path) {  // (after the last / or \)
    const char *b = path;
    for (const char *p = path; p && *p; p++)
        if (*p == '/' || *p == '\\') b = p + 1;
    return b;
}

// rom:/MOVIE/EV_0201_01.mp4 (or a host path to it)
void achievements_movie(const char *path) {
    const char *b = base_name(path);
    if (!b || strncmp(b, "EV_", 3) || !isdigit((unsigned char)b[3]) || !isdigit((unsigned char)b[4])) return;
    char name[64], ev[80];
    snprintf(name, sizeof name, "%s", b);
    char *dot = strrchr(name, '.');
    if (dot) *dot = 0;
    snprintf(ev, sizeof ev, "movie_%s", name);
    rl_ach_event(ev);
    int chapter = (b[3] - '0') * 10 + (b[4] - '0');
    static int last = -1;
    if (chapter != last) {
        last = chapter;
        snprintf(ev, sizeof ev, "chapter_%02d", chapter);
        rl_ach_event(ev);
        char presence[48];
        snprintf(presence, sizeof presence, "Story: chapter %d", chapter);
        rl_ach_presence(presence);
        LOG("[achievements] %s (%s)\n", ev, name);
    }
}

// A file the game read from offset 0: its own save slots are save00, save01, ...
void achievements_file_read(const char *path) {
    const char *b = base_name(path);
    if (b && !strncmp(b, "save", 4) && isdigit((unsigned char)b[4]) && isdigit((unsigned char)b[5]) && !b[6]) {
        rl_ach_save_loaded();
        LOG("[achievements] save loaded (%s)\n", b);
    }
}
