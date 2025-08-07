#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define PADDLE_WIDTH_INITIAL 100
#define PADDLE_WIDTH_STEP 20
#define PADDLE_HEIGHT 15
#define BALL_SIZE 15
#define BRICK_WIDTH 64
#define BRICK_HEIGHT 32
#define BRICK_ROWS 6
#define BRICK_COLS 10
#define RIGHT_MARGIN 30
#define POWERUP_SIZE 15
#define MAX_POWERUPS 10
#define POWERUP_SPAWN_COOLDOWN 250 // 0.25 seconds
#define PADDLE_COLLISION_COOLDOWN 200 // 0.2 seconds
#define MAX_BALLS 5
#define PADDLE_SPEED 500.0f

typedef enum {
    SCREEN_TITLE,
    SCREEN_GAMEPLAY,
    SCREEN_GAMEOVER
} GameScreen;

typedef enum {
    POWERUP_ADD_LIFE,
    POWERUP_REMOVE_LIFE,
    POWERUP_PADDLE_WIDER,
    POWERUP_PADDLE_NARROWER,
    POWERUP_BALL_SPLIT
} PowerUpType;

typedef struct {
    SDL_FRect rect;
    bool active;
    PowerUpType type;
} PowerUp;

typedef struct {
    SDL_FRect rect;
    float vel_x;
    float vel_y;
    bool active;
    Uint64 last_collision_time;
} Ball;

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    SDL_Texture* spritesheet;
    SDL_FRect paddle;
    SDL_FRect bricks[BRICK_ROWS][BRICK_COLS];
    PowerUp powerups[MAX_POWERUPS];
    Ball balls[MAX_BALLS];
    bool ball_launched;
    bool left_pressed;
    bool right_pressed;
    int lives;
    int paddle_size_level;
    Uint64 last_powerup_spawn_time;
    bool quit;
    Uint64 last_frame_time;
    GameScreen current_screen;
} GameState;

void draw_filled_circle(SDL_Renderer* renderer, float center_x, float center_y, float radius) {
    for (float y = -radius; y <= radius; y++) {
        for (float x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                SDL_RenderPoint(renderer, center_x + x, center_y + y);
            }
        }
    }
}

void draw_rounded_rect(SDL_Renderer* renderer, SDL_FRect* rect, float radius) {
    float x = rect->x;
    float y = rect->y;
    float w = rect->w;
    float h = rect->h;

    // Draw the main body
    SDL_FRect body = {x + radius, y, w - 2 * radius, h};
    SDL_RenderFillRect(renderer, &body);
    SDL_FRect body2 = {x, y + radius, w, h - 2 * radius};
    SDL_RenderFillRect(renderer, &body2);

    // Draw the four corner circles
    draw_filled_circle(renderer, x + radius, y + radius, radius);
    draw_filled_circle(renderer, x + w - radius, y + radius, radius);
    draw_filled_circle(renderer, x + radius, y + h - radius, radius);
    draw_filled_circle(renderer, x + w - radius, y + h - radius, radius);
}

void initialize_powerups(GameState* gs) {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        gs->powerups[i].active = false;
    }
}

void spawn_powerup(GameState* gs, float x, float y) {
    Uint64 current_time = SDL_GetTicks();
    if (current_time - gs->last_powerup_spawn_time < POWERUP_SPAWN_COOLDOWN) {
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
        if (!gs->powerups[i].active) {
            gs->powerups[i].active = true;
            gs->powerups[i].rect.x = x;
            gs->powerups[i].rect.y = y;
            gs->powerups[i].rect.w = POWERUP_SIZE;
            gs->powerups[i].rect.h = POWERUP_SIZE;
            gs->powerups[i].type = type;
            gs->last_powerup_spawn_time = current_time;
            break;
        }
    }
}

