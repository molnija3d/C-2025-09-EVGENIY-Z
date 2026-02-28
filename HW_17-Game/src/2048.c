#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "game.h"
#include "render.h"
#include "audio.h"
#include "leaderboard.h"


int main(int __attribute__((unused)) argc,  __attribute__((unused)) char*  argv[]) {
    /* Инициализация SDL (видео, аудио, события) */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("Ошибка инициализации SDL: %s\n", SDL_GetError());
        return 1;
    }

    /* Инициализация аудио */
    AudioContext audio;
    if (!audioInit(&audio)) {
        printf("Предупреждение, аудио не инициализировано.\n");
    }

    SDL_Window* window = SDL_CreateWindow("2048 на Си",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          600, 600,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        printf("Ошибка создания окна: %s\n", SDL_GetError());
        audioDestroy(&audio);
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Ошибка создания renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        audioDestroy(&audio);
        SDL_Quit();
        return 1;
    }

    RenderContext renderCtx;
    if (!renderInit(&renderCtx, window)) {
        printf("Предупреждение: ошибка renderInit, работаем без шрифта.\n");
    }

    GameState game;
    gameInit(&game);

    /* Переменные для меню */
    GamePhase currentPhase = PHASE_MENU;
    int selectedMenuItem = 0;          /* 0: Новая игра, 1: Управление, 2: Таблица лидеров, 3: Выход */
    const char* menuItems[] = {"Новая игра", "Управление", "Таблица лидеров", "Выход"};
    int menuItemsCount = 4;

    /* Для таблицы лидеров */
    LeaderboardEntry leaderboard[MAX_LEADERBOARD];
    int leaderboardCount = 0;
    /* Загружаем таблицу лидеров при старте */
    leaderboardCount = loadLeaderboard(leaderboard, MAX_LEADERBOARD);

    /* Запускаем фоновую музыку, если есть */
    audioPlayMusic(&audio);

    bool running = true;
    SDL_Event event;

    /* главный цикл */
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (currentPhase) {
                case PHASE_MENU:
                    switch (event.key.keysym.sym) {
                    case SDLK_UP:
                        selectedMenuItem = (selectedMenuItem - 1 + menuItemsCount) % menuItemsCount;
                        audioPlayMove(&audio); /* звук перемещения */
                        break;
                    case SDLK_DOWN:
                        selectedMenuItem = (selectedMenuItem + 1) % menuItemsCount;
                        audioPlayMove(&audio);
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        audioPlayMove(&audio);
                        switch (selectedMenuItem) {
                        case 0: /* New Game */
                            gameInit(&game);
                            currentPhase = PHASE_PLAY;
                            break;
                        case 1: /* Controls */
                            currentPhase = PHASE_CONTROLS;
                            break;
                        case 2: /* Leaderboard */
                            /* Перезагружаем таблицу (на случай, если изменилась) */
                            leaderboardCount = loadLeaderboard(leaderboard, MAX_LEADERBOARD);
                            currentPhase = PHASE_LEADERBOARD;
                            break;
                        case 3: /* Выход */
                            running = false;
                            break;
                        }
                        break;
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    }
                    break;

                case PHASE_PLAY:
                {
                    bool movePerformed = false;
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
                        movePerformed = true;
                        break;
                    case SDLK_m:
                        audioToggle(&audio);
                        break;
                    case SDLK_ESCAPE:
                        currentPhase = PHASE_MENU;
                        break;
                    }
                    if (movePerformed) {
                        audioPlayMove(&audio);
                    }
                    if (game.win) {
                        audioPlayWin(&audio);
                        /* Проверяем рекорд */
                        if (addLeaderboardEntry(leaderboard, &leaderboardCount, "Игрок", game.score)) {
                            saveLeaderboard(leaderboard, leaderboardCount);
                            currentPhase = PHASE_LEADERBOARD;
                        }
                    } else if (game.gameOver) {
                        audioPlayGameOver(&audio);
                        if (addLeaderboardEntry(leaderboard, &leaderboardCount, "Игрок", game.score)) {
                            saveLeaderboard(leaderboard, leaderboardCount);
                        }
                        currentPhase = PHASE_LEADERBOARD;
                    }
                }
                break;

                case PHASE_CONTROLS:
                    currentPhase = PHASE_CONTROLS;
                    audioPlayMove(&audio);
                    /* проверяем нажатие ESC */
                    switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        currentPhase = PHASE_MENU;
                        break;
                    }
                    break;

                case PHASE_LEADERBOARD:
                    currentPhase = PHASE_LEADERBOARD;
                    audioPlayMove(&audio);
                    /* проверяем нажатие ESC */
                    switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        currentPhase = PHASE_MENU;
                        break;
                    }


                    break;
                }
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    renderCtx.windowWidth = event.window.data1;
                    renderCtx.windowHeight = event.window.data2;
                }
            }
        }

        /* Отрисовка в зависимости от фазы */
        switch (currentPhase) {
        case PHASE_MENU:
            renderMenu(&renderCtx, menuItems, menuItemsCount, selectedMenuItem);
            break;
        case PHASE_PLAY:
            renderGame(&renderCtx, &game);
            break;
        case PHASE_CONTROLS:
            renderControls(&renderCtx);
            break;
        case PHASE_LEADERBOARD:
            renderLeaderboard(&renderCtx, leaderboard, leaderboardCount);
            break;
        }

        SDL_Delay(16);
    }

    renderDestroy(&renderCtx);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    audioDestroy(&audio);
    SDL_Quit();
    return 0;
}
