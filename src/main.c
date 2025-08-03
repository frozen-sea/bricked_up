#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define PADDLE_WIDTH_INITIAL 100
#define PADDLE_WIDTH_STEP 20
#define PADDLE_HEIGHT 15
#define BALL_SIZE 15
#define BRICK_WIDTH 60
#define BRICK_HEIGHT 20
#define BRICK_ROWS 5
#define BRICK_COLS 10
#define RIGHT_MARGIN 30
#define POWERUP_SIZE 15
#define MAX_POWERUPS 10
#define POWERUP_SPAWN_COOLDOWN 250 // 0.25 seconds
#define MAX_BALLS 5

typedef enum {
    POWERUP_ADD_LIFE,
    POWERUP_REMOVE_LIFE,
    POWERUP_PADDLE_WIDER,
    POWERUP_PADDLE_NARROWER,
    POWERUP_BALL_SPLIT
} PowerUpType;

typedef struct {
    SDL_Rect rect;
    bool active;
    PowerUpType type;
} PowerUp;

typedef struct {
    SDL_Rect rect;
    float vel_x;
    float vel_y;
    bool active;
} Ball;

PowerUp powerups[MAX_POWERUPS];
Ball balls[MAX_BALLS];

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;

SDL_Rect paddle;
SDL_Rect bricks[BRICK_ROWS][BRICK_COLS];

bool ball_launched = false;

bool left_pressed = false;
bool right_pressed = false;
bool debug_mode = false;
int lives;
int paddle_size_level;
Uint32 last_powerup_spawn_time = 0;

void draw_filled_circle(SDL_Renderer* renderer, int center_x, int center_y, int radius) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                SDL_RenderDrawPoint(renderer, center_x + x, center_y + y);
            }
        }
    }
}

void draw_rounded_rect(SDL_Renderer* renderer, SDL_Rect* rect, int radius) {
    int x = rect->x;
    int y = rect->y;
    int w = rect->w;
    int h = rect->h;

    // Draw the main body
    SDL_Rect body = {x + radius, y, w - 2 * radius, h};
    SDL_RenderFillRect(renderer, &body);
    SDL_Rect body2 = {x, y + radius, w, h - 2 * radius};
    SDL_RenderFillRect(renderer, &body2);

    // Draw the four corner circles
    draw_filled_circle(renderer, x + radius, y + radius, radius);
    draw_filled_circle(renderer, x + w - radius, y + radius, radius);
    draw_filled_circle(renderer, x + radius, y + h - radius, radius);
    draw_filled_circle(renderer, x + w - radius, y + h - radius, radius);
}

void initialize_powerups() {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        powerups[i].active = false;
    }
}

void spawn_powerup(int x, int y) {
    Uint32 current_time = SDL_GetTicks();
    if (current_time - last_powerup_spawn_time < POWERUP_SPAWN_COOLDOWN) {
        return;
    }

    int rand_val = rand() % 100;
    PowerUpType type;

    if (rand_val < 5) { // 5%
        type = POWERUP_BALL_SPLIT;
    } else if (rand_val < 15) { // 10%
        type = POWERUP_ADD_LIFE;
    } else if (rand_val < 30) { // 15%
        type = POWERUP_PADDLE_WIDER;
    } else if (rand_val < 50) { // 20%
        type = POWERUP_REMOVE_LIFE;
    } else if (rand_val < 75) { // 25%
        type = POWERUP_PADDLE_NARROWER;
    } else {
        return; // 25% chance of no powerup
    }

    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!powerups[i].active) {
            powerups[i].active = true;
            powerups[i].rect.x = x;
            powerups[i].rect.y = y;
            powerups[i].rect.w = POWERUP_SIZE;
            powerups[i].rect.h = POWERUP_SIZE;
            powerups[i].type = type;
            last_powerup_spawn_time = current_time;
            break;
        }
    }
}

void reset_ball() {
    ball_launched = false;
    for (int i = 0; i < MAX_BALLS; i++) {
        balls[i].active = false;
    }
    balls[0].active = true;
    balls[0].vel_x = 0;
    balls[0].vel_y = 0;
    balls[0].rect.w = BALL_SIZE;
    balls[0].rect.h = BALL_SIZE;
    balls[0].rect.x = paddle.x + (paddle.w / 2) - (BALL_SIZE / 2);
    balls[0].rect.y = paddle.y - BALL_SIZE;
    initialize_powerups();
}

