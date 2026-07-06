#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#ifndef __EMSCRIPTEN__
#include <stdio.h>
#include <string.h>
#endif


static inline uint32_t rng_seed(uint32_t x) {
    for (int i = 0; i < 4; ++i) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
    }
    return x;
}

static inline uint32_t rng_u32(uint32_t x) {
    /* xorshift32 */
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static inline uint32_t rng_range(uint32_t *rng_state, uint32_t n) {
    /* multiply-high mapping: (rng_u32() * n) >> 32 */
    uint32_t x = rng_u32(*rng_state);
    *rng_state = x;
    uint64_t prod = (uint64_t)x * (uint64_t)n;
    return (uint32_t)(prod >> 32);
}

static inline int popcount_u32(uint32_t x) {
    int cnt = 0;
    while (x) {
        x &= x - 1;
        cnt++;
    }
    return cnt;
}

typedef const char *(*hex_cell_cb)(int x, int y, int w, void *userdata);

#ifndef __EMSCRIPTEN__
void hexboard_render(FILE *out, int width, int height, hex_cell_cb cb, void *userdata)
{
    if (!out || width <= 0 || height <= 0 || !cb) return;

    for (int y = 0; y < height; ++y) {
        int odd = y & 1;

        /* Middle line with cell content */
        if (odd) fputs(" ", out);
        for (int x = 0; x + x + odd < width; ++x) {
            const char *cell = cb(x, y, width, userdata);
            if (!cell) cell = "  ";
            fputs(cell, out);
        }
        fputc('\n', out);
    }
}
#endif

static void hexboard_rel_rot(int x, int y, int dirs, int ndirs, int out[2], int rot) {
    int cx = x, cy = y;
    for (int i = 0; i < ndirs; ++i) {
        int dir = (dirs >> (i * 4)) & 0x7;
        int odd = cy & 1;
        int dx = 0, dy = 0;
        if (dir < 6) dir = (dir + rot) % 6;
        switch (dir) {
        case 0: /* E */
            dx = 1; dy = 0;
            break;
        case 1: /* NE */
            dx = odd ? 1 : 0; dy = -1;
            break;
        case 2: /* NW */
            dx = odd ? 0 : -1; dy = -1;
            break;
        case 3: /* W */
            dx = -1; dy = 0;
            break;
        case 4: /* SW */
            dx = odd ? 0 : -1; dy = 1;
            break;
        case 5: /* SE */
            dx = odd ? 1 : 0; dy = 1;
            break;
        default:
            dx = 0; dy = 0;
            break;
        }
        cx += dx;
        cy += dy;
    }
    out[0] = cx;
    out[1] = cy;
}
static void hexboard_rel(int x, int y, int dirs, int ndirs, int out[2]) {
    hexboard_rel_rot(x, y, dirs, ndirs, out, 0);
}

#define TOKEN_EMPTY 0
#define TOKEN_RIVER 1
#define TOKEN_PLAINS 2
#define TOKEN_WOOD 3
#define TOKEN_MOUNTAIN 4
#define TOKEN_TREE 5
#define TOKEN_BUILDING 6
#define TOKEN_FINISHED_BUILDING 7
#define TOKEN_2X_MOUNTAIN 8
#define TOKEN_3X_MOUNTAIN 9
#define TOKEN_2X_TREE 10
#define TOKEN_3X_TREE 11
#define TOKEN_2X_WOOD 12
#define TOKEN_MASK 15
#define TOKEN_ANIMAL 16
#define TOKEN_SPIRIT 32
#define TOKEN_INPUT 64
#ifndef __EMSCRIPTEN__
static const char *token_to_string[32] = {
    /* empty, gray bg*/ "\x1b[47m  \x1b[0m",
    /* 1 = river, blue bg, river kanji*/ "\x1b[44;1m川\x1b[0m",
    /* 2 = plains, yellow bg, field kanji*/ "\x1b[43;1m田\x1b[0m",
    /* 3 = wood, brown bg, wooden kanji*/ "\x1b[48:5:88;1m木\x1b[0m",
    /* 4 = mountain, gray bg, mountain kanji*/ "\x1b[100;1m山\x1b[0m",
    /* 5 = tree, green bg, tree kanji*/ "\x1b[42;1m木\x1b[0m",
    /* 6 = building, red bg, stone kanji*/ "\x1b[41;1m石\x1b[0m",
    /* 7 = finished building, red bg, house kanji*/ "\x1b[41;1m館\x1b[0m",
    /* 8 = 2x mountain, gray bg, 2 mountains kanji*/ "\x1b[100;1m屾\x1b[0m",
    /* 9 = 3x mountain, gray bg, 3 mountains kanji*/ "\x1b[100;1m嶺\x1b[0m",
    /* 10 = 2x tree, green bg, 2 trees kanji*/ "\x1b[42;1m林\x1b[0m",
    /* 11 = 3x tree, green bg, 3 trees kanji*/ "\x1b[42;1m森\x1b[0m",
    /* 12 = 2x wood, brown bg, 2 woods kanji*/ "\x1b[45;1m林\x1b[0m",
    // 13 - 16 unused
    0,0,0,0,
    /* 17 = river, blue bg, river kanji*/ "\x1b[44m川\x1b[0m",
    /* 18 = plains, yellow bg, field kanji*/ "\x1b[43m田\x1b[0m",
    /* 19 = wood, brown bg, wooden kanji*/ "\x1b[45m木\x1b[0m",
    /* 20 = mountain, gray bg, mountain kanji*/ "\x1b[100m山\x1b[0m",
    /* 21 = tree, green bg, tree kanji*/ "\x1b[42m木\x1b[0m",
    /* 22 = building, red bg, stone kanji*/ "\x1b[41m石\x1b[0m",
    /* 23 = finished building, red bg, house kanji*/ "\x1b[41m館\x1b[0m",
    /* 24 = 2x mountain, gray bg, 2 mountains kanji*/ "\x1b[100m屾\x1b[0m",
    /* 25 = 3x mountain, gray bg, 3 mountains kanji*/ "\x1b[100m嶺\x1b[0m",
    /* 26 = 2x tree, green bg, 2 trees kanji*/ "\x1b[42m林\x1b[0m",
    /* 27 = 3x tree, green bg, 3 trees kanji*/ "\x1b[42m森\x1b[0m",
    /* 28 = 2x wood, brown bg, 2 woods kanji*/ "\x1b[45m林\x1b[0m",
};
#endif
static uint8_t token_to_color[13] = {
    0, 1, 2, 3, 4, 5, 6, 6, 4, 4, 5, 5, 3
};
static uint8_t token_to_height[13] = {
    0, 1, 1, 1, 1, 1, 1, 2, 2, 3, 2, 3, 2
};

#define ACTION_CHOOSE_TOKENS 1 // params 0-5
#define ACTION_PLACE_TOKEN 2 // params idx 0-24
#define ACTION_CHOOSE_ANIMAL 3 // params animal idx 0-4
#define ACTION_PLACE_ANIMAL 4 // params animal idx, board pos 0-3, 0-24 (2 bits + 5 bits)
#define ACTION_END_ROUND 5
#define ACTION_CHOOSE_SPIRIT 6 // params spirit idx 0-1

#define BASE_NORMAL_ANIMAL_COUNT 32
#define SPIRIT_ANIMAL_START BASE_NORMAL_ANIMAL_COUNT
#define SPIRIT_ANIMAL_COUNT 11
#define BASE_SPIRIT_ANIMAL_COUNT 10
#define TREX_SPIRIT_ID (SPIRIT_ANIMAL_START + BASE_SPIRIT_ANIMAL_COUNT)
#define DLC_NORMAL_ANIMAL_START (SPIRIT_ANIMAL_START + SPIRIT_ANIMAL_COUNT)
#define DLC_NORMAL_ANIMAL_COUNT 10
#define ANIMAL_DECK_CAPACITY (BASE_NORMAL_ANIMAL_COUNT + DLC_NORMAL_ANIMAL_COUNT)
#define TOTAL_ANIMAL_COUNT (BASE_NORMAL_ANIMAL_COUNT + SPIRIT_ANIMAL_COUNT + DLC_NORMAL_ANIMAL_COUNT)

#define MAX_ANIMAL_PROGRESS 5
struct animal {
    uint8_t token;
    struct env {
        uint8_t token;
        uint8_t pos_a;
        uint8_t pos_b;
    } env[3];
    int8_t score[MAX_ANIMAL_PROGRESS];
} animals[TOTAL_ANIMAL_COUNT] = {
    { 1, { { 1, 0xF3 }, { 11, 0x33 } }, { 4, 5, 6 } }, // Crocodile
    { 1, { { 4, 0xF4 }, {  4, 0xF3 } }, { 4, 6, 6 } }, // Ray
    { 1, { { 9, 0xF3 }               }, { 3, 3, 4, 6 } }, // Salmon
    { 1, { { 5, 0xF3 }, {  5, 0x33 } }, { 5, 5, 6 } }, // Otter
    { 1, { { 5, 0xF3 }               }, { 2, 2, 2, 4, 5 } }, // Frog
    { 1, { { 7, 0xF3 }               }, { 2, 2, 4, 5 } }, // Duck
    { 1, { { 2, 0xF4 }, {  2, 0xF3 } }, { 4, 6, 6 } }, // Flamingo
    { 7, { { 2, 0xF3 }, {  2, 0x33 } }, { 5, 5, 6 } }, // Gecko
    { 7, { { 2, 0xF0 }, {  2, 0xF4 } }, { 5, 5, 7 } }, // Shrew
    { 7, { { 1, 0xF0 }, {  1, 0xF4 } }, { 5, 5, 7 } }, // Peacock

    { 7, { { 11, 0xF3 }              }, { 4, 5, 6 } }, // Squirrel
    { 7, { { 10, 0xF4 }, { 10, 0xF3 } }, { 5, 7 } }, // Hedgehog
    { 10, { { 2, 0xF0 }, { 2, 0xF5 }, { 2, 0xF4 }}, { 8, 10 } }, // Bee
    { 5, { { 8, 0xF4 }, { 8, 0xF3 } }, { 5, 6 } }, // Bear
    { 5, { { 5, 0xF3 }, { 7, 0x33 } }, { 5, 5, 7 } }, // Rabbit
    { 10, { { 1, 0xF4 }, { 1, 0xF3 } }, { 4, 5, 5 } }, // Macaw
    { 10, { { 7, 0xF3 }              }, { 4, 4, 5 } }, // Boar
    { 10, { { 5, 0xF3 }              }, { 3, 3, 4, 5 } }, // Koala
    { 11, { { 2, 0xF4 }, { 2, 0xF3 } }, { 4, 6, 6 } }, // Wolfdfs
    { 11, { { 1, 0xF0 }, { 1, 0xF4 } }, { 5, 6, 7 } }, // Kingfisher

    { 4, { { 1, 0xF0 }, { 1, 0xF4 }}, { 4, 6, 6 } }, // Penguin
    { 4, { { 11, 0xF3 } }, { 3, 3, 4, 5 }}, // Bat
    { 4, { { 4, 0xF3 }, { 2, 0x33 }}, { 4, 5, 7 }}, // Fennec
    { 8, { { 1, 0xF4 }, { 1, 0xF3 }}, { 5, 6 }}, // Macaque
    { 9, { { 2, 0xF3 }             }, { 5, 6 }}, // Bateleur
    { 4, { { 2, 0xF3 }             }, { 2, 3, 4, 5 }}, // Meerkat
    { 2, { { 7, 0xF0 }, { 7, 0xF4 }}, { 4, 5 }}, // Raven
    { 2, { { 2, 0xF3 }, { 8, 0x33 }}, { 5, 7 }}, // Alpaca
    { 2, { { 10, 0xF0 }, { 10, 0xF4 }}, { 5, 5, 7 }}, // Arctic Fox
    { 2, { { 1, 0xF0 }, { 1, 0xF5 }, { 1, 0xF4 }}, { 6, 6 }}, // Raccoon

    { 2, { { 5, 0xF3 }             }, { 2, 3, 3, 4, 5 }}, // Ladybug
    { 2, { { 10, 0xF3 }, { 10, 0x33 }}, { 5, 6 }}, // Panther

    { 2, { { 2, 0xF0 }, { 10, 0xF3 }}, { -1, 2, 2, 10 } }, // Lion
    { 2, { { 1, 0xF3 }, { 1, 0xF4 }, { 2, 0x34 }}, { -1, 5, 5, 5 } }, // Butterfly
    { 10, { { 5, 0xF0 }, { 11, 0xF3 }}, { -2, 0, 4, 4 }}, // Elk
    { 11, { { 5, 0xF4 }, { 5, 0xF3 }}, { -2, 3, 3, 1 }}, // Owl
    { 7, { { 5, 0xF3 }, { 7, 0x33 }}, { -1, 4, 4, 4 }}, // Cat
    { 7, { { 7, 0xF3 }, { 2, 0x33 }}, { -1, 0, 6, 6 }}, // Stork
    { 9, { { 8, 0xF3 }}, { -2, 0, 4, 4 }}, // Goat
    { 8, { { 4, 0xF0 }, { 4, 0xF4 }}, { -2, 3, 3, 1 }}, // Beaver
    { 1, { { 10, 0xF0 }, { 10, 0xF3 }}, { -1, 0, 7, 7 }}, // Dragonfly
    { 1, { { 1, 0xF0 }, { 8, 0xF3 }}, { -2, 2, 0, 0 }}, // Tortoise
    { 2, { { 9, 0xF0 }, { 11, 0xF3 } }, { -3 } }, // T-Rex promo spirit

    // Harmonies: Pulse DLC normal animals (single card, optional dual composition)
    { 1, { { 11, 0xF4, 0xF3 }, { 2, 0x34, 0x34 } }, { 6, 7 } }, // Piranha
    { 1, { { 10, 0xF3, 0xF4 }, { 8, 0xF4, 0xF3 } }, { 7, 9 } }, // Axolotl
    { 7, { { 4, 0xF3, 0xF4 }, { 5, 0xF4, 0xF3 } }, { 4, 6, 7 } }, // Robin
    { 7, { { 2, 0xF4, 0xF3 }, { 1, 0x34, 0x34 } }, { 5, 6 } }, // Seagull
    { 5, { { 4, 0xF4, 0xF3 }, { 1, 0x34, 0x34 } }, { 3, 7, 6 } }, // Tiger
    { 5, { { 1, 0xF3, 0xF5 }, { 2, 0xF5, 0xF3 } }, { 4, 4, 7 } }, // Chameleon
    { 4, { { 7, 0xF3, 0xF5 }, { 2, 0xF5, 0xF3 } }, { 4, 5, 7 } }, // Camel
    { 8, { { 2, 0xF3 }, { 1, 0xF0 } }, { 7, 9 } }, // Yak
    { 2, { { 4, 0xF3, 0xF4 }, { 11, 0xF4, 0xF3 } }, { 5, 8 } }, // Panda
    { 2, { { 4, 0xF3 }, { 7, 0xF0 } }, { 5, 7 } }, // Beetle
};
struct state {
    uint32_t rng_state;
    uint16_t spirit_deck; // bitmap
    uint8_t round;
    uint8_t round_state;
    uint8_t animal_deck_size;
    uint8_t animal_deck[ANIMAL_DECK_CAPACITY]; // normal animal deck indices
    uint8_t token_bag[6]; // 120 tokens
    int8_t animals[5];
    int8_t token_supply[5][3];
    struct player {
        uint8_t board[25];
        int16_t animals[4]; // id + progress
        uint8_t score;
        int8_t spirit_state;
    } p[4];
} s;
#define MAX_PLAYER_COUNT 4
#define MAX_BOARD_SIZE 25
#define MAX_ACTIONS (MAX_BOARD_SIZE * 4 + 4)
static int player_count = 2;
static int board_size = 23;
static int board_width = 9;
#define WATER_SCORE_TYPE_RIVER 0
#define WATER_SCORE_TYPE_ISLANDS 1
static int water_score_type = 0;
static int use_spirits = 0;
static int use_dlc_animals = 0;
static int ai_playouts = 10000;

int board_get_neighbour2_rot(int idx, int dirs, int ndirs, int width, int rot);

static inline int is_spirit_animal_id(int animal_id) {
    return animal_id >= SPIRIT_ANIMAL_START && animal_id < SPIRIT_ANIMAL_START + SPIRIT_ANIMAL_COUNT;
}

static inline uint8_t env_pos_for_mode(const struct env *e, int mode) {
    if (mode == 0 || e->pos_b == 0) return e->pos_a;
    return e->pos_b;
}

static int animal_matches_at(struct state *s, int player_idx, int board_pos, const struct animal *a) {
    int has_alt_mode = 0;
    for (int e = 0; e < 3; ++e) {
        if (a->env[e].token && a->env[e].pos_b && a->env[e].pos_b != a->env[e].pos_a) {
            has_alt_mode = 1;
            break;
        }
    }
    int mode_count = has_alt_mode ? 2 : 1;
    for (int mode = 0; mode < mode_count; ++mode) {
        for (int r = 0; r < 6; ++r) {
            int e = 0;
            for (; e < 3; ++e) if (a->env[e].token) {
                uint8_t env_pos = env_pos_for_mode(&a->env[e], mode);
                int idx = board_get_neighbour2_rot(board_pos, env_pos, 2, board_width, r);
                if (idx < 0 || idx >= board_size || (s->p[player_idx].board[idx] & TOKEN_MASK) != a->env[e].token) {
                    break;
                }
            }
            if (e == 3) return 1;
        }
    }
    return 0;
}


void init_game(struct state *s, uint32_t seed)
{
    s->rng_state = rng_seed(seed);
    s->round = 0;
    for (int i = 0; i < ANIMAL_DECK_CAPACITY; ++i) {
        s->animal_deck[i] = 0;
    }
    for (int i = 0; i < BASE_NORMAL_ANIMAL_COUNT; ++i) {
        s->animal_deck[i] = i;
    }
    s->animal_deck_size = BASE_NORMAL_ANIMAL_COUNT;
    if (use_dlc_animals) {
        for (int i = 0; i < DLC_NORMAL_ANIMAL_COUNT; ++i) {
            s->animal_deck[s->animal_deck_size++] = DLC_NORMAL_ANIMAL_START + i;
        }
    }
    s->token_bag[0] = 23; // river
    s->token_bag[1] = 19; // plains
    s->token_bag[2] = 21; // wood
    s->token_bag[3] = 23; // mountain
    s->token_bag[4] = 19; // tree
    s->token_bag[5] = 15; // building
    // set animal and token supply to -1 (none)
    for (int i = 0; i < 5; ++i) {
        s->animals[i] = -1;
    }
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 3; ++j) {
            s->token_supply[i][j] = -1;
        }
    }
    // clear player states
    for (int p = 0; p < 4; ++p) {
        for (int i = 0; i < 25; ++i) {
            s->p[p].board[i] = 0;
        }
        for (int i = 0; i < 4; ++i) {
            s->p[p].animals[i] = -1;
        }
        s->p[p].score = 0;
        s->p[p].spirit_state = -1;
    }
    int spirit_pool_size = (use_spirits == 2) ? SPIRIT_ANIMAL_COUNT : BASE_SPIRIT_ANIMAL_COUNT;
    s->spirit_deck = (1 << spirit_pool_size) - 1;
}

