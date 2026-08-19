/* world.c -- deterministic world generation for all three dimensions.
 *
 * Only two 256x80 byte buffers exist: buffer 0 is the persistent
 * overworld, buffer 1 is regenerated on every nether/end entry.
 * Generation uses an xorshift RNG and no division in loops.
 */
#include "mc.h"

unsigned char worlds[2][WORLD_W * WORLD_H];
unsigned char *wmap;
u8 curbuf;

i16 sh_x, sh_y;
i16 ov_portal_x = -1, ov_portal_y = -1;
i16 spawner_x = -1, spawner_y = -1;
i16 end_spawn_x, end_spawn_y;

static u16 rs;

u16 rng(void)
{
  rs ^= (u16)(rs << 7);
  rs ^= (u16)(rs >> 9);
  rs ^= (u16)(rs << 8);
  return rs;
}

static int rint(int n)            /* 0..n-1 ; init-time only */
{
  return (int)(rng() & 0x7fff) % n;
}

void world_init(u16 seed)
{
  rs = seed ? seed : (u16)1960u;
}

/* ------------------------------------------------------------------ */
/* Safe accessors (bounds clamped; used at init time and by code that  */
/* can hit the world edge).  Hot render loops index wmap directly.     */
/* ------------------------------------------------------------------ */
u8 get_t(int x, int y)
{
  if (x < 0 || x >= WORLD_W || y < 0) return B_AIR;
  if (y >= WORLD_H) return B_BEDROCK;
  return wmap[x + (y << WORLD_SHIFT)];
}

void set_t(int x, int y, u8 id)
{
  if ((unsigned)x < WORLD_W && (unsigned)y < WORLD_H)
    wmap[x + (y << WORLD_SHIFT)] = id;
}

