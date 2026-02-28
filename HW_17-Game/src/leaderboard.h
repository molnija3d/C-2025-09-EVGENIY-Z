#ifndef LEADERBOARD_H
#define LEADERBOARD_H

#include <stdbool.h>

#define MAX_LEADERBOARD 10
#define NAME_LENGTH 20

typedef struct {
    char name[NAME_LENGTH];
    int score;
} LeaderboardEntry;

// Возвращает путь к файлу с таблицей (статический буфер)
const char* getLeaderboardPath(void);

// Загружает таблицу из файла, возвращает количество записей
int loadLeaderboard(LeaderboardEntry* entries, int maxEntries);

// Сохраняет таблицу в файл
void saveLeaderboard(const LeaderboardEntry* entries, int count);

// Добавляет новую запись, если она достойна (сортировка и обрезка до 10)
// Возвращает true, если запись добавлена
bool addLeaderboardEntry(LeaderboardEntry* entries, int* count, const char* name, int score);

// Проверяет, попадает ли счёт в таблицу рекордов
bool isHighScore(const LeaderboardEntry* entries, int count, int score);

#endif