void reset_ball(GameState* gs) {
    gs->ball_launched = false;
    for (int i = 0; i < MAX_BALLS; i++) {
        gs->balls[i].active = false;
    }
    gs->balls[0].active = true;
    gs->balls[0].vel_x = 0;
    gs->balls[0].vel_y = 0;
    gs->balls[0].rect.w = BALL_SIZE;
    gs->balls[0].rect.h = BALL_SIZE;
    gs->balls[0].rect.x = gs->paddle.x + (gs->paddle.w / 2) - (BALL_SIZE / 2);
    gs->balls[0].rect.y = gs->paddle.y - BALL_SIZE;
    gs->balls[0].last_collision_time = 0;
    initialize_powerups(gs);
}

void reset_game(GameState* gs) {
    gs->lives = 3;
    gs->paddle_size_level = 0;
    gs->paddle.w = PADDLE_WIDTH_INITIAL;
    gs->paddle.x = (SCREEN_WIDTH - gs->paddle.w) / 2;
    gs->paddle.y = SCREEN_HEIGHT - PADDLE_HEIGHT - 10;
    gs->paddle.h = PADDLE_HEIGHT;

    for (int i = 0; i < BRICK_ROWS; i++) {
        for (int j = 0; j < BRICK_COLS; j++) {
            gs->bricks[i][j].w = BRICK_WIDTH;
            gs->bricks[i][j].h = BRICK_HEIGHT;
            gs->bricks[i][j].x = j * (BRICK_WIDTH + 5) + 42;
            gs->bricks[i][j].y = i * (BRICK_HEIGHT + 5) + 35;
        }
    }

    reset_ball(gs);
}

void handle_events_gameplay(GameState* gs) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_EVENT_QUIT) {
            gs->quit = true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
                case SDLK_LEFT:
                    gs->left_pressed = true;
                    break;
                case SDLK_RIGHT:
                    gs->right_pressed = true;
                    break;
                case SDLK_SPACE:
                    if (!gs->ball_launched) {
                        gs->ball_launched = true;
                        gs->balls[0].vel_x = 0;
                        gs->balls[0].vel_y = -5;
                    }
                    break;
            }
        }
        if (e.type == SDL_EVENT_KEY_UP) {
            switch (e.key.key) {
                case SDLK_LEFT:
                    gs->left_pressed = false;
                    break;
                case SDLK_RIGHT:
                    gs->right_pressed = false;
                    break;
            }
        }
    }
}

void handle_events_title(GameState* gs) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_EVENT_QUIT) {
            gs->quit = true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_RETURN) {
                gs->current_screen = SCREEN_GAMEPLAY;
                reset_game(gs);
            }
        }
    }
}

void handle_events_gameover(GameState* gs) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_EVENT_QUIT) {
            gs->quit = true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_RETURN) {
                gs->current_screen = SCREEN_TITLE;
            }
        }
    }
}