uint32_t token_stack[64] = {
    0x654321, // empty
    0, // river
    0, // plains
    0x7A0C00, // wood
    0x708000, // mountain
    0, // tree
    0x700000, // building
    0, // finished building
    0x009000, // 2x mountain
    0, // 3x mountain
    0, // 2x tree
    0, // 3x tree
    0x0B0000, // 2x wood
};
static int stack_token(int prev_token, int token) {
    return (token_stack[prev_token] >> ((token - 1) * 4)) & 0xF;
    // if (!prev_token) return token;
    // if (token == TOKEN_MOUNTAIN) {
    //     if (prev_token == TOKEN_MOUNTAIN) return TOKEN_2X_MOUNTAIN;
    //     if (prev_token == TOKEN_2X_MOUNTAIN) return TOKEN_3X_MOUNTAIN;
    // }
    // if (token == TOKEN_TREE) {
    //     if (prev_token == TOKEN_WOOD) return TOKEN_2X_TREE;
    //     if (prev_token == TOKEN_2X_WOOD) return TOKEN_3X_TREE;
    // }
    // if (token == TOKEN_WOOD) {
    //     if (prev_token == TOKEN_WOOD) return TOKEN_2X_WOOD;
    // }
    // if (token == TOKEN_BUILDING) {
    //     if (prev_token == TOKEN_BUILDING) return TOKEN_FINISHED_BUILDING;
    //     if (prev_token == TOKEN_WOOD) return TOKEN_FINISHED_BUILDING;
    //     if (prev_token == TOKEN_MOUNTAIN) return TOKEN_FINISHED_BUILDING;
    // }
    // return TOKEN_EMPTY;
}
void init_round(struct state *s)
{
    s->round_state = 0;
    // refill animal slots
    for (int i = 0; i < 5; ++i) {
        if (s->animals[i] == -1 && s->animal_deck_size > 0) {
            int idx = rng_range(&s->rng_state, s->animal_deck_size);
            s->animals[i] = s->animal_deck[idx];
            s->animal_deck[idx] = s->animal_deck[--s->animal_deck_size];
        }
    }
    // refill token supply
    int tokens_left = 0;
    for (int t = 0; t < 6; ++t) {
        tokens_left += s->token_bag[t];
    }
    for (int t = 0; t < 5; ++t) {
        for (int j = 0; j < 3; ++j) {
            if (s->token_supply[t][j] == -1 && tokens_left > 0) {
                int idx = rng_range(&s->rng_state, tokens_left);
                tokens_left--;
                int token = -1;
                // find which token this is
                for (int tt = 0; tt < 6; ++tt) {
                    if (idx < s->token_bag[tt]) {
                        token = tt;
                        break;
                    }
                    idx -= s->token_bag[tt];
                }
                s->token_supply[t][j] = token + 1;
                s->token_bag[token]--;
            }
        }
    }
    if (use_spirits && s->round < player_count) {
        int spirit_count = popcount_u32(s->spirit_deck);
        int idx1 = rng_range(&s->rng_state, spirit_count);
        int idx2 = rng_range(&s->rng_state, spirit_count - 1);
        int spirit1 = -1, spirit2 = -1;
        for (int i = 0; i < 8 * sizeof(s->spirit_deck); ++i) if (s->spirit_deck & (1 << i)) if (idx1-- == 0) spirit1 = i;
        s->spirit_deck &= ~(1 << spirit1);
        for (int i = 0; i < 8 * sizeof(s->spirit_deck); ++i) if (s->spirit_deck & (1 << i)) if (idx2-- == 0) spirit2 = i;
        s->spirit_deck &= ~(1 << spirit2);
        s->p[s->round].animals[0] = (spirit1 + SPIRIT_ANIMAL_START) << 3;
        s->p[s->round].animals[1] = (spirit2 + SPIRIT_ANIMAL_START) << 3;
    }
}
static int board_xy_to_index(int x, int y, int width)
{
    int odd = y & 1;
    int even_count = (width + 1) / 2; /* number of cells in even rows */
    int odd_count = width / 2;      /* number of cells in odd rows */
    if (y < 0 || x < 0 || x >= (odd ? odd_count : even_count)) return -1;
    int index = (y / 2) * width + (odd ? even_count : 0) + x;
    return index;
}
static int board_index_to_xy(int index, int width, int out[2])
{
    int even_count = (width + 1) / 2; /* number of cells in even rows */
    int row = (index / width) * 2;
    int col = index % width;
    if (col >= even_count) {
        out[0] = col - even_count;
        out[1] = row + 1;
    } else {
        out[0] = col;
        out[1] = row;
    }
    return 0;
}
#ifndef __EMSCRIPTEN__
static void board_put_at(uint8_t *board, int w, int x, int y, uint8_t v) {
    board[board_xy_to_index(x, y, w)] = v;
}
static const char *demo_cb(int x, int y, int w, void *ud)
{
    int index = board_xy_to_index(x, y, w);
    int token = ((uint8_t *)ud)[index];
    if (token & TOKEN_INPUT) {
        static char buf[16];
        snprintf(buf, sizeof(buf), "\x1b[43;1m%02d\x1b[0m", token & (TOKEN_INPUT - 1));
        return buf;
    }
    return token_to_string[token & (TOKEN_MASK | TOKEN_ANIMAL)];
}


