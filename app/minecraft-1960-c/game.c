/* game.c -- all game logic, ticked at 4 Hz.
 *
 * Fixed point only (16 units per tile), no floats, no division in hot
 * loops.  The one or two divisions that exist (dragon dive slope,
 * smelting counter replaced by a mask) run once per event.
 */
#include "mc.h"
#include "display.h"

GameState g;

/* ------------------------------------------------------------------ */
/* Block property tables                                               */
/* ------------------------------------------------------------------ */
static const u8 solid_tab[B_NB] = {
  0,                                  /* AIR        */
  1,1,1,1,1,                          /* GRASS..BEDROCK */
  1,1,1,1,1,                          /* LOG..FURNACE   */
  1,1,1,1,1,                          /* SAND..DIAMOND  */
  0,0,1,0,                            /* WATER,LAVA,OBSIDIAN,PORTAL */
  1,1,1,1,                            /* NETHERRACK..ENDSTONE */
  1,1,1,0                             /* SBRICK,FRAME,FRAME_EYE,ENDPORTAL */
};

/* work units needed to break (with the correct tool power) */
static const u8 work_tab[B_NB] = {
  0, 4,4,12,12,255, 6,1,5,5,12, 4,5,12,16,18, 0,0,60,0, 6,10,255,12, 10,255,255,0
};

/* required pickaxe tier: 1 wood, 2 stone, 3 iron, 4 diamond */
static const u8 req_tab[B_NB] = {
  0, 0,0,1,1,0, 0,0,0,0,0, 0,0,1,2,3, 0,0,4,0, 0,0,0,0, 0,0,0,0
};

/* material kind: 0 loose, 1 stone-like (pickaxe), 2 wood-like (axe) */
static const u8 kind_tab[B_NB] = {
  0, 0,0,1,1,1, 2,0,2,2,1, 0,0,1,1,1, 0,0,1,0, 1,1,1,1, 1,1,1,0
};

/* ------------------------------------------------------------------ */
/* Progress flags for the hint system                                  */
/* ------------------------------------------------------------------ */
static u16 prog;
#define P_LOG      0x0001u
#define P_PLANKS   0x0002u
#define P_TABLE    0x0004u
#define P_PTAB     0x0008u
#define P_STONE    0x0010u
#define P_SPICK    0x0020u
#define P_FURN     0x0040u
#define P_INGOT    0x0080u
#define P_DIAMOND  0x0100u
#define P_OBSIDIAN 0x0200u
#define P_PORTAL   0x0400u
#define P_ROD      0x0800u
#define P_PEARL    0x1000u
#define P_EYE      0x2000u
#define P_END      0x4000u

Mob   mobs[MAXMOBS];
Drop  drops[MAXDROPS];
Shot  shots[MAXSHOTS];
static u8 dbg_tstate;
static u8 hint_t;

/* ------------------------------------------------------------------ */
/* Small utilities                                                     */
/* ------------------------------------------------------------------ */
void msg(const char *s)
{
  u8 i;
  for (i = 0; i < 23 && s[i]; i++) g.msg[i] = s[i];
  g.msg[i] = 0;
  g.msgt = 12;
}

static int iabs(int v) { return v < 0 ? -v : v; }

