#include "audio.h"
#include <stdio.h>

bool audioInit(AudioContext* ctx) {
    // Открываем аудиоустройство с параметрами по умолчанию
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("SDL_mixer could not initialize! Error: %s\n", Mix_GetError());
        return false;
    }

    // Устанавливаем количество каналов для одновременного воспроизведения
    Mix_AllocateChannels(16);

    // Загружаем музыку и звуки (пути к файлам нужно будет создать)
    // Пока оставим заглушки, позже добавим реальные файлы
    ctx->bgMusic = Mix_LoadMUS("assets/sounds/bg.ogg");
    ctx->soundMove = Mix_LoadWAV("assets/sounds/move.ogg");
    ctx->soundWin =  Mix_LoadWAV("assets/sounds/win.ogg");
    ctx->soundGameOver = Mix_LoadWAV("assets/sounds/gameover.ogg");

    // Если файлы не найдены, просто продолжаем без звуков (не фатально)
    if (!ctx->bgMusic) printf("Warning: could not load background music\n");
    if (!ctx->soundMove) printf("Warning: could not load move sound\n");
    if (!ctx->soundWin) printf("Warning: could not load win sound\n");
    if (!ctx->soundGameOver) printf("Warning: could not load gameover sound\n");

    ctx->enabled = true;
    ctx->volume = MIX_MAX_VOLUME; // 128
    Mix_VolumeMusic(ctx->volume);
    // Устанавливаем громкость для всех каналов
    Mix_Volume(-1, ctx->volume);

    return true;
}

void audioDestroy(AudioContext* ctx) {
    if (ctx->bgMusic) Mix_FreeMusic(ctx->bgMusic);
    if (ctx->soundMove) Mix_FreeChunk(ctx->soundMove);
    if (ctx->soundWin) Mix_FreeChunk(ctx->soundWin);
    if (ctx->soundGameOver) Mix_FreeChunk(ctx->soundGameOver);
    Mix_CloseAudio();
}

void audioToggle(AudioContext* ctx) {
    ctx->enabled = !ctx->enabled;
    if (!ctx->enabled) {
        Mix_HaltMusic();
        Mix_HaltChannel(-1);
    } else {
        // Если была остановлена музыка, можно возобновить
         audioPlayMusic(ctx);
    }
}

void audioSetVolume(AudioContext* ctx, int volume) {
    if (volume < 0) volume = 0;
    if (volume > MIX_MAX_VOLUME) volume = MIX_MAX_VOLUME;
    ctx->volume = volume;
    Mix_VolumeMusic(volume);
    Mix_Volume(-1, volume);
}

void audioPlayMove(AudioContext* ctx) {
    if (ctx->enabled && ctx->soundMove) {
        Mix_PlayChannel(-1, ctx->soundMove, 0);
    }
}

void audioPlayWin(AudioContext* ctx) {
    if (ctx->enabled && ctx->soundWin) {
        Mix_PlayChannel(-1, ctx->soundWin, 0);
    }
}

void audioPlayGameOver(AudioContext* ctx) {
    if (ctx->enabled && ctx->soundGameOver) {
        Mix_PlayChannel(-1, ctx->soundGameOver, 0);
    }
}

void audioPlayMusic(AudioContext* ctx) {
    if (ctx->enabled && ctx->bgMusic) {
        Mix_PlayMusic(ctx->bgMusic, -1); // зациклить
    }
}

void audioStopMusic( __attribute__((unused)) AudioContext* ctx) {
    Mix_HaltMusic();
}
