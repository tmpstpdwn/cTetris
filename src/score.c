#include <stdio.h>
#include <stdlib.h>

#include "score.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#endif

static void get_hs_path(char *buf, size_t len) {
#if defined(_WIN32)
    const char *base = getenv("LOCALAPPDATA");
    if (!base)
        base = ".";
    char dir[512];
    snprintf(dir, sizeof(dir), "%s\\ctetris", base);
    CreateDirectoryA(dir, NULL); // no-op if already exists
    snprintf(buf, len, "%s\\ctetris.hs", dir);

#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    if (!home)
        home = ".";
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/Library/Application Support/ctetris", home);
    mkdir(dir, 0755);
    snprintf(buf, len, "%s/ctetris.hs", dir);

#else
    const char *xdg = getenv("XDG_DATA_HOME");
    char dir[512];
    if (xdg && xdg[0])
        snprintf(dir, sizeof(dir), "%s/ctetris", xdg);
    else {
        const char *home = getenv("HOME");
        if (!home)
            home = ".";
        snprintf(dir, sizeof(dir), "%s/.local/share/ctetris", home);
    }
    mkdir(dir, 0755);
    snprintf(buf, len, "%s/ctetris.hs", dir);
#endif
}

uint32_t hs_load(void) {
    char path[512];
    get_hs_path(path, sizeof(path));
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return 0;
    uint32_t score = 0;
    if (fread(&score, sizeof(score), 1, fp) != 1)
        score = 0;
    fclose(fp);
    return score;
}

bool hs_save(uint32_t score) {
    uint32_t current = hs_load();
    if (score <= current)
        return true;
    char path[512];
    get_hs_path(path, sizeof(path));
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return false;
    bool ok = fwrite(&score, sizeof(score), 1, fp) == 1;
    fclose(fp);
    return ok;
}
