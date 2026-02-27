#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// Инициализация генератора случайных чисел (вызывать один раз при запуске)
void gameInitRandom() {
    static bool seeded = false;
    if (!seeded) {
        srand(time(NULL));
        seeded = true;
    }
}

void gameInit(GameState* state) {
    gameInitRandom();
    memset(state->board, 0, sizeof(state->board));
    state->score = 0;
    state->gameOver = false;
    state->win = false;
    // Добавляем две начальные плитки
    gameAddRandomTile(state);
    gameAddRandomTile(state);
}

bool gameAddRandomTile(GameState* state) {
    // Подсчёт пустых клеток
    int empty = 0;
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            if (state->board[i][j] == 0)
                empty++;
    if (empty == 0) return false;

    // Выбираем случайную пустую клетку
    int target = rand() % empty;
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (state->board[i][j] == 0) {
                if (target == 0) {
                    // 90% - 2, 10% - 4
                    state->board[i][j] = (rand() % 10 < 9) ? 2 : 4;
                    return true;
                }
                target--;
            }
        }
    }
    return false; // не должно достигаться
}

// Вспомогательная функция для сдвига строки влево и объединения
// Возвращает true, если строка изменилась
static bool moveRowLeft(int row[SIZE], int* score) {
    int temp[SIZE] = {0};
    int pos = 0;
    bool changed = false;
    // Собираем ненулевые элементы
    for (int i = 0; i < SIZE; i++) {
        if (row[i] != 0) {
            temp[pos++] = row[i];
        }
    }
    // Объединяем соседние одинаковые
    for (int i = 0; i < pos - 1; i++) {
        if (temp[i] == temp[i + 1]) {
            temp[i] *= 2;
            *score += temp[i];
            // сдвигаем оставшиеся
            for (int j = i + 1; j < pos - 1; j++) {
                temp[j] = temp[j + 1];
            }
            temp[pos - 1] = 0;
            pos--;
            changed = true;
        }
    }
    // Сравниваем с исходной строкой
    for (int i = 0; i < SIZE; i++) {
        if (row[i] != temp[i]) {
            changed = true;
            row[i] = temp[i];
        }
    }
    return changed;
}

bool gameMoveLeft(GameState* state) {
    bool changed = false;
    for (int i = 0; i < SIZE; i++) {
        if (moveRowLeft(state->board[i], &state->score)) {
            changed = true;
        }
    }
    if (changed) {
        gameAddRandomTile(state);
        state->win = gameCheckWin(state);
        state->gameOver = gameCheckGameOver(state);
    }
    return changed;
}

// Для перемещения вправо используем отражение строки
static void reverseRow(int row[SIZE]) {
    for (int i = 0; i < SIZE / 2; i++) {
        int tmp = row[i];
        row[i] = row[SIZE - 1 - i];
        row[SIZE - 1 - i] = tmp;
    }
}

bool gameMoveRight(GameState* state) {
    bool changed = false;
    for (int i = 0; i < SIZE; i++) {
        reverseRow(state->board[i]);
        if (moveRowLeft(state->board[i], &state->score)) {
            changed = true;
        }
        reverseRow(state->board[i]);
    }
    if (changed) {
        gameAddRandomTile(state);
        state->win = gameCheckWin(state);
        state->gameOver = gameCheckGameOver(state);
    }
    return changed;
}

// Для вертикальных перемещений транспонируем матрицу
static void transpose(int board[SIZE][SIZE]) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = i + 1; j < SIZE; j++) {
            int tmp = board[i][j];
            board[i][j] = board[j][i];
            board[j][i] = tmp;
        }
    }
}

bool gameMoveUp(GameState* state) {
    transpose(state->board);
    bool changed = gameMoveLeft(state);
    transpose(state->board);
    return changed;
}

bool gameMoveDown(GameState* state) {
    transpose(state->board);
    bool changed = gameMoveRight(state);
    transpose(state->board);
    return changed;
}

bool gameCanMove(const GameState* state) {
    // Если есть пустая клетка, ход возможен
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            if (state->board[i][j] == 0)
                return true;
    // Проверяем соседние клетки по горизонтали и вертикали
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE - 1; j++) {
            if (state->board[i][j] == state->board[i][j + 1])
                return true;
        }
    }
    for (int j = 0; j < SIZE; j++) {
        for (int i = 0; i < SIZE - 1; i++) {
            if (state->board[i][j] == state->board[i + 1][j])
                return true;
        }
    }
    return false;
}

bool gameCheckWin(const GameState* state) {
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            if (state->board[i][j] == TARGET)
                return true;
    return false;
}

bool gameCheckGameOver(const GameState* state) {
    return !gameCanMove(state);
}

void gamePrint(const GameState* state) {
    printf("Score: %d\n", state->score);
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (state->board[i][j] == 0)
                printf("%4c ", '.');
            else
                printf("%4d ", state->board[i][j]);
        }
        printf("\n");
    }
    if (state->win) printf("You win!\n");
    if (state->gameOver) printf("Game Over!\n");
}
