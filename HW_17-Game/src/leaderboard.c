#include "leaderboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <libgen.h>

static char config_path[512] = {0};

// Вспомогательная функция для получения пути к файлу с рекордами
static const char* getLeaderboardPath() {
    if (config_path[0] == '\0') {
        const char *homedir;
        if ((homedir = getenv("HOME")) == NULL) {
            homedir = getpwuid(getuid())->pw_dir;
        }
        snprintf(config_path, sizeof(config_path), "%s/.local/share/2048", homedir);
        // Создаём директорию, если её нет
        mkdir(config_path, 0755); // может не создаться, но мы проверим позже
        strncat(config_path, "/scores.txt", sizeof(config_path) - strlen(config_path) - 1);
    }
    return config_path;
}

void leaderboardInit() {
    // Создаём директорию при необходимости
    const char* path = getLeaderboardPath();
    char *dir = strdup(path);
    if (dir) {
        char *dname = dirname(dir);
        mkdir(dname, 0755); // игнорируем ошибки
        free(dir);
    }
}

int leaderboardLoad(LeaderboardEntry entries[MAX_ENTRIES]) {
    const char* path = getLeaderboardPath();
    FILE* f = fopen(path, "r");
    if (!f) return 0;

    int count = 0;
    char line[256];
    while (count < MAX_ENTRIES && fgets(line, sizeof(line), f)) {
        // Удаляем возможный \n в конце
        line[strcspn(line, "\n")] = 0;
        // Формат: "Имя счёт"
        char* space = strrchr(line, ' ');
        if (space) {
            *space = '\0';
            strncpy(entries[count].name, line, MAX_NAME_LEN);
            entries[count].name[MAX_NAME_LEN] = '\0';
            entries[count].score = atoi(space + 1);
            count++;
        }
    }
    fclose(f);
    return count;
}

void leaderboardSave(const LeaderboardEntry entries[MAX_ENTRIES], int count) {
    const char* path = getLeaderboardPath();
    FILE* f = fopen(path, "w");
    if (!f) return;

    for (int i = 0; i < count; i++) {
        fprintf(f, "%s %d\n", entries[i].name, entries[i].score);
    }
    fclose(f);
}

bool leaderboardIsHighScore(int score) {
    LeaderboardEntry entries[MAX_ENTRIES];
    int count = leaderboardLoad(entries);

    // Если меньше MAX_ENTRIES, то любой счёт подходит (кроме 0, но 0 не должен быть)
    if (count < MAX_ENTRIES) return score > 0;

    // Иначе нужно быть больше минимального счёта в таблице
    int minScore = entries[count-1].score; // последний (наименьший)
    return score > minScore;
}

int leaderboardAddEntry(const char* name, int score) {
    if (!leaderboardIsHighScore(score)) return -1;

    LeaderboardEntry entries[MAX_ENTRIES];
    int count = leaderboardLoad(entries);

    // Добавляем новую запись
    LeaderboardEntry newEntry;
    strncpy(newEntry.name, name, MAX_NAME_LEN);
    newEntry.name[MAX_NAME_LEN] = '\0';
    newEntry.score = score;

    // Вставляем в нужное место (сортировка по убыванию)
    int pos = count; // позиция для вставки, если в конец
    for (int i = 0; i < count; i++) {
        if (score > entries[i].score) {
            pos = i;
            break;
        }
    }

    // Сдвигаем элементы вправо, если нужно
    if (pos < count) {
        for (int i = count; i > pos; i--) {
            if (i < MAX_ENTRIES) {
                entries[i] = entries[i-1];
            }
        }
    }

    // Вставляем
    if (pos < MAX_ENTRIES) {
        entries[pos] = newEntry;
        if (count < MAX_ENTRIES) count++;
    }

    // Сохраняем
    leaderboardSave(entries, count);
    return pos + 1; // место (1-индексированное)
}

int leaderboardGetTop(LeaderboardEntry entries[MAX_ENTRIES]) {
    return leaderboardLoad(entries);
}