const char *supply_cb(int x, int y, int w, void *ud) {
    struct state *st = ud;
    int i = x/4;
    int j = x%4;
    if (j == 3) return "  ";
    int token = st->token_supply[i][j];
    if (token == -1) return "\x1b[47m--\x1b[0m";
    return token_to_string[token];
}
#endif

static int8_t neigbour[6][MAX_BOARD_SIZE];
void init_neigbours(int width) {
    for (int i = 0; i < board_size; ++i) {
        int xy[2];
        board_index_to_xy(i, width, xy);
        for (int d = 0; d < 6; ++d) {
            int nxy[2];
            hexboard_rel(xy[0], xy[1], d, 1, nxy);
            int nidx = board_xy_to_index(nxy[0], nxy[1], width);
            neigbour[d][i] = (nidx >= 0 && nidx < board_size) ? nidx : -1;
        }
    }
}
static int board_get_neighbour(int idx, int d, int width) {
    return neigbour[d][idx];
    // int xy[2];
    // board_index_to_xy(idx, width, xy);
    // hexboard_rel(xy[0], xy[1], d, 1, xy);
    // return board_xy_to_index(xy[0], xy[1], width);
}
int board_get_neighbour2_rot(int idx, int dirs, int ndirs, int width, int rot) {
    int dir1 = dirs & 0xF;
    int n1 = board_get_neighbour(idx, (dir1 + rot) % 6, width);
    if (n1 >= 0 && dirs < 0xF0) {
        int dir2 = (dirs >> 4) & 0xF;
        n1 = board_get_neighbour(n1, (dir2 + rot) % 6, width);
    }
    // int xy[2];
    // board_index_to_xy(idx, width, xy);
    // hexboard_rel_rot(xy[0], xy[1], dirs, ndirs, xy, rot);
    // int n2 = board_xy_to_index(xy[0], xy[1], width);
    return n1;
}

