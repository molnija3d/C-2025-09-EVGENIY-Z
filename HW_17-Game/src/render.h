#ifndef RENDER_H
#define RENDER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "game.h"
#include "leaderboard.h"

// Структура для хранения ресурсов рендерера
typedef struct {
    SDL_Renderer* renderer;
    TTF_Font* font;
    int windowWidth;
    int windowHeight;
    // Можно добавить текстуры, если будем использовать
} RenderContext;

// Инициализация рендерера и загрузка шрифта
bool renderInit(RenderContext* ctx, SDL_Window* window);

// Освобождение ресурсов
void renderDestroy(RenderContext* ctx);

// Отрисовка всего игрового поля
void renderGame(const RenderContext* ctx, const GameState* state);

// Отрисовка таблицы лидеров
void renderLeaderboard(const RenderContext* ctx, const LeaderboardEntry entries[MAX_LEADERBOARD], int count);

void renderMenu(const RenderContext* ctx, const char* items[], int count, int selected);
void renderControls(const RenderContext* ctx);

#endif