int surface_y(int x)
{
  int y;
  for (y = 0; y < WORLD_H; y++)
    if (wmap[x + (y << WORLD_SHIFT)] != B_AIR) return y;
  return WORLD_H - 1;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static void vein(u8 id, int count, int ymin, int ymax, int big)
{
  int i, j, x, y, dx, dy;
  for (i = 0; i < count; i++) {
    x = rint(WORLD_W);
    y = ymin + rint(ymax - ymin);
    for (j = 0; j < 3 + rint(big); j++) {
      if (get_t(x, y) == B_STONE) set_t(x, y, id);
      dx = rint(3) - 1; dy = rint(3) - 1;
      x += dx; y += dy;
      if (x < 0) x = 0;
      if (x >= WORLD_W) x = WORLD_W - 1;
      if (y < ymin) y = ymin;
      if (y >= WORLD_H - 1) y = WORLD_H - 2;
    }
  }
}

static void cave_walk(int x, int y, int steps)
{
  int i, d, r;
  for (i = 0; i < steps; i++) {
    r = 1 + (rng() & 1);
    for (d = -r; d <= r; d++) {
      set_t(x + d, y, B_AIR);
      set_t(x, y + d, B_AIR);
    }
    switch (rng() & 3) {
      case 0: x++; break;
      case 1: x--; break;
      case 2: y++; break;
      default: y--; break;
    }
    if (x < 2) x = 2;
    if (x > WORLD_W - 3) x = WORLD_W - 3;
    if (y < 8) y = 8;
    if (y > WORLD_H - 3) y = WORLD_H - 3;
  }
}

static void tree(int x, int gy)
{
  /* trunk 4-5 tall so a 2-tall player walks under the canopy */
  int h = 4 + rint(2), i, dx, dy;
  for (i = 1; i <= h; i++) set_t(x, gy - i, B_LOG);
  for (dy = -2; dy <= 0; dy++)
    for (dx = -2; dx <= 2; dx++) {
      if (dx == 0 && dy >= -1) continue;          /* trunk space */
      if (get_t(x + dx, gy - h - 1 + dy) == B_AIR)
        set_t(x + dx, gy - h - 1 + dy, B_LEAVES);
    }
  set_t(x, gy - h - 2, B_LEAVES);
}

/* ------------------------------------------------------------------ */
/* Overworld                                                           */
/* ------------------------------------------------------------------ */
void gen_overworld(void)
{
  int x, y, i, h, sx;
  static int hs[WORLD_W];
  u8 *w = worlds[0];

  curbuf = 0; wmap = w;

  for (i = 0; i < WORLD_W * WORLD_H; i++) w[i] = B_AIR;

  /* heightmap: bounded random walk */
  h = 24;
  for (x = 0; x < WORLD_W; x++) {
    h += rint(3) - 1;
    if (h < 16) h = 16;
    if (h > 36) h = 36;
    hs[x] = h;
  }
  /* flatten the spawn area */
  for (x = 120; x < 136; x++) hs[x] = hs[127];

  for (x = 0; x < WORLD_W; x++) {
    h = hs[x];
    for (y = h; y < WORLD_H; y++) {
      i = x + (y << WORLD_SHIFT);
      if (y == WORLD_H - 1) w[i] = B_BEDROCK;
      else if (y == h)      w[i] = B_GRASS;
      else if (y < h + 4)   w[i] = B_DIRT;
      else                  w[i] = B_STONE;
    }
  }

  /* lakes: basins sunk below y=26 filled with water up to y=26 */
  for (i = 0; i < 3; i++) {
    int cx = 30 + rint(WORLD_W - 60), cw = 8 + rint(10);
    if (cx > 100 && cx < 156) cx += 56;   /* keep spawn dry */
    if (cx > WORLD_W - 16) cx -= 56;
    for (x = cx - cw; x <= cx + cw; x++) {
      int depth = 3 + rint(4);
      if (x < 2 || x >= WORLD_W - 2) continue;
      for (y = 26; y <= 26 + depth; y++) set_t(x, y, B_AIR);
      for (y = 26; y <= 26 + depth; y++)
        if (get_t(x, y) == B_AIR) set_t(x, y, B_WATER);
      set_t(x, 26 + depth + 1, B_DIRT);
    }
  }

  /* caves: drunken walkers */
  for (i = 0; i < 16; i++)
    cave_walk(rint(WORLD_W), 36 + rint(40), 140 + rint(120));

  /* ores */
  vein(B_GRAVEL, 14, 30, 74, 5);
  vein(B_COAL,   56, 28, 78, 6);
  vein(B_IRON,   36, 40, 78, 5);
  vein(B_DIAMOND,16, 62, 78, 4);

  /* lava pools on cave floors: open to air above, carved down so the
   * basin is filled like a real pool */
  for (i = 0; i < 60; i++) {
    int dx, dy, w, d;
    x = 1 + rint(WORLD_W - 2); y = 60 + rint(18);
    if (get_t(x, y) != B_AIR || get_t(x, y - 1) != B_AIR) continue;
    w = 2 + rint(3);
    d = 2 + rint(2);
    for (dx = 0; dx < w && x + dx < WORLD_W - 1; dx++) {
      if (get_t(x + dx, y) != B_AIR || get_t(x + dx, y - 1) != B_AIR)
        continue;
      for (dy = 0; dy <= d; dy++) {
        if (y + dy >= WORLD_H - 1) break;
        if (get_t(x + dx, y + dy) == B_BEDROCK) break;
        set_t(x + dx, y + dy, B_LAVA);
      }
    }
  }

  /* beach sand near water */
  for (x = 1; x < WORLD_W - 1; x++) {
    int sy = surface_y(x);
    if (get_t(x, sy) == B_GRASS &&
        (get_t(x - 1, sy) == B_WATER || get_t(x + 1, sy) == B_WATER)) {
      set_t(x, sy, B_SAND);
      set_t(x, sy + 1, B_SAND);
    }
  }

  /* trees (skip lake water columns) */
  x = 4;
  while (x < WORLD_W - 4) {
    int gy = surface_y(x);
    if (get_t(x, gy) == B_GRASS) { tree(x, gy); x += 5 + rint(8); }
    else x += 2;
  }
  /* guarantee trees near (but not on) spawn */
  for (sx = 118; sx <= 140; sx += 11) {
    int gy = surface_y(sx);
    if (get_t(sx, gy) != B_GRASS) continue;
    if (get_t(sx, gy - 1) != B_AIR) continue;
    tree(sx, gy);
  }

  /* stronghold: buried stonebrick room with the end portal frames */
  sh_x = (i16)(rng() & 1 ? 200 + rint(40) : 16 + rint(40));
  sh_y = 52;
  for (y = sh_y - 3; y <= sh_y + 3; y++)
    for (x = sh_x - 6; x <= sh_x + 6; x++)
      set_t(x, y, B_AIR);
  for (x = sh_x - 6; x <= sh_x + 6; x++) {
    set_t(x, sh_y - 4, B_SBRICK);
    set_t(x, sh_y + 3, B_SBRICK);
  }
  for (y = sh_y - 3; y <= sh_y + 2; y++) {
    set_t(sh_x - 7, y, B_SBRICK);
    set_t(sh_x + 7, y, B_SBRICK);
  }
  /* lava basin under the portal */
  for (x = sh_x - 3; x <= sh_x + 3; x++) {
    set_t(x, sh_y + 2, B_LAVA);
    set_t(x, sh_y + 3, B_SBRICK);
  }
  /* six empty end portal frames on a stone ledge */
  for (x = sh_x - 3; x <= sh_x + 2; x++) {
    set_t(x, sh_y + 1, B_STONE);
    set_t(x, sh_y, B_FRAME);
  }

  ov_portal_x = -1; ov_portal_y = -1;
}

/* ------------------------------------------------------------------ */
/* Nether: regenerated on every entry, portal rebuilt at the saved     */
/* overworld portal coordinates.                                       */
/* ------------------------------------------------------------------ */
static void nportal(int px, int py)
{
  int x, y;
  /* clear interior + frame region */
  for (y = py - 2; y <= py + 2; y++)
    for (x = px - 1; x <= px + 2; x++) set_t(x, y, B_AIR);
  for (y = py - 2; y <= py + 1; y++) set_t(px - 1, y, B_OBSIDIAN);
  set_t(px, py - 2, B_OBSIDIAN); set_t(px + 1, py - 2, B_OBSIDIAN);
  set_t(px, py + 2, B_OBSIDIAN); set_t(px + 1, py + 2, B_OBSIDIAN);
  for (y = py - 1; y <= py + 1; y++) {
    set_t(px, y, B_PORTAL);
    set_t(px + 1, y, B_PORTAL);
  }
  /* floor beneath the portal */
  for (x = px - 2; x <= px + 3; x++) {
    if (get_t(x, py + 3) == B_AIR) set_t(x, py + 3, B_NETHERRACK);
  }
}

void gen_nether(void)
{
  int x, y, i;
  u8 *w = worlds[1];

  curbuf = 1; wmap = w;

  for (i = 0; i < WORLD_W * WORLD_H; i++) w[i] = B_NETHERRACK;
  for (x = 0; x < WORLD_W; x++) {
    w[x] = B_BEDROCK;
    w[x + ((WORLD_H - 1) << WORLD_SHIFT)] = B_BEDROCK;
  }

  /* large caverns */
  for (i = 0; i < 26; i++)
    cave_walk(rint(WORLD_W), 12 + rint(58), 160 + rint(120));

  /* lava sea */
  for (x = 1; x < WORLD_W - 1; x++)
    for (y = 66; y < WORLD_H - 1; y++)
      if (w[x + (y << WORLD_SHIFT)] == B_AIR)
        w[x + (y << WORLD_SHIFT)] = B_LAVA;

  /* fortress: two nether-brick corridors with a spawner room */
  {
    int fy = 26 + rint(12);
    int fx0 = 40 + rint(40), fx1 = 150 + rint(60);
    for (x = fx0; x <= fx1; x++) {
      for (y = fy - 3; y <= fy + 3; y++) set_t(x, y, B_AIR);
      set_t(x, fy - 4, B_NBRICK);
      set_t(x, fy + 2, B_NBRICK);
    }
    /* spawner room */
    for (x = fx1 - 8; x <= fx1; x++)
      for (y = fy - 6; y <= fy + 2; y++) set_t(x, y, B_AIR);
    for (x = fx1 - 8; x <= fx1; x++) set_t(x, fy - 7, B_NBRICK);
    spawner_x = (i16)(fx1 - 4);
    spawner_y = (i16)(fy - 1);
    set_t(spawner_x, spawner_y, B_SPAWNER);
    for (x = fx1 - 8; x <= fx1; x++) set_t(x, fy + 2, B_NBRICK);
  }

  /* arrival portal */
  if (ov_portal_x >= 0) {
    nportal(ov_portal_x, ov_portal_y);
  } else {
    /* no overworld portal recorded: place a safe entry platform */
    int px = 20, py = 20;
    for (x = px - 2; x <= px + 4; x++)
      for (y = py - 4; y <= py + 4; y++) set_t(x, y, B_AIR);
    for (x = px - 2; x <= px + 4; x++) set_t(x, py + 3, B_NETHERRACK);
  }
}

/* ------------------------------------------------------------------ */
/* End: floating endstone island, obsidian pillars, dragon.            */
/* ------------------------------------------------------------------ */
void gen_end(void)
{
  int x, y, i, dx, dy;
  u8 *w = worlds[1];

  curbuf = 1; wmap = w;

  for (i = 0; i < WORLD_W * WORLD_H; i++) w[i] = B_AIR;

  /* island: ellipse centred on (128, 50) */
  for (dy = -6; dy <= 14; dy++)
    for (dx = -52; dx <= 52; dx++) {
      long ex = (long)dx * dx * 6;
      long ey = (long)dy * dy * 130;
      if (ex + ey <= 16000L)
        set_t(128 + dx, 50 + dy, B_ENDSTONE);
    }
  for (x = 76; x <= 180; x++) set_t(x, 50, B_ENDSTONE);

  /* obsidian pillars */
  for (i = 0; i < 4; i++) {
    int px = 100 + i * 19, ph = 6 + rint(5);
    for (y = 50 - ph; y < 50; y++) set_t(px, y, B_OBSIDIAN);
  }

  end_spawn_x = 128; end_spawn_y = 40;
}
