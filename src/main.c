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
#define PADDLE_WIDTH_INITIAL 80
#define PADDLE_WIDTH_STEP 10
#define PADDLE_HEIGHT 20
#define BALL_SIZE 24
#define BRICK_WIDTH 64
#define BRICK_HEIGHT 32
#define BRICK_ROWS 6
#define BRICK_COLS 10
#define TOP_MARGIN 70
#define BORDER_THICKNESS 3
#define POWERUP_SIZE 15
#define MAX_POWERUPS 10
#define POWERUP_SPAWN_COOLDOWN 250 // 0.25 seconds
#define PADDLE_COLLISION_COOLDOWN 200 // 0.2 seconds
#define MAX_BALLS 5
#define PADDLE_SPEED 500.0f
#define PADDLE_ACCELERATION 10.0f
#define BALL_SPEED 350.0f
#define POWERUP_SPEED 100.0f
#define BRICK_ANIMATION_SPEED 50 // ms per frame
#define MAX_PARTICLES 200

typedef struct {
    SDL_FPoint pos;
    SDL_FPoint vel;
    SDL_Color color;
    float lifetime_ms;
} Particle;


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
    POWERUP_BALL_SPLIT,
    POWERUP_STICKY_PADDLE
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
    bool is_stuck;
    float stuck_offset_x;
} Ball;

typedef struct {
    SDL_FRect rect;
    bool active;
    int animation_frame; // 0 = solid, 1-10 = animation
    float animation_timer;
} Brick;

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    SDL_Texture* spritesheet;
    SDL_FRect paddle;
    Brick bricks[BRICK_ROWS][BRICK_COLS];
    PowerUp powerups[MAX_POWERUPS];
    Ball balls[MAX_BALLS];
    bool ball_launched;
    bool left_pressed;
    bool right_pressed;
    int lives;
    int paddle_size_level;
    Uint64 last_powerup_spawn_time;
    Uint64 sticky_paddle_timer_ms;
    Particle particles[MAX_PARTICLES];
    float force_field_y_offset;
    float force_field_anim_timer;
    bool quit;
    bool paused;
    Uint64 last_frame_time;
    GameScreen current_screen;
    bool debug_mode;
    bool debug_render_collisions;
    float game_speed;
    Uint64 show_speed_timer;
    float paddle_vel_x;
} GameState;