#ifndef __EMSCRIPTEN__
void print_state(struct state *s)
{
    printf("Round %d\n", s->round);
    printf("Animals supply:\n");
    uint8_t animal_display_board[500];
    for (int i = 0; i < (int)sizeof(animal_display_board); ++i)
        animal_display_board[i] = 0;
    for (int i = 0; i < 5; ++i) {
        if (s->animals[i] != -1) {
            struct animal *a = &animals[s->animals[i]];
            board_put_at(animal_display_board, 5*5*2, i * 5 + 2, 0, a->token | TOKEN_ANIMAL);
            for (int e = 0; e < 3; ++e) {
                if (a->env[e].token) {
                    int xy[2];
                        hexboard_rel(i * 5 + 2, 0, a->env[e].pos_a, 2, xy);
                    board_put_at(animal_display_board, 5*5*2, xy[0], xy[1], a->env[e].token);
                }
            }
        }
    }
    hexboard_render(stdout, 5*5*2, 2, demo_cb, animal_display_board);
    printf("Token supply: ");
    hexboard_render(stdout, 4*5*2, 1, supply_cb, s);
    for (int p = 0; p < player_count; ++p) {
        printf("Player %d (score %d):\n", p + 1, s->p[p].score);
        uint8_t player_animal_display_board[4*5*2];
        for (int i = 0; i < (int)sizeof(player_animal_display_board); ++i)
            player_animal_display_board[i] = 0;
        for (int i = 0; i < 4; ++i) {
            if (s->p[p].animals[i] != -1) {
                int animal_id = s->p[p].animals[i] >> 3;
                struct animal *a = &animals[animal_id];
                board_put_at(player_animal_display_board, 4*5*2, i * 5 + 2, 0, a->token | TOKEN_ANIMAL);
                for (int e = 0; e < 3; ++e) {
                    if (a->env[e].token) {
                        int xy[2];
                        hexboard_rel(i * 5 + 2, 0, a->env[e].pos_a, 2, xy);
                        board_put_at(player_animal_display_board, 4*5*2, xy[0], xy[1], a->env[e].token);
                    }
                }
            }
        }
        hexboard_render(stdout, 4*5*2, 2, demo_cb, player_animal_display_board);
        hexboard_render(stdout, board_width, board_size * 2 / board_width, demo_cb, s->p[p].board);
    }
}
#endif

