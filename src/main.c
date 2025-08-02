#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <math.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define PADDLE_WIDTH 100
#define PADDLE_HEIGHT 20
#define BALL_SIZE 15
#define BRICK_WIDTH 60
#define BRICK_HEIGHT 20
#define BRICK_ROWS 5
#define BRICK_COLS 10

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;

SDL_Rect paddle;
SDL_Rect ball;
SDL_Rect bricks[BRICK_ROWS][BRICK_COLS];

float ball_vel_x = 0;
float ball_vel_y = 0;

bool ball_launched = false;

bool left_pressed = false;
bool right_pressed = false;
bool debug_mode = false;
int lives;

void reset_ball() {
    ball_launched = false;
    ball_vel_x = 0;
    ball_vel_y = 0;
    ball.x = paddle.x + (PADDLE_WIDTH / 2) - (BALL_SIZE / 2);
    ball.y = paddle.y - BALL_SIZE;
}

void reset_game() {
    lives = 3;
    paddle.x = (SCREEN_WIDTH - PADDLE_WIDTH) / 2;
    paddle.y = SCREEN_HEIGHT - PADDLE_HEIGHT - 10;
    paddle.w = PADDLE_WIDTH;
    paddle.h = PADDLE_HEIGHT;

    ball.w = BALL_SIZE;
    ball.h = BALL_SIZE;

    for (int i = 0; i < BRICK_ROWS; i++) {
        for (int j = 0; j < BRICK_COLS; j++) {
            bricks[i][j].w = BRICK_WIDTH;
            bricks[i][j].h = BRICK_HEIGHT;
            bricks[i][j].x = j * (BRICK_WIDTH + 10) + 35;
            bricks[i][j].y = i * (BRICK_HEIGHT + 10) + 35;
        }
    }

    reset_ball();
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    window = SDL_CreateWindow("Bricked Up", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    font = TTF_OpenFont("assets/NotoSansMono-Regular.ttf", 16);
    if (font == NULL) {
        printf("Failed to load font: %s\n", TTF_GetError());
        return 1;
    }

    reset_game();

    bool quit = false;
    SDL_Event e;

    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_LEFT:
                        left_pressed = true;
                        break;
                    case SDLK_RIGHT:
                        right_pressed = true;
                        break;
                    case SDLK_SPACE:
                        if (!ball_launched) {
                            ball_launched = true;
                            ball_vel_x = 0;
                            ball_vel_y = -5;
                        }
                        break;
                    case SDLK_d:
                        debug_mode = !debug_mode;
                        break;
                }
            }
            if (e.type == SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                    case SDLK_LEFT:
                        left_pressed = false;
                        break;
                    case SDLK_RIGHT:
                        right_pressed = false;
                        break;
                }
            }
        }

        if (paddle.x < 0) {
            paddle.x = 0;
        }
        if (paddle.x > SCREEN_WIDTH - PADDLE_WIDTH) {
            paddle.x = SCREEN_WIDTH - PADDLE_WIDTH;
        }

        if (left_pressed) {
            paddle.x -= 5;
        }
        if (right_pressed) {
            paddle.x += 5;
        }

        if (ball_launched && fabs(ball_vel_y) < 1.0f) {
            ball_vel_y = ball_vel_y < 0 ? -1.0f : 1.0f;
        }

        if (ball_launched) {
            ball.x += ball_vel_x;
            ball.y += ball_vel_y;

            // Paddle collision
            if (SDL_HasIntersection(&ball, &paddle)) {
                // Get speed before collision adjustment
                float speed = sqrt(ball_vel_x * ball_vel_x + ball_vel_y * ball_vel_y);

                // Reflect the ball vertically
                ball_vel_y = -ball_vel_y;

                // Get the angle after basic reflection
                float angle = atan2(ball_vel_y, ball_vel_x);

                // Adjust angle based on hit location
                float paddle_third = PADDLE_WIDTH / 3.0f;
                float ball_center_x = ball.x + BALL_SIZE / 2.0f;

                if (ball_center_x < paddle.x + paddle_third) { // Left third
                    angle -= 10.0f * M_PI / 180.0f; // 10 degrees left
                } else if (ball_center_x > paddle.x + 2 * paddle_third) { // Right third
                    angle += 10.0f * M_PI / 180.0f; // 10 degrees right
                }
                // Middle third has no change to the angle.

                // Clamp angle to ensure the ball always goes up
                float ten_degrees_rad = 10.0f * M_PI / 180.0f;
                if (angle > -ten_degrees_rad) {
                    angle = -ten_degrees_rad;
                }
                if (angle < -M_PI + ten_degrees_rad) {
                    angle = -M_PI + ten_degrees_rad;
                }

                // Recalculate velocities to maintain constant speed
                ball_vel_x = speed * cos(angle);
                ball_vel_y = speed * sin(angle);
            }

            // Wall collision
            if (ball.x < 0 || ball.x > SCREEN_WIDTH - BALL_SIZE) {
                ball_vel_x = -ball_vel_x;
            }
            if (ball.y < 0) {
                ball.y = 0;
                if (ball_vel_y < 0) { // If ball is moving up
                    ball_vel_y = -ball_vel_y; // Reverse vertical velocity
                }
            }
            if (ball.y > SCREEN_HEIGHT) {
                lives--;
                if (lives > 0) {
                    reset_ball();
                } else {
                    reset_game();
                }
            }

            // Brick collision
            bool all_bricks_destroyed = true;
            for (int i = 0; i < BRICK_ROWS; i++) {
                for (int j = 0; j < BRICK_COLS; j++) {
                    if (bricks[i][j].w != 0 && SDL_HasIntersection(&ball, &bricks[i][j])) {
                        bricks[i][j].w = 0;

                        float ball_center_x = ball.x + BALL_SIZE / 2.0f;
                        float ball_center_y = ball.y + BALL_SIZE / 2.0f;
                        float brick_center_x = bricks[i][j].x + BRICK_WIDTH / 2.0f;
                        float brick_center_y = bricks[i][j].y + BRICK_HEIGHT / 2.0f;

                        float overlap_x = (BALL_SIZE / 2.0f + BRICK_WIDTH / 2.0f) - fabs(ball_center_x - brick_center_x);
                        float overlap_y = (BALL_SIZE / 2.0f + BRICK_HEIGHT / 2.0f) - fabs(ball_center_y - brick_center_y);

                        if (overlap_x < overlap_y) {
                            ball_vel_x = -ball_vel_x;
                        } else {
                            ball_vel_y = -ball_vel_y;
                        }
                    }
                    if (bricks[i][j].w != 0) {
                        all_bricks_destroyed = false;
                    }
                }
            }

            if (all_bricks_destroyed) {
                reset_game();
            }
        } else {
            reset_ball();
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &paddle);
        SDL_RenderFillRect(renderer, &ball);

        for (int i = 0; i < BRICK_ROWS; i++) {
            for (int j = 0; j < BRICK_COLS; j++) {
                if (bricks[i][j].w != 0) {
                    SDL_RenderFillRect(renderer, &bricks[i][j]);
                }
            }
        }

        for (int i = 0; i < lives; i++) {
            SDL_Rect life_ball = {
                SCREEN_WIDTH - 20,
                SCREEN_HEIGHT - 20 - (i * (BALL_SIZE + 5)),
                BALL_SIZE,
                BALL_SIZE
            };
            SDL_RenderFillRect(renderer, &life_ball);
        }

        if (debug_mode) {
            char debug_text_x[50], debug_text_y[50], debug_text_vx[50], debug_text_vy[50];
            sprintf(debug_text_x, "x: %.2f", (float)ball.x);
            sprintf(debug_text_y, "y: %.2f", (float)ball.y);
            sprintf(debug_text_vx, "vx: %.2f", ball_vel_x);
            sprintf(debug_text_vy, "vy: %.2f", ball_vel_y);

            SDL_Color text_color = {255, 255, 255, 255};
            int y_offset = 10;

            SDL_Surface* text_surface_x = TTF_RenderText_Solid(font, debug_text_x, text_color);
            SDL_Texture* text_texture_x = SDL_CreateTextureFromSurface(renderer, text_surface_x);
            SDL_Rect text_rect_x = {10, SCREEN_HEIGHT - text_surface_x->h - y_offset, text_surface_x->w, text_surface_x->h};
            SDL_RenderCopy(renderer, text_texture_x, NULL, &text_rect_x);
            y_offset += text_surface_x->h;

            SDL_Surface* text_surface_y = TTF_RenderText_Solid(font, debug_text_y, text_color);
            SDL_Texture* text_texture_y = SDL_CreateTextureFromSurface(renderer, text_surface_y);
            SDL_Rect text_rect_y = {10, SCREEN_HEIGHT - text_surface_y->h - y_offset, text_surface_y->w, text_surface_y->h};
            SDL_RenderCopy(renderer, text_texture_y, NULL, &text_rect_y);
            y_offset += text_surface_y->h;

            SDL_Surface* text_surface_vx = TTF_RenderText_Solid(font, debug_text_vx, text_color);
            SDL_Texture* text_texture_vx = SDL_CreateTextureFromSurface(renderer, text_surface_vx);
            SDL_Rect text_rect_vx = {10, SCREEN_HEIGHT - text_surface_vx->h - y_offset, text_surface_vx->w, text_surface_vx->h};
            SDL_RenderCopy(renderer, text_texture_vx, NULL, &text_rect_vx);
            y_offset += text_surface_vx->h;

            SDL_Surface* text_surface_vy = TTF_RenderText_Solid(font, debug_text_vy, text_color);
            SDL_Texture* text_texture_vy = SDL_CreateTextureFromSurface(renderer, text_surface_vy);
            SDL_Rect text_rect_vy = {10, SCREEN_HEIGHT - text_surface_vy->h - y_offset, text_surface_vy->w, text_surface_vy->h};
            SDL_RenderCopy(renderer, text_texture_vy, NULL, &text_rect_vy);

            SDL_FreeSurface(text_surface_x);
            SDL_DestroyTexture(text_texture_x);
            SDL_FreeSurface(text_surface_y);
            SDL_DestroyTexture(text_texture_y);
            SDL_FreeSurface(text_surface_vx);
            SDL_DestroyTexture(text_texture_vx);
            SDL_FreeSurface(text_surface_vy);
            SDL_DestroyTexture(text_texture_vy);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
