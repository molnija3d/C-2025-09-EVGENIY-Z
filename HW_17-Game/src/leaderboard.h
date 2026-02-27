#ifndef LEADERBOARD_H
#define LEADERBOARD_H

#include <stdbool.h>

#define MAX_NAME_LEN 20
#define MAX_ENTRIES 10

typedef struct {
    char name[MAX_NAME_LEN + 1];
    int score;
} LeaderboardEntry;

// Инициализация: создаёт папку для сохранения, загружает записи
void leaderboardInit();

// Загружает записи из файла в предоставленный массив
// Возвращает количество загруженных записей (до MAX_ENTRIES)
int leaderboardLoad(LeaderboardEntry entries[MAX_ENTRIES]);

// Сохраняет массив записей (количество count) в файл
void leaderboardSave(const LeaderboardEntry entries[MAX_ENTRIES], int count);

// Проверяет, достоин ли счёт попасть в таблицу (больше минимального или есть место)
bool leaderboardIsHighScore(int score);

// Добавляет новую запись (имя, счёт) в таблицу, сортирует и сохраняет
// Возвращает место, на которое попала запись (1-10) или -1, если не попала
int leaderboardAddEntry(const char* name, int score);

// Получает текущую таблицу для отображения (заполняет массив, возвращает количество)
int leaderboardGetTop(LeaderboardEntry entries[MAX_ENTRIES]);

#endif