static int get_actions(struct state *s, uint16_t *out_actions, int opt)
{
    int action_count = 0;
    int player_idx = s->round % player_count;
    int token_supply_idx = s->round_state & 0x7;
    int tokens_chosen = s->round_state & 0x8;
    int animal_chosen = s->round_state & 0x10;
    int8_t *token_supply = s->token_supply[token_supply_idx];
    if (use_spirits && is_spirit_animal_id(s->p[player_idx].animals[1] >> 3)) {
        out_actions[action_count++] = (ACTION_CHOOSE_SPIRIT << 8) | 0;
        out_actions[action_count++] = (ACTION_CHOOSE_SPIRIT << 8) | 1;
        return action_count;
    }
    if (tokens_chosen) {
        // place tokens
        if (opt) {
            int min_token = 0x7F, min_idx = -1;
            for (int i = 0; i < 3; ++i) if (token_supply[i] != -1 && token_supply[i] < min_token) {
                min_token = token_supply[i];
                min_idx = i;
            }
            if (min_token != 0x7F) {
                for (int j = 0; j < board_size; ++j) {
                    if (stack_token(s->p[player_idx].board[j], token_supply[min_idx]) != TOKEN_EMPTY) {
                        out_actions[action_count++] = (ACTION_PLACE_TOKEN << 8) | (min_idx << 5) | j;
                    }
                }
            }
        } else
        for (int i = 0; i < 3; ++i) {
            if (token_supply[i] != -1) {
                for (int j = 0; j < board_size; ++j) {
                    if (stack_token(s->p[player_idx].board[j], token_supply[i]) != TOKEN_EMPTY) {
                        out_actions[action_count++] = (ACTION_PLACE_TOKEN << 8) | (i << 5) | j;
                    }
                }
            }
        }
    }
    int has_tokens_left = action_count > 0;
    if (opt && action_count) return action_count;
    // choose tokens
    if (!tokens_chosen) for (int i = 0; i < 5; ++i) {
        if (s->token_supply[i][0] != -1) {
            out_actions[action_count++] = (ACTION_CHOOSE_TOKENS << 8) | i;
        }
    }
    if (opt && action_count) return action_count;
    // place animal
    for (int i = 0; i < 4; ++i) {
        if (s->p[player_idx].animals[i] != -1) {
            int animal_id = s->p[player_idx].animals[i] >> 3;
            struct animal *a = &animals[animal_id];
            for (int j = 0; j < board_size; ++j) {
                if (s->p[player_idx].board[j] == a->token && animal_matches_at(s, player_idx, j, a)) {
                    out_actions[action_count++] = (ACTION_PLACE_ANIMAL << 8) | (i << 5) | j;
                }
            }
        }
    }
    if (opt && action_count) return action_count;
    // choose animal
    if (!animal_chosen) {
        int has_animal_slot = 0;
        for (int i = 0; i < 4; ++i) {
            if (s->p[player_idx].animals[i] == -1) {
                has_animal_slot = 1;
                break;
            }
        }
        if (has_animal_slot) for (int i = 0; i < 5; ++i) {
            if (s->animals[i] != -1) {
                out_actions[action_count++] = (ACTION_CHOOSE_ANIMAL << 8) | i;
            }
        }
    }
    // end round
    if (tokens_chosen && !has_tokens_left) out_actions[action_count++] = (ACTION_END_ROUND << 8);
    return action_count;
}
int is_game_over(struct state *s)
{
    // empty supply
    for (int i = 0; i < 5; ++i) if (s->token_supply[i][0] == -1) return 1;
    // player boards full
    for (int p = 0; p < player_count; ++p) {
        int empty_cnt = 0;
        for (int i = 0; i < board_size; ++i) {
            if (s->p[p].board[i] == 0) {
                empty_cnt++;
            }
        }
        if (empty_cnt < 3) return 1;
    }
    return 0;
}


int do_action(struct state *s, uint16_t action)
{
    int action_type = (action >> 8) & 0xFF;
    int params = action & 0xFF;
    int player_idx = s->round % player_count;
    switch (action_type) {
    case ACTION_CHOOSE_TOKENS: {
        int idx = params & 0x7;
        s->round_state |= 0x8 | idx; // tokens chosen
        break;
    }
    case ACTION_PLACE_TOKEN: {
        int supply_idx = s->round_state & 0x7;
        int token_idx = (params >> 5) & 0x7;
        int board_pos = params & 0x1F;
        uint8_t token = s->token_supply[supply_idx][token_idx];
        s->p[player_idx].board[board_pos] = stack_token(s->p[player_idx].board[board_pos], token);
        s->token_supply[supply_idx][token_idx] = -1;
        break;
    }
    case ACTION_CHOOSE_ANIMAL: {
        int idx = params & 0x7;
        s->round_state |= 0x10; // animal chosen
        int animal_idx = 0;
        while (s->p[player_idx].animals[animal_idx] != -1 && animal_idx < 4) animal_idx++;
        s->p[player_idx].animals[animal_idx] = s->animals[idx] << 3; // progress 0
        s->animals[idx] = -1;
        break;
    }
    case ACTION_PLACE_ANIMAL: {
        int animal_idx = (params >> 5) & 0x3;
        int board_pos = params & 0x1F;
        int animal_state = s->p[player_idx].animals[animal_idx];
        int animal_id = animal_state >> 3;
        int animal_progress = animal_state & 0x7;
        struct animal *a = &animals[animal_id];
        s->p[player_idx].board[board_pos] |= TOKEN_ANIMAL;
        int score = a->score[animal_progress];
        if (animal_progress + 1 >= MAX_ANIMAL_PROGRESS || a->score[animal_progress + 1] == 0) {
            s->p[player_idx].animals[animal_idx] = -1;
        } else {
            s->p[player_idx].animals[animal_idx]++;
        }
        if (score > 0) s->p[player_idx].score += score;
        else {
            s->p[player_idx].board[board_pos] |= TOKEN_SPIRIT;
            s->p[player_idx].spirit_state = animal_id;
            s->p[player_idx].animals[animal_idx] = -1;
        }
        break;
    }
    case ACTION_END_ROUND: {
        s->round++;
        init_round(s);
        if (s->round % player_count == 0 && is_game_over(s)) {
            return 1;
        }
        break;
    }
    case ACTION_CHOOSE_SPIRIT: {
        if (params & 1) s->p[player_idx].animals[0] = s->p[player_idx].animals[1];
        s->p[player_idx].animals[1] = -1;
        break;
    }
    default:
        break;
    }
    return 0;
}
#ifndef __EMSCRIPTEN__
void print_actions(struct state *s, uint16_t *actions, int action_count)
{
    int supply_idx = s->round_state & 0x7;
    int player_idx = s->round % player_count;
    for (int j = 0; j < 3; ++j) {
        uint8_t token = s->token_supply[supply_idx][j];
        int cnt = 0;
        uint8_t tmp[MAX_BOARD_SIZE];
        for (int k = 0; k < board_size; ++k) tmp[k] = s->p[player_idx].board[k];
        for (int i = 0; i < action_count; ++i) {
            if (((actions[i] >> 8) & 0xFF) == ACTION_PLACE_TOKEN) {
                int token_idx = (actions[i] >> 5) & 0x7;
                if (token_idx == j) {
                    int board_pos = actions[i] & 0x1F;
                    tmp[board_pos] = i | TOKEN_INPUT;
                    cnt++;
                }
            }
        }
        if (cnt) {
            printf("Place token %s\n", token_to_string[token]);
            hexboard_render(stdout, board_width, board_size * 2 / board_width, demo_cb, tmp);
        }
    }

    for (int i = 0; i < action_count; ++i) {
        int action_type = (actions[i] >> 8) & 0xFF;
        int params = actions[i] & 0xFF;
        switch (action_type) {
        case ACTION_CHOOSE_TOKENS:
            printf("  Action %d: CHOOSE_TOKENS from supply %d\n", i, params & 0x7);
            break;
        case ACTION_PLACE_TOKEN:
            break;
        case ACTION_CHOOSE_ANIMAL:
            printf("  Action %d: CHOOSE_ANIMAL idx %d\n", i, params & 0x7);
            break;
        case ACTION_PLACE_ANIMAL:
            printf("  Action %d: PLACE_ANIMAL animal idx %d at board pos %d\n", i, (params >> 5) & 0x3, params & 0x1F);
            break;
        case ACTION_END_ROUND:
            printf("  Action %d: END_ROUND\n", i);
            break;
        case ACTION_CHOOSE_SPIRIT:
            printf("  Action %d: CHOOSE_SPIRIT idx %d\n", i, params & 0x1);
            break;
        default:
            printf("  Action %d: UNKNOWN action %d\n", i, action_type);
            break;
        }
    }
}
#endif
int find_river_length(uint8_t *board, int board_idx) {
    // find the furthest water token from board_idx using BFS
    uint8_t visited[MAX_BOARD_SIZE] = {0};
    int local_queue[MAX_BOARD_SIZE];
    int queue_start = 0, queue_end = 0;
    local_queue[queue_end++] = board_idx;
    visited[board_idx] = 1;
    int length = 0;
    while (queue_start < queue_end) {
        int qsize = queue_end - queue_start;
        for (int i = 0; i < qsize; ++i) {
            int idx = local_queue[queue_start++];
            // check neighbors
            for (int d = 0; d < 6; ++d) {
                int board_idx = board_get_neighbour(idx, d, board_width);
                if (board_idx >= 0 && board_idx < board_size && !visited[board_idx]) {
                    int neighbor_token = board[board_idx] & TOKEN_MASK;
                    if (neighbor_token == TOKEN_RIVER) {
                        visited[board_idx] = 1;
                        local_queue[queue_end++] = board_idx;
                    }
                }
            }
        }
        length++;
    }
    return length;
}
int find_components(uint8_t *board, int board_size, int target_token_mask, int *out_components, int *out_elements) {
    int component_count = 0;
    uint8_t visited[MAX_BOARD_SIZE] = {0};
    for (int i = 0; i < board_size; ++i) {
        if (visited[i]) continue;
        int token = 1 << (board[i] & TOKEN_MASK);
        if (!(token & target_token_mask)) continue;
        // new component
        int local_stack[MAX_BOARD_SIZE], *stack = out_elements ? out_elements : local_stack;
        int stack_size = 0;
        stack[stack_size++] = i;
        visited[i] = 1;
        int component_size = 0;
        while (stack_size > 0) {
            int idx = stack[--stack_size];
            component_size++;
            // check neighbors
            for (int d = 0; d < 6; ++d) {
                int board_idx = board_get_neighbour(idx, d, board_width);
                if (board_idx >= 0 && board_idx < board_size && !visited[board_idx]) {
                    int neighbor_token = 1 << (board[board_idx] & TOKEN_MASK);
                    if (neighbor_token & target_token_mask) {
                        visited[board_idx] = 1;
                        stack[stack_size++] = board_idx;
                    }
                }
            }
        }
        out_components[component_count++] = component_size;
    }
    return component_count;
}

