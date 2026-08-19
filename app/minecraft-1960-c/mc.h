/* mc.h -- Minecraft 1960: a beatable 2D Minecraft for 20 KIPS CPUs.
 *
 * Screen: 192x168, 1-bit.  Only two screen primitives exist:
 *   screen_clear()   -- blank the whole display
 *   screen_set(x,y)  -- set one pixel
 * They live in display.h so the native display code can be swapped in.
 *
 * All game code is freestanding-friendly C: no heap, no floats, no
 * division/modulo in hot loops (shifts + adds only), static data only.
 */
#ifndef MC_H
#define MC_H

typedef unsigned char  u8;
typedef signed char    i8;
typedef unsigned short u16;
typedef short          i16;

/* ------------------------------------------------------------------ */
/* Display geometry                                                    */
/* ------------------------------------------------------------------ */
#define SCR_W      192
#define SCR_H      168
#define TILE       8                 /* pixels per tile              */
#define VIEW_TX    (SCR_W/TILE)      /* 24 tiles wide                */
#define VIEW_TY    (SCR_H/TILE)      /* 21 tiles tall                */

/* ------------------------------------------------------------------ */
/* World geometry.  WORLD_W is a power of two so tile index            */
/* (x + y*WORLD_W) compiles to a shift.                                */
/* ------------------------------------------------------------------ */
#define WORLD_W    256
#define WORLD_H    80
#define WORLD_SHIFT 8

/* Two world buffers: the overworld is persistent, the second buffer
 * is regenerated each time the nether or the end is entered. */
#define DIM_OVER    0
#define DIM_NETHER 1
#define DIM_END    2

/* ------------------------------------------------------------------ */
/* Timing: logic ticks at 4 Hz, a frame is rendered every 2 ticks (2 fps) */
/* ------------------------------------------------------------------ */
#define TICK_MS    250
#define FRAME_EVERY 2

/* Fixed point: 16 units per tile (4 fractional bits). */
#define FPB        16

/* ------------------------------------------------------------------ */
/* Block ids (placed in the world)                                     */
/* ------------------------------------------------------------------ */
enum {
  B_AIR = 0,
  B_GRASS, B_DIRT, B_STONE, B_COBBLE, B_BEDROCK,
  B_LOG, B_LEAVES, B_PLANKS, B_CRAFT, B_FURNACE,
  B_SAND, B_GRAVEL, B_COAL, B_IRON, B_DIAMOND,
  B_WATER, B_LAVA, B_OBSIDIAN, B_PORTAL,
  B_NETHERRACK, B_NBRICK, B_SPAWNER, B_ENDSTONE,
  B_SBRICK, B_FRAME, B_FRAME_EYE, B_ENDPORTAL,
  B_NB
};

/* ------------------------------------------------------------------ */
/* Item ids (held in inventory; blocks are also items)                 */
/* ------------------------------------------------------------------ */
#define I_STICK     40
#define I_COAL      41
#define I_FLINT     42
#define I_IRONING   43
#define I_DIAMOND   44
#define I_BUCKET    45
#define I_BUCKET_W  46
#define I_BUCKET_L  47
#define I_FSTEEL    48
#define I_ROD       49
#define I_POWDER    50
#define I_PEARL     51
#define I_EYE       52
/* tools: order matters (groups of 3: pick, axe, sword per tier) */
#define I_WPICK     53
#define I_WAXE      54
#define I_WSWORD    55
#define I_SPICK     56
#define I_SAXE      57
#define I_SSWORD    58
#define I_IPICK     59
#define I_IAXE      60
#define I_ISWORD    61
#define I_DPICK     62
#define I_DAXE      63
#define I_DSWORD    64
#define I_MAX       65

/* tool tier of a tool item id, 0 if not a tool */
#define TOOL_TIER(id) (((id) >= I_WPICK && (id) <= I_DSWORD) ? \
                       (u8)(((id) - I_WPICK) / 3 + 1) : 0)
/* which tool kind: 0 pick, 1 axe, 2 sword */
#define TOOL_KIND(id) (((id) - I_WPICK) % 3)

/* ------------------------------------------------------------------ */
/* Input key codes (provided by the display layer)                     */
/* ------------------------------------------------------------------ */
enum {
  K_LEFT = 0, K_RIGHT, K_UP, K_DOWN,
  K_ACT,          /* mine / attack / interact / confirm  */
  K_USE,          /* place block / use held item / close */
  K_MENU,         /* open crafting                       */
  K_BACK,         /* close menu                          */
  K_NEXT, K_PREV, /* cycle hotbar slot                   */
  K_AIM,          /* toggle aim mode (cursor with WASD)  */
  K_FPS,          /* toggle 2/4 fps rendering            */
  K_DROP,         /* drop one of the held item           */
  K_CREATIVE,     /* toggle creative mode (fly, no damage) */
  K_D1, K_D2, K_D3, K_D4, K_D5, K_D6, K_D7, K_D8, K_D9, K_D0,
  K_DBG_G, K_DBG_T, K_DBG_H, K_DBG_P, K_DBG_L, K_DBG_M,  /* debug */
  K_COUNT
};

