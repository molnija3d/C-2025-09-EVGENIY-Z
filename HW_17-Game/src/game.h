#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

#define SIZE 4
#define TARGET 2048

// Структура состояния игры
typedef struct {
    int board[SIZE][SIZE];
    int score;
    bool gameOver;
    bool win;
} GameState;

typedef enum {
    PHASE_MENU,
    PHASE_PLAY,
    PHASE_CONTROLS,
    PHASE_LEADERBOARD
} GamePhase;

// Инициализация нового игрового поля (две случайные плитки)
void gameInit(GameState* state);

// Добавление случайной плитки (2 с вероятностью 0.9, 4 с вероятностью 0.1)
bool gameAddRandomTile(GameState* state);

// Перемещение влево (основная функция, остальные через повороты)
bool gameMoveLeft(GameState* state);

// Перемещение вправо (через отражение)
bool gameMoveRight(GameState* state);

// Перемещение вверх
bool gameMoveUp(GameState* state);

// Перемещение вниз
bool gameMoveDown(GameState* state);

// Проверка, можно ли сделать ход (есть пустые клетки или соседние одинаковые)
bool gameCanMove(const GameState* state);

// Проверка победы (появилась плитка со значением TARGET)
bool gameCheckWin(const GameState* state);

// Проверка поражения (нет ходов)
bool gameCheckGameOver(const GameState* state);

// Вывод поля в консоль (для отладки)
void gamePrint(const GameState* state);

#endif
