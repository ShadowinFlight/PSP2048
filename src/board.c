/*
 * board.c - 2048 核心游戏逻辑
 */
#include "game.h"

// 方向常量
#define DIR_LEFT  0
#define DIR_RIGHT 1
#define DIR_UP    2
#define DIR_DOWN  3

// ---------- 辅助：单行向左滑动并合并 ----------
// 将长度为 4 的数组 line 向左滑动合并，返回：合并数（0/1/2）
// *score_out = 本次合并获得的基础分数
static int slide_left(int line[4], int *score_out) {
    int score = 0;
    int merges = 0;

    // 1. 压缩：移除所有 0
    int compact[4] = {0};
    int j = 0;
    for (int i = 0; i < 4; i++) {
        if (line[i] != 0) compact[j++] = line[i];
    }

    // 2. 相邻合并（每个方块在一次移动中只能合并一次）
    bool did_merge[4] = {false, false, false, false};
    for (int i = 0; i < 3; i++) {
        if (compact[i] != 0 && compact[i] == compact[i + 1] && !did_merge[i]) {
            compact[i] *= 2;
            score += compact[i];
            merges++;
            did_merge[i] = true;
            compact[i + 1] = 0;
        }
    }

    // 3. 再次压缩
    int result[4] = {0};
    j = 0;
    for (int i = 0; i < 4; i++) {
        if (compact[i] != 0) result[j++] = compact[i];
    }

    // 写回原数组
    for (int i = 0; i < 4; i++) line[i] = result[i];

    *score_out = score;
    return merges;
}

// ---------- 辅助：旋转棋盘（顺时针 90°） ----------
static void rotate_cw(int grid[BOARD_SIZE][BOARD_SIZE]) {
    int tmp[BOARD_SIZE][BOARD_SIZE];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            tmp[c][3 - r] = grid[r][c];
        }
    }
    memcpy(grid, tmp, sizeof(tmp));
}

// ---------- board 重置 ----------
// 注意：不要在 memset/memcpy 之后立刻写 g_game 成员
//       -O2 优化下编译器假设 memset 返回第一个参数（C 标准），直接用 v0+偏移访问
//       - PPSSPP 的 memset HLE 没正确设置 v0，导致写 NULL+0x40 崩溃
//       解决方案：手写循环清 grid，彻底不碰 v0
void Board_Reset(void) {
    Game *g = &g_game;
    // 手写循环，不用 memset（避免编译器假设 v0 = g_game 基址）
    for (int i = 0; i < 16; i++) g->grid[i / 4][i % 4] = 0;
    g->score = 0;
    g->has_won = false;
    g->keep_playing = false;
    g->combo_streak = 0;
    g->combo_count = 0;
    g->combo_base = 0;
    g->last_bonus = 0;
    g->max_combo = 0;

    Board_AddRandomTile();
    Board_AddRandomTile();
}

// ---------- 随机添加一个方块 ----------
void Board_AddRandomTile(void) {
    // 先收集所有空位
    int empties[16];
    int count = 0;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (g_game.grid[r][c] == 0) empties[count++] = r * 4 + c;
        }
    }
    if (count == 0) return;

    int idx = empties[rand() % count];
    g_game.grid[idx / 4][idx % 4] = (rand() % 10 < 9) ? 2 : 4;
}

// ---------- 执行一次移动 ----------
bool Board_Move(int direction) {
    int grid[4][4];
    for (int i = 0; i < 16; i++) grid[i / 4][i % 4] = g_game.grid[i / 4][i % 4];

    int rot;
    switch (direction) {
        case DIR_LEFT:  rot = 0; break;
        case DIR_DOWN:  rot = 1; break;
        case DIR_RIGHT: rot = 2; break;
        case DIR_UP:    rot = 3; break;
        default: rot = 0; break;
    }
    for (int i = 0; i < rot; i++) rotate_cw(grid);

    // 对每一行执行 left-slide，统计总合并数和基础分
    int total_merges = 0;       // 本次移动内总共发生多少次合成
    int base_score = 0;         // 本次移动的基础分（合并值之和）
    for (int r = 0; r < 4; r++) {
        int row_score = 0;
        int m = slide_left(grid[r], &row_score);
        total_merges += m;
        base_score += row_score;
    }

    // 旋转回去（顺时针 (4-rot) 次 = 逆时针 rot 次）
    int back = (4 - rot) % 4;
    for (int i = 0; i < back; i++) rotate_cw(grid);

    // 比较：与原棋盘是否相同
    bool changed = false;
    for (int r = 0; r < 4 && !changed; r++)
        for (int c = 0; c < 4 && !changed; c++)
            if (grid[r][c] != g_game.grid[r][c]) changed = true;

    if (changed) {
        Game *g = &g_game;
        // 不调 memcpy —— 避免编译器 -O2 假设返回值 = g_game 基址
        for (int i = 0; i < 16; i++) g->grid[i / 4][i % 4] = grid[i / 4][i % 4];
        g->score += base_score;

        // ====== 连击系统 ======
        g->combo_count = total_merges;
        g->combo_base = base_score;
        if (total_merges >= 1) {
            // 本次移动产生合成 → 连击 +1
            g->combo_streak++;
            if (g->combo_streak > g->max_combo) g->max_combo = g->combo_streak;
            // 奖励：基础分 × 合成数；连续 3 回合连击开始翻倍
            int bonus = base_score * total_merges;
            if (g->combo_streak >= 3) bonus *= 2;
            g->last_bonus = bonus;
            g->score += bonus;
        } else {
            // 本次无合成 → 重置连击
            g->combo_streak = 0;
            g->last_bonus = 0;
        }
        // =====================

        if (g->score > g->best_score) {
            g->best_score = g->score;
            Save_SaveBest();
        }

        // 检查是否达到 2048
        if (!g->has_won) {
            for (int r = 0; r < 4; r++)
                for (int c = 0; c < 4; c++)
                    if (g->grid[r][c] >= 2048) g->has_won = true;
        }

        Board_AddRandomTile();

        if (g_game.has_won && !g_game.keep_playing) {
            g_game.state = STATE_WIN;
        } else if (!Board_CanMove()) {
            g_game.state = STATE_GAMEOVER;
        }
    }
    return changed;
}

// ---------- 是否还能继续移动 ----------
bool Board_CanMove(void) {
    // 1. 还有空位
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (g_game.grid[r][c] == 0) return true;

    // 2. 存在可合并相邻对
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int v = g_game.grid[r][c];
            if (c + 1 < 4 && g_game.grid[r][c + 1] == v) return true;
            if (r + 1 < 4 && g_game.grid[r + 1][c] == v) return true;
        }
    }
    return false;
}

int Board_GetMaxTile(void) {
    int m = 0;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (g_game.grid[r][c] > m) m = g_game.grid[r][c];
    return m;
}

// ---------- 按键防抖处理 ----------
// 不再自己读 pad 或用 tick 防抖：由 main() 传边沿检测好的按键状态
// 这样 Title → Playing 状态切换同一帧不会重复触发 Board_Move
void Board_HandleInput(unsigned int pressed_buttons) {
    if (pressed_buttons & PSP_CTRL_LEFT)  { Board_Move(DIR_LEFT);  return; }
    if (pressed_buttons & PSP_CTRL_RIGHT) { Board_Move(DIR_RIGHT); return; }
    if (pressed_buttons & PSP_CTRL_UP)    { Board_Move(DIR_UP);    return; }
    if (pressed_buttons & PSP_CTRL_DOWN)  { Board_Move(DIR_DOWN);  return; }
}
