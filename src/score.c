/* [ SCORE_C ] */

/* [ INCLUDES ] */

#include <stdio.h>
#include <stdlib.h>

#include "score.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#endif

/* [ DEFINES ] */

#define APP_DIR_NAME "cTetris"
#define SAVE_FILE_NAME "cTetris.hs"

#define PATH_BUF_SIZE 1024

/* [ INTERNAL HELPERS ] */

// Get path to the score file based on the OS.
static void get_score_path(char *buf, size_t len) {
#if defined(__EMSCRIPTEN__)
    (void)buf;
    (void)len;
    return;
    // Not used. Uses browser LocalStorage instead.
#else
    char dir[PATH_BUF_SIZE];
#if defined(_WIN32)
    const char *base = getenv("LOCALAPPDATA");
    if (!base)
        base = ".";
    snprintf(dir, sizeof(dir), "%s\\%s", base, APP_DIR_NAME);
    CreateDirectoryA(dir, NULL);
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
    mkdir(dir, 0755);
    snprintf(buf, len, "%s/%s", dir, SAVE_FILE_NAME);
#endif
#endif // __EMSCRIPTEN__
}

/* [ PUBLIC API ] */

// Return saved score.
// If not saved previously return 0.
uint32_t score_load(void) {
#if defined(__EMSCRIPTEN__)
    return EM_ASM_INT({
        const value = localStorage.getItem("cTetrisHighScore");
        if (!value)
            return 0;
        return Number(value);
    });
#else
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
#endif
}

// Save the given score.
void score_save(uint32_t score) {
#if defined(__EMSCRIPTEN__)
    EM_ASM(
        { localStorage.setItem("cTetrisHighScore", String($0)); }, (int)score);
#else
    char path[PATH_BUF_SIZE];
    get_score_path(path, sizeof(path));
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return;
    fwrite(&score, sizeof(score), 1, fp);
    fclose(fp);
#endif
}

/* [ END ] */