void update_gameplay(GameState* gs, float delta_time) {
    if (gs->left_pressed) {
        gs->paddle.x -= PADDLE_SPEED * delta_time;
    }
    if (gs->right_pressed) {
        gs->paddle.x += PADDLE_SPEED * delta_time;
    }

    if (gs->paddle.x < 0) {
        gs->paddle.x = 0;
    }
    if (gs->paddle.x > SCREEN_WIDTH - gs->paddle.w - RIGHT_MARGIN) {
        gs->paddle.x = SCREEN_WIDTH - gs->paddle.w - RIGHT_MARGIN;
    }

    if (gs->ball_launched) {
        for (int k = 0; k < MAX_BALLS; k++) {
            if (!gs->balls[k].active) continue;

            if (fabs(gs->balls[k].vel_y) < 1.0f) {
                gs->balls[k].vel_y = gs->balls[k].vel_y < 0 ? -1.0f : 1.0f;
            }

            gs->balls[k].rect.x += gs->balls[k].vel_x;
            gs->balls[k].rect.y += gs->balls[k].vel_y;

            // Paddle collision
            if (SDL_GetTicks() - gs->balls[k].last_collision_time > PADDLE_COLLISION_COOLDOWN && SDL_HasRectIntersectionFloat(&gs->balls[k].rect, &gs->paddle)) {
                gs->balls[k].last_collision_time = SDL_GetTicks();
                
                float ball_center_x = gs->balls[k].rect.x + gs->balls[k].rect.w / 2.0f;
                float ball_center_y = gs->balls[k].rect.y + gs->balls[k].rect.h / 2.0f;
                float paddle_center_x = gs->paddle.x + gs->paddle.w / 2.0f;
                float paddle_center_y = gs->paddle.y + gs->paddle.h / 2.0f;

                float overlap_x = (gs->balls[k].rect.w / 2.0f + gs->paddle.w / 2.0f) - fabsf(ball_center_x - paddle_center_x);
                float overlap_y = (gs->balls[k].rect.h / 2.0f + gs->paddle.h / 2.0f) - fabsf(ball_center_y - paddle_center_y);

                // Determine bounce direction
                if (overlap_y <= overlap_x) { // Top/Bottom collision
                    gs->balls[k].vel_y = -gs->balls[k].vel_y;
                    if (ball_center_y < paddle_center_y) gs->balls[k].rect.y = gs->paddle.y - gs->balls[k].rect.h;
                    else gs->balls[k].rect.y = gs->paddle.y + gs->paddle.h;
                } else { // Side collision
                    gs->balls[k].vel_x = -gs->balls[k].vel_x;
                    gs->balls[k].vel_y = -gs->balls[k].vel_y; // Bounce up
                    if (ball_center_x < paddle_center_x) gs->balls[k].rect.x = gs->paddle.x - gs->balls[k].rect.w;
                    else gs->balls[k].rect.x = gs->paddle.x + gs->paddle.w;
                }

                // Calculate new velocity based on angle
                float speed = sqrtf(gs->balls[k].vel_x * gs->balls[k].vel_x + gs->balls[k].vel_y * gs->balls[k].vel_y);
                speed *= 1.025f;
                float angle = atan2f(gs->balls[k].vel_y, gs->balls[k].vel_x);
                
                float paddle_fifth = gs->paddle.w / 5.0f;

                // Adjust angle based on hit location
                if (overlap_y > overlap_x) { // Side hit
                    if (ball_center_x < paddle_center_x) angle -= 20.0f * M_PI / 180.0f; // Left side
                    else angle += 20.0f * M_PI / 180.0f; // Right side
                } else { // Top hit
                    if (ball_center_x < gs->paddle.x + paddle_fifth) angle -= 20.0f * M_PI / 180.0f;
                    else if (ball_center_x < gs->paddle.x + 2 * paddle_fifth) angle -= 10.0f * M_PI / 180.0f;
                    else if (ball_center_x > gs->paddle.x + 4 * paddle_fifth) angle += 10.0f * M_PI / 180.0f;
                    else if (ball_center_x > gs->paddle.x + 3 * paddle_fifth) angle += 20.0f * M_PI / 180.0f;
                }

                // Adjust angle based on paddle movement
                float paddle_vel_x = 0;
                if (gs->left_pressed) paddle_vel_x = -PADDLE_SPEED;
                if (gs->right_pressed) paddle_vel_x = PADDLE_SPEED;

                if (paddle_vel_x != 0) {
                    float angle_adjustment = 10.0f * M_PI / 180.0f;
                    if ((paddle_vel_x > 0 && gs->balls[k].vel_x > 0) || (paddle_vel_x < 0 && gs->balls[k].vel_x < 0)) {
                        if (gs->balls[k].vel_x > 0) angle += angle_adjustment;
                        else angle -= angle_adjustment;
                    } else {
                        if (gs->balls[k].vel_x > 0) angle -= angle_adjustment;
                        else angle += angle_adjustment;
                    }
                }

                // Clamp angle to prevent near-horizontal bounces
                float twenty_degrees_rad = 20.0f * M_PI / 180.0f;
                if (angle > -twenty_degrees_rad) angle = -twenty_degrees_rad;
                if (angle < -M_PI + twenty_degrees_rad) angle = -M_PI + twenty_degrees_rad;

                gs->balls[k].vel_x = speed * cosf(angle);
                gs->balls[k].vel_y = speed * sinf(angle);
            }

            // Wall collision
            if (gs->balls[k].rect.x < 0 || gs->balls[k].rect.x > SCREEN_WIDTH - BALL_SIZE - RIGHT_MARGIN) {
                gs->balls[k].vel_x = -gs->balls[k].vel_x;
            }
            if (gs->balls[k].rect.y < 0) {
                gs->balls[k].rect.y = 0;
                if (gs->balls[k].vel_y < 0) {
                    gs->balls[k].vel_y = -gs->balls[k].vel_y;
                }
            }
            if (gs->balls[k].rect.y > SCREEN_HEIGHT) {
                gs->balls[k].active = false;
                int active_balls = 0;
                for (int l = 0; l < MAX_BALLS; l++) {
                    if (gs->balls[l].active) active_balls++;
                }
                if (active_balls == 0) {
                    gs->lives--;
                    if (gs->lives <= 0) {
                        gs->current_screen = SCREEN_GAMEOVER;
                    } else {
                        reset_ball(gs);
                    }
                }
            }

            // Brick collision
            bool all_bricks_destroyed = true;
            for (int i = 0; i < BRICK_ROWS; i++) {
                for (int j = 0; j < BRICK_COLS; j++) {
                    if (gs->bricks[i][j].w != 0 && SDL_HasRectIntersectionFloat(&gs->balls[k].rect, &gs->bricks[i][j])) {
                        gs->bricks[i][j].w = 0;
                        spawn_powerup(gs, gs->bricks[i][j].x + (BRICK_WIDTH / 2) - (POWERUP_SIZE / 2), gs->bricks[i][j].y + (BRICK_HEIGHT / 2) - (POWERUP_SIZE / 2));
                        float ball_center_x = gs->balls[k].rect.x + BALL_SIZE / 2.0f;
                        float ball_center_y = gs->balls[k].rect.y + BALL_SIZE / 2.0f;
                        float brick_center_x = gs->bricks[i][j].x + BRICK_WIDTH / 2.0f;
                        float brick_center_y = gs->bricks[i][j].y + BRICK_HEIGHT / 2.0f;
                        float overlap_x = ( BALL_SIZE / 2.0f + BRICK_WIDTH / 2.0f) - fabsf(ball_center_x - brick_center_x);
                        float overlap_y = ( BALL_SIZE / 2.0f + BRICK_HEIGHT / 2.0f) - fabsf(ball_center_y - brick_center_y);
                        if (overlap_x < overlap_y) {
                            gs->balls[k].vel_x = -gs->balls[k].vel_x;
                        } else {
                            gs->balls[k].vel_y = -gs->balls[k].vel_y;
                        }
                    }
                    if (gs->bricks[i][j].w != 0) {
                        all_bricks_destroyed = false;
                    }
                }
            }
            if (all_bricks_destroyed) {
                reset_game(gs);
            }
        }
    } else {
        gs->balls[0].rect.x = gs->paddle.x + (gs->paddle.w / 2) - (BALL_SIZE / 2);
        gs->balls[0].rect.y = gs->paddle.y - BALL_SIZE;
    }

    // Update powerups
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (gs->powerups[i].active) {
            gs->powerups[i].rect.y += 2;
            if (SDL_HasRectIntersectionFloat(&gs->powerups[i].rect, &gs->paddle)) {
                gs->powerups[i].active = false;
                if (gs->powerups[i].type == POWERUP_ADD_LIFE) {
                    gs->lives++;
                } else if (gs->powerups[i].type == POWERUP_REMOVE_LIFE) {
                    gs->lives--;
                     if (gs->lives <= 0) {
                        gs->current_screen = SCREEN_GAMEOVER;
                    }
                } else if (gs->powerups[i].type == POWERUP_PADDLE_WIDER) {
                    if (gs->paddle_size_level < 3) {
                        gs->paddle_size_level++;
                    }
                } else if (gs->powerups[i].type == POWERUP_PADDLE_NARROWER) {
                    if (gs->paddle_size_level > -3) {
                        gs->paddle_size_level--;
                    }
                } else if (gs->powerups[i].type == POWERUP_BALL_SPLIT) {
                    int first_active_ball = -1;
                    for (int l = 0; l < MAX_BALLS; l++) {
                        if (gs->balls[l].active) {
                            first_active_ball = l;
                            break;
                        }
                    }

                    if (first_active_ball != -1) {
                        for (int l = 0; l < MAX_BALLS; l++) {
                            if (!gs->balls[l].active) {
                                gs->balls[l] = gs->balls[first_active_ball];
                                gs->balls[l].vel_x = -gs->balls[first_active_ball].vel_x;
                                break;
                            }
                        }
                    }
                }

                float old_width = gs->paddle.w;
                gs->paddle.w = PADDLE_WIDTH_INITIAL + gs->paddle_size_level * PADDLE_WIDTH_STEP;
                gs->paddle.x -= (gs->paddle.w - old_width) / 2;

            } else if (gs->powerups[i].rect.y > SCREEN_HEIGHT) {
                gs->powerups[i].active = false;
            }
        }
    }
}