void reset_game() {
    lives = 3;
    paddle_size_level = 0;
    paddle.w = PADDLE_WIDTH_INITIAL;
    paddle.x = (SCREEN_WIDTH - paddle.w) / 2;
    paddle.y = SCREEN_HEIGHT - PADDLE_HEIGHT - 10;
    paddle.h = PADDLE_HEIGHT;

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
    srand(time(NULL));

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
                            balls[0].vel_x = 0;
                            balls[0].vel_y = -5;
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
        if (paddle.x > SCREEN_WIDTH - paddle.w - RIGHT_MARGIN) {
            paddle.x = SCREEN_WIDTH - paddle.w - RIGHT_MARGIN;
        }

        if (left_pressed) {
            paddle.x -= 5;
        }
        if (right_pressed) {
            paddle.x += 5;
        }

        if (ball_launched) {
            for (int k = 0; k < MAX_BALLS; k++) {
                if (!balls[k].active) continue;

                if (fabs(balls[k].vel_y) < 1.0f) {
                    balls[k].vel_y = balls[k].vel_y < 0 ? -1.0f : 1.0f;
                }

                balls[k].rect.x += balls[k].vel_x;
                balls[k].rect.y += balls[k].vel_y;

                // Paddle collision
                if (SDL_HasIntersection(&balls[k].rect, &paddle)) {
                    float speed = sqrt(balls[k].vel_x * balls[k].vel_x + balls[k].vel_y * balls[k].vel_y);
                    speed *= 1.025f; // Increase speed by 2.5%
                    balls[k].vel_y = -balls[k].vel_y;
                    float angle = atan2(balls[k].vel_y, balls[k].vel_x);
                    float paddle_fifth = paddle.w / 5.0f;
                    float ball_center_x = balls[k].rect.x + BALL_SIZE / 2.0f;

                    if (ball_center_x < paddle.x + paddle_fifth) {
                        angle -= 20.0f * M_PI / 180.0f;
                    } else if (ball_center_x < paddle.x + 2 * paddle_fifth) {
                        angle -= 10.0f * M_PI / 180.0f;
                    } else if (ball_center_x > paddle.x + 4 * paddle_fifth) {
                        angle += 10.0f * M_PI / 180.0f;
                    } else if (ball_center_x > paddle.x + 3 * paddle_fifth) {
                        angle += 20.0f * M_PI / 180.0f;
                    }

                    float paddle_vel_x = 0;
                    if (left_pressed) paddle_vel_x = -5;
                    if (right_pressed) paddle_vel_x = 5;

                    if (paddle_vel_x != 0) {
                        float angle_adjustment = 10.0f * M_PI / 180.0f;
                        if ((paddle_vel_x > 0 && balls[k].vel_x > 0) || (paddle_vel_x < 0 && balls[k].vel_x < 0)) {
                            if (balls[k].vel_x > 0) angle += angle_adjustment;
                            else angle -= angle_adjustment;
                        } else {
                            if (balls[k].vel_x > 0) angle -= angle_adjustment;
                            else angle += angle_adjustment;
                        }
                    }

                    float twenty_degrees_rad = 20.0f * M_PI / 180.0f;
                    if (angle > -twenty_degrees_rad) angle = -twenty_degrees_rad;
                    if (angle < -M_PI + twenty_degrees_rad) angle = -M_PI + twenty_degrees_rad;

                    balls[k].vel_x = speed * cos(angle);
                    balls[k].vel_y = speed * sin(angle);
                }

                // Wall collision
                if (balls[k].rect.x < 0 || balls[k].rect.x > SCREEN_WIDTH - BALL_SIZE - RIGHT_MARGIN) {
                    balls[k].vel_x = -balls[k].vel_x;
                }
                if (balls[k].rect.y < 0) {
                    balls[k].rect.y = 0;
                    if (balls[k].vel_y < 0) {
                        balls[k].vel_y = -balls[k].vel_y;
                    }
                }
                if (balls[k].rect.y > SCREEN_HEIGHT) {
                    balls[k].active = false;
                    int active_balls = 0;
                    for (int l = 0; l < MAX_BALLS; l++) {
                        if (balls[l].active) active_balls++;
                    }
                    if (active_balls == 0) {
                        lives--;
                        if (lives > 0) {
                            reset_ball();
                        } else {
                            reset_game();
                        }
                    }
                }

                // Brick collision
                bool all_bricks_destroyed = true;
                for (int i = 0; i < BRICK_ROWS; i++) {
                    for (int j = 0; j < BRICK_COLS; j++) {
                        if (bricks[i][j].w != 0 && SDL_HasIntersection(&balls[k].rect, &bricks[i][j])) {
                            bricks[i][j].w = 0;
                            spawn_powerup(bricks[i][j].x + (BRICK_WIDTH / 2) - (POWERUP_SIZE / 2), bricks[i][j].y + (BRICK_HEIGHT / 2) - (POWERUP_SIZE / 2));
                            float ball_center_x = balls[k].rect.x + BALL_SIZE / 2.0f;
                            float ball_center_y = balls[k].rect.y + BALL_SIZE / 2.0f;
                            float brick_center_x = bricks[i][j].x + BRICK_WIDTH / 2.0f;
                            float brick_center_y = bricks[i][j].y + BRICK_HEIGHT / 2.0f;
                            float overlap_x = (BALL_SIZE / 2.0f + BRICK_WIDTH / 2.0f) - fabs(ball_center_x - brick_center_x);
                            float overlap_y = (BALL_SIZE / 2.0f + BRICK_HEIGHT / 2.0f) - fabs(ball_center_y - brick_center_y);
                            if (overlap_x < overlap_y) {
                                balls[k].vel_x = -balls[k].vel_x;
                            } else {
                                balls[k].vel_y = -balls[k].vel_y;
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
            }
        } else {
            balls[0].rect.x = paddle.x + (paddle.w / 2) - (BALL_SIZE / 2);
            balls[0].rect.y = paddle.y - BALL_SIZE;
        }

        // Update powerups
        for (int i = 0; i < MAX_POWERUPS; i++) {
            if (powerups[i].active) {
                powerups[i].rect.y += 2;
                if (SDL_HasIntersection(&powerups[i].rect, &paddle)) {
                    powerups[i].active = false;
                    if (powerups[i].type == POWERUP_ADD_LIFE) {
                        lives++;
                    } else if (powerups[i].type == POWERUP_REMOVE_LIFE) {
                        if (lives > 0) {
                            lives--;
                        }
                    } else if (powerups[i].type == POWERUP_PADDLE_WIDER) {
                        if (paddle_size_level < 3) {
                            paddle_size_level++;
                        }
                    } else if (powerups[i].type == POWERUP_PADDLE_NARROWER) {
                        if (paddle_size_level > -3) {
                            paddle_size_level--;
                        }
                    } else if (powerups[i].type == POWERUP_BALL_SPLIT) {
                        int first_active_ball = -1;
                        for (int l = 0; l < MAX_BALLS; l++) {
                            if (balls[l].active) {
                                first_active_ball = l;
                                break;
                            }
                        }

                        if (first_active_ball != -1) {
                            for (int l = 0; l < MAX_BALLS; l++) {
                                if (!balls[l].active) {
                                    balls[l] = balls[first_active_ball];
                                    balls[l].vel_x = -balls[first_active_ball].vel_x;
                                    break;
                                }
                            }
                        }
                    }

                    int old_width = paddle.w;
                    paddle.w = PADDLE_WIDTH_INITIAL + paddle_size_level * PADDLE_WIDTH_STEP;
                    paddle.x -= (paddle.w - old_width) / 2;

                } else if (powerups[i].rect.y > SCREEN_HEIGHT) {
                    powerups[i].active = false;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Draw bounce area outline
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect top_outline = {0, 0, SCREEN_WIDTH - RIGHT_MARGIN, 3};
        SDL_RenderFillRect(renderer, &top_outline);
        SDL_Rect left_outline = {0, 0, 3, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &left_outline);
        SDL_Rect right_outline = {SCREEN_WIDTH - RIGHT_MARGIN - 3, 0, 3, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &right_outline);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        draw_rounded_rect(renderer, &paddle, 5);
        for (int i = 0; i < MAX_BALLS; i++) {
            if (balls[i].active) {
                draw_filled_circle(renderer, balls[i].rect.x + balls[i].rect.w / 2, balls[i].rect.y + balls[i].rect.h / 2, balls[i].rect.w / 2);
            }
        }

        for (int i = 0; i < BRICK_ROWS; i++) {
            for (int j = 0; j < BRICK_COLS; j++) {
                if (bricks[i][j].w != 0) {
                    draw_rounded_rect(renderer, &bricks[i][j], 3);
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
            draw_filled_circle(renderer, life_ball.x + life_ball.w / 2, life_ball.y + life_ball.h / 2, life_ball.w / 2);
        }

        // Draw powerups
        for (int i = 0; i < MAX_POWERUPS; i++) {
            if (powerups[i].active) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                draw_rounded_rect(renderer, &powerups[i].rect, 3);

                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                int line_thickness = POWERUP_SIZE / 5;
                if (powerups[i].type == POWERUP_ADD_LIFE) {
                    SDL_Rect h_line = {powerups[i].rect.x, powerups[i].rect.y + (POWERUP_SIZE / 2) - (line_thickness / 2), POWERUP_SIZE, line_thickness};
                    SDL_Rect v_line = {powerups[i].rect.x + (POWERUP_SIZE / 2) - (line_thickness / 2), powerups[i].rect.y, line_thickness, POWERUP_SIZE};
                    SDL_RenderFillRect(renderer, &h_line);
                    SDL_RenderFillRect(renderer, &v_line);
                } else if (powerups[i].type == POWERUP_REMOVE_LIFE) {
                    SDL_Rect h_line = {powerups[i].rect.x, powerups[i].rect.y + (POWERUP_SIZE / 2) - (line_thickness / 2), POWERUP_SIZE, line_thickness};
                    SDL_RenderFillRect(renderer, &h_line);
                } else if (powerups[i].type == POWERUP_PADDLE_WIDER) {
                    SDL_RenderDrawLine(renderer, powerups[i].rect.x, powerups[i].rect.y, powerups[i].rect.x + powerups[i].rect.w, powerups[i].rect.y + powerups[i].rect.h / 2);
                    SDL_RenderDrawLine(renderer, powerups[i].rect.x + powerups[i].rect.w, powerups[i].rect.y + powerups[i].rect.h / 2, powerups[i].rect.x, powerups[i].rect.y + powerups[i].rect.h);
                } else if (powerups[i].type == POWERUP_PADDLE_NARROWER) {
                    SDL_RenderDrawLine(renderer, powerups[i].rect.x + powerups[i].rect.w, powerups[i].rect.y, powerups[i].rect.x, powerups[i].rect.y + powerups[i].rect.h / 2);
                    SDL_RenderDrawLine(renderer, powerups[i].rect.x, powerups[i].rect.y + powerups[i].rect.h / 2, powerups[i].rect.x + powerups[i].rect.w, powerups[i].rect.y + powerups[i].rect.h);
                } else if (powerups[i].type == POWERUP_BALL_SPLIT) {
                    int cx = powerups[i].rect.x + POWERUP_SIZE / 2;
                    int cy = powerups[i].rect.y + POWERUP_SIZE / 2;
                    int r = POWERUP_SIZE / 2;
                    SDL_RenderDrawLine(renderer, cx, cy - r, cx, cy + r);
                    SDL_RenderDrawLine(renderer, cx - r, cy, cx + r, cy);
                    SDL_RenderDrawLine(renderer, cx - r, cy - r, cx + r, cy + r);
                    SDL_RenderDrawLine(renderer, cx - r, cy + r, cx + r, cy - r);
                }
            }
        }

        if (debug_mode) {
            int first_active_ball = -1;
            for (int i = 0; i < MAX_BALLS; i++) {
                if (balls[i].active) {
                    first_active_ball = i;
                    break;
                }
            }

            if (first_active_ball != -1) {
                char debug_text_x[50], debug_text_y[50], debug_text_vx[50], debug_text_vy[50];
                sprintf(debug_text_x, "x: %.2f", (float)balls[first_active_ball].rect.x);
                sprintf(debug_text_y, "y: %.2f", (float)balls[first_active_ball].rect.y);
                sprintf(debug_text_vx, "vx: %.2f", balls[first_active_ball].vel_x);
                sprintf(debug_text_vy, "vy: %.2f", balls[first_active_ball].vel_y);

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
