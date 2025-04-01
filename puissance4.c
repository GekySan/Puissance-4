/*
 * Compilation
 *
 * Dynamique
 * vcpkg install sdl3
 * cl /std:c17 /utf-8 puissance4.c /I"PATH VCPKG\vcpkg\installed\x64-windows\include" /Puissance4.exe /link /LIBPATH:"PATH VCPKG\vcpkg\installed\x64-windows\lib" SDL3.lib
 *
 * Statique
 * vcpkg install sdl3:x64-windows-static
 * cl /std:c17 /utf-8 puissance4.c /I"PATH VCPKG\vcpkg\installed\x64-windows-static\include" /DSDL_STATIC /Puissance4.exe /link /LIBPATH:"PATH VCPKG\vcpkg\installed\x64-windows-static\lib" SDL3-static.lib user32.lib gdi32.lib winmm.lib ole32.lib advapi32.lib shell32.lib setupapi.lib cfgmgr32.lib oleaut32.lib version.lib imm32.lib
*/

#define SDL_MAIN_HANDLED

#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include <SDL3/SDL.h>

#define ROWS         6
#define COLS         7

static const uint8_t EMPTY          = 0;  // Case vide
static const uint8_t PLAYER_PIECE   = 1;  // Joueur (JAUNE)
static const uint8_t AI_PIECE       = 2;  // Ordi "IA" (ROUGE)

#define AI_DEPTH     4  // Profondeur de recherche Minimax

// Dimensions pour l’affichage
#define SQUARE_SIZE  100.0f
#define RADIUS       (SQUARE_SIZE / 2 - 5)

// Couleurs en RGB(A)
static SDL_Color BLUE   = { 0,   0,   255, 255 };
static SDL_Color BLACK  = { 0,   0,   0,   255 };
static SDL_Color RED    = { 255, 0,   0,   255 };
static SDL_Color YELLOW = { 255, 255, 0,   255 };
static SDL_Color WHITE  = { 255, 255, 255, 255 };

// Taille de la fenêtre : (COLS * SQUARE_SIZE) x ((ROWS+1) * SQUARE_SIZE)
static const float WINDOW_WIDTH = COLS * SQUARE_SIZE;
static const float WINDOW_HEIGHT = (ROWS + 1) * SQUARE_SIZE;

// 60 FPS
static const float FRAME_DELAY_F = 1000.0f / 60.0f;

typedef struct {
    uint8_t board[ROWS][COLS];  // Plateau de jeu (6x7)
    bool    gameOver;
    bool    turn;               // false = Joueur, true = Ordi
} GameState;

// Structure pour animer la chute d’un pion.

typedef struct {
    float   x, y;
    float   vy, ay;
    bool    active;
    uint8_t piece;
    uint8_t finalRow;
    uint8_t finalCol;
} FallingPiece;


// Efface l’écran avec la couleur voulue
static void clearScreen(SDL_Renderer* renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer);
}