static i8 sign16(i16 v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

/* ------------------------------------------------------------------ */
/* Inventory                                                           */
/* ------------------------------------------------------------------ */
u8 inv_count(u8 id)
{
  u8 i, n = 0;
  for (i = 0; i < MAXSLOTS; i++)
    if (g.inv[i].id == id) n += g.inv[i].n;
  return n;
}

u8 inv_add(u8 id, u8 n)
{
  u8 i;
  for (i = 0; i < MAXSLOTS && n; i++)
    if (g.inv[i].id == id && g.inv[i].n < STACK) {
      u8 t = (u8)(STACK - g.inv[i].n);
      if (t > n) t = n;
      g.inv[i].n += t; n -= t;
    }
  for (i = 0; i < MAXSLOTS && n; i++)
    if (g.inv[i].id == 0) {
      u8 t = n > STACK ? STACK : n;
      g.inv[i].id = id; g.inv[i].n = t; n -= t;
    }
  return n == 0;
}

void inv_remove(u8 id, u8 n)
{
  u8 i;
  for (i = 0; i < MAXSLOTS && n; i++)
    if (g.inv[i].id == id) {
      u8 t = g.inv[i].n > n ? n : g.inv[i].n;
      g.inv[i].n -= t; n -= t;
      if (g.inv[i].n == 0) g.inv[i].id = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Tile helpers                                                        */
/* ------------------------------------------------------------------ */
static u8 solid_at_t(int tx, int ty)
{
  if (tx < 0 || tx >= WORLD_W) return 1;
  if (ty < 0) return 0;
  if (ty >= WORLD_H) return 1;
  return solid_tab[wmap[tx + (ty << WORLD_SHIFT)]];
}

/* AABB move.  Returns 1 when landing on ground. */
static u8 phys_move(i16 *xx, i16 *yy, i16 vx, i16 vy, u8 w, u8 h)
{
  i16 x = *xx, y = *yy, nx, ny, t;
  u8 grounded = 0;
  int r, c;

  nx = (i16)(x + vx);
  if (vx > 0) {
    t = (i16)((nx + w - 1) >> 4);
    for (r = y >> 4; r <= (y + h - 1) >> 4; r++)
      if (solid_at_t(t, r)) { nx = (i16)((t << 4) - w); break; }
  } else if (vx < 0) {
    t = (i16)(nx >> 4);
    for (r = y >> 4; r <= (y + h - 1) >> 4; r++)
      if (solid_at_t(t, r)) { nx = (i16)((t + 1) << 4); break; }
  }

  ny = (i16)(y + vy);
  if (vy > 0) {
    t = (i16)((ny + h - 1) >> 4);
    for (c = nx >> 4; c <= (nx + w - 1) >> 4; c++)
      if (solid_at_t(c, t)) { ny = (i16)((t << 4) - h); grounded = 1; break; }
  } else if (vy < 0) {
    t = (i16)(ny >> 4);
    for (c = nx >> 4; c <= (nx + w - 1) >> 4; c++)
      if (solid_at_t(c, t)) { ny = (i16)((t + 1) << 4); break; }
  }

  *xx = nx; *yy = ny;
  return grounded;
}

static u8 rect_hit(i16 ax, i16 ay, i16 bx, i16 by, u8 w, u8 h)
{
  return (u8)(iabs((int)ax - bx) < w && iabs((int)ay - by) < h);
}

/* ------------------------------------------------------------------ */
/* Damage                                                              */
/* ------------------------------------------------------------------ */
static void regen_reset(void);

void hurt(u8 dmg)
{
  if (g.god || g.creative || g.invuln || g.hp == 0) return;
  dmg >>= 1;                 /* all damage halved */
  if (!dmg) return;
  if (dmg >= g.hp) { g.hp = 0; } else g.hp -= dmg;
  g.invuln = 8;
  regen_reset();
  if (g.hp == 0) {
    g.dead = 1; g.deadt = 10;
    msg("YOU DIED");
  }
}

static void hurt_dir(u8 dmg, i8 dir)
{
  hurt(dmg);
  if (!g.dead) {
    g.pvx = (i16)(dir * 5);
    g.pvy = -4;
  }
}

/* ------------------------------------------------------------------ */
/* Drops                                                               */
/* ------------------------------------------------------------------ */
Drop *spawn_drop(u8 id, u8 n, i16 x, i16 y)
{
  u8 i;
  for (i = 0; i < MAXDROPS; i++)
    if (drops[i].n == 0) {
      drops[i].id = id; drops[i].n = n;
      drops[i].x = x; drops[i].y = y;
      drops[i].vx = 0; drops[i].vy = 0;
      drops[i].ttl = 240; drops[i].age = 0;
      return &drops[i];
    }
  return 0;
}

/* ------------------------------------------------------------------ */
/* Dimension travel                                                    */
/* ------------------------------------------------------------------ */
static void clear_ents(void)
{
  u8 i;
  for (i = 0; i < MAXMOBS; i++) mobs[i].dead = 1;
  for (i = 0; i < MAXSHOTS; i++) shots[i].ttl = 0;
  for (i = 0; i < MAXDROPS; i++) drops[i].n = 0;
}

static void try_spawn(u8 type, int tx, int ty);

/* ty < 0 means: place the player on the surface at tx */
static void travel(u8 dim, int tx, int ty)
{
  clear_ents();
  g.dim = dim;
  if (dim == DIM_OVER) { curbuf = 0; wmap = worlds[0]; }
  else if (dim == DIM_NETHER) { gen_nether(); }
  else {
    gen_end();
    /* end_spawn is only known after gen_end */
    tx = end_spawn_x; ty = end_spawn_y;
    {
      u8 i;
      for (i = 0; i < MAXMOBS; i++)
        if (mobs[i].dead) {
          mobs[i].dead = 0; mobs[i].type = M_DRAGON;
          mobs[i].hp = 60; mobs[i].x = 128 * 16; mobs[i].y = 12 * 16;
          mobs[i].vx = 6; mobs[i].vy = 0;
          mobs[i].t1 = 0; mobs[i].t2 = 40; mobs[i].flags = 0; mobs[i].ft = 0;
          break;
        }
    }
    /* a few endermen roam the island */
    try_spawn(M_ENDERMAN, 104, surface_y(104) - 1);
    try_spawn(M_ENDERMAN, 116, surface_y(116) - 1);
    try_spawn(M_ENDERMAN, 140, surface_y(140) - 1);
    try_spawn(M_ENDERMAN, 152, surface_y(152) - 1);
    prog |= P_END;
  }
  if (ty < 0) ty = surface_y(tx) - 3;
  g.px = (i16)(tx << 4); g.py = (i16)(ty << 4);
  g.pvx = 0; g.pvy = 0;
  g.portal_t = -16;   /* cooldown so the arrival portal doesn't bounce */
  if (dim == DIM_NETHER) msg("THE NETHER");
  if (dim == DIM_END) msg("THE END");
  if (dim == DIM_OVER) msg("THE OVERWORLD");
}

/* ------------------------------------------------------------------ */
/* Block break / place                                                 */
/* ------------------------------------------------------------------ */
static void break_block(int tx, int ty)
{
  u8 id = get_t(tx, ty);
  u8 drop_id = 0, drop_n = 1;
  switch (id) {
    case B_GRASS:   drop_id = B_DIRT; break;
    case B_STONE:   drop_id = B_COBBLE; break;
    case B_COAL:    drop_id = I_COAL; break;
    case B_DIAMOND: drop_id = I_DIAMOND; prog |= P_DIAMOND; break;
    case B_LEAVES:
      if ((rng() & 7) < 3) { drop_id = I_STICK; } else drop_n = 0;
      break;
    case B_GRAVEL:
      /* 50% flint, like minecraft */
      drop_id = (rng() & 1) ? I_FLINT : B_GRAVEL;
      break;
    case B_IRON:    drop_id = B_IRON; break;
    case B_OBSIDIAN: drop_id = B_OBSIDIAN; prog |= P_OBSIDIAN; break;
    case B_PORTAL: case B_ENDPORTAL: case B_BEDROCK:
    case B_SPAWNER: case B_FRAME: case B_FRAME_EYE:
    case B_WATER: case B_LAVA:
      return;
    default: drop_id = id; break;
  }
  set_t(tx, ty, B_AIR);
  if (drop_n && drop_id)
    spawn_drop(drop_id, drop_n, (i16)(tx << 4), (i16)(ty << 4));
  /* sand/gravel above collapse */
  {
    u8 up = get_t(tx, ty - 1);
    if (up == B_SAND || up == B_GRAVEL) {
      set_t(tx, ty - 1, B_AIR);
      spawn_drop(up, 1, (i16)(tx << 4), (i16)((ty - 1) << 4));
    }
  }
}

static u8 overlap_player(int tx, int ty)
{
  i16 bx = (i16)(tx << 4), by = (i16)(ty << 4);
  return rect_hit(g.px, g.py, bx, by, 26, 42);
}

/* exact hitbox test (aim mode); overlap_player is the padded one used
 * by auto-placement */
static u8 inside_player(int tx, int ty)
{
  i16 bx = (i16)(tx << 4), by = (i16)(ty << 4);
  return (u8)(bx + 16 > g.px && bx < g.px + 12 &&
              by + 16 > g.py && by < g.py + 28);
}

/* place one block at an exact tile */
static u8 place_at(int x, int y, u8 id, u8 strict)
{
  u8 t = get_t(x, y);
  if (t != B_AIR && t != B_WATER) return 0;
  if (!solid_at_t(x - 1, y) && !solid_at_t(x + 1, y) &&
      !solid_at_t(x, y - 1) && !solid_at_t(x, y + 1)) return 0;
  if (strict ? inside_player(x, y) : overlap_player(x, y)) return 0;
  set_t(x, y, id);
  /* water/lava interaction */
  if (id == B_WATER) {
    if (get_t(x - 1, y) == B_LAVA) set_t(x - 1, y, B_OBSIDIAN);
    if (get_t(x + 1, y) == B_LAVA) set_t(x + 1, y, B_OBSIDIAN);
    if (get_t(x, y + 1) == B_LAVA) set_t(x, y + 1, B_OBSIDIAN);
  }
  if (id == B_LAVA) {
    if (get_t(x - 1, y) == B_WATER || get_t(x + 1, y) == B_WATER ||
        get_t(x, y - 1) == B_WATER || get_t(x, y + 1) == B_WATER)
      set_t(x, y, B_OBSIDIAN);
  }
  return 1;
}

/* auto placement: first free tile next to the player */
static u8 try_place(u8 id)
{
  static const i8 dxs[6] = { 1, 1, 2, 2, 0, 0 };
  static const i8 dys[6] = { 0, -1, 0, -1, 1, -2 };
  int tx = (g.px + 6) >> 4, ty = (g.py + 20) >> 4;
  u8 k;
  for (k = 0; k < 6; k++) {
    int x = tx + (g.face > 0 ? dxs[k] : -dxs[k]);
    int y = ty + dys[k];
    if (place_at(x, y, id, 0)) return 1;
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/* Target scanning (keyboard-only aiming)                              */
/* ------------------------------------------------------------------ */
static const i8 sc_dx[7] = { 1, 1, 1, 2, 2, 0, 0 };
static const i8 sc_dy[7] = { -1, 0, 1, -1, 0, -2, 1 };

/* first solid breakable tile near the player, facing direction */
static u8 scan_break(int *ox, int *oy)
{
  int tx = (g.px + 6) >> 4, ty = (g.py + 20) >> 4;
  u8 k;
  for (k = 0; k < 7; k++) {
    int x = tx + (g.face > 0 ? sc_dx[k] : -sc_dx[k]);
    int y = ty + sc_dy[k];
    u8 id = get_t(x, y);
    if (id == B_AIR || id == B_WATER || id == B_LAVA ||
        id == B_PORTAL || id == B_ENDPORTAL) continue;
    *ox = x; *oy = y;
    return 1;
  }
  return 0;
}

static u8 scan_interact(int *ox, int *oy)
{
  int tx = (g.px + 6) >> 4, ty = (g.py + 20) >> 4;
  u8 k;
  for (k = 0; k < 7; k++) {
    int x = tx + (g.face > 0 ? sc_dx[k] : -sc_dx[k]);
    int y = ty + sc_dy[k];
    u8 id = get_t(x, y);
    if (id == B_CRAFT || id == B_FURNACE) { *ox = x; *oy = y; return 1; }
  }
  return 0;
}

static u8 scan_liquid(int *ox, int *oy)
{
  int tx = (g.px + 6) >> 4, ty = (g.py + 20) >> 4;
  u8 k;
  for (k = 0; k < 7; k++) {
    int x = tx + (g.face > 0 ? sc_dx[k] : -sc_dx[k]);
    int y = ty + sc_dy[k];
    u8 id = get_t(x, y);
    if (id == B_WATER || id == B_LAVA) { *ox = x; *oy = y; return id; }
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/* Mining                                                              */
/* ------------------------------------------------------------------ */
static u8 held_item(void) { return g.inv[g.sel].id; }

static void do_mine_at(int x, int y)
{
  u8 id, req, kind, power, tool;
  id = get_t(x, y);
  if (id == B_AIR || id == B_WATER || id == B_LAVA ||
      id == B_PORTAL || id == B_ENDPORTAL) { g.mtarget_valid = 0; return; }
  if (x != g.mtx || y != g.mty || !g.mtarget_valid) {
    g.mtx = (u8)x; g.mty = (u8)y;
    g.mprogress = 0; g.mtarget_valid = 1;
    g.mhard = work_tab[id];
  }
  req = req_tab[id];
  kind = kind_tab[id];
  tool = held_item();
  /* tier check for stone-like blocks */
  if (kind == 1 && req > 0) {
    if (TOOL_KIND(tool) != 0 || TOOL_TIER(tool) < req) {
      if (g.msgt == 0) msg("NEED A BETTER PICKAXE");
      return;
    }
  }
  if (kind == 0) power = 2;
  else if (kind == 1 && TOOL_KIND(tool) == 0) power = (u8)(TOOL_TIER(tool) + 1);
  else if (kind == 2 && TOOL_KIND(tool) == 1) power = (u8)(TOOL_TIER(tool) + 1);
  else power = 1;
  if (work_tab[id] >= 250) return;
  g.mprogress += power;
  if (g.mprogress >= g.mhard) {
    break_block(x, y);
    g.mtarget_valid = 0; g.mprogress = 0;
  }
}

/* ------------------------------------------------------------------ */
/* Combat                                                              */
/* ------------------------------------------------------------------ */
static u8 weapon_dmg(void)
{
  u8 t = held_item();
  if (t >= I_WSWORD && t <= I_DSWORD && TOOL_KIND(t) == 2)
    return (u8)(TOOL_TIER(t) + 3);
  if (t >= I_WAXE && t <= I_DAXE && TOOL_KIND(t) == 1)
    return (u8)(TOOL_TIER(t) + 1);
  if (TOOL_TIER(t)) return 1;
  return 1;
}

/* natural regeneration: +1 hp every 20 ticks, starting 20 ticks after
 * the last damage */
static u8 regen_t;

static void regen_tick(void)
{
  if (g.hp == 0 || g.hp >= 20) { regen_t = 0; return; }
  if (++regen_t >= 20) {
    regen_t = 0;
    g.hp++;
  }
}

static void regen_reset(void) { regen_t = 0; }

static Mob *mob_in_reach(void)
{
  u8 i;
  i16 cx = (i16)(g.px + 6), cy = (i16)(g.py + 14);
  for (i = 0; i < MAXMOBS; i++) {
    Mob *m = &mobs[i];
    if (m->dead) continue;
    if (iabs((int)m->x - cx) <= 36 && iabs((int)m->y - cy) <= 48) {
      if (sign16((i16)(m->x - cx)) == g.face || iabs((int)m->x - cx) <= 16)
        return m;
    }
  }
  return 0;
}

static void hit_mob(Mob *m)
{
  u8 d = weapon_dmg();
  m->ft = 4;                       /* damage flash */
  if (m->hp <= d) m->hp = 0; else m->hp -= d;
  if (m->type != M_DRAGON) {
    m->vx = (i16)(g.face * 6);
    m->vy = -4;
  }
  if (m->hp == 0) {
    m->dead = 1;
    switch (m->type) {
      case M_ENDERMAN:
        spawn_drop(I_PEARL, 2, m->x, m->y);   /* 3 kills = 6 pearls */
        break;
      case M_BLAZE:
        spawn_drop(I_ROD, 2, m->x, m->y);     /* 2 kills = 8 powder */
        break;
      case M_DRAGON:
        /* victory */
        g.won = 1; g.winscreen = 1;
        msg("YOU BEAT MINECRAFT!");
        {
          int x;
          for (x = 126; x <= 130; x++) set_t(x, 49, B_ENDPORTAL);
        }
        break;
      default: break;
    }
  } else if (m->type == M_ENDERMAN) {
    m->flags |= 1;
  }
}

static void shoot(i16 x, i16 y, i8 dir, u8 src)
{
  u8 i;
  for (i = 0; i < MAXSHOTS; i++)
    if (shots[i].ttl == 0) {
      shots[i].x = x; shots[i].y = y;
      shots[i].vx = (i16)(dir * 5); shots[i].vy = 0;
      shots[i].ttl = 40; shots[i].src = src;
      return;
    }
}

/* ------------------------------------------------------------------ */
/* Nether portal ignition (flint and steel)                            */
/* ------------------------------------------------------------------ */
/* side-view doorway: back column + lintel + floor of obsidian, front
 * open so the player can walk in.  side=-1: back on the left. */
static u8 frame_ok(int x0, int y0, int side)
{
  int x, y;
  int bx = side < 0 ? x0 - 1 : x0 + 2;
  /* interior 2 wide, 3 tall must be air */
  for (y = y0 - 1; y <= y0 + 1; y++)
    for (x = x0; x <= x0 + 1; x++)
      if (get_t(x, y) != B_AIR) return 0;
  /* back column */
  for (y = y0 - 2; y <= y0 + 2; y++)
    if (get_t(bx, y) != B_OBSIDIAN) return 0;
  /* lintel and floor */
  for (x = x0; x <= x0 + 1; x++) {
    if (get_t(x, y0 - 2) != B_OBSIDIAN) return 0;
    if (get_t(x, y0 + 2) != B_OBSIDIAN) return 0;
  }
  return 1;
}

static u8 try_ignite(int tx, int ty)
{
  int x0, y, side;
  for (x0 = tx - 1; x0 <= tx; x0++)
    for (side = -1; side <= 1; side += 2) {
    if (!frame_ok(x0, ty, side)) continue;
    for (y = ty - 1; y <= ty + 1; y++) {
      set_t(x0, y, B_PORTAL);
      set_t(x0 + 1, y, B_PORTAL);
    }
    if (g.dim == DIM_OVER) {
      ov_portal_x = (i16)x0; ov_portal_y = (i16)ty;
    }
    prog |= P_PORTAL;
    msg("PORTAL LIT");
    return 1;
    }
  return 0;
}

/* ------------------------------------------------------------------ */
/* End portal frames                                                   */
/* ------------------------------------------------------------------ */
static void place_eye(int x, int y)
{
  int fx;
  u8 full = 1;
  set_t(x, y, B_FRAME_EYE);
  inv_remove(I_EYE, 1);
  for (fx = sh_x - 3; fx <= sh_x + 2; fx++)
    if (get_t(fx, sh_y) != B_FRAME_EYE) full = 0;
  if (full) {
    for (fx = sh_x - 3; fx <= sh_x + 2; fx++)
      set_t(fx, sh_y - 1, B_ENDPORTAL);
    msg("THE PORTAL IS OPEN");
  } else msg("EYE PLACED");
}

static void use_eye(void)
{
  int tx = (g.px + 6) >> 4, ty = (g.py + 20) >> 4;
  int dx, dy;
  if (g.aim) {
    if (get_t(g.aimx, g.aimy) == B_FRAME) { place_eye(g.aimx, g.aimy); }
    else msg("THE EYE POINTS THE WAY");
    return;
  }
  for (dy = -2; dy <= 2; dy++)
    for (dx = -2; dx <= 2; dx++) {
      if (get_t(tx + dx, ty + dy) != B_FRAME) continue;
      place_eye(tx + dx, ty + dy);
      return;
    }
  msg("THE EYE POINTS THE WAY");
}

/* ------------------------------------------------------------------ */
/* Use item / place block                                              */
/* ------------------------------------------------------------------ */
static void consume_held(void)
{
  if (g.inv[g.sel].n > 1) g.inv[g.sel].n--;
  else g.inv[g.sel].id = 0;
}

static void do_use(void)
{
  u8 it = held_item();
  int x, y;
  u8 liq;
  switch (it) {
    case 0: return;
    case I_BUCKET:
      liq = g.aim ? get_t(g.aimx, g.aimy) : scan_liquid(&x, &y);
      if (liq == B_WATER) {
        if (g.aim) set_t(g.aimx, g.aimy, B_AIR);
        else set_t(x, y, B_AIR);
        g.inv[g.sel].id = I_BUCKET_W; msg("WATER BUCKET");
      } else if (liq == B_LAVA) {
        if (g.aim) set_t(g.aimx, g.aimy, B_AIR);
        else set_t(x, y, B_AIR);
        g.inv[g.sel].id = I_BUCKET_L; msg("LAVA BUCKET");
      }
      return;
    case I_BUCKET_W:
      if ((g.aim ? place_at(g.aimx, g.aimy, B_WATER, 1) : try_place(B_WATER)))
        g.inv[g.sel].id = I_BUCKET;
      return;
    case I_BUCKET_L:
      if ((g.aim ? place_at(g.aimx, g.aimy, B_LAVA, 1) : try_place(B_LAVA)))
        g.inv[g.sel].id = I_BUCKET;
      return;
    case I_FSTEEL:
      if (g.aim) {
        if (try_ignite(g.aimx, g.aimy)) return;
        msg("NOTHING TO LIGHT");
        return;
      }
      if (scan_break(&x, &y)) {
        /* try the air tiles beside the target */
        if (try_ignite(x + (g.face > 0 ? 1 : -1), y) ||
            try_ignite(x, y - 1) || try_ignite(x, y + 1) ||
            try_ignite(x + (g.face > 0 ? 1 : -1), y - 1) ||
            try_ignite(x + (g.face > 0 ? 1 : -1), y + 1))
          return;
      }
      msg("NOTHING TO LIGHT");
      return;
    case I_EYE:
      use_eye();
      return;
    default:
      if (it < B_NB && solid_tab[it]) {
        if (g.aim ? place_at(g.aimx, g.aimy, it, 1) : try_place(it)) {
          consume_held();
          if (it == B_CRAFT) prog |= P_PTAB;
          if (it == B_FURNACE) prog |= P_FURN;
        } else msg("CANNOT PLACE HERE");
      }
      return;
  }
}

/* ------------------------------------------------------------------ */
/* Mob updates                                                        */
/* ------------------------------------------------------------------ */
static void mob_zombie(Mob *m)
{
  i8 dir = sign16((i16)(g.px - m->x));
  if (m->t1 == 0) { m->t1 = 8 + (u8)(rng() & 7); }
  m->t1--;
  if (iabs((int)g.px - m->x) > 320) {
    if ((rng() & 15) == 0) dir = (i8)-dir;
  }
  m->vx = (i16)(dir * 2);
  if (m->flags & 2) m->vy = 0;
  else { m->vy += 3; if (m->vy > 24) m->vy = 24; }
  {
    /* vy 0 -> probe 1 unit down so resting contact is detected */
    u8 gr = phys_move(&m->x, &m->y, m->vx, m->vy ? m->vy : 1, 12, 28);
    if (gr) {
      m->flags |= 2; m->vy = 0;
      /* jump when blocked */
      if (m->vx && solid_at_t((m->x + (m->vx > 0 ? 13 : -2)) >> 4,
                               (m->y + 20) >> 4))
        m->vy = -11, m->flags &= ~2;
    } else m->flags &= ~2;
  }
}

static void mob_enderman(Mob *m)
{
  i8 dir;
  if (m->flags & 1) {
    dir = sign16((i16)(g.px - m->x));
    m->vx = (i16)(dir * 3);
  } else {
    if (m->t1 == 0) {
      m->t1 = 12 + (u8)(rng() & 15);
      m->vx = (i16)(((int)(rng() & 3) - 1) * 2);
    }
    m->t1--;
  }
  if (m->flags & 2) m->vy = 0;
  else { m->vy += 3; if (m->vy > 24) m->vy = 24; }
  {
    u8 gr = phys_move(&m->x, &m->y, m->vx, m->vy ? m->vy : 1, 10, 44);
    if (gr) {
      m->flags |= 2; m->vy = 0;
      if (m->vx && solid_at_t((m->x + (m->vx > 0 ? 11 : -2)) >> 4,
                               (m->y + 36) >> 4))
        m->vy = -11, m->flags &= ~2;
    } else m->flags &= ~2;
  }
}

static void mob_blaze(Mob *m)
{
  i16 dy = (i16)(g.py - 32 - m->y);
  i8 dir = sign16((i16)(g.px - m->x));
  if (iabs((int)g.px - m->x) > 56) m->vx = (i16)(dir * 3);
  else if (iabs((int)g.px - m->x) < 24) m->vx = (i16)(-dir * 2);
  else m->vx = 0;
  m->vy = (i16)(sign16(dy) * 2);
  if ((rng() & 3) == 0) m->vy += (i16)((int)(rng() & 3) - 1);
  m->x += m->vx; m->y += m->vy;
  if (m->y < 16) m->y = 16;
  if (m->t1 == 0) {
    m->t1 = 10 + (u8)(rng() & 7);
    if (iabs((int)g.px - m->x) < 160)
      shoot(m->x, m->y + 6, dir, 1);
  }
  m->t1--;
}

static void mob_dragon(Mob *m)
{
  if (m->t1 == 0) {
    /* circling high above the island */
    m->x += m->vx;
    if (m->x < 80 * 16) m->vx = 6;
    if (m->x > 176 * 16) m->vx = -6;
    m->y = (i16)(12 * 16 + ((g.time & 31) < 16 ? (g.time & 15) * 6
                                                 : (31 - (g.time & 31)) * 6));
    if ((g.time & 7) == 0)
      shoot(m->x, m->y + 10, sign16((i16)(g.px - m->x)), 2);
    /* dive when roughly above the player */
    if (m->t2) m->t2--;
    else if (iabs((int)m->x - g.px) < 48) {
      m->t1 = 1;
      m->vy = 6;
      m->flags = (u8)((m->flags & ~4) | (m->vx > 0 ? 4 : 0));
      m->vx = sign16((i16)(g.px - m->x)) * 4;
    }
  } else if (m->t1 == 1) {
    /* diving, tracking the player horizontally */
    if (m->x < g.px - 8) m->vx = 4;
    else if (m->x > g.px + 8) m->vx = -4;
    else m->vx = 0;
    m->vy = 6;
    m->x += m->vx;
    m->y += m->vy;
    if (m->y > 44 * 16) { m->t1 = 2; m->vy = -6; }
  } else {
    /* climbing back up */
    m->x += (m->flags & 4) ? 5 : -5;
    m->y += m->vy;
    if (m->y <= 12 * 16) {
      m->t1 = 0;
      m->t2 = 24;
      m->vx = (m->flags & 4) ? 6 : -6;
    }
  }
}

static void update_mobs(void)
{
  u8 i;
  for (i = 0; i < MAXMOBS; i++) {
    Mob *m = &mobs[i];
    if (m->dead) continue;
    switch (m->type) {
      case M_ZOMBIE:   mob_zombie(m); break;
      case M_ENDERMAN: mob_enderman(m); break;
      case M_BLAZE:    mob_blaze(m); break;
      case M_DRAGON:   mob_dragon(m); break;
    }
    /* melee contact damage: zombie always, enderman once provoked */
    if ((m->type == M_ZOMBIE || (m->type == M_ENDERMAN && (m->flags & 1))) &&
        rect_hit(m->x, m->y, g.px, g.py, 20, 36))
      hurt_dir(m->type == M_ENDERMAN ? 6 : 4, sign16((i16)(g.px - m->x)));
    if (m->ft) m->ft--;
    /* despawn far mobs (never the dragon) */
    if (m->type != M_DRAGON && iabs((int)m->x - g.px) > 600) m->dead = 1;
  }
}

/* ------------------------------------------------------------------ */
/* Mob spawning                                                        */
/* ------------------------------------------------------------------ */
static u8 mob_count(u8 type)
{
  u8 i, n = 0;
  for (i = 0; i < MAXMOBS; i++)
    if (!mobs[i].dead && mobs[i].type == type) n++;
  return n;
}

static u8 mob_total(void)
{
  u8 i, n = 0;
  for (i = 0; i < MAXMOBS; i++)
    if (!mobs[i].dead) n++;
  return n;
}

static void try_spawn(u8 type, int tx, int ty)
{
  u8 i;
  if (tx < 2 || tx >= WORLD_W - 2 || ty < 2 || ty >= WORLD_H - 2) return;
  if (get_t(tx, ty) != B_AIR || get_t(tx, ty - 1) != B_AIR) return;
  if (!solid_at_t(tx, ty + 1)) return;
  for (i = 0; i < MAXMOBS; i++)
    if (mobs[i].dead) {
      mobs[i].dead = 0; mobs[i].type = type;
      mobs[i].hp = type == M_ENDERMAN ? 16 : 8;
      mobs[i].x = (i16)(tx << 4); mobs[i].y = (i16)((ty - 1) << 4);
      mobs[i].vx = 0; mobs[i].vy = 0;
      mobs[i].t1 = 0; mobs[i].t2 = 0; mobs[i].flags = 0; mobs[i].ft = 0;
      return;
    }
}

static void spawn_logic(void)
{
  int px = g.px >> 4, py = g.py >> 4;
  int side, tx, ty, tries;
  u8 night = (u8)((g.time & 1023) >= 512);
  u8 cave = (u8)(py > surface_y(px) + 6);
  if (g.dim != DIM_OVER) return;
  if (mob_total() >= 6) return;
  side = (rng() & 1) ? 1 : -1;
  tx = px + side * (12 + (int)(rng() & 7));
  if (!night && !cave) {
    /* daytime surface: only endermen roam, and rarely */
    if ((rng() & 15) == 0) try_spawn(M_ENDERMAN, tx, surface_y(tx) - 1);
    return;
  }
  for (tries = 0; tries < 8; tries++) {
    int sx = tx + (int)(rng() & 7) - 3;
    if (night && !cave) {
      ty = surface_y(sx) - 1;          /* surface at night */
    } else {
      ty = py + (int)(rng() & 9) - 4;  /* caves near player */
    }
    if (ty > 2 && ty < WORLD_H - 2) {
      u8 type = M_ZOMBIE;
      if ((rng() & 15) == 0) type = M_ENDERMAN;
      try_spawn(type, sx, ty);
      return;
    }
  }
}

/* ------------------------------------------------------------------ */
/* Blazes from the nether spawner                                      */
/* ------------------------------------------------------------------ */
static void spawner_logic(void)
{
  u8 i;
  if (g.dim != DIM_NETHER || spawner_x < 0) return;
  if (iabs((int)(spawner_x << 4) - g.px) > 180) return;
  if (iabs((int)(spawner_y << 4) - g.py) > 120) return;
  if (mob_count(M_BLAZE) >= 3) return;
  if ((g.time & 15) != 0) return;
  for (i = 0; i < MAXMOBS; i++)
    if (mobs[i].dead) {
      mobs[i].dead = 0; mobs[i].type = M_BLAZE;
      mobs[i].hp = 8;
      mobs[i].x = (i16)(spawner_x << 4);
      mobs[i].y = (i16)((spawner_y - 1) << 4);
      mobs[i].vx = 0; mobs[i].vy = 0;
      mobs[i].t1 = 8; mobs[i].t2 = 0; mobs[i].flags = 0; mobs[i].ft = 0;
      return;
    }
}

/* ------------------------------------------------------------------ */
/* Drops / shots update                                                */
/* ------------------------------------------------------------------ */
static void update_drops(void)
{
  u8 i;
  for (i = 0; i < MAXDROPS; i++) {
    Drop *d = &drops[i];
    u8 t;
    if (d->n == 0) continue;
    d->ttl--;
    if (d->ttl == 0) { d->n = 0; continue; }
    d->age++;
    t = get_t(d->x >> 4, (d->y + 7) >> 4);
    if (t == B_WATER) { d->vy = -2; d->vx = 0; }
    else {
      d->vy += 3; if (d->vy > 12) d->vy = 12;
    }
    /* horizontal toss, stopped by walls */
    if (d->vx) {
      i16 nx = (i16)(d->x + d->vx);
      if (solid_at_t((nx + (d->vx > 0 ? 7 : 0)) >> 4, (d->y + 4) >> 4))
        d->vx = 0;
      else d->x = nx;
    }
    {
      i16 ny = (i16)(d->y + d->vy);
      if (d->vy > 0 && solid_at_t(d->x >> 4, (ny + 7) >> 4)) {
        ny = (i16)((((ny + 7) >> 4) << 4) - 8);
        d->vy = 0;
        /* slide to a stop (halve toward zero, no signed >>) */
        if (d->vx < 0) d->vx = (i16)-((int)(-d->vx) >> 1);
        else d->vx >>= 1;
      }
      d->y = ny;
    }
    /* grace period so dropped items don't boomerang */
    if (d->age > 8 && rect_hit(d->x, d->y, g.px, g.py, 24, 40)) {
      if (inv_add(d->id, d->n)) {
        d->n = 0;
        if (d->id == B_LOG) prog |= P_LOG;
        if (d->id == B_COBBLE) prog |= P_STONE;
        if (d->id == I_ROD) prog |= P_ROD;
        if (d->id == I_PEARL) prog |= P_PEARL;
      }
    }
  }
}

static void update_shots(void)
{
  u8 i;
  for (i = 0; i < MAXSHOTS; i++) {
    Shot *s = &shots[i];
    if (s->ttl == 0) continue;
    s->ttl--;
    /* gentle homing */
    if (s->ttl & 1) {
      if (g.py > s->y && s->vy < 4) s->vy++;
      else if (g.py < s->y && s->vy > -4) s->vy--;
    }
    s->x += s->vx; s->y += s->vy;
    if (solid_at_t(s->x >> 4, s->y >> 4)) { s->ttl = 0; continue; }
    if (rect_hit(s->x, s->y, g.px, g.py, 20, 36)) {
      hurt_dir(s->src == 2 ? 4 : 3, sign16((i16)(s->x - g.px)));
      s->ttl = 0;
    }
  }
}

/* ------------------------------------------------------------------ */
/* Menu handling                                                       */
/* ------------------------------------------------------------------ */
static void menu_tick(void)
{
  if (g.menu == 1) {
    if (kpressed(K_UP)) {
      if (g.menusel == 0) g.menusel = (u8)(recipe_count - 1);
      else g.menusel--;
    }
    if (kpressed(K_DOWN)) {
      g.menusel++;
      if (g.menusel >= recipe_count) g.menusel = 0;
    }
    if (kpressed(K_ACT)) {
      if (can_craft(g.menusel)) {
        do_craft(g.menusel);
        if (recipes[g.menusel].out == B_PLANKS) prog |= P_PLANKS;
        if (recipes[g.menusel].out == B_CRAFT) prog |= P_TABLE;
        if (recipes[g.menusel].out == I_SPICK) prog |= P_SPICK;
        if (recipes[g.menusel].out == B_FURNACE) prog |= P_FURN;
      } else msg(recipes[g.menusel].table && !near_table()
                     ? "NEED CRAFTING TABLE NEARBY"
                     : "MISSING MATERIALS");
    }
    if (kpressed(K_USE) || kpressed(K_BACK)) g.menu = 0;
    /* keep the list window scrolled around the cursor */
    if (g.menusel < g.menutop) g.menutop = g.menusel;
    if (g.menusel > g.menutop + 7) g.menutop = (u8)(g.menusel - 7);
    return;
  }
  if (g.menu == 2) {
    /* smelting: hold ACT near the furnace */
    if (key(K_ACT) && near_furnace() &&
        inv_count(B_IRON) > 0 && inv_count(I_COAL) > 0) {
      g.smelt_t++;
      if (g.smelt_t >= 12) {
        g.smelt_t = 0;
        inv_remove(B_IRON, 1);
        inv_remove(I_COAL, 1);
        inv_add(I_IRONING, 1);
        prog |= P_INGOT;
        msg("IRON INGOT");
      }
    } else if (g.smelt_t) g.smelt_t--;
    if (kpressed(K_USE) || kpressed(K_BACK)) g.menu = 0;
    return;
  }
}

/* ------------------------------------------------------------------ */
/* Debug helpers                                                       */
/* ------------------------------------------------------------------ */
static void dbg_give(void)
{
  inv_add(I_DPICK, 1); inv_add(I_DSWORD, 1); inv_add(I_FSTEEL, 1);
  inv_add(I_BUCKET_W, 1); inv_add(I_EYE, 6); inv_add(I_ROD, 3);
  inv_add(I_PEARL, 3); inv_add(B_OBSIDIAN, 20); inv_add(B_FURNACE, 1);
  inv_add(I_COAL, 16); inv_add(B_IRON, 8); inv_add(B_CRAFT, 1);
  msg("DEBUG KIT");
}

/* debug: build and light a nether portal two tiles ahead */
static void dbg_portal(void)
{
  int tx = ((g.px + 6) >> 4) + (g.face > 0 ? 2 : -3);
  int ty = (g.py >> 4) + 1;   /* player's feet row (tree-proof) */
  int x, y;
  if (g.dim != DIM_OVER) return;
  for (y = ty - 3; y <= ty; y++)
    for (x = tx - 4; x <= tx + 3; x++) set_t(x, y, B_AIR);
  for (y = ty - 3; y <= ty + 1; y++) set_t(tx + 2, y, B_OBSIDIAN);
  for (x = tx; x <= tx + 1; x++) set_t(x, ty - 3, B_OBSIDIAN);
  for (x = tx - 4; x <= tx + 2; x++) set_t(x, ty + 1, B_OBSIDIAN);
  try_ignite(tx, ty - 1);
  /* stand the player on the level approach so tests can walk in */
  g.px = (i16)((tx - 2) << 4);
  g.py = (i16)(((ty + 1) << 4) - 28);
  g.pvx = 0; g.pvy = 0;
}

/* debug: a little lava pool two tiles ahead (obsidian conversion test) */
static void dbg_lava(void)
{
  int tx = ((g.px + 6) >> 4) + (g.face > 0 ? 2 : -3);
  int ty = (g.py >> 4) + 1;   /* player's feet row (tree-proof) */
  set_t(tx, ty, B_LAVA);
  set_t(tx + 1, ty, B_LAVA);
  set_t(tx, ty + 1, B_OBSIDIAN);
  set_t(tx + 1, ty + 1, B_OBSIDIAN);
}

static void dbg_teleport(void)
{
  dbg_tstate++;
  switch (dbg_tstate & 3) {
    case 0:
      travel(DIM_OVER, g.spawn_x, g.spawn_y);
      break;
    case 1:
      if (g.dim != DIM_OVER) travel(DIM_OVER, sh_x, -1);
      g.px = (i16)(sh_x << 4); g.py = (i16)((sh_y - 3) << 4);
      g.pvx = 0; g.pvy = 0;
      msg("STRONGHOLD");
      break;
    case 2:
      travel(DIM_NETHER, 20, 17);
      if (spawner_x >= 0) {
        g.px = (i16)((spawner_x - 2) << 4);
        g.py = (i16)((spawner_y + 1) << 4);
        g.pvx = 0; g.pvy = 0;
      }
      break;
    default:
      travel(DIM_END, end_spawn_x, end_spawn_y);
      break;
  }
}

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */
void game_init(u16 seed)
{
  u8 i;
  world_init(seed);
  gen_overworld();
  for (i = 0; i < sizeof(GameState); i++) ((u8 *)&g)[i] = 0;
  for (i = 0; i < MAXMOBS; i++) mobs[i].dead = 1;
  for (i = 0; i < MAXDROPS; i++) drops[i].n = 0;
  for (i = 0; i < MAXSHOTS; i++) shots[i].ttl = 0;
  g.dim = DIM_OVER;
  g.hp = 20;
  g.face = 1;
  g.spawn_x = 128;
  {
    int x;
    for (x = 124; x <= 132; x++) {
      int sy = surface_y(x);
      /* reject tree-canopy "ground" */
      if (get_t(x, sy) == B_LEAVES) continue;
      if (get_t(x, sy - 1) == B_AIR && get_t(x, sy - 2) == B_AIR &&
          get_t(x, sy - 3) == B_AIR) { g.spawn_x = (i16)x; break; }
    }
  }
  g.spawn_y = (i16)(surface_y(g.spawn_x) - 3);
  g.px = (i16)(g.spawn_x << 4);
  g.py = (i16)(g.spawn_y << 4);
  prog = 0;
  dbg_tstate = 0;
  msg("MINECRAFT 1960");
}

/* ------------------------------------------------------------------ */
/* One logic tick (4 Hz)                                               */
/* ------------------------------------------------------------------ */
void game_tick(void)
{
  u8 k;

  if (g.winscreen) {
    if (kpressed(K_BACK)) g.winscreen = 0;
    return;
  }

  /* slot selection */
  if (kpressed(K_NEXT)) g.sel = (u8)((g.sel + 1) % MAXSLOTS);
  if (kpressed(K_PREV)) g.sel = (u8)((g.sel + MAXSLOTS - 1) % MAXSLOTS);
  for (k = 0; k < 9; k++)
    if (kpressed(K_D1 + k)) g.sel = k;
  if (kpressed(K_D0) && MAXSLOTS > 9) g.sel = 9;

  if (kpressed(K_FPS)) {
    g.hifps = (u8)!g.hifps;
    msg(g.hifps ? "HIGH FPS MODE" : "2 FPS MODE");
  }

  if (kpressed(K_CREATIVE)) {
    g.creative = (u8)!g.creative;
    if (!g.creative) g.flying = 0;
    msg(g.creative ? "CREATIVE MODE" : "SURVIVAL MODE");
  }

  /* double-tap jump key toggles flight in creative; taps need a
   * release between them so held-key repeat never retriggers */
  {
    static u8 up_held;
    /* wtap stores tap-time+1 so 0 means "no recent tap" (a plain 0
     * would double-tap with the first ticks after world start) */
    if (kpressed(K_UP) && !up_held && !g.aim && g.creative) {
      if (g.wtap && (u16)(g.time + 1 - g.wtap) <= 5) {
        g.flying = (u8)!g.flying;
        g.wtap = 0;
      } else g.wtap = (u16)(g.time + 1);
    }
    up_held = (u8)key(K_UP);
  }

  if (kpressed(K_DROP)) {
    Slot *sl = &g.inv[g.sel];
    if (sl->id) {
      /* toss in the facing direction */
      Drop *d = spawn_drop(sl->id, 1, (i16)(g.px + 4), (i16)(g.py + 6));
      if (d) {
        d->vx = g.face ? 6 : -6;
        d->vy = -6;
      }
      if (sl->n > 1) sl->n--;
      else sl->id = 0;
    }
  }

  if (g.menu) { menu_tick(); return; }

  if (g.dead) {
    if (g.deadt) {
      g.deadt--;
    } else {
      g.dead = 0; g.hp = 20; g.invuln = 12;
      if (g.dim != DIM_OVER) travel(DIM_OVER, g.spawn_x, g.spawn_y);
      else {
        g.px = (i16)(g.spawn_x << 4);
        g.py = (i16)(g.spawn_y << 4);
        g.pvx = 0; g.pvy = 0;
      }
      msg("RESPAWNED");
    }
    return;
  }

  g.time++;
  if ((g.time & 1023) == 0) g.days++;
  if (g.invuln) g.invuln--;
  if (g.msgt) g.msgt--;

  if (kpressed(K_MENU)) {
    if (near_furnace() && inv_count(B_IRON) > 0) { g.menu = 2; g.smelt_t = 0; }
    else { g.menu = 1; g.menusel = 0; g.menutop = 0; }
    return;
  }
  if (kpressed(K_DBG_G)) dbg_give();
  if (kpressed(K_DBG_T)) { dbg_teleport(); return; }
  if (kpressed(K_DBG_H)) { g.god = !g.god; msg(g.god ? "GOD MODE ON" : "GOD MODE OFF"); }
  if (kpressed(K_DBG_P)) dbg_portal();
  if (kpressed(K_DBG_L)) dbg_lava();
  if (kpressed(K_DBG_M)) g.nomine = !g.nomine;

  /* ---------------- aim mode ---------------- */
  if (kpressed(K_AIM)) {
    g.aim = (u8)!g.aim;
    if (g.aim) {
      g.aimx = (i16)(((g.px + 6) >> 4) + (g.face ? 1 : -1));
      g.aimy = (i16)((g.py + 14) >> 4);
      msg("AIM: WASD CURSOR, Z/X ACT");
    }
  }
  if (g.aim) {
    i16 ptx = (i16)((g.px + 6) >> 4), pty = (i16)((g.py + 14) >> 4);
    if (kpressed(K_LEFT)  || (key(K_LEFT)  && (g.time & 3) == 0)) g.aimx--;
    if (kpressed(K_RIGHT) || (key(K_RIGHT) && (g.time & 3) == 0)) g.aimx++;
    if (kpressed(K_UP)    || (key(K_UP)    && (g.time & 3) == 0)) g.aimy--;
    if (kpressed(K_DOWN)  || (key(K_DOWN)  && (g.time & 3) == 0)) g.aimy++;
    if (g.aimx < (i16)(ptx - 4)) g.aimx = (i16)(ptx - 4);
    if (g.aimx > (i16)(ptx + 4)) g.aimx = (i16)(ptx + 4);
    if (g.aimy < (i16)(pty - 4)) g.aimy = (i16)(pty - 4);
    if (g.aimy > (i16)(pty + 4)) g.aimy = (i16)(pty + 4);
  }

  /* ---------------- movement ---------------- */
  {
    u8 in_water = (u8)(get_t((g.px + 6) >> 4, (g.py + 14) >> 4) == B_WATER ||
                       get_t((g.px + 6) >> 4, (g.py + 26) >> 4) == B_WATER);
    u8 in_lava  = (u8)(get_t((g.px + 6) >> 4, (g.py + 20) >> 4) == B_LAVA);
    i16 prefall = g.pvy;

    if (g.aim) g.pvx = 0;                    /* WASD drives the cursor */
    else if (key(K_LEFT))  { g.pvx = -4; g.face = 0; }
    else if (key(K_RIGHT)) { g.pvx = 4; g.face = 1; }
    else g.pvx = 0;

    if (g.flying) {
      /* creative flight: no gravity, W/S for vertical */
      if (!g.aim && key(K_UP)) g.pvy = -6;
      else if (!g.aim && key(K_DOWN)) g.pvy = 6;
      else g.pvy = 0;
    } else if (in_water) {
      if (!g.aim && key(K_UP)) g.pvy = -5;
      else { g.pvy += 1; if (g.pvy > 3) g.pvy = 3; }
    } else if (in_lava) {
      if (!g.aim && key(K_UP)) g.pvy = -3;
      else { g.pvy += 1; if (g.pvy > 2) g.pvy = 2; }
      if (!g.god && !g.creative) {
        if (g.hp > 1) { g.hp -= 1; regen_reset(); }
        else { g.hp = 0; g.dead = 1; g.deadt = 10; msg("YOU DIED"); }
      }
    } else {
      /* -13: after same-tick gravity the apex is ~22 units, enough to
       * hop onto a full block (16) */
      if (!g.aim && key(K_UP) && g.onground) g.pvy = -13;
      g.pvy += 3;
      if (g.pvy > 24) g.pvy = 24;
    }

    g.onground = phys_move(&g.px, &g.py, g.pvx, g.pvy, 12, 28);
    if (g.onground) {
      if (g.pvy > 18 && !in_water && !in_lava) {
        /* 4x the halved fall damage: hurt() halves again, so the
         * damage dealt ends up pvy-18 */
        u8 fd = (u8)((g.pvy - 18) << 1);
        if (fd) hurt_dir(fd, 0);
      }
      g.pvy = 0;
    }
    if (prefall > 24) g.pvy = 24;
  }

  /* ---------------- portals ---------------- */
  {
    u8 ct = get_t((g.px + 6) >> 4, (g.py + 8) >> 4);
    u8 cf = get_t((g.px + 6) >> 4, (g.py + 24) >> 4);
    if (g.portal_t < 0) {
      g.portal_t++;
    } else if (ct == B_PORTAL || cf == B_PORTAL) {
      g.portal_t++;
      if (g.portal_t >= 4) {
        if (g.dim == DIM_OVER) travel(DIM_NETHER, ov_portal_x, ov_portal_y);
        else travel(DIM_OVER, ov_portal_x, ov_portal_y);
        return;
      }
    } else if (ct == B_ENDPORTAL || cf == B_ENDPORTAL) {
      g.portal_t++;
      if (g.portal_t >= 2) {
        if (g.dim == DIM_OVER) travel(DIM_END, end_spawn_x, end_spawn_y);
        else {
          travel(DIM_OVER, sh_x, -1);
          if (g.won) msg("THE END. SANDBOX MODE");
        }
        return;
      }
    } else g.portal_t = 0;
  }

  /* ---------------- actions ---------------- */
  /* X: interactable blocks win over using the held item */
  if (kpressed(K_USE)) {
    int x = 0, y = 0;
    u8 got = g.aim ? 1 : scan_interact(&x, &y);
    if (g.aim) { x = g.aimx; y = g.aimy; }
    if (got) {
      u8 id = get_t(x, y);
      if (id == B_FURNACE) { g.menu = 2; g.smelt_t = 0; }
      else if (id == B_CRAFT) { g.menu = 1; g.menusel = 0; g.menutop = 0; }
      else got = 0;
    }
    if (!got) do_use();
  }

  if (key(K_ACT)) {
    Mob *m = mob_in_reach();
    if (m) {
      if ((g.time & 1) == 0) hit_mob(m);
    }
    if (!mob_in_reach() && !g.menu && !g.nomine) {
      if (g.aim) do_mine_at(g.aimx, g.aimy);
      else {
        int x, y;
        if (scan_break(&x, &y)) do_mine_at(x, y);
        else g.mtarget_valid = 0;
      }
    }
  } else {
    g.mtarget_valid = 0; g.mprogress = 0;
  }

  /* ---------------- world simulation ---------------- */
  regen_tick();
  update_mobs();
  spawner_logic();
  update_drops();
  update_shots();
  if ((g.time & 15) == 0) spawn_logic();

  /* ---------------- hints ---------------- */
  if (g.msgt == 0 && ++hint_t >= 20) {
    hint_t = 0;
    if (!(prog & P_LOG))                        msg("HOLD Z TO CHOP A TREE");
    else if (!(prog & P_PLANKS))                msg("PRESS C TO CRAFT");
    else if (!(prog & P_TABLE))                 msg("CRAFT A CRAFTING TABLE");
    else if (!(prog & P_PTAB))                  msg("PLACE TABLE WITH X");
    else if (!(prog & P_STONE))                 msg("MINE STONE: WOOD PICKAXE");
    else if (!(prog & P_SPICK))                 msg("CRAFT A STONE PICKAXE");
    else if (!(prog & P_FURN))                  msg("CRAFT FURNACE: 8 COBBLE");
    else if (!(prog & P_INGOT))                 msg("SMELT IRON ORE AND COAL");
    else if (!(prog & P_DIAMOND))               msg("DIG DEEP FOR DIAMONDS");
    else if (!(prog & P_OBSIDIAN)) {
      if (inv_count(I_BUCKET_W))
        msg("POUR WATER ON LAVA: OBSIDIAN");
      else if (inv_count(I_BUCKET))
        msg("SCOOP WATER, POUR ON LAVA");
      else if (inv_count(I_IRONING) >= 3)
        msg("CRAFT A BUCKET: 3 IRON");
      else msg("OBSIDIAN: NEED BUCKET");
    }
    else if (!(prog & P_PORTAL))                msg("FRAME OBSIDIAN, LIGHT IT");
    else if (!(prog & P_ROD))                   msg("NETHER: KILL BLAZES");
    else if (!(prog & P_PEARL))                 msg("KILL ENDERMEN FOR PEARLS");
    else if (!(prog & P_EYE))                   msg("CRAFT EYES OF ENDER");
    else if (!(prog & P_END))                   msg("FIND THE STRONGHOLD");
    else if (!g.won)                            msg("ENTER THE END PORTAL");
  }
  if (inv_count(I_EYE)) prog |= P_EYE;
}
