/* [ SCORE_C ] */

/* [ INCLUDES ] */

#include <stdio.h>
#include <stdlib.h>

#include "score.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#endif

/* [ DEFINES ] */

#define APP_DIR_NAME "ctetris"
#define SAVE_FILE_NAME "ctetris.hs"

#define PATH_BUF_SIZE 1024

/* [ FN DEF ] */

// Get path to the score file based on the os.
static void get_score_path(char *buf, size_t len) {
    char dir[PATH_BUF_SIZE];
#if defined(_WIN32)
    const char *base = getenv("LOCALAPPDATA");
    if (!base)
        base = ".";
    snprintf(dir, sizeof(dir), "%s\\%s", base, APP_DIR_NAME);
    CreateDirectoryA(dir, NULL); // No-op if already exists
    snprintf(buf, len, "%s\\%s", dir, SAVE_FILE_NAME);

#else // Linux.
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg && xdg[0]) {
        snprintf(dir, sizeof(dir), "%s/%s", xdg, APP_DIR_NAME);
    } else {
        const char *home = getenv("HOME");
        if (!home)
            home = ".";
        snprintf(dir, sizeof(dir), "%s/.local/share/%s", home, APP_DIR_NAME);
    }
    mkdir(dir, 0755); // fails safely if already exists.
    snprintf(buf, len, "%s/%s", dir, SAVE_FILE_NAME);
#endif
}

// Return saved score.
// If not saved previously return 0.
uint32_t score_load(void) {
    char path[PATH_BUF_SIZE];
    get_score_path(path, sizeof(path));
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return 0;

    uint32_t hs = 0;
    if (fread(&hs, sizeof(hs), 1, fp) != 1)
        hs = 0;

    fclose(fp);
    return hs;
}

// Save the given score on to the disk.
void score_save(uint32_t score) {
    char path[PATH_BUF_SIZE];
    get_score_path(path, sizeof(path));
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return;

    fwrite(&score, sizeof(score), 1, fp);
    fclose(fp);
}

/* [ END ] */
