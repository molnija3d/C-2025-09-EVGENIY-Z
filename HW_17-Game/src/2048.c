#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "game.h"
#include "render.h"
#include "audio.h" 

int main(int argc, char* argv[]) {
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

    SDL_Window* window = SDL_CreateWindow("2048",
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

    while (running) {
        movePerformed = false;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP:    movePerformed = gameMoveUp(&game); break;
                    case SDLK_DOWN:  movePerformed = gameMoveDown(&game); break;
                    case SDLK_LEFT:  movePerformed = gameMoveLeft(&game); break;
                    case SDLK_RIGHT: movePerformed = gameMoveRight(&game); break;
                    case SDLK_r:     
                        gameInit(&game); 
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
                } else if (game.gameOver) {
                    audioPlayGameOver(&audio);
                }
            } else if (event.type == SDL_WINDOWEVENT) {
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



/*
int main(int argc, char* argv[]) {
    // Инициализация SDL (видео и события)
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("2048",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          600, 600,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        printf("Window creation error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer creation error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Инициализация контекста рендеринга
    RenderContext renderCtx;
    if (!renderInit(&renderCtx, window)) {
        // Если шрифт не загружен, продолжаем работу (но без цифр)
        printf("Warning: renderInit failed, continuing without font.\n");
    }

    // Инициализация игрового состояния
    GameState game;
    gameInit(&game);

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                // Обработка клавиш
                bool moved = false;
                switch (event.key.keysym.sym) {
                    case SDLK_UP:    moved = gameMoveUp(&game); break;
                    case SDLK_DOWN:  moved = gameMoveDown(&game); break;
                    case SDLK_LEFT:  moved = gameMoveLeft(&game); break;
                    case SDLK_RIGHT: moved = gameMoveRight(&game); break;
                    case SDLK_r:     gameInit(&game); moved = true; break; // рестарт
                    case SDLK_ESCAPE: running = false; break;
                }
                // Здесь можно добавить звук, если moved == true
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    // Обновляем размеры в контексте рендера
                    renderCtx.windowWidth = event.window.data1;
                    renderCtx.windowHeight = event.window.data2;
                }
            }
        }

        // Отрисовка
        renderGame(&renderCtx, &game);

        // Небольшая задержка для снижения нагрузки на CPU
        SDL_Delay(16);
    }

    renderDestroy(&renderCtx);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
*/


/*
int main() {
    GameState game;
    gameInit(&game);

    char input;
    while (!game.gameOver && !game.win) {
        gamePrint(&game);
        printf("\nEnter move (w/a/s/d) or q to quit: ");
        scanf(" %c", &input);
        switch (input) {
            case 'w': gameMoveUp(&game); break;
            case 's': gameMoveDown(&game); break;
            case 'a': gameMoveLeft(&game); break;
            case 'd': gameMoveRight(&game); break;
            case 'q': exit(0);
            default: printf("Invalid input\n");
        }
    }
    gamePrint(&game);
    printf("Game finished.\n");
    return 0;
}
*/
/*
int main(int argc, char* argv[]) {
    // Инициализация SDL с видео-подсистемой
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    // Создание окна
    SDL_Window* window = SDL_CreateWindow(
        "My super-puper 2048",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,          // ширина, высота
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Создание рендерера (для последующей графики)
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Главный цикл
    int running = 1;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        // Заливка фона цветом (для проверки)
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);

        // Небольшая задержка, чтобы не грузить процессор
        SDL_Delay(10);
    }

    // Очистка
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

*/