// Dessine un rectangle plein
static void drawFilledRect(SDL_Renderer* renderer, float x, float y, float w, float h, SDL_Color color)
{
    SDL_FRect rect = { x, y, w, h };
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

// Dessine un pixel
static void putPixel(SDL_Renderer* renderer, float x, float y, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderPoint(renderer, x, y);
}

// Dessine un cercle plein
static void drawFilledCircle(SDL_Renderer* renderer, float cx, float cy, float radius, SDL_Color color)
{
    for (int dy = (int)(-radius); dy <= (int)radius; dy++)
    {
        float dxFloat = sqrtf(radius * radius - (float)(dy * dy));

        int xMin = (int)ceilf(-dxFloat);
        int xMax = (int)floorf(dxFloat);

        int realY = (int)(cy + dy);

        for (int x = xMin; x <= xMax; x++)
        {
            int realX = (int)(cx + x);
            putPixel(renderer, realX, realY, color);
        }
    }
}

static void initBoard(GameState* game)
{
    for (uint8_t r = 0; r < ROWS; r++) {
        for (uint8_t c = 0; c < COLS; c++) {
            game->board[r][c] = EMPTY;
        }
    }
    game->gameOver = false;
    game->turn = false;
}

static bool isValidLocation(const GameState* game, uint8_t col)
{
    return (col < COLS && game->board[ROWS - 1][col] == EMPTY);
}

static uint8_t getNextOpenRow(const GameState* game, uint8_t col)
{
    for (uint8_t r = 0; r < ROWS; r++) {
        if (game->board[r][col] == EMPTY) {
            return r;
        }
    }
    return 0;
}

static void dropPieceOnBoard(GameState* game, uint8_t row, uint8_t col, uint8_t piece)
{
    game->board[row][col] = piece;
}

static bool winningMove(const GameState* game, uint8_t piece)
{
    for (uint8_t r = 0; r < ROWS; r++) {
        for (uint8_t c = 0; c < COLS - 3; c++) {
            if (game->board[r][c] == piece &&
                game->board[r][c + 1] == piece &&
                game->board[r][c + 2] == piece &&
                game->board[r][c + 3] == piece) {
                return true;
            }
        }
    }
    for (uint8_t c = 0; c < COLS; c++) {
        for (uint8_t r = 0; r < ROWS - 3; r++) {
            if (game->board[r][c] == piece &&
                game->board[r + 1][c] == piece &&
                game->board[r + 2][c] == piece &&
                game->board[r + 3][c] == piece) {
                return true;
            }
        }
    }
    for (uint8_t r = 0; r < ROWS - 3; r++) {
        for (uint8_t c = 0; c < COLS - 3; c++) {
            if (game->board[r][c] == piece &&
                game->board[r + 1][c + 1] == piece &&
                game->board[r + 2][c + 2] == piece &&
                game->board[r + 3][c + 3] == piece) {
                return true;
            }
        }
    }
    for (uint8_t r = 3; r < ROWS; r++) {
        for (uint8_t c = 0; c < COLS - 3; c++) {
            if (game->board[r][c] == piece &&
                game->board[r - 1][c + 1] == piece &&
                game->board[r - 2][c + 2] == piece &&
                game->board[r - 3][c + 3] == piece) {
                return true;
            }
        }
    }
    return false;
}

static bool isTerminalNode(const GameState* game)
{
    if (winningMove(game, PLAYER_PIECE)) return true;
    if (winningMove(game, AI_PIECE))     return true;

    for (uint8_t c = 0; c < COLS; c++) {
        if (isValidLocation(game, c)) {
            return false;
        }
    }
    return true;
}

static int evaluateWindow(const uint8_t window[4], uint8_t piece)
{
    int score = 0;
    uint8_t opp = (piece == PLAYER_PIECE) ? AI_PIECE : PLAYER_PIECE;

    int countPiece = 0, countOpp = 0, countEmpty = 0;
    for (int i = 0; i < 4; i++) {
        if (window[i] == piece) {
            countPiece++;
        }
        else if (window[i] == opp) {
            countOpp++;
        }
        else if (window[i] == EMPTY) {
            countEmpty++;
        }
    }

    if (countPiece == 4) {
        score += 100;
    }
    else if (countPiece == 3 && countEmpty == 1) {
        score += 5;
    }
    else if (countPiece == 2 && countEmpty == 2) {
        score += 2;
    }

    if (countOpp == 3 && countEmpty == 1) {
        score -= 4;
    }
    return score;
}

static int scorePosition(const GameState* game, uint8_t piece)
{
    int score = 0;

    // Bonus pour la colonne centrale
    uint8_t center = COLS / 2;
    int centerCount = 0;
    for (uint8_t r = 0; r < ROWS; r++) {
        if (game->board[r][center] == piece) {
            centerCount++;
        }
    }
    score += centerCount * 3;

    // Analyse horizontale
    for (uint8_t r = 0; r < ROWS; r++) {
        for (uint8_t c = 0; c < COLS - 3; c++) {
            uint8_t window[4] = {
                game->board[r][c],
                game->board[r][c + 1],
                game->board[r][c + 2],
                game->board[r][c + 3]
            };
            score += evaluateWindow(window, piece);
        }
    }
    // Analyse verticale
    for (uint8_t c = 0; c < COLS; c++) {
        for (uint8_t r = 0; r < ROWS - 3; r++) {
            uint8_t window[4] = {
                game->board[r][c],
                game->board[r + 1][c],
                game->board[r + 2][c],
                game->board[r + 3][c]
            };
            score += evaluateWindow(window, piece);
        }
    }
    // Diagonales ↗
    for (uint8_t r = 0; r < ROWS - 3; r++) {
        for (uint8_t c = 0; c < COLS - 3; c++) {
            uint8_t window[4] = {
                game->board[r][c],
                game->board[r + 1][c + 1],
                game->board[r + 2][c + 2],
                game->board[r + 3][c + 3]
            };
            score += evaluateWindow(window, piece);
        }
    }
    // Diagonales ↘
    for (uint8_t r = 3; r < ROWS; r++) {
        for (uint8_t c = 0; c < COLS - 3; c++) {
            uint8_t window[4] = {
                game->board[r][c],
                game->board[r - 1][c + 1],
                game->board[r - 2][c + 2],
                game->board[r - 3][c + 3]
            };
            score += evaluateWindow(window, piece);
        }
    }

    return score;
}

static GameState* cloneGame(const GameState* game)
{
    GameState* clonePtr = (GameState*)malloc(sizeof(GameState));
    if (!clonePtr) {
        fprintf_s(stderr, "Erreur: échec d'allocation mémoire.\n");
        exit(EXIT_FAILURE);
    }

    errno_t err = memcpy_s(clonePtr, sizeof(GameState), game, sizeof(GameState));
    if (err != 0) {
        fprintf_s(stderr, "Erreur: memcpy_s a échoué.\n");
        free(clonePtr);
        exit(EXIT_FAILURE);
    }

    return clonePtr;
}

static void minimax(GameState* game, int depth, int alpha, int beta,
    bool maximizing, int* bestScore, int* bestCol)
{
    if (depth == 0 || isTerminalNode(game)) {
        if (winningMove(game, AI_PIECE)) {
            *bestScore = 1000000;
        }
        else if (winningMove(game, PLAYER_PIECE)) {
            *bestScore = -1000000;
        }
        else {
            *bestScore = scorePosition(game, AI_PIECE);
        }
        *bestCol = -1;
        return;
    }

    int colChoice = -1;

    if (maximizing) {
        int value = -999999;
        for (uint8_t c = 0; c < COLS; c++) {
            if (isValidLocation(game, c)) {
                uint8_t row = getNextOpenRow(game, c);
                GameState* temp = cloneGame(game);
                temp->board[row][c] = AI_PIECE;

                int newScore = 0, dummyCol = -1;
                minimax(temp, depth - 1, alpha, beta, false, &newScore, &dummyCol);
                free(temp);

                if (newScore > value) {
                    value = newScore;
                    colChoice = c;
                }
                if (value > alpha) alpha = value;
                if (alpha >= beta) break;
            }
        }
        *bestScore = value;
        *bestCol = colChoice;
    }
    else {
        int value = 999999;
        for (uint8_t c = 0; c < COLS; c++) {
            if (isValidLocation(game, c)) {
                uint8_t row = getNextOpenRow(game, c);
                GameState* temp = cloneGame(game);
                temp->board[row][c] = PLAYER_PIECE;

                int newScore = 0, dummyCol = -1;
                minimax(temp, depth - 1, alpha, beta, true, &newScore, &dummyCol);
                free(temp);

                if (newScore < value) {
                    value = newScore;
                    colChoice = c;
                }
                if (value < beta) beta = value;
                if (alpha >= beta) break;
            }
        }
        *bestScore = value;
        *bestCol = colChoice;
    }
}

static void drawBoard(SDL_Renderer* renderer, const GameState* game)
{
    clearScreen(renderer, BLACK);

    // Dessine la grille en bleu + cercles vides
    for (uint8_t c = 0; c < COLS; c++) {
        for (uint8_t r = 0; r < ROWS; r++) {
            float x = c * SQUARE_SIZE;
            float y = (r + 1) * SQUARE_SIZE;

            drawFilledRect(renderer, x, y, SQUARE_SIZE, SQUARE_SIZE, BLUE);

            /* cercle noir (trou) */
            float centerX = x + (SQUARE_SIZE / 2);
            float centerY = y + (SQUARE_SIZE / 2);
            drawFilledCircle(renderer, centerX, centerY, RADIUS, BLACK);
        }
    }

    // Dessine les pions
    for (uint8_t c = 0; c < COLS; c++) {
        for (uint8_t r = 0; r < ROWS; r++) {
            uint8_t piece = game->board[r][c];
            if (piece != EMPTY) {
                float centerX = c * SQUARE_SIZE + (SQUARE_SIZE / 2);
                float centerY = (ROWS - r) * SQUARE_SIZE + (SQUARE_SIZE / 2);

                if (piece == PLAYER_PIECE) {
                    drawFilledCircle(renderer, centerX, centerY, RADIUS, YELLOW);
                }
                else if (piece == AI_PIECE) {
                    drawFilledCircle(renderer, centerX, centerY, RADIUS, RED);
                }
            }
        }
    }
}

static void drawHoverPiece(SDL_Renderer* renderer, float mouseX, uint8_t piece)
{
    SDL_Color color = (piece == PLAYER_PIECE) ? YELLOW : RED;
    drawFilledCircle(renderer, mouseX, SQUARE_SIZE / 2, RADIUS, color);
}

// Dessine le pion en chute
static void drawFallingPiece(SDL_Renderer* renderer, const FallingPiece* fp)
{
    if (!fp->active) return;
    SDL_Color color = (fp->piece == PLAYER_PIECE) ? YELLOW : RED;
    drawFilledCircle(renderer, fp->x, fp->y, RADIUS, color);
}


int main(void)
{

    // initLocale();
    setlocale(LC_ALL, "fr_FR.UTF-8");

    // SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf_s(stderr, "Erreur: SDL_Init: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Window* window;
    SDL_Renderer* renderer;

    if (!SDL_CreateWindowAndRenderer("Puissance 4", (int)WINDOW_WIDTH, (int)WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        fprintf(stderr, "Erreur: SDL_CreateWindowAndRenderer: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Init
    GameState game;
    initBoard(&game);

    FallingPiece falling;
    falling.active = false;
    falling.piece = EMPTY;
    falling.x = 0.0f;
    falling.y = 0.0f;
    falling.vy = 0.0f;
    falling.ay = 2000.0f;
    falling.finalRow = 0;
    falling.finalCol = 0;

    bool quit = false;
    float mouseX = WINDOW_WIDTH / 2;
    Uint64 lastTime = SDL_GetTicks();

    drawBoard(renderer, &game);
    SDL_RenderPresent(renderer);

    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                mouseX = event.motion.x;
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (!game.gameOver && !falling.active && !game.turn) {
                    int col = (int)(mouseX / SQUARE_SIZE);
                    if (col < 0) col = 0;
                    if (col >= COLS) col = COLS - 1;

                    if (isValidLocation(&game, (uint8_t)col)) {
                        uint8_t row = getNextOpenRow(&game, (uint8_t)col);

                        falling.active = true;
                        falling.piece = PLAYER_PIECE;
                        falling.finalRow = row;
                        falling.finalCol = (uint8_t)col;
                        falling.x = (float)(col * SQUARE_SIZE) + (SQUARE_SIZE / 2.0f);
                        falling.y = (SQUARE_SIZE / 2.0f);
                        falling.vy = 0.0f;
                        falling.ay = 2000.0f;
                    }
                }
            }
        }

        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (float)(currentTime - lastTime) / 1000.0f;

        lastTime = currentTime;

        if (falling.active) {
            falling.vy += falling.ay * deltaTime;
            falling.y += falling.vy * deltaTime;

            float targetY = (float)((ROWS - falling.finalRow) * SQUARE_SIZE + (SQUARE_SIZE / 2));

            if (falling.y >= targetY) {
                falling.y = targetY;
                falling.active = false;

                dropPieceOnBoard(&game, falling.finalRow, falling.finalCol, falling.piece);

                if (winningMove(&game, falling.piece)) {
                    if (falling.piece == PLAYER_PIECE) {
                        printf_s("Gagné !\n");
                    }
                    else {
                        printf_s("Perdu !\n");
                    }
                    game.gameOver = true;
                }
                else if (isTerminalNode(&game)) {
                    printf_s("Match nul ou plus de coups possibles.\n");
                    game.gameOver = true;
                }
                if (!game.gameOver) {
                    game.turn = !game.turn;
                }
            }
        }
        else {
            if (!game.gameOver && game.turn) {
                int bestScore = 0;
                int bestCol = -1;
                minimax(&game, AI_DEPTH, -2468, 2468, true, &bestScore, &bestCol);

                if (bestCol >= 0 && bestCol < (int)COLS &&
                    isValidLocation(&game, (uint8_t)bestCol)) {
                    uint8_t row = getNextOpenRow(&game, (uint8_t)bestCol);

                    falling.active = true;
                    falling.piece = AI_PIECE;
                    falling.finalRow = row;
                    falling.finalCol = (uint8_t)bestCol;
                    falling.x = (float)(bestCol * SQUARE_SIZE) + (SQUARE_SIZE / 2.0f);
                    falling.y = (SQUARE_SIZE / 2.0f);
                    falling.vy = 0.0f;
                    falling.ay = 2000.0f;
                }
            }
        }

        drawBoard(renderer, &game);

        drawFallingPiece(renderer, &falling);

        if (!falling.active && !game.gameOver && !game.turn) {
            drawHoverPiece(renderer, mouseX, PLAYER_PIECE);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay((uint32_t)(FRAME_DELAY_F + 0.5f));
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