static int score_trex_connected_animals(uint8_t *board) {
    int spirit_idx = -1;
    for (int i = 0; i < board_size; ++i) {
        if (board[i] & TOKEN_SPIRIT) {
            spirit_idx = i;
            break;
        }
    }
    if (spirit_idx < 0 || !(board[spirit_idx] & TOKEN_ANIMAL)) return 0;

    uint8_t visited[MAX_BOARD_SIZE] = {0};
    int stack[MAX_BOARD_SIZE];
    int stack_size = 0;
    int connected_animals = 0;

    visited[spirit_idx] = 1;
    stack[stack_size++] = spirit_idx;
    while (stack_size > 0) {
        int idx = stack[--stack_size];
        connected_animals++;
        for (int d = 0; d < 6; ++d) {
            int nidx = board_get_neighbour(idx, d, board_width);
            if (nidx >= 0 && nidx < board_size && !visited[nidx] && (board[nidx] & TOKEN_ANIMAL)) {
                visited[nidx] = 1;
                stack[stack_size++] = nidx;
            }
        }
    }
    if (connected_animals <= 1) return 0; // exclude T-Rex itself
    return (connected_animals - 1) * 2;
}

int player_score(struct state *s, int player_idx, uint8_t *components) {
    int score = s->p[player_idx].score;
    int score_trees = 0;
    int score_fields = 0;
    int score_water = 0;
    int score_mountains = 0;
    int score_buildings = 0;
    int score_spirit = 0;
    int score_tiebreak = 0;
    uint8_t *board = s->p[player_idx].board;
    for (int i = 0; i < board_size; i++) {
        int token = board[i] & TOKEN_MASK;
        if (board[i] & TOKEN_ANIMAL) score_tiebreak++;
        int token_color = token_to_color[token];
        if (token == TOKEN_TREE) score_trees += 1;
        if (token == TOKEN_2X_TREE) score_trees += 3;
        if (token == TOKEN_3X_TREE) score_trees += 7;
        if (token_color == TOKEN_MOUNTAIN) {
            int has_mountain_neighbor = 0;
            for (int d = 0; d < 6; ++d) {
                int board_idx = board_get_neighbour(i, d, board_width);
                if (board_idx >= 0 && board_idx < board_size) {
                    int neighbor_token = board[board_idx] & TOKEN_MASK;
                    if (token_to_color[neighbor_token] == TOKEN_MOUNTAIN) {
                        has_mountain_neighbor = 1;
                        break;
                    }
                }
            }
            if (has_mountain_neighbor) {
                score_mountains += token == TOKEN_MOUNTAIN ? 1 : (token == TOKEN_2X_MOUNTAIN ? 3 : 7);
            }
        }
        if (token == TOKEN_FINISHED_BUILDING) {
            int colors = 0;
            for(int d = 0; d < 6; ++d) {
                int board_idx = board_get_neighbour(i, d, board_width);
                if (board_idx >= 0 && board_idx < board_size && board[board_idx]) {
                    int neighbor_token = board[board_idx] & TOKEN_MASK;
                    int neighbor_color = token_to_color[neighbor_token];
                    colors |= (1 << neighbor_color);
                }
            }
            if (popcount_u32(colors) >= 3) score_buildings += 5;
        }
        if (token == TOKEN_RIVER && water_score_type == WATER_SCORE_TYPE_RIVER) {
            int river_length = find_river_length(board, i);
            int river_score = river_length < 5 ? (0x258 >> (4 * (4 - river_length))) & 0xF : river_length * 4 - 9;
            if (river_score > score_water)
            score_water = river_score;
        }
    }
    // fields - find connected components of plains
    int field_components[MAX_BOARD_SIZE];
    int field_count = find_components(board, board_size, 1 << TOKEN_PLAINS, field_components, NULL);
    for (int i = 0; i < field_count; ++i) if (field_components[i] > 1) score_fields += 5;
    if (water_score_type == WATER_SCORE_TYPE_ISLANDS) {
        int island_components[MAX_BOARD_SIZE];
        score_water = 5 * find_components(board, board_size, ~(1 << TOKEN_RIVER), island_components, NULL);
    }
    if (s->p[player_idx].spirit_state >= 0) {
        int spirit_id = s->p[player_idx].spirit_state;
        struct animal *spirit = &animals[spirit_id];
        int score_type = spirit->score[0];
        if (score_type == -1) {
            int spirit_component_sizes[MAX_BOARD_SIZE];
            int count = find_components(board, board_size, 1 << spirit->token, spirit_component_sizes, NULL);
            for (int i = 0; i < count; ++i) {
                int sz = spirit_component_sizes[i];
                score_spirit += spirit->score[sz > 3 ? 3 : sz];
            }
        } else if (score_type == -2) {
            int spirit_color = token_to_color[spirit->token];
            for (int i = 0; i < board_size; i++) {
                int token = board[i] & TOKEN_MASK;
                if (token_to_color[token] == spirit_color) score_spirit += spirit->score[token_to_height[token]];
            }
        } else if (score_type == -3 && spirit_id == TREX_SPIRIT_ID) {
            score_spirit += score_trex_connected_animals(board);
        }
    }
    if (components) {
        components[0] = score_trees;
        components[1] = score_mountains;
        components[2] = score_fields;
        components[3] = score_buildings;
        components[4] = score_water;
        components[5] = score;
        components[6] = score_spirit;
        components[7] = score_tiebreak;
    }
    return score + score_trees + score_fields + score_water + score_mountains + score_buildings + score_spirit;
}
int global_action_cnt = 0;