void render_gameplay(GameState* gs) {
    SDL_SetRenderDrawColor(gs->renderer, 0, 0, 0, 255);
    SDL_RenderClear(gs->renderer);

    if (!gs->ball_launched) {
        SDL_Color text_color = {255, 255, 255, 255};
        SDL_Surface* text_surface = TTF_RenderText_Solid(gs->font, "Use arrows to move and space to shoot", 0, text_color);
        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(gs->renderer, text_surface);
        SDL_FRect text_rect = {
            (SCREEN_WIDTH - text_surface->w) / 2.0f,
            (SCREEN_HEIGHT - text_surface->h) / 2.0f,
            text_surface->w,
            text_surface->h
        };
        SDL_RenderTexture(gs->renderer, text_texture, NULL, &text_rect);
        SDL_DestroyTexture(text_texture);
        SDL_DestroySurface(text_surface);
    }

    // Draw bounce area outline
    SDL_SetRenderDrawColor(gs->renderer, 255, 255, 255, 255);
    SDL_FRect top_outline = {0, 0, SCREEN_WIDTH - RIGHT_MARGIN, 3};
    SDL_RenderFillRect(gs->renderer, &top_outline);
    SDL_FRect left_outline = {0, 0, 3, SCREEN_HEIGHT};
    SDL_RenderFillRect(gs->renderer, &left_outline);
    SDL_FRect right_outline = {SCREEN_WIDTH - RIGHT_MARGIN - 3, 0, 3, SCREEN_HEIGHT};
    SDL_RenderFillRect(gs->renderer, &right_outline);

    SDL_SetRenderDrawColor(gs->renderer, 255, 255, 255, 255);
    draw_rounded_rect(gs->renderer, &gs->paddle, 5);
    for (int i = 0; i < MAX_BALLS; i++) {
        if (gs->balls[i].active) {
            draw_filled_circle(gs->renderer, gs->balls[i].rect.x + gs->balls[i].rect.w / 2, gs->balls[i].rect.y + gs->balls[i].rect.h / 2, gs->balls[i].rect.w / 2);
        }
    }

    for (int i = 0; i < BRICK_ROWS; i++) {
        for (int j = 0; j < BRICK_COLS; j++) {
            if (gs->bricks[i][j].w != 0) {
                SDL_FRect src_rect = { 32, 176 + i * 16, 32, 16 };
                SDL_RenderTexture(gs->renderer, gs->spritesheet, &src_rect, &gs->bricks[i][j]);
            }
        }
    }

    for (int i = 0; i < gs->lives; i++) {
        SDL_FRect life_ball = {
            SCREEN_WIDTH - 20,
            SCREEN_HEIGHT - 20 - (i * (BALL_SIZE + 5)),
            BALL_SIZE,
            BALL_SIZE
        };
        draw_filled_circle(gs->renderer, life_ball.x + life_ball.w / 2, life_ball.y + life_ball.h / 2, life_ball.w / 2);
    }

    // Draw powerups
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (gs->powerups[i].active) {
            SDL_SetRenderDrawColor(gs->renderer, 255, 255, 255, 255);
            draw_rounded_rect(gs->renderer, &gs->powerups[i].rect, 3);

            SDL_SetRenderDrawColor(gs->renderer, 0, 0, 0, 255);
            float line_thickness = POWERUP_SIZE / 5.0f;
            if (gs->powerups[i].type == POWERUP_ADD_LIFE) {
                SDL_FRect h_line = {gs->powerups[i].rect.x, gs->powerups[i].rect.y + (POWERUP_SIZE / 2.0f) - (line_thickness / 2.0f), POWERUP_SIZE, line_thickness};
                SDL_FRect v_line = {gs->powerups[i].rect.x + (POWERUP_SIZE / 2.0f) - (line_thickness / 2.0f), gs->powerups[i].rect.y, line_thickness, POWERUP_SIZE};
                SDL_RenderFillRect(gs->renderer, &h_line);
                SDL_RenderFillRect(gs->renderer, &v_line);
            } else if (gs->powerups[i].type == POWERUP_REMOVE_LIFE) {
                SDL_FRect h_line = {gs->powerups[i].rect.x, gs->powerups[i].rect.y + (POWERUP_SIZE / 2.0f) - (line_thickness / 2.0f), POWERUP_SIZE, line_thickness};
                SDL_RenderFillRect(gs->renderer, &h_line);
            } else if (gs->powerups[i].type == POWERUP_PADDLE_WIDER) {
                SDL_RenderLine(gs->renderer, gs->powerups[i].rect.x, gs->powerups[i].rect.y, gs->powerups[i].rect.x + gs->powerups[i].rect.w, gs->powerups[i].rect.y + gs->powerups[i].rect.h / 2);
                SDL_RenderLine(gs->renderer, gs->powerups[i].rect.x + gs->powerups[i].rect.w, gs->powerups[i].rect.y + gs->powerups[i].rect.h / 2, gs->powerups[i].rect.x, gs->powerups[i].rect.y + gs->powerups[i].rect.h);
            } else if (gs->powerups[i].type == POWERUP_PADDLE_NARROWER) {
                SDL_RenderLine(gs->renderer, gs->powerups[i].rect.x + gs->powerups[i].rect.w, gs->powerups[i].rect.y, gs->powerups[i].rect.x, gs->powerups[i].rect.y + gs->powerups[i].rect.h / 2);
                SDL_RenderLine(gs->renderer, gs->powerups[i].rect.x, gs->powerups[i].rect.y + gs->powerups[i].rect.h / 2, gs->powerups[i].rect.x + gs->powerups[i].rect.w, gs->powerups[i].rect.y + gs->powerups[i].rect.h);
            } else if (gs->powerups[i].type == POWERUP_BALL_SPLIT) {
                float cx = gs->powerups[i].rect.x + POWERUP_SIZE / 2;
                float cy = gs->powerups[i].rect.y + POWERUP_SIZE / 2;
                float r = POWERUP_SIZE / 2;
                SDL_RenderLine(gs->renderer, cx, cy - r, cx, cy + r);
                SDL_RenderLine(gs->renderer, cx - r, cy, cx + r, cy);
                SDL_RenderLine(gs->renderer, cx - r, cy - r, cx + r, cy + r);
                SDL_RenderLine(gs->renderer, cx - r, cy + r, cx + r, cy - r);
            }
        }
    }

    SDL_RenderPresent(gs->renderer);
}

