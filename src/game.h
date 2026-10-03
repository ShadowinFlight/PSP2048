#ifndef GAME_H
#define GAME_H

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psprtc.h>
#include <pspiofilemgr.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// 屏幕与缓冲
#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 272
#define BUFFER_WIDTH  512

// GU 颜色格式是 0xAABBGGRR（R 在低字节），而代码里的颜色常量沿用网页 2048
// 的 0xAARRGGBB 写法 —— 提交给 GU 前用 FIXC 交换 R/B 通道（米黄色系 R≈B
// 看不出来，但橙色 8 分块不换会变成蓝色）。
#define FIXC(c) (((c) & 0xFF00FF00u) | (((c) & 0xFFu) << 16) | (((c) >> 16) & 0xFFu))

// 真机 GE 用 DMA 读顶点，不经过 CPU 数据缓存 —— 顶点写入必须走非缓存别名，
// 否则真机读到旧数据（PPSSPP 不模拟缓存所以看不出来）。
#define UNCACHED(p) ((void*)((unsigned int)(p) | 0x40000000u))

// 棋盘尺寸
#define BOARD_SIZE 4
#define BOARD_CELLS (BOARD_SIZE * BOARD_SIZE)

// 游戏状态
typedef enum {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_WIN,
    STATE_GAMEOVER
} GameState;

// 2048 游戏核心数据
typedef struct {
    int grid[BOARD_SIZE][BOARD_SIZE]; // 4x4 棋盘，0 表示空
    int score;                         // 当前得分
    int best_score;                    // 最高分
    bool has_won;                      // 是否出现过 2048
    bool keep_playing;                 // 胜利后继续游戏
    GameState state;
    SceCtrlData pad;

    // 连击系统
    int  combo_streak;   // 连续多少回合出现连击
    int  combo_count;    // 最近一次移动内的合成数
    int  combo_base;     // 最近一次连击的基础分
    int  last_bonus;     // 最近一次连击奖励分
    int  max_combo;      // 本局历史最高连击数
} Game;

// 全局游戏实例和 GU 命令列表
extern Game g_game;
extern unsigned int list[524288];

// ============ main.c ============
int  SetupCallbacks(void);
void InitGU(void);
void Game_Init(void);

// ============ board.c ============
void Board_Reset(void);
void Board_AddRandomTile(void);
bool Board_Move(int direction);    // 0:left 1:right 2:up 3:down，返回是否有变化
bool Board_CanMove(void);          // 是否还有可移动空间
int  Board_GetMaxTile(void);
void Board_HandleInput(unsigned int pressed_buttons); // 根据边沿触发的按键执行移动

// ============ render.c ============
void Render_BeginFrame(void);
void Render_EndFrame(void);
void Render_DrawRect(float x, float y, float w, float h, unsigned int color);
void Render_DrawText(float x, float y, const char *text, unsigned int color);
void Render_DrawBoard(void);
void Render_DrawHeader(void);
void Render_DrawFooter(void);
void Render_DrawTitle(void);
void Render_DrawPaused(void);
void Render_DrawWin(void);
void Render_DrawGameOver(void);

// ============ save.c ============
void Save_LoadBest(void);
void Save_SaveBest(void);
void Save_LoadProgress(void);
void Save_SaveProgress(void);

#endif // GAME_H
