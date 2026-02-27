#include <SDL2/SDL.h>
#include <stdio.h>

#include <stdio.h>
#include <stdlib.h>
#include "game.h"

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