void render_title_screen(GameState* gs) {
    SDL_SetRenderDrawColor(gs->renderer, 0, 0, 0, 255);
    SDL_RenderClear(gs->renderer);

    SDL_Color text_color = {255, 255, 255, 255};
    SDL_Surface* title_surface = TTF_RenderText_Solid(gs->font, "Bricked Up", 0, text_color);
    SDL_Texture* title_texture = SDL_CreateTextureFromSurface(gs->renderer, title_surface);
    SDL_FRect title_rect = {
        (SCREEN_WIDTH - title_surface->w) / 2.0f,
        (SCREEN_HEIGHT / 2.0f) - title_surface->h,
        title_surface->w,
        title_surface->h
    };
    SDL_RenderTexture(gs->renderer, title_texture, NULL, &title_rect);
    SDL_DestroyTexture(title_texture);
    SDL_DestroySurface(title_surface);

    SDL_Surface* instruction_surface = TTF_RenderText_Solid(gs->font, "Press Enter to Start", 0, text_color);
    SDL_Texture* instruction_texture = SDL_CreateTextureFromSurface(gs->renderer, instruction_surface);
    SDL_FRect instruction_rect = {
        (SCREEN_WIDTH - instruction_surface->w) / 2.0f,
        (SCREEN_HEIGHT / 2.0f) + instruction_surface->h,
        instruction_surface->w,
        instruction_surface->h
    };
    SDL_RenderTexture(gs->renderer, instruction_texture, NULL, &instruction_rect);
    SDL_DestroyTexture(instruction_texture);
    SDL_DestroySurface(instruction_surface);

    SDL_RenderPresent(gs->renderer);
}