int best_random(struct state *s, uint16_t *action, int action_count) {
    if (action_count <= 0) return -1;
    uint16_t local_actions[MAX_ACTIONS];
    long best_total = -1;
    int best_idx = 0;
    if (action_count <= 1) {
        return 0;
    }

    for (int ai = 0; ai < action_count; ++ai) {
        long total = 0;
        for (int pl = 0; pl < ai_playouts; ++pl) {
            struct state ts = *s; /* copy state */
            ts.rng_state ^= (pl + 1) * 0x9E3779B9; /* different RNG seed per playout */
            /* apply first action */
            uint16_t first = (uint16_t)action[ai];
            int end = do_action(&ts, first);
            /* play random until game end */
            while (!end) {
                int cnt = get_actions(&ts, local_actions, 1);
                if (cnt <= 0) break;
                int choice = rng_range(&ts.rng_state, cnt);
                end = do_action(&ts, local_actions[choice]);
            }
            total += player_score(&ts, s->round % player_count, 0);
        }
        if (total > best_total) {
            best_total = total;
            best_idx = ai;
        }
    }
    return best_idx;
}
static void set_ai_speed_level(int speed_level) {
    switch (speed_level) {
    case 0:
        ai_playouts = 1000;
        break;
    case 1:
        ai_playouts = 3000;
        break;
    case 2:
        ai_playouts = 10000;
        break;
    case 3:
        ai_playouts = 30000;
        break;
    case 4:
        ai_playouts = 100000;
        break;
    default:
        ai_playouts = 10000;
        break;
    }
}
struct state global_state;
void wasm_init_game(uint32_t seed, int num_players, int board_variant, int use_spirits_flag) {
    player_count = num_players;
    use_spirits = use_spirits_flag;
    if (board_variant == 1) {
        board_size = 25;
        board_width = 7;
        water_score_type = WATER_SCORE_TYPE_ISLANDS;
    } else {
        board_size = 23;
        board_width = 9;
        water_score_type = WATER_SCORE_TYPE_RIVER;
    }

    init_neigbours(board_width);
    init_game(&global_state, seed);
    init_round(&global_state);
}
void wasm_set_ai_speed(int speed_level) {
    set_ai_speed_level(speed_level);
}
void wasm_set_use_dlc(int enabled) {
    use_dlc_animals = enabled ? 1 : 0;
}
int wasm_get_actions(uint16_t *out_actions) {
    return get_actions(&global_state, out_actions, 0);
}
int wasm_do_action(uint16_t action) {
    return do_action(&global_state, action);
}
int wasm_player_score(int player_idx, uint8_t *components) {
    return player_score(&global_state, player_idx, components);
}
int wasm_get_ai_action(void) {
    uint16_t action[MAX_ACTIONS];
    int action_count = get_actions(&global_state, action, 1);
    global_action_cnt += action_count;
    return action[best_random(&global_state, action, action_count)];
}
#ifndef __EMSCRIPTEN__
int human_player_action() {
    uint16_t out_actions[MAX_ACTIONS];
    int action_count = get_actions(&global_state, out_actions, 0);
    printf("\nYour turn: %d possible actions\n", action_count);
    print_actions(&global_state, out_actions, action_count);

    int choice = -2;
    while (1) {
        printf("Choose action index (0-%d): ", action_count - 1);
        if (scanf("%d", &choice) != 1) {
            int c;
            while ((c = getchar()) != EOF && c != '\n') { }
            printf("Invalid input\n");
            continue;
        }
        if (choice >= 0 && choice < action_count) break;
        printf("Index out of range\n");
    }
    return out_actions[choice];
}
#endif
struct state* wasm_get_state(void) {
    return &global_state;
}
struct animal* wasm_get_animal(int idx) {
    if (idx < 0 || idx >= TOTAL_ANIMAL_COUNT) return NULL;
    return &animals[idx];
}
int wasm_get_animal_count(void) {
    return TOTAL_ANIMAL_COUNT;
}
int wasm_get_spirit_animal_start(void) {
    return SPIRIT_ANIMAL_START;
}
int wasm_get_spirit_animal_count(void) {
    return SPIRIT_ANIMAL_COUNT;
}
#ifndef __EMSCRIPTEN__
static const char *token_metric_names[13] = {
    "empty", "river", "plains", "wood", "mountain", "tree", "building", "finished_building",
    "mountain_2x", "mountain_3x", "tree_2x", "tree_3x", "wood_2x"
};

static void count_board_tokens(uint8_t *board, int out_counts[13]) {
    for (int t = 0; t < 13; ++t) out_counts[t] = 0;
    for (int i = 0; i < board_size; ++i) {
        int token = board[i] & TOKEN_MASK;
        if (token >= 0 && token <= 12) out_counts[token]++;
    }
}

static int is_file_empty(FILE *f) {
    long cur = ftell(f);
    if (cur < 0) cur = 0;
    if (fseek(f, 0, SEEK_END) != 0) return 1;
    long sz = ftell(f);
    if (sz < 0) sz = 0;
    (void)fseek(f, cur, SEEK_SET);
    return sz == 0;
}

static void write_batch_csv_header(FILE *csv) {
    fprintf(csv, "game_index,seed,players,board_variant,use_spirits,use_dlc,rounds");
    for (int p = 0; p < MAX_PLAYER_COUNT; ++p) {
        int pn = p + 1;
        fprintf(csv, ",p%d_total_score,p%d_score_trees,p%d_score_mountains,p%d_score_fields,p%d_score_buildings,p%d_score_water,p%d_score_animals,p%d_score_spirit,p%d_tiebreak",
            pn, pn, pn, pn, pn, pn, pn, pn, pn);
        for (int t = 1; t <= 12; ++t) {
            fprintf(csv, ",p%d_tok_%s", pn, token_metric_names[t]);
        }
        for (int a = 0; a < TOTAL_ANIMAL_COUNT; ++a) {
            fprintf(csv, ",p%d_a%02d_used,p%d_a%02d_completed,p%d_a%02d_face_score", pn, a, pn, a, pn, a);
        }
    }
    fputc('\n', csv);
}

static void print_usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s [seed] [board_variant] [use_spirits] [use_dlc]            # old demo mode\n", prog);
    printf("  %s --ai-batch <games> [--seed N] [--players 2-4] [--board 0|1] [--spirits 0|1|2] [--dlc 0|1] [--csv path]\n", prog);
}

