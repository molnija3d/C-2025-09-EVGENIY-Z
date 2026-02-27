#include "render.h"
#include <stdio.h>
#include <math.h>

// Цвета плиток (можно настроить)
static SDL_Color colors[] = {
    {205, 193, 180, 255}, // 0 (пустая)
    {238, 228, 218, 255}, // 2
    {237, 224, 200, 255}, // 4
    {242, 177, 121, 255}, // 8
    {245, 149, 99, 255},  // 16
    {246, 124, 95, 255},  // 32
    {246, 94, 59, 255},   // 64
    {237, 207, 114, 255}, // 128
    {237, 204, 97, 255},  // 256
    {237, 200, 80, 255},  // 512
    {237, 197, 63, 255},  // 1024
    {237, 194, 46, 255},  // 2048
    // выше 2048 можно сделать один цвет
    {60, 58, 50, 255}     // >2048
};

// Вспомогательная функция: получить индекс цвета по значению плитки
static int colorIndex(int value) {
    if (value == 0) return 0;
    int index = (int)log2(value); // 2->1, 4->2, 8->3, ...
    if (index >= (int) (sizeof(colors)/sizeof(colors[0]))) {
        index = (int) (sizeof(colors)/sizeof(colors[0])) - 1;
    }
    return index;
}

bool renderInit(RenderContext* ctx, SDL_Window* window) {
    ctx->renderer = SDL_GetRenderer(window);
    if (!ctx->renderer) {
        // Если рендерер не был создан, создадим его
        ctx->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!ctx->renderer) {
            printf("Failed to create renderer: %s\n", SDL_GetError());
            return false;
        }
    }
    // Инициализация SDL_ttf
    if (TTF_Init() < 0) {
        printf("TTF_Init error: %s\n", TTF_GetError());
        return false;
    }
    // Загрузка шрифта (путь к файлу нужно будет указать)
    // Пока используем стандартный шрифт, но лучше положить .ttf в assets/fonts/
    ctx->font = TTF_OpenFont("assets/fonts/LiberationSans-Bold.ttf", 48);
    if (!ctx->font) {
        printf("ERROR: No font \"LiberationSans-Bold.ttf\" in assets/fonts\r\n");
        ctx->font = TTF_OpenFont("/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf", 48);
        if (!ctx->font) {
            // Попробуем другой распространённый путь
            ctx->font = TTF_OpenFont("/usr/share/fonts/liberation/LiberationSans-Bold.ttf", 48);
            if (!ctx->font) {
                // Попробуем еще один распространённый путь
                ctx->font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 48);
                if (!ctx->font) {
                    printf("Failed to load font: %s\n", TTF_GetError());
                    // Можно продолжить без шрифта, но тогда не будут отображаться цифры
                    // или можно создать fallback через поверхности
                }
            }
        }
    }
    SDL_GetWindowSize(window, &ctx->windowWidth, &ctx->windowHeight);
    return true;
}

void renderDestroy(RenderContext* ctx) {
    if (ctx->font) TTF_CloseFont(ctx->font);
    // Рендерер не уничтожаем, так как он принадлежит окну (или мы его создали)
    TTF_Quit();
}

// Отрисовка текста в центре прямоугольника
static void renderTextCentered(SDL_Renderer* renderer, TTF_Font* font, const char* text, SDL_Rect rect, SDL_Color color) {
    if (!font) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;

    int texW, texH;
    SDL_QueryTexture(tex, NULL, NULL, &texW, &texH);
    SDL_Rect dst = {
        rect.x + (rect.w - texW) / 2,
        rect.y + (rect.h - texH) / 2,
        texW, texH
    };
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

void renderGame(const RenderContext* ctx, const GameState* state) {
    SDL_Renderer* ren = ctx->renderer;
    int w = ctx->windowWidth;
    int h = ctx->windowHeight;

    // Заливка фона
    SDL_SetRenderDrawColor(ren, 250, 248, 239, 255);
    SDL_RenderClear(ren);

    // Параметры сетки
    int padding = 20; // отступ от краёв окна
    int spacing = 10; // промежуток между плитками
    int boardSize = (w < h ? w : h) - 2 * padding;
    int cellSize = (boardSize - (SIZE - 1) * spacing) / SIZE;
    int boardX = (w - (cellSize * SIZE + spacing * (SIZE - 1))) / 2;
    int boardY = (h - (cellSize * SIZE + spacing * (SIZE - 1))) / 2;

    // Рисуем плитки
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            int value = state->board[i][j];
            SDL_Rect tileRect = {
                boardX + j * (cellSize + spacing),
                boardY + i * (cellSize + spacing),
                cellSize,
                cellSize
            };
            // Выбираем цвет плитки
            SDL_Color bgColor = colors[colorIndex(value)];
            SDL_SetRenderDrawColor(ren, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
            SDL_RenderFillRect(ren, &tileRect);

            // Рисуем рамку (опционально)
            SDL_SetRenderDrawColor(ren, 187, 173, 160, 255);
            SDL_RenderDrawRect(ren, &tileRect);

            // Если значение не 0, отображаем число
            if (value > 0) {
                char text[16];
                snprintf(text, sizeof(text), "%d", value);
                // Цвет текста: тёмный для светлых плиток, светлый для тёмных
                SDL_Color textColor = (value <= 4) ? (SDL_Color) {
                    119, 110, 101, 255
} :
                (SDL_Color) {
                    249, 246, 242, 255
                };
                renderTextCentered(ren, ctx->font, text, tileRect, textColor);
            }
        }
    }

    // Отображение счёта (вверху или внизу)
    char scoreText[32];
    snprintf(scoreText, sizeof(scoreText), "Score: %d", state->score);
    SDL_Color scoreColor = {119, 110, 101, 255};
    // Создаём временную текстуру для счёта и размещаем вверху справа
    if (ctx->font) {
        SDL_Surface* surf = TTF_RenderUTF8_Blended(ctx->font, scoreText, scoreColor);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
            SDL_Rect dst = { w - surf->w - 20, 20, surf->w, surf->h };
            SDL_RenderCopy(ren, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
    }

    // Обновляем экран
    SDL_RenderPresent(ren);
}