/* ------------------------------------------------------------------ */
/* Entity / misc limits                                                */
/* ------------------------------------------------------------------ */
#define MAXSLOTS   12
#define MAXMOBS    10
#define MAXDROPS   24
#define MAXSHOTS   8
#define STACK      64

#define M_ZOMBIE   0
#define M_ENDERMAN 1
#define M_BLAZE    2
#define M_DRAGON   3

/* ------------------------------------------------------------------ */
/* Shared game state                                                   */
/* ------------------------------------------------------------------ */
typedef struct { u8 id, n; } Slot;

typedef struct {
  u8  type, hp, dead;
  i16 x, y, vx, vy;      /* sixteenths of a tile */
  u8  t1, t2;            /* ai timers            */
  u8  flags;             /* bit0 hostile, bit1 onground */
  u8  ft;                /* hit-flash timer (damage anim) */
} Mob;

typedef struct { u8 id, n, ttl, age; i16 x, y, vx, vy; } Drop;

typedef struct { u8 ttl, src; i16 x, y, vx, vy; } Shot;

typedef struct {
  /* player position in sixteenths of a tile, top-left of hitbox */
  i16 px, py, pvx, pvy;
  u8  face, onground, hp, invuln, dead, deadt;
  i16 spawn_x, spawn_y;
  int portal_t;              /* standing in a portal             */
  u8  portal_dim;            /* destination                      */
  i16 portal_dx, portal_dy;  /* arrival offset (tiles)           */
  /* mining */
  u8  mtarget_valid, mprogress, mhard;
  u8  mtx, mty;
  /* aim mode */
  u8  aim;
  i16 aimx, aimy;
  /* inventory */
  Slot inv[MAXSLOTS];
  u8  sel;
  /* smelting */
  u8  smelt_t;               /* 0 = idle */
  /* world info */
  u8  dim;
  u16 time;                  /* ticks, wraps at 1024; day = <512 */
  u16 days;
  /* win state */
  u8  won, winscreen;
  /* menu state */
  u8  menu;                  /* 0 none, 1 craft, 2 smelt */
  u8  menusel, menutop;
  /* messages */
  char msg[24];
  u8  msgt;
  /* options */
  u8  hifps, fps_now;
  u8  creative, flying;
  u16 wtap;   /* tick of last jump-key tap (double-tap flight) */
  /* debug */
  u8  god, nomine;
} GameState;

extern GameState g;

/* ------------------------------------------------------------------ */
/* world.c                                                             */
/* ------------------------------------------------------------------ */
extern unsigned char worlds[2][WORLD_W * WORLD_H];
extern unsigned char *wmap;        /* points at current dimension   */
extern u8 curbuf;                  /* buffer index of wmap          */
extern i16 sh_x, sh_y;             /* stronghold centre (overworld) */
extern i16 ov_portal_x, ov_portal_y;   /* built nether portal       */
extern i16 spawner_x, spawner_y;       /* nether blaze spawner      */
extern i16 end_spawn_x, end_spawn_y;   /* end island arrival        */

void world_init(u16 seed);
void gen_overworld(void);
void gen_nether(void);
void gen_end(void);
u8   get_t(int x, int y);          /* safe accessor (init/edge only) */
void set_t(int x, int y, u8 id);
int  surface_y(int x);
u16  rng(void);                    /* xorshift, shifts+xors only    */

/* ------------------------------------------------------------------ */
/* game.c                                                              */
/* ------------------------------------------------------------------ */
extern Mob   mobs[MAXMOBS];
extern Drop  drops[MAXDROPS];
extern Shot  shots[MAXSHOTS];

void game_init(u16 seed);
void game_tick(void);
void msg(const char *s);
u8   inv_count(u8 id);
u8   inv_add(u8 id, u8 n);         /* returns 1 on success */
void inv_remove(u8 id, u8 n);
void hurt(u8 dmg);
Drop *spawn_drop(u8 id, u8 n, i16 x, i16 y);

/* ------------------------------------------------------------------ */
/* craft.c                                                             */
/* ------------------------------------------------------------------ */
typedef struct {
  u8 out, n;
  u8 need[6];          /* (id,count) pairs, id==0 terminates */
  u8 table;            /* requires crafting table nearby     */
} Recipe;

extern const Recipe recipes[];
extern const u8 recipe_count;

u8 can_craft(u8 r);
void do_craft(u8 r);
u8 near_table(void);
u8 near_furnace(void);
const char *item_name(u8 id);

/* ------------------------------------------------------------------ */
/* render.c                                                            */
/* ------------------------------------------------------------------ */
extern i16 cam_x, cam_y;           /* camera, in tiles */
void render_frame(void);

#endif /* MC_H */