static int run_ai_batch(
    int games,
    uint32_t seed,
    int num_players,
    int board_variant,
    int use_spirits_flag,
    int use_dlc_flag,
    const char *csv_path
) {
    if (games <= 0) {
        fprintf(stderr, "games must be > 0\n");
        return 1;
    }
    if (num_players < 2 || num_players > 4) {
        fprintf(stderr, "players must be 2..4\n");
        return 1;
    }
    if (use_spirits_flag < 0 || use_spirits_flag > 2) {
        fprintf(stderr, "spirits must be 0, 1, or 2\n");
        return 1;
    }
    FILE *csv = fopen(csv_path, "a+");
    if (!csv) {
        fprintf(stderr, "failed to open csv file: %s\n", csv_path);
        return 1;
    }
    if (is_file_empty(csv)) {
        write_batch_csv_header(csv);
    }
    if (fseek(csv, 0, SEEK_END) != 0) {
        fclose(csv);
        fprintf(stderr, "failed to seek csv file\n");
        return 1;
    }

    uint8_t components[8];
    for (int g = 0; g < games; ++g) {
        uint32_t game_seed = seed + (uint32_t)g * 2654435761u;
        int used[MAX_PLAYER_COUNT][TOTAL_ANIMAL_COUNT] = {0};
        int completed[MAX_PLAYER_COUNT][TOTAL_ANIMAL_COUNT] = {0};
        int face_score[MAX_PLAYER_COUNT][TOTAL_ANIMAL_COUNT] = {0};
        int token_counts[MAX_PLAYER_COUNT][13] = {{0}};
        int score_total[MAX_PLAYER_COUNT] = {0};
        int score_trees[MAX_PLAYER_COUNT] = {0};
        int score_mountains[MAX_PLAYER_COUNT] = {0};
        int score_fields[MAX_PLAYER_COUNT] = {0};
        int score_buildings[MAX_PLAYER_COUNT] = {0};
        int score_water[MAX_PLAYER_COUNT] = {0};
        int score_animals[MAX_PLAYER_COUNT] = {0};
        int score_spirit[MAX_PLAYER_COUNT] = {0};
        int score_tiebreak[MAX_PLAYER_COUNT] = {0};

        wasm_set_use_dlc(use_dlc_flag);
        wasm_init_game(game_seed, num_players, board_variant, use_spirits_flag);

        int end = 0;
        int guard = 0;
        while (!end && guard++ < 10000) {
            int player_idx = global_state.round % player_count;
            uint16_t action = (uint16_t)wasm_get_ai_action();
            int action_type = (action >> 8) & 0xFF;
            int params = action & 0xFF;

            if (action_type == ACTION_CHOOSE_ANIMAL) {
                int supply_idx = params & 0x7;
                int animal_id = global_state.animals[supply_idx];
                if (animal_id >= 0 && animal_id < TOTAL_ANIMAL_COUNT) {
                    used[player_idx][animal_id]++;
                }
            } else if (action_type == ACTION_CHOOSE_SPIRIT) {
                int slot = params & 1;
                int animal_state = global_state.p[player_idx].animals[slot];
                if (animal_state != -1) {
                    int animal_id = animal_state >> 3;
                    if (animal_id >= 0 && animal_id < TOTAL_ANIMAL_COUNT) {
                        used[player_idx][animal_id]++;
                    }
                }
            } else if (action_type == ACTION_PLACE_ANIMAL) {
                int animal_slot = (params >> 5) & 0x3;
                int animal_state = global_state.p[player_idx].animals[animal_slot];
                if (animal_state != -1) {
                    int animal_id = animal_state >> 3;
                    int animal_progress = animal_state & 0x7;
                    if (animal_id >= 0 && animal_id < TOTAL_ANIMAL_COUNT) {
                        struct animal *a = &animals[animal_id];
                        int score = a->score[animal_progress];
                        if (score > 0) face_score[player_idx][animal_id] += score;
                        if (score <= 0 || animal_progress + 1 >= MAX_ANIMAL_PROGRESS || a->score[animal_progress + 1] == 0) {
                            completed[player_idx][animal_id]++;
                        }
                    }
                }
            }

            end = wasm_do_action(action);
        }

        if (guard >= 10000) {
            fprintf(stderr, "warning: game %d reached action guard limit\n", g + 1);
        }

        for (int p = 0; p < MAX_PLAYER_COUNT; ++p) {
            if (p < num_players) {
                score_total[p] = player_score(&global_state, p, components);
                score_trees[p] = components[0];
                score_mountains[p] = components[1];
                score_fields[p] = components[2];
                score_buildings[p] = components[3];
                score_water[p] = components[4];
                score_animals[p] = components[5];
                score_spirit[p] = components[6];
                score_tiebreak[p] = components[7];
                count_board_tokens(global_state.p[p].board, token_counts[p]);
            }
        }

        fprintf(csv, "%d,%u,%d,%d,%d,%d,%d", g + 1, game_seed, num_players, board_variant, use_spirits_flag, use_dlc_flag, global_state.round);
        for (int p = 0; p < MAX_PLAYER_COUNT; ++p) {
            fprintf(csv, ",%d,%d,%d,%d,%d,%d,%d,%d,%d",
                score_total[p], score_trees[p], score_mountains[p], score_fields[p], score_buildings[p],
                score_water[p], score_animals[p], score_spirit[p], score_tiebreak[p]);
            for (int t = 1; t <= 12; ++t) {
                fprintf(csv, ",%d", token_counts[p][t]);
            }
            for (int a = 0; a < TOTAL_ANIMAL_COUNT; ++a) {
                fprintf(csv, ",%d,%d,%d", used[p][a], completed[p][a], face_score[p][a]);
            }
        }
        fputc('\n', csv);
        fflush(csv);
    }

    fclose(csv);
    printf("Batch completed: %d games -> %s\n", games, csv_path);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (argc > 2 && strcmp(argv[1], "--ai-batch") == 0) {
        int games = atoi(argv[2]);
        uint32_t seed = (uint32_t)time(NULL);
        int num_players = 2;
        int board_variant = 0;
        int use_spirits_flag = 0;
        int use_dlc_flag = 0;
        const char *csv_path = "ai_batch_results.csv";

        for (int i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
                seed = (uint32_t)strtoul(argv[++i], NULL, 10);
            } else if (strcmp(argv[i], "--players") == 0 && i + 1 < argc) {
                num_players = atoi(argv[++i]);
            } else if (strcmp(argv[i], "--board") == 0 && i + 1 < argc) {
                board_variant = atoi(argv[++i]);
            } else if (strcmp(argv[i], "--spirits") == 0 && i + 1 < argc) {
                use_spirits_flag = atoi(argv[++i]);
            } else if (strcmp(argv[i], "--dlc") == 0 && i + 1 < argc) {
                use_dlc_flag = atoi(argv[++i]);
            } else if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
                csv_path = argv[++i];
            } else {
                fprintf(stderr, "unknown argument: %s\n", argv[i]);
                print_usage(argv[0]);
                return 1;
            }
        }

        return run_ai_batch(games, seed, num_players, board_variant, use_spirits_flag, use_dlc_flag, csv_path);
    }

    uint32_t seed = time(NULL);
    int board_variant = 0, use_spirits_flag = 0, use_dlc_flag = 0;
    if (argc > 1) {
        seed = (uint32_t)strtoul(argv[1], NULL, 10);
    }
    if (argc > 2) {
        board_variant = atoi(argv[2]);
    }
    if (argc > 3) {
        use_spirits_flag = atoi(argv[3]);
    }
    if (argc > 4) {
        use_dlc_flag = atoi(argv[4]);
    }
    wasm_set_use_dlc(use_dlc_flag);
    wasm_init_game(seed, 2, board_variant, use_spirits_flag);
    print_state(&global_state);

    for (int itr = 0; itr < 1000; ++itr) {
        int player_idx = wasm_get_state()->round % player_count;

        uint16_t chosen_action = player_idx >= 0 ? (uint16_t)wasm_get_ai_action() : human_player_action();
        int end = wasm_do_action(chosen_action);

        print_state(&global_state);

        if (end) {
            printf("Game over!\n");
            for (int p = 0; p < player_count; ++p) {
                uint8_t components[8];
                int final_score = wasm_player_score(p, components);
                printf("Player %d final score: %d\n", p + 1, final_score);
                printf("  Score breakdown: trees %d, mountains %d, fields %d, buildings %d, water %d, base %d, spirit %d\n",
                    components[0], components[1], components[2], components[3], components[4], components[5], components[6]);
            }
            printf("Total actions considered by AI: %d\n", global_action_cnt);
            break;
        }
    }

    return 0;
}
#endif
