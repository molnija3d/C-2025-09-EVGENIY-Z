#include "leaderboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

/* Получаем путь к домашней директории пользователя */
static const char* getHomeDir(void) {
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    return home ? home : ".";
}

const char* getLeaderboardPath(void) {
    static char path[512];
    const char* home = getHomeDir();
    snprintf(path, sizeof(path), "%s/.local/share/2048/scores.txt", home);
    return path;
}

/* Создаёт директорию, если её нет */
static void ensureDirExists(const char* path) {
    char* dir = strdup(path);
    char* lastSlash = strrchr(dir, '/');
    if (lastSlash) {
        *lastSlash = '\0';
        mkdir(dir, 0755); /* создаём директорию, если её нет */
    }
    free(dir);
}

int loadLeaderboard(LeaderboardEntry* entries, int maxEntries) {
    const char* path = getLeaderboardPath();
    FILE* f = fopen(path, "r");
    if (!f) return 0; /* файла нет – пустая таблица */

    int count = 0;
    char line[256];
    while (fgets(line, sizeof(line), f) && count < maxEntries) {
        /* Ожидаем формат: "Имя число" */
        char name[NAME_LENGTH];
        int score;
        if (sscanf(line, "%19s %d", name, &score) == 2) {
            strncpy(entries[count].name, name, NAME_LENGTH - 1);
            entries[count].name[NAME_LENGTH - 1] = '\0';
            entries[count].score = score;
            count++;
        }
    }
    fclose(f);
    return count;
}

void saveLeaderboard(const LeaderboardEntry* entries, int count) {
    const char* path = getLeaderboardPath();
    ensureDirExists(path); /* убедимся, что директория существует */

    FILE* f = fopen(path, "w");
    if (!f) return;

    for (int i = 0; i < count; i++) {
        fprintf(f, "%s %d\n", entries[i].name, entries[i].score);
    }
    fclose(f);
}

/* Сортировка по убыванию счёта */
static int compareEntries(const void* a, const void* b) {
    const LeaderboardEntry* ea = (const LeaderboardEntry*)a;
    const LeaderboardEntry* eb = (const LeaderboardEntry*)b;
    return eb->score - ea->score;
}

bool addLeaderboardEntry(LeaderboardEntry* entries, int* count, const char* name, int score) {
    if (!isHighScore(entries, *count, score)) return false;

    /* Если таблица заполнена, обрезаем до 9 (переписываем последнюю запись) */
    if (*count >= MAX_LEADERBOARD) {
        *count = MAX_LEADERBOARD - 1;
    }
    /* Добавляем новую запись */
    strncpy(entries[*count].name, name, NAME_LENGTH - 1);
    entries[*count].name[NAME_LENGTH - 1] = '\0';
    entries[*count].score = score;
    (*count)++;
    /* Сортируем */
    qsort(entries, *count, sizeof(LeaderboardEntry), compareEntries);

    return true;
}

bool isHighScore(const LeaderboardEntry* entries, int count, int score) {
    if (count < MAX_LEADERBOARD) return true; /* есть свободное место */
    /* Проверяем, больше ли счёт минимального в таблице */
    int minScore = entries[count - 1].score; /* после сортировки последний - минимальный */
    return score > minScore;
}