void render_game_over_screen(GameState* gs) {
    SDL_SetRenderDrawColor(gs->renderer, 0, 0, 0, 255);
    SDL_RenderClear(gs->renderer);

    SDL_Color text_color = {255, 255, 255, 255};
    SDL_Surface* title_surface = TTF_RenderText_Solid(gs->font, "Game Over", 0, text_color);
    SDL_Texture* title_texture = SDL_CreateTextureFromSurface(gs->renderer, title_surface);
    SDL_FRect title_rect = {
        (SCREEN_WIDTH - title_surface->w) / 2.0f,
        (SCREEN_HEIGHT / 2.0f) - title_surface->h,
        title_surface->w,
        title_surface->h
    };
    SDL_RenderTexture(gs->renderer, title_texture, NULL, &title_rect);
    SDL_DestroyTexture(title_texture);
    SDL_DestroySurface(title_surface);

    SDL_Surface* instruction_surface = TTF_RenderText_Solid(gs->font, "Press Enter to Return to Title", 0, text_color);
    SDL_Texture* instruction_texture = SDL_CreateTextureFromSurface(gs->renderer, instruction_surface);
    SDL_FRect instruction_rect = {
        (SCREEN_WIDTH - instruction_surface->w) / 2.0f,
        (SCREEN_HEIGHT / 2.0f) + instruction_surface->h,
        instruction_surface->w,
        instruction_surface->h
    };
    SDL_RenderTexture(gs->renderer, instruction_texture, NULL, &instruction_rect);
    SDL_DestroyTexture(instruction_texture);
    SDL_DestroySurface(instruction_surface);

    SDL_RenderPresent(gs->renderer);
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    GameState gs;
    gs.window = SDL_CreateWindow("Bricked Up", SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    gs.renderer = SDL_CreateRenderer(gs.window, NULL);
    gs.font = TTF_OpenFont("assets/NotoSansMono-Regular.ttf", 16);
    if (gs.font == NULL) {
        printf("Failed to load font: %s\n", SDL_GetError());
        return 1;
    }

    gs.spritesheet = IMG_LoadTexture(gs.renderer, "assets/spritesheet-breakout.png");
    if (gs.spritesheet == NULL) {
        printf("Failed to load spritesheet: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetTextureScaleMode(gs.spritesheet, SDL_SCALEMODE_NEAREST);

    reset_game(&gs);
    srand(time(NULL));

    gs.quit = false;
    gs.last_frame_time = SDL_GetTicks();
    gs.current_screen = SCREEN_TITLE;

    while (!gs.quit) {
        Uint64 current_time = SDL_GetTicks();
        float delta_time = (current_time - gs.last_frame_time) / 1000.0f;
        gs.last_frame_time = current_time;

        switch (gs.current_screen) {
            case SCREEN_TITLE:
                handle_events_title(&gs);
                render_title_screen(&gs);
                break;
            case SCREEN_GAMEPLAY:
                handle_events_gameplay(&gs);
                update_gameplay(&gs, delta_time);
                render_gameplay(&gs);
                break;
            case SCREEN_GAMEOVER:
                handle_events_gameover(&gs);
                render_game_over_screen(&gs);
                break;
        }

        SDL_Delay(16);
    }

    SDL_DestroyTexture(gs.spritesheet);
    TTF_CloseFont(gs.font);
    SDL_DestroyRenderer(gs.renderer);
    SDL_DestroyWindow(gs.window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}