void launch_ball(Ball* ball, float paddle_x, float paddle_w) {
    ball->is_stuck = false;
    float ball_center_x = ball->rect.x + ball->rect.w / 2.0f;
    float paddle_center_x = paddle_x + paddle_w / 2.0f;
    
    float diff = (ball_center_x - paddle_center_x) / (paddle_w / 2.0f);
    
    float angle = diff * (M_PI / 4.0f); // Max angle 45 degrees
    
    ball->vel_x = BALL_SPEED * sinf(angle);
    ball->vel_y = -BALL_SPEED * cosf(angle);
}

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

    SDL_FRect body = {x + radius, y, w - 2 * radius, h};
    SDL_RenderFillRect(renderer, &body);
    SDL_FRect body2 = {x, y + radius, w, h - 2 * radius};
    SDL_RenderFillRect(renderer, &body2);

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

    if (rand_val < 5) {
        type = POWERUP_BALL_SPLIT;
    } else if (rand_val < 10) {
        type = POWERUP_STICKY_PADDLE;
    } else if (rand_val < 20) {
        type = POWERUP_ADD_LIFE;
    } else if (rand_val < 35) {
        type = POWERUP_PADDLE_WIDER;
    } else if (rand_val < 50) {
        type = POWERUP_REMOVE_LIFE;
    } else if (rand_val < 75) {
        type = POWERUP_PADDLE_NARROWER;
    } else {
        return;
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
        gs->balls[i].is_stuck = false;
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
    gs->sticky_paddle_timer_ms = 0;
    gs->force_field_y_offset = 0;
    gs->force_field_anim_timer = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        gs->particles[i].lifetime_ms = 0;
    }
    gs->paused = false;
    gs->left_pressed = false;
    gs->right_pressed = false;
    gs->debug_mode = false;
    gs->debug_render_collisions = false;
    gs->game_speed = 1.0f;
    gs->show_speed_timer = 0;
    gs->paddle_vel_x = 0.0f;

    float total_bricks_width = BRICK_COLS * (BRICK_WIDTH + 11) - 11;
    float side_margin = (SCREEN_WIDTH - total_bricks_width) / 2.0f;
    for (int i = 0; i < BRICK_ROWS; i++) {
        for (int j = 0; j < BRICK_COLS; j++) {
            gs->bricks[i][j].active = true;
            gs->bricks[i][j].animation_frame = 0;
            gs->bricks[i][j].animation_timer = 0;
            gs->bricks[i][j].rect.w = BRICK_WIDTH;
            gs->bricks[i][j].rect.h = BRICK_HEIGHT;
            gs->bricks[i][j].rect.x = side_margin + j * (BRICK_WIDTH + 11);
            gs->bricks[i][j].rect.y = i * (BRICK_HEIGHT + 11) + 35 + TOP_MARGIN;
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
                case SDLK_P:
                    gs->paused = !gs->paused;
                    break;
                case SDLK_LEFT:
                    gs->left_pressed = true;
                    break;
                case SDLK_RIGHT:
                    gs->right_pressed = true;
                    break;
                case SDLK_SPACE:
                    if (!gs->paused) {
                        if (!gs->ball_launched) {
                            gs->ball_launched = true;
                            launch_ball(&gs->balls[0], gs->paddle.x, gs->paddle.w);
                        } else {
                            for (int i = 0; i < MAX_BALLS; i++) {
                                if (gs->balls[i].active && gs->balls[i].is_stuck) {
                                    launch_ball(&gs->balls[i], gs->paddle.x, gs->paddle.w);
                                }
                            }
                        }
                    }
                    break;
                case SDLK_D:
                    gs->debug_mode = !gs->debug_mode;
                    break;
                case SDLK_C:
                    if (gs->debug_mode) {
                        gs->debug_render_collisions = !gs->debug_render_collisions;
                    }
                    break;
                case SDLK_S:
                    if (gs->debug_mode) {
                        gs->game_speed -= 0.1f;
                        if (gs->game_speed < 0.1f) gs->game_speed = 0.1f;
                        gs->show_speed_timer = 2000;
                    }
                    break;
                case SDLK_F:
                    if (gs->debug_mode) {
                        gs->game_speed += 0.1f;
                        gs->show_speed_timer = 2000;
                    }
                    break;
                case SDLK_R:
                    if (gs->debug_mode) {
                        gs->game_speed = 1.0f;
                        gs->show_speed_timer = 2000;
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


float swept_aabb(SDL_FRect b1, SDL_FPoint vel, SDL_FRect b2, float* normal_x, float* normal_y) {
    float inv_entry_x, inv_entry_y;
    float inv_exit_x, inv_exit_y;

    if (vel.x > 0.0f) {
        inv_entry_x = b2.x - (b1.x + b1.w);
        inv_exit_x = (b2.x + b2.w) - b1.x;
    } else {
        inv_entry_x = (b2.x + b2.w) - b1.x;
        inv_exit_x = b2.x - (b1.x + b1.w);
    }

    if (vel.y > 0.0f) {
        inv_entry_y = b2.y - (b1.y + b1.h);
        inv_exit_y = (b2.y + b2.h) - b1.y;
    } else {
        inv_entry_y = (b2.y + b2.h) - b1.y;
        inv_exit_y = b2.y - (b1.y + b1.h);
    }

    float entry_x, entry_y;
    float exit_x, exit_y;

    if (vel.x == 0.0f) {
        if (b1.x + b1.w < b2.x || b1.x > b2.x + b2.w) {
            *normal_x = 0.0f;
            *normal_y = 0.0f;
            return 1.0f;
        }
        entry_x = -INFINITY;
        exit_x = INFINITY;
    } else {
        entry_x = inv_entry_x / vel.x;
        exit_x = inv_exit_x / vel.x;
    }

    if (vel.y == 0.0f) {
        if (b1.y + b1.h < b2.y || b1.y > b2.y + b2.h) {
            *normal_x = 0.0f;
            *normal_y = 0.0f;
            return 1.0f;
        }
        entry_y = -INFINITY;
        exit_y = INFINITY;
    } else {
        entry_y = inv_entry_y / vel.y;
        exit_y = inv_exit_y / vel.y;
    }

    float entry_time = fmaxf(entry_x, entry_y);
    float exit_time = fminf(exit_x, exit_y);

    if (entry_time > exit_time || entry_x < 0.0f && entry_y < 0.0f || entry_x > 1.0f || entry_y > 1.0f) {
        *normal_x = 0.0f;
        *normal_y = 0.0f;
        return 1.0f;
    }

    if (entry_x > entry_y) {
        *normal_x = (vel.x > 0.0f) ? -1.0f : 1.0f;
        *normal_y = 0.0f;
    } else {
        *normal_x = 0.0f;
        *normal_y = (vel.y > 0.0f) ? -1.0f : 1.0f;
    }

    return entry_time;
}

void update_gameplay(GameState* gs, Uint64 unscaled_delta_ms) {
    if (gs->paused) return;

    if (gs->show_speed_timer > 0) {
        if (unscaled_delta_ms >= gs->show_speed_timer) {
            gs->show_speed_timer = 0;
        } else {
            gs->show_speed_timer -= unscaled_delta_ms;
        }
    }

    Uint64 delta_ms = unscaled_delta_ms * gs->game_speed;
    float delta_seconds = delta_ms / 1000.0f;

    float target_vel_x = 0.0f;
    if (gs->left_pressed && !gs->right_pressed) {
        target_vel_x = -PADDLE_SPEED;
    } else if (gs->right_pressed && !gs->left_pressed) {
        target_vel_x = PADDLE_SPEED;
    }

    if (target_vel_x != 0) {
        gs->paddle_vel_x += (target_vel_x - gs->paddle_vel_x) * PADDLE_ACCELERATION * delta_seconds;
    } else {
        gs->paddle_vel_x = 0;
    }

    gs->paddle.x += gs->paddle_vel_x * delta_seconds;

    if (gs->paddle.x < BORDER_THICKNESS) {
        gs->paddle.x = BORDER_THICKNESS;
    }
    if (gs->paddle.x > SCREEN_WIDTH - gs->paddle.w - BORDER_THICKNESS) {
        gs->paddle.x = SCREEN_WIDTH - gs->paddle.w - BORDER_THICKNESS;
    }

    bool is_sticky_paddle_active = gs->sticky_paddle_timer_ms > 0;

    for (int k = 0; k < MAX_BALLS; k++) {
        if (!gs->balls[k].active) continue;
        if (gs->balls[k].is_stuck) {
            gs->balls[k].rect.x = gs->paddle.x + gs->balls[k].stuck_offset_x;
            gs->balls[k].rect.y = gs->paddle.y - BALL_SIZE;
            continue;
        }

        if (gs->ball_launched) {
            float remaining_time = delta_seconds;
            
            while (remaining_time > 0.00001f) {
                float min_collision_time = remaining_time;
                float combined_normal_x = 0.0f, combined_normal_y = 0.0f;
                int num_collisions = 0;

                Brick* colliding_bricks[BRICK_ROWS * BRICK_COLS] = {NULL};
                int num_colliding_bricks = 0;
                bool paddle_collided = false;

                SDL_FPoint vel = {gs->balls[k].vel_x, gs->balls[k].vel_y};

                // Brick collision
                for (int i = 0; i < BRICK_ROWS; i++) {
                    for (int j = 0; j < BRICK_COLS; j++) {
                        if (gs->bricks[i][j].active && gs->bricks[i][j].animation_frame == 0) {
                            float nx, ny;
                            float t = swept_aabb(gs->balls[k].rect, vel, gs->bricks[i][j].rect, &nx, &ny);
                            if (t < min_collision_time) {
                                min_collision_time = t;
                                combined_normal_x = nx;
                                combined_normal_y = ny;
                                num_collisions = 1;
                                paddle_collided = false;
                                num_colliding_bricks = 1;
                                colliding_bricks[0] = &gs->bricks[i][j];
                            } else if (t == min_collision_time) {
                                combined_normal_x += nx;
                                combined_normal_y += ny;
                                num_collisions++;
                                colliding_bricks[num_colliding_bricks++] = &gs->bricks[i][j];
                            }
                        }
                    }
                }

                // Paddle collision
                if (SDL_GetTicks() - gs->balls[k].last_collision_time > PADDLE_COLLISION_COOLDOWN) {
                    float nx, ny;
                    float t = swept_aabb(gs->balls[k].rect, vel, gs->paddle, &nx, &ny);
                    if (t < min_collision_time) {
                        min_collision_time = t;
                        num_collisions = 1;
                        paddle_collided = true;
                        num_colliding_bricks = 0;
                    } else if (t == min_collision_time) {
                        paddle_collided = true;
                        num_collisions++;
                    }
                }

                // Wall collisions
                SDL_FRect walls[] = {
                    {0, TOP_MARGIN - 10, SCREEN_WIDTH, 10}, // Top
                    {BORDER_THICKNESS - 10, 0, 10, SCREEN_HEIGHT}, // Left
                    {SCREEN_WIDTH - BORDER_THICKNESS, 0, 10, SCREEN_HEIGHT} // Right
                };
                for (int i = 0; i < 3; i++) {
                    float nx, ny;
                    float t = swept_aabb(gs->balls[k].rect, vel, walls[i], &nx, &ny);
                    if (t < min_collision_time) {
                        min_collision_time = t;
                        combined_normal_x = nx;
                        combined_normal_y = ny;
                        num_collisions = 1;
                        paddle_collided = false;
                        num_colliding_bricks = 0;
                    } else if (t == min_collision_time) {
                        combined_normal_x += nx;
                        combined_normal_y += ny;
                        num_collisions++;
                    }
                }

                gs->balls[k].rect.x += gs->balls[k].vel_x * min_collision_time;
                gs->balls[k].rect.y += gs->balls[k].vel_y * min_collision_time;
                
                if (num_collisions > 0) {
                    if (paddle_collided) {
                        gs->balls[k].last_collision_time = SDL_GetTicks();
                        if (is_sticky_paddle_active) {
                            gs->balls[k].is_stuck = true;
                            gs->balls[k].stuck_offset_x = gs->balls[k].rect.x - gs->paddle.x;
                            gs->balls[k].vel_x = 0;
                            gs->balls[k].vel_y = 0;
                            break; 
                        } else {
                            launch_ball(&gs->balls[k], gs->paddle.x, gs->paddle.w);
                        }
                    } else {
                        for (int i = 0; i < num_colliding_bricks; i++) {
                            Brick* brick = colliding_bricks[i];
                            if (brick && brick->animation_frame == 0) {
                                brick->animation_frame = 1;
                                brick->animation_timer = 0;
                                spawn_powerup(gs, brick->rect.x + (BRICK_WIDTH / 2) - (POWERUP_SIZE / 2), brick->rect.y + (BRICK_HEIGHT / 2) - (POWERUP_SIZE / 2));
                            }
                        }

                        float magnitude = sqrtf(combined_normal_x * combined_normal_x + combined_normal_y * combined_normal_y);
                        if (magnitude > 0.0f) {
                            float normalized_x = combined_normal_x / magnitude;
                            float normalized_y = combined_normal_y / magnitude;
                            
                            float dot_product = gs->balls[k].vel_x * normalized_x + gs->balls[k].vel_y * normalized_y;
                            gs->balls[k].vel_x -= 2 * dot_product * normalized_x;
                            gs->balls[k].vel_y -= 2 * dot_product * normalized_y;
                        }
                    }
                }
                
                remaining_time -= min_collision_time;
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
    }

    bool all_bricks_destroyed = true;
    for (int i = 0; i < BRICK_ROWS; i++) {
        for (int j = 0; j < BRICK_COLS; j++) {
            if (gs->bricks[i][j].active) {
                all_bricks_destroyed = false;
                break;
            }
        }
        if (!all_bricks_destroyed) break;
    }

    if (all_bricks_destroyed) {
        reset_game(gs);
    }

    if (!gs->ball_launched) {
        gs->balls[0].rect.x = gs->paddle.x + (gs->paddle.w / 2) - (BALL_SIZE / 2);
        gs->balls[0].rect.y = gs->paddle.y - BALL_SIZE;
    }

    // Update powerups
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (gs->powerups[i].active) {
            gs->powerups[i].rect.y += POWERUP_SPEED * delta_seconds;
            if (SDL_HasRectIntersectionFloat(&gs->powerups[i].rect, &gs->paddle)) {
                gs->powerups[i].active = false;
                if (gs->powerups[i].type == POWERUP_ADD_LIFE) {
                    gs->lives++;
                } else if (gs->powerups[i].type == POWERUP_REMOVE_LIFE) {
                    gs->lives--;
                } else if (gs->powerups[i].type == POWERUP_PADDLE_WIDER) {
                    if (gs->paddle_size_level < 3) {
                        gs->paddle_size_level++;
                    }
                } else if (gs->powerups[i].type == POWERUP_PADDLE_NARROWER) {
                    if (gs->paddle_size_level > -3) {
                        gs->paddle_size_level--;
                    }
                } else if (gs->powerups[i].type == POWERUP_STICKY_PADDLE) {
                    gs->sticky_paddle_timer_ms = 15000;
                } else if (gs->powerups[i].type == POWERUP_BALL_SPLIT) {
                    int first_active_ball = -1;
                    for (int l = 0; l < MAX_BALLS; l++) {
                        if (gs->balls[l].active && !gs->balls[l].is_stuck) {
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

    // Update brick animations
    for (int i = 0; i < BRICK_ROWS; i++) {
        for (int j = 0; j < BRICK_COLS; j++) {
            if (gs->bricks[i][j].active && gs->bricks[i][j].animation_frame > 0) {
                gs->bricks[i][j].animation_timer += delta_ms;
                if (gs->bricks[i][j].animation_timer > BRICK_ANIMATION_SPEED) {
                    gs->bricks[i][j].animation_frame++;
                    gs->bricks[i][j].animation_timer -= BRICK_ANIMATION_SPEED;
                    if (gs->bricks[i][j].animation_frame > 10) {
                        gs->bricks[i][j].active = false;
                    }
                }
            }
        }
    }
    
    if (gs->sticky_paddle_timer_ms > 0) {
        if (unscaled_delta_ms >= gs->sticky_paddle_timer_ms) {
            gs->sticky_paddle_timer_ms = 0;
        } else {
            gs->sticky_paddle_timer_ms -= unscaled_delta_ms;
        }

        if (gs->sticky_paddle_timer_ms == 0) {
            for (int i = 0; i < MAX_BALLS; i++) {
                if (gs->balls[i].active && gs->balls[i].is_stuck) {
                    launch_ball(&gs->balls[i], gs->paddle.x, gs->paddle.w);
                }
            }
        }
    }

    // Update force field animation
    if (is_sticky_paddle_active) {
        gs->force_field_anim_timer += delta_ms;
        gs->force_field_y_offset = sinf(gs->force_field_anim_timer / 200.0f) * 3.0f;

        // Spawn particles
        for (int j = 0; j < MAX_PARTICLES; j++) {
            if (gs->particles[j].lifetime_ms <= 0) {
                gs->particles[j].lifetime_ms = 1000;
                float left_x = gs->paddle.x - 13 + 12;
                float right_x = gs->paddle.x + gs->paddle.w - 10 + 12;
                gs->particles[j].pos.x = left_x + (rand() / (float)RAND_MAX) * (right_x - left_x);
                gs->particles[j].pos.y = gs->paddle.y - 5 + gs->force_field_y_offset;
                gs->particles[j].vel.x = 0;
                gs->particles[j].vel.y = -0.025f - (rand() / (float)RAND_MAX) * 0.025f;
                gs->particles[j].color.r = 100 + rand() % 50;
                gs->particles[j].color.g = 150 + rand() % 50;
                gs->particles[j].color.b = 255;
                gs->particles[j].color.a = 255;
                break;
            }
        }
    }

    // Update particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (gs->particles[i].lifetime_ms > 0) {
            gs->particles[i].pos.x += gs->particles[i].vel.x * delta_ms;
            gs->particles[i].pos.y += gs->particles[i].vel.y * delta_ms;
            gs->particles[i].lifetime_ms -= delta_ms;
            if (gs->particles[i].lifetime_ms < 0) {
                gs->particles[i].lifetime_ms = 0;
            }
            gs->particles[i].color.a = (gs->particles[i].lifetime_ms / 1000.0f) * 255;
        }
    }
}

void render_gameplay(GameState* gs) {
    float scale = 2.0f;
    SDL_SetRenderDrawColor(gs->renderer, 0, 0, 0, 255);
    SDL_RenderClear(gs->renderer);

    if (!gs->ball_launched && !gs->paused) {
        SDL_Color white = {255, 255, 255, 255};
        SDL_Color gray = {192, 192, 192, 255};

        const char* text1 = "USE ";
        const char* text2 = "ARROWS";
        const char* text3 = " TO MOVE AND ";
        const char* text4 = "SPACE";
        const char* text5 = " TO SHOOT";

        SDL_Surface* s1 = TTF_RenderText_Blended(gs->font, text1, 0, gray);
        SDL_Surface* s2 = TTF_RenderText_Blended(gs->font, text2, 0, white);
        SDL_Surface* s3 = TTF_RenderText_Blended(gs->font, text3, 0, gray);
        SDL_Surface* s4 = TTF_RenderText_Blended(gs->font, text4, 0, white);
        SDL_Surface* s5 = TTF_RenderText_Blended(gs->font, text5, 0, gray);

        float total_width = s1->w + s2->w + s3->w + s4->w + s5->w;
        float current_x = (SCREEN_WIDTH - total_width) / 2.0f;
        float y = TOP_MARGIN + (SCREEN_HEIGHT - TOP_MARGIN - s1->h) / 2.0f + 80.0f;

        SDL_Texture* t1 = SDL_CreateTextureFromSurface(gs->renderer, s1);
        SDL_FRect r1 = { current_x, y, s1->w, s1->h };
        SDL_RenderTexture(gs->renderer, t1, NULL, &r1);
        current_x += s1->w;

        SDL_Texture* t2 = SDL_CreateTextureFromSurface(gs->renderer, s2);
        SDL_FRect r2 = { current_x, y, s2->w, s2->h };
        SDL_RenderTexture(gs->renderer, t2, NULL, &r2);
        current_x += s2->w;

        SDL_Texture* t3 = SDL_CreateTextureFromSurface(gs->renderer, s3);
        SDL_FRect r3 = { current_x, y, s3->w, s3->h };
        SDL_RenderTexture(gs->renderer, t3, NULL, &r3);
        current_x += s3->w;

        SDL_Texture* t4 = SDL_CreateTextureFromSurface(gs->renderer, s4);
        SDL_FRect r4 = { current_x, y, s4->w, s4->h };
        SDL_RenderTexture(gs->renderer, t4, NULL, &r4);
        current_x += s4->w;

        SDL_Texture* t5 = SDL_CreateTextureFromSurface(gs->renderer, s5);
        SDL_FRect r5 = { current_x, y, s5->w, s5->h };
        SDL_RenderTexture(gs->renderer, t5, NULL, &r5);

        SDL_DestroyTexture(t1);
        SDL_DestroyTexture(t2);
        SDL_DestroyTexture(t3);
        SDL_DestroyTexture(t4);
        SDL_DestroyTexture(t5);
        SDL_DestroySurface(s1);
        SDL_DestroySurface(s2);
        SDL_DestroySurface(s3);
        SDL_DestroySurface(s4);
        SDL_DestroySurface(s5);
    }

    // Draw borders
    SDL_SetRenderDrawColor(gs->renderer, 192, 192, 192, 255);
    SDL_FRect top_border = {0, TOP_MARGIN - BORDER_THICKNESS, SCREEN_WIDTH, BORDER_THICKNESS};
    SDL_RenderFillRect(gs->renderer, &top_border);
    SDL_FRect left_border = {0, 0, BORDER_THICKNESS, SCREEN_HEIGHT};
    SDL_RenderFillRect(gs->renderer, &left_border);
    SDL_FRect right_border = {SCREEN_WIDTH - BORDER_THICKNESS, 0, BORDER_THICKNESS, SCREEN_HEIGHT};
    SDL_RenderFillRect(gs->renderer, &right_border);

    // Draw paddle
    if (gs->debug_mode && gs->debug_render_collisions) {
        SDL_SetRenderDrawColor(gs->renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(gs->renderer, &gs->paddle);
    } else {
        bool is_sticky_paddle_active = gs->sticky_paddle_timer_ms > 0;

        SDL_FRect left_paddle_src = { 112, 48, 6, 14 };
        SDL_FRect right_paddle_src = { 138, 48, 6, 14 };
        SDL_FRect middle_paddle_src = { 118, 50, 20, 10 };

        float left_w = left_paddle_src.w * scale;
        float right_w = right_paddle_src.w * scale;
        float middle_h = middle_paddle_src.h * scale;

        SDL_FRect left_paddle_dest = { gs->paddle.x, gs->paddle.y - 4, left_w, 28 };
        SDL_FRect right_paddle_dest = { gs->paddle.x + gs->paddle.w - right_w, gs->paddle.y - 4, right_w, 28 };
        SDL_FRect middle_paddle_dest = { gs->paddle.x + left_w, gs->paddle.y + (PADDLE_HEIGHT - middle_h) / 2.0f, gs->paddle.w - left_w - right_w, middle_h };

        SDL_RenderTexture(gs->renderer, gs->spritesheet, &left_paddle_src, &left_paddle_dest);
        SDL_RenderTexture(gs->renderer, gs->spritesheet, &right_paddle_src, &right_paddle_dest);
        SDL_RenderTexture(gs->renderer, gs->spritesheet, &middle_paddle_src, &middle_paddle_dest);

        if (is_sticky_paddle_active) {
            SDL_FRect sticky_src = { 132, 16, 12, 16 };
            SDL_FRect sticky_dest_left = { gs->paddle.x - 13, gs->paddle.y - 5, 12 * scale, 16 * scale };
            SDL_RenderTexture(gs->renderer, gs->spritesheet, &sticky_src, &sticky_dest_left);

            SDL_FRect sticky_dest_right = { gs->paddle.x + gs->paddle.w - 10, gs->paddle.y - 5, 12 * scale, 16 * scale };
            SDL_RenderTextureRotated(gs->renderer, gs->spritesheet, &sticky_src, &sticky_dest_right, 0, NULL, SDL_FLIP_HORIZONTAL);

            // Draw force field
            float left_x = sticky_dest_left.x + sticky_dest_left.w / 2;
            float right_x = sticky_dest_right.x + sticky_dest_right.w / 2;
            float y = sticky_dest_left.y + 2 + gs->force_field_y_offset;
            
            Uint8 r = 100 + sinf(gs->force_field_anim_timer / 150.0f) * 50;
            Uint8 g = 150 + sinf(gs->force_field_anim_timer / 180.0f) * 50;
            SDL_SetRenderDrawColor(gs->renderer, r, g, 255, 150);
            SDL_RenderLine(gs->renderer, left_x, y, right_x, y);
            SDL_RenderLine(gs->renderer, left_x, y+1, right_x, y+1);
        }
    }

    // Draw particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (gs->particles[i].lifetime_ms > 0) {
            SDL_SetRenderDrawColor(gs->renderer, gs->particles[i].color.r, gs->particles[i].color.g, gs->particles[i].color.b, gs->particles[i].color.a);
            SDL_FRect particle_rect = { gs->particles[i].pos.x, gs->particles[i].pos.y, scale, scale };
            SDL_RenderFillRect(gs->renderer, &particle_rect);
        }
    }

    SDL_FRect ball_src_rect = { 50, 34, 12, 12 };
    for (int i = 0; i < MAX_BALLS; i++) {
        if (gs->balls[i].active) {
            if (gs->debug_mode && gs->debug_render_collisions) {
                SDL_SetRenderDrawColor(gs->renderer, 0, 255, 0, 255);
                SDL_RenderFillRect(gs->renderer, &gs->balls[i].rect);
            } else {
                SDL_RenderTexture(gs->renderer, gs->spritesheet, &ball_src_rect, &gs->balls[i].rect);
            }
        }
    }

    for (int i = 0; i < BRICK_ROWS; i++) {
        for (int j = 0; j < BRICK_COLS; j++) {
            if (gs->bricks[i][j].active) {
                if (gs->debug_mode && gs->debug_render_collisions) {
                    SDL_SetRenderDrawColor(gs->renderer, 0, 0, 255, 255);
                    SDL_RenderFillRect(gs->renderer, &gs->bricks[i][j].rect);
                } else {
                    int frame = gs->bricks[i][j].animation_frame;
                    int src_x = 32 + (frame * 32);
                    int src_y = 176 + i * 16;
                    SDL_FRect src_rect = { src_x, src_y, 32, 16 };
                    SDL_RenderTexture(gs->renderer, gs->spritesheet, &src_rect, &gs->bricks[i][j].rect);
                }
            }
        }
    }

    int balls_per_col = (TOP_MARGIN - 2 * BORDER_THICKNESS) / (BALL_SIZE + 3);
    for (int i = 0; i < gs->lives; i++) {
        int col = i / balls_per_col;
        int row = i % balls_per_col;
        SDL_FRect life_ball = {
            SCREEN_WIDTH - BORDER_THICKNESS - 5 - (col + 1) * (BALL_SIZE + 3) + 3,
            BORDER_THICKNESS + 5 + row * (BALL_SIZE + 3),
            BALL_SIZE,
            BALL_SIZE
        };
        SDL_RenderTexture(gs->renderer, gs->spritesheet, &ball_src_rect, &life_ball);
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
            } else if (gs->powerups[i].type == POWERUP_STICKY_PADDLE) {
                float x = gs->powerups[i].rect.x;
                float y = gs->powerups[i].rect.y;
                float w = gs->powerups[i].rect.w;
                float h = gs->powerups[i].rect.h;
                SDL_RenderLine(gs->renderer, x + w/4, y, x + w/4, y + h);
                SDL_RenderLine(gs->renderer, x + 3*w/4, y, x + 3*w/4, y + h);
                SDL_RenderLine(gs->renderer, x, y + h/4, x + w, y + h/4);
                SDL_RenderLine(gs->renderer, x, y + 3*h/4, x + w, y + 3*h/4);
            }
        }
    }

    if (gs->paused) {
        SDL_Color text_color = {255, 255, 255, 255};
        SDL_Surface* text_surface = TTF_RenderText_Blended(gs->font, "PAUSED", 0, text_color);
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

    if (gs->show_speed_timer > 0) {
        SDL_Color text_color = {255, 255, 255, 255};
        char speed_text[20];
        snprintf(speed_text, 20, "SPEED %.0f%%", gs->game_speed * 100);
        SDL_Surface* text_surface = TTF_RenderText_Blended(gs->font, speed_text, 0, text_color);
        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(gs->renderer, text_surface);
        SDL_FRect text_rect = {
            (SCREEN_WIDTH - text_surface->w) / 2.0f,
            (SCREEN_HEIGHT - text_surface->h) / 2.0f + 30,
            text_surface->w,
            text_surface->h
        };
        SDL_RenderTexture(gs->renderer, text_texture, NULL, &text_rect);
        SDL_DestroyTexture(text_texture);
        SDL_DestroySurface(text_surface);
    }

    if (gs->debug_mode) {
        SDL_Color text_color = {255, 255, 255, 255};
        SDL_Surface* text_surface = TTF_RenderText_Blended(gs->font, "DEBUG", 0, text_color);
        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(gs->renderer, text_surface);
        SDL_FRect text_rect = {
            5,
            SCREEN_HEIGHT - text_surface->h - 5,
            text_surface->w,
            text_surface->h
        };
        SDL_RenderTexture(gs->renderer, text_texture, NULL, &text_rect);
        SDL_DestroyTexture(text_texture);
        SDL_DestroySurface(text_surface);
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
    gs.font = TTF_OpenFont("assets/NotoSansMono-Regular.ttf", 20);
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
        Uint64 delta_ms = current_time - gs.last_frame_time;
        gs.last_frame_time = current_time;

        switch (gs.current_screen) {
            case SCREEN_TITLE:
                handle_events_title(&gs);
                render_title_screen(&gs);
                break;
            case SCREEN_GAMEPLAY:
                handle_events_gameplay(&gs);
                update_gameplay(&gs, delta_ms);
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
