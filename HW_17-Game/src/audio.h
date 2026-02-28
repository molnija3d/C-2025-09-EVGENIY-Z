#ifndef AUDIO_H
#define AUDIO_H

#include <SDL2/SDL_mixer.h>
#include <stdbool.h>

/* Структура для хранения звуковых ресурсов */
typedef struct {
    Mix_Music* bgMusic;
    Mix_Chunk* soundMove;
    Mix_Chunk* soundWin;
    Mix_Chunk* soundGameOver;
    bool enabled;
    int volume; /* 0-128 */
} AudioContext;

/* Инициализация аудио (открытие устройства, загрузка файлов) */
bool audioInit(AudioContext* ctx);

/* Освобождение ресурсов */
void audioDestroy(AudioContext* ctx);

/* Включение/выключение звука */
void audioToggle(AudioContext* ctx);

/* Установка громкости (0-128) */
void audioSetVolume(AudioContext* ctx, int volume);

/* Проигрывание звука перемещения */
void audioPlayMove(AudioContext* ctx);

/* Проигрывание звука победы */
void audioPlayWin(AudioContext* ctx);

/* Проигрывание звука поражения */
void audioPlayGameOver(AudioContext* ctx);

/* Запуск фоновой музыки (зациклено) */
void audioPlayMusic(AudioContext* ctx);

/* Остановка музыки */
void audioStopMusic(AudioContext* ctx);

#endif
