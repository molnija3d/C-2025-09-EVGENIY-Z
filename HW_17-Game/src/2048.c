#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "game.h"
#include "render.h"
#include "audio.h"

int main(int __attribute__((unused)) argc,  __attribute__((unused)) char*  argv[]) {
    // Инициализация SDL (видео, аудио, события)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    // Инициализация аудио
    AudioContext audio;
    if (!audioInit(&audio)) {
        printf("Warning: audio initialization failed, continuing without sound.\n");
        // Но мы всё равно можем продолжать, звука не будет
    }

    SDL_Window* window = SDL_CreateWindow("My super 2048. ESC -выход, m -выключить звук, r -рестарт",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          600, 600,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        printf("Window creation error: %s\n", SDL_GetError());
        audioDestroy(&audio);
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer creation error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        audioDestroy(&audio);
        SDL_Quit();
        return 1;
    }

    RenderContext renderCtx;
    if (!renderInit(&renderCtx, window)) {
        printf("Warning: renderInit failed, continuing without font.\n");
    }

    GameState game;
    gameInit(&game);



    // Запускаем фоновую музыку, если есть
    audioPlayMusic(&audio);

    bool running = true;
    SDL_Event event;
    bool movePerformed;
    // главный цикл
    while (running) {
        movePerformed = false;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_UP:
                    movePerformed = gameMoveUp(&game);
                    break;
                case SDLK_DOWN:
                    movePerformed = gameMoveDown(&game);
                    break;
                case SDLK_LEFT:
                    movePerformed = gameMoveLeft(&game);
                    break;
                case SDLK_RIGHT:
                    movePerformed = gameMoveRight(&game);
                    break;
                case SDLK_r:
                    gameInit(&game);
                    // Запускаем фоновую музыку
                    audioPlayMusic(&audio);
                    movePerformed = true;
                    break;
                case SDLK_m:     // клавиша M для включения/выключения звука
                    audioToggle(&audio);
                    break;
                case SDLK_ESCAPE:
                    running = false;
                    break;
                }
                // Если был произведён ход, проигрываем звук
                if (movePerformed) {
                    audioPlayMove(&audio);
                }
                // Проверяем, не наступила ли победа/поражение, и проигрываем соответствующий звук
                if (game.win) {
                    audioPlayWin(&audio);
                }
            } else if (game.gameOver) {
                audioPlayGameOver(&audio);
                audioStopMusic(&audio);
            }
            else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    renderCtx.windowWidth = event.window.data1;
                    renderCtx.windowHeight = event.window.data2;
                }
            }
        }

        renderGame(&renderCtx, &game);
        SDL_Delay(16);
    }

    renderDestroy(&renderCtx);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    audioDestroy(&audio);
    SDL_Quit();
    return 0;
}
