/*
 * save.c - 最高分存档（ms0:/PSP/SAVEDATA/PSP2048/）
 */
#include "game.h"
#include <string.h>

static const char *SAVE_DIR  = "ms0:/PSP/SAVEDATA/PSP2048";
static const char *SAVE_FILE = "ms0:/PSP/SAVEDATA/PSP2048/savedata.bin";

typedef struct {
    int  magic;      // 0x50323034 ('P' '2' '0' '4')
    int  version;    // 1
    int  best_score;
    int  score;
    int  has_won;
    int  keep_playing;
    int  grid[16];
    int  pad[12];    // 留作未来扩展
} SaveData;

#define SAVE_MAGIC 0x50323034

static void ensure_dir(void) {
    sceIoMkdir(SAVE_DIR, 0777);
}

void Save_LoadBest(void) {
    ensure_dir();
    SceUID fd = sceIoOpen(SAVE_FILE, PSP_O_RDONLY, 0);
    if (fd < 0) { g_game.best_score = 0; return; }

    SaveData sd;
    memset(&sd, 0, sizeof(sd));
    int rd = sceIoRead(fd, &sd, sizeof(sd));
    sceIoClose(fd);

    if (rd != sizeof(sd) || sd.magic != SAVE_MAGIC || sd.version != 1) {
        g_game.best_score = 0;
        return;
    }
    g_game.best_score = sd.best_score < 0 ? 0 : sd.best_score;
}

void Save_SaveBest(void) {
    ensure_dir();
    SceUID fd = sceIoOpen(SAVE_FILE, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return;

    SaveData sd;
    memset(&sd, 0, sizeof(sd));
    sd.magic     = SAVE_MAGIC;
    sd.version   = 1;
    sd.best_score = g_game.best_score;
    sd.score     = g_game.score;
    sd.has_won   = g_game.has_won ? 1 : 0;
    sd.keep_playing = g_game.keep_playing ? 1 : 0;

    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            sd.grid[r * 4 + c] = g_game.grid[r][c];

    sceIoWrite(fd, &sd, sizeof(sd));
    sceIoClose(fd);
}

// 额外：恢复对局
void Save_LoadProgress(void) {
    ensure_dir();
    SceUID fd = sceIoOpen(SAVE_FILE, PSP_O_RDONLY, 0);
    if (fd < 0) return;

    SaveData sd;
    memset(&sd, 0, sizeof(sd));
    int rd = sceIoRead(fd, &sd, sizeof(sd));
    sceIoClose(fd);
    if (rd != sizeof(sd) || sd.magic != SAVE_MAGIC) return;

    memcpy(g_game.grid, sd.grid, sizeof(g_game.grid));
    g_game.score = sd.score;
    g_game.has_won = (sd.has_won != 0);
    g_game.keep_playing = (sd.keep_playing != 0);
}

void Save_SaveProgress(void) {
    Save_SaveBest(); // 同一文件，包含进度
}
