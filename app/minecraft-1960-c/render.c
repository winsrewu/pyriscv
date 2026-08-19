/* render.c -- 1-bit renderer for the 192x168 screen.
 *
 * Style: blocks are drawn as OUTLINES (only faces exposed to air)
 * plus 2-6 "speckle" pixels that identify the block type.  A full
 * frame costs at most ~1600 screen_set() calls, so a 20 KIPS CPU can
 * sustain 2 fps.  When a menu is open the world is not redrawn at all.
 */
#include "mc.h"
#include "display.h"

i16 cam_x, cam_y;

/* ------------------------------------------------------------------ */
/* Primitives                                                          */
/* ------------------------------------------------------------------ */
/* rows >= icon_clip are skipped; set only while drawing hotbar icons
 * so item sprites keep clear of the slot's bottom border */
static int icon_clip = SCR_H;

static void px(int x, int y)
{
  if ((unsigned)x < SCR_W && (unsigned)y < SCR_H && y < icon_clip)
    screen_set(x, y);
}

#ifdef PY_RISCV_GFX
static void ph(int x0, int x1, int y) { screen_hline(x0, x1, y); }
#else
static void ph(int x0, int x1, int y)
{
  int x;
  if ((unsigned)y >= SCR_H || y >= icon_clip) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= SCR_W) x1 = SCR_W - 1;
  for (x = x0; x <= x1; x++) screen_set(x, y);
}
#endif

#ifdef PY_RISCV_GFX
static void pv(int x, int y0, int y1) { screen_vline(x, y0, y1); }
#else
static void pv(int x, int y0, int y1)
{
  int y;
  if ((unsigned)x >= SCR_W) return;
  if (y0 < 0) y0 = 0;
  if (y1 >= SCR_H) y1 = SCR_H - 1;
  if (y1 >= icon_clip) y1 = icon_clip - 1;
  for (y = y0; y <= y1; y++) screen_set(x, y);
}
#endif

#ifdef PY_RISCV_GFX
static void rect(int x, int y, int w, int h) { screen_rect(x, y, w, h); }
#else
static void rect(int x, int y, int w, int h)
{
  ph(x, x + w - 1, y);
  ph(x, x + w - 1, y + h - 1);
  pv(x, y + 1, y + h - 2);
  pv(x + w - 1, y + 1, y + h - 2);
}
#endif

/* ------------------------------------------------------------------ */
/* 3x5 font                                                            */
/* ------------------------------------------------------------------ */
static const u8 font[] = {
  7,5,5,5,7,  /* 0 */
  2,6,2,2,7,  /* 1 */
  6,1,2,4,7,  /* 2 */
  6,1,2,1,6,  /* 3 */
  5,5,7,1,1,  /* 4 */
  7,4,6,1,6,  /* 5 */
  3,4,7,5,7,  /* 6 */
  7,1,2,2,2,  /* 7 */
  7,5,7,5,7,  /* 8 */
  7,5,7,1,6,  /* 9 */
  2,5,7,5,5,  /* A */
  6,5,6,5,6,  /* B */
  3,4,4,4,3,  /* C */
  6,5,5,5,6,  /* D */
  7,4,6,4,7,  /* E */
  7,4,6,4,4,  /* F */
  3,4,5,5,3,  /* G */
  5,5,7,5,5,  /* H */
  7,2,2,2,7,  /* I */
  1,1,1,5,2,  /* J */
  5,5,6,5,5,  /* K */
  4,4,4,4,7,  /* L */
  5,7,7,5,5,  /* M */
  6,5,5,5,5,  /* N */
  2,5,5,5,2,  /* O */
  6,5,6,4,4,  /* P */
  2,5,5,6,1,  /* Q */
  6,5,6,5,5,  /* R */
  3,4,2,1,6,  /* S */
  7,2,2,2,2,  /* T */
  5,5,5,5,7,  /* U */
  5,5,5,5,2,  /* V */
  5,5,7,7,5,  /* W */
  5,5,2,5,5,  /* X */
  5,5,2,2,2,  /* Y */
  7,1,2,4,7,  /* Z */
  0,0,0,0,0,  /* space */
  0,0,0,0,2,  /* . */
  0,2,0,2,0,  /* : */
  2,2,2,0,2,  /* ! */
  6,1,2,0,2,  /* ? */
  0,0,7,0,0,  /* - */
  0,2,7,2,0,  /* + */
  4,2,1,2,4,  /* > */
  1,2,4,2,1,  /* < */
  1,1,2,4,4,  /* / */
  2,2,0,0,0,  /* ' */
  0,0,0,2,4   /* , */
};

static int glyph_of(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
  if (c >= 'a' && c <= 'z') return c - 'a' + 10;
  switch (c) {
    case '.': return 37;  case ':': return 38;  case '!': return 39;
    case '?': return 40;  case '-': return 41;  case '+': return 42;
    case '>': return 43;  case '<': return 44;  case '/': return 45;
    case '\'': return 46; case ',': return 47;
  }
  return 36;
}

#ifdef PY_RISCV_GFX
static void draw_char(int x, int y, char c) { screen_char(x, y, c); }
#else
static void draw_char(int x, int y, char c)
{
  const u8 *g = &font[glyph_of(c) * 5];
  int r, b;
  for (r = 0; r < 5; r++) {
    u8 row = g[r];
    for (b = 0; b < 3; b++)
      if ((row >> (2 - b)) & 1) px(x + b, y + r);
  }
}
#endif

#ifdef PY_RISCV_GFX
static void draw_str(int x, int y, const char *s) { screen_text(x, y, s); }
#else
static void draw_str(int x, int y, const char *s)
{
  while (*s) { draw_char(x, y, *s); x += 4; s++; }
}
#endif

static int str_len(const char *s)
{
  int n = 0;
  while (*s) { n++; s++; }
  return n;
}

/* division-free: repeated subtraction (values are small) */
#ifdef PY_RISCV_GFX
static void draw_num(int x, int y, int v) { screen_num(x, y, v); }
#else
static void draw_num(int x, int y, int v)
{
  static const int pw[4] = { 1000, 100, 10, 1 };
  char buf[5];
  int i = 0, k, d;
  u8 lead = 0;
  if (v < 0) v = 0;
  if (v > 9999) v = 9999;
  for (k = 0; k < 4; k++) {
    d = 0;
    while (v >= pw[k]) { v -= pw[k]; d++; }
    if (d || lead || k == 3) { buf[i++] = (char)('0' + d); lead = 1; }
  }
  buf[i] = 0;
  draw_str(x, y, buf);
}
#endif

/* ------------------------------------------------------------------ */
/* Item / block icons (8x8, procedural)                                */
/* ------------------------------------------------------------------ */
#ifdef PY_RISCV_GFX
static void icon(u8 id, int x, int y) { screen_icon(id, x, y, icon_clip); }
#else
static void icon(u8 id, int x, int y)
{
  u8 t;
  switch (id) {
    case B_GRASS:
      rect(x + 1, y + 1, 6, 6);
      px(x + 2, y); px(x + 4, y); px(x + 6, y);
      px(x + 3, y + 4); px(x + 5, y + 5);
      break;
    case B_DIRT:
      rect(x + 1, y + 1, 6, 6);
      px(x + 3, y + 3); px(x + 5, y + 4); px(x + 2, y + 5);
      break;
    case B_STONE:
      rect(x + 1, y + 1, 6, 6);
      px(x + 3, y + 3); px(x + 5, y + 3); px(x + 4, y + 5);
      break;
    case B_COBBLE:
      rect(x + 1, y + 1, 6, 6);
      px(x + 2, y + 2); px(x + 5, y + 2); px(x + 3, y + 4); px(x + 5, y + 5);
      break;
    case B_LOG:
      pv(x + 2, y + 1, y + 6); pv(x + 5, y + 1, y + 6);
      ph(x + 2, x + 5, y); ph(x + 2, x + 5, y + 7);
      break;
    case B_LEAVES:
      px(x + 1, y + 1); px(x + 4, y + 2); px(x + 6, y + 1);
      px(x + 2, y + 4); px(x + 5, y + 5); px(x + 3, y + 6);
      break;
    case B_PLANKS:
      rect(x + 1, y + 1, 6, 6);
      px(x + 2, y + 3); px(x + 4, y + 3); px(x + 6, y + 3);
      px(x + 1, y + 5); px(x + 3, y + 5); px(x + 5, y + 5);
      break;
    case B_CRAFT:
      rect(x + 1, y + 2, 6, 5);
      ph(x + 1, x + 6, y + 1);
      px(x + 3, y + 4); px(x + 4, y + 4); px(x + 2, y + 5); px(x + 5, y + 5);
      break;
    case B_FURNACE:
      rect(x + 1, y + 1, 6, 6);
      ph(x + 3, x + 5, y + 4);
      px(x + 3, y + 5); px(x + 5, y + 5);
      break;
    case B_SAND:
      rect(x + 1, y + 1, 6, 6);
      px(x + 2, y + 2); px(x + 5, y + 3); px(x + 3, y + 5);
      break;
    case B_GRAVEL:
      rect(x + 1, y + 1, 6, 6);
      px(x + 2, y + 2); px(x + 5, y + 2); px(x + 3, y + 4);
      px(x + 2, y + 5); px(x + 5, y + 5);
      break;
    case B_COAL:
      rect(x + 1, y + 1, 6, 6);
      px(x + 3, y + 3); px(x + 4, y + 3); px(x + 3, y + 4); px(x + 4, y + 4);
      break;
    case B_IRON:
      rect(x + 1, y + 1, 6, 6);
      px(x + 2, y + 2); px(x + 5, y + 3); px(x + 3, y + 5); px(x + 5, y + 5);
      break;
    case B_DIAMOND:
      rect(x + 1, y + 1, 6, 6);
      px(x + 4, y + 2); px(x + 3, y + 3); px(x + 5, y + 3); px(x + 4, y + 4);
      break;
    case B_OBSIDIAN:
      rect(x + 1, y + 1, 6, 6);
      px(x + 2, y + 2); px(x + 3, y + 3); px(x + 4, y + 4); px(x + 5, y + 5);
      px(x + 5, y + 2); px(x + 2, y + 5);
      break;
    case B_NETHERRACK:
      rect(x + 1, y + 1, 6, 6);
      px(x + 2, y + 2); px(x + 5, y + 3); px(x + 3, y + 5);
      break;
    case B_NBRICK: case B_SBRICK:
      rect(x + 1, y + 1, 6, 6);
      ph(x + 1, x + 6, y + 4);
      pv(x + 3, y + 1, y + 3); pv(x + 5, y + 5, y + 6);
      break;
    case B_ENDSTONE:
      rect(x + 1, y + 1, 6, 6);
      px(x + 2, y + 2); px(x + 5, y + 2); px(x + 3, y + 4); px(x + 5, y + 5);
      break;
    case B_BEDROCK:
      rect(x + 1, y + 1, 6, 6);
      px(x + 2, y + 2); px(x + 5, y + 2); px(x + 3, y + 3);
      px(x + 2, y + 5); px(x + 5, y + 5); px(x + 4, y + 6);
      break;
    case I_STICK:
      px(x + 2, y + 6); px(x + 3, y + 5); px(x + 4, y + 4);
      px(x + 5, y + 3); px(x + 6, y + 2);
      break;
    case I_COAL:
      px(x + 3, y + 3); px(x + 4, y + 3); px(x + 3, y + 4);
      px(x + 4, y + 4); px(x + 5, y + 5);
      break;
    case I_FLINT:
      px(x + 4, y + 2); px(x + 3, y + 3); px(x + 4, y + 3); px(x + 5, y + 3);
      px(x + 2, y + 4); px(x + 3, y + 4); px(x + 4, y + 4);
      px(x + 3, y + 5);
      break;
    case I_IRONING:
      rect(x + 1, y + 3, 6, 3);
      px(x + 3, y + 4); px(x + 4, y + 4);
      break;
    case I_DIAMOND:
      px(x + 3, y + 1); px(x + 4, y + 1);
      px(x + 2, y + 2); px(x + 5, y + 2);
      px(x + 1, y + 3); px(x + 6, y + 3);
      px(x + 2, y + 4); px(x + 5, y + 4);
      px(x + 3, y + 5); px(x + 4, y + 5);
      break;
    case I_BUCKET:
      ph(x + 2, x + 5, y + 2);
      pv(x + 2, y + 3, y + 6); pv(x + 5, y + 3, y + 6);
      ph(x + 2, x + 5, y + 6);
      px(x + 3, y + 1); px(x + 4, y + 1);
      break;
    case I_BUCKET_W:
      icon(I_BUCKET, x, y);
      px(x + 3, y + 4); px(x + 4, y + 5);
      break;
    case I_BUCKET_L:
      icon(I_BUCKET, x, y);
      px(x + 3, y + 4); px(x + 4, y + 4); px(x + 3, y + 5); px(x + 4, y + 5);
      break;
    case I_FSTEEL:
      px(x + 2, y + 2); px(x + 2, y + 3); px(x + 2, y + 4);
      px(x + 3, y + 5); px(x + 4, y + 5); px(x + 4, y + 4);
      px(x + 6, y + 2); px(x + 5, y + 1);
      break;
    case I_ROD:
      pv(x + 4, y + 2, y + 6);
      px(x + 3, y + 1); px(x + 5, y + 1); px(x + 4, y);
      break;
    case I_POWDER:
      px(x + 2, y + 5); px(x + 4, y + 5); px(x + 6, y + 5);
      px(x + 3, y + 6); px(x + 5, y + 6); px(x + 3, y + 3);
      break;
    case I_PEARL:
      px(x + 3, y + 1); px(x + 4, y + 1);
      px(x + 2, y + 2); px(x + 5, y + 2);
      pv(x + 1, y + 3, y + 4); pv(x + 6, y + 3, y + 4);
      px(x + 2, y + 5); px(x + 5, y + 5);
      px(x + 3, y + 6); px(x + 4, y + 6);
      px(x + 4, y + 4);
      break;
    case I_EYE:
      px(x + 3, y + 1); px(x + 4, y + 1);
      px(x + 2, y + 2); px(x + 5, y + 2);
      pv(x + 1, y + 3, y + 4); pv(x + 6, y + 3, y + 4);
      px(x + 2, y + 5); px(x + 5, y + 5);
      px(x + 3, y + 6); px(x + 4, y + 6);
      px(x + 3, y + 3); px(x + 4, y + 3); px(x + 3, y + 4); px(x + 4, y + 4);
      break;
    default:
      if (id >= I_WPICK && id <= I_DSWORD) {
        t = TOOL_KIND(id);
        if (t == 0) {                       /* pickaxe */
          px(x + 1, y + 3); px(x + 2, y + 2); px(x + 3, y + 1);
          px(x + 4, y + 1); px(x + 5, y + 1); px(x + 6, y + 2);
          px(x + 6, y + 3);
          px(x + 3, y + 3); px(x + 3, y + 4); px(x + 2, y + 5);
          px(x + 1, y + 6);
        } else if (t == 1) {                /* axe */
          px(x + 2, y + 1); px(x + 3, y + 1); px(x + 4, y + 1);
          px(x + 2, y + 2); px(x + 3, y + 2); px(x + 4, y + 2);
          px(x + 2, y + 3); px(x + 3, y + 3);
          px(x + 4, y + 4); px(x + 3, y + 5); px(x + 2, y + 6);
        } else {                            /* sword */
          px(x + 6, y + 1); px(x + 5, y + 2); px(x + 4, y + 3);
          px(x + 3, y + 4);
          px(x + 2, y + 3); px(x + 4, y + 5);
          px(x + 1, y + 5); px(x + 2, y + 6); px(x + 1, y + 6);
        }
        /* tier notches along the bottom */
        {
          u8 tier = TOOL_TIER(id), i2;
          for (i2 = 0; i2 < tier; i2++) px(x + 4 + i2, y + 7);
        }
      }
      break;
  }
}
#endif

/* ------------------------------------------------------------------ */
/* World rendering                                                     */
/* ------------------------------------------------------------------ */
static const u8 solid_r[B_NB] = {
  0, 1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1, 0,0,1,0, 1,1,1,1, 1,1,1,0
};

#ifdef PY_RISCV_GFX
static void draw_speckle(u8 id, int x, int y) { screen_speckle(id, x, y, g.time, g.smelt_t); }
#else
static void draw_speckle(u8 id, int x, int y)
{
  switch (id) {
    case B_GRASS:
      px(x + 1, y + 1); px(x + 4, y + 1);
      px(x + 2, y + 4); px(x + 5, y + 5);
      break;
    case B_DIRT:
      px(x + 2, y + 2); px(x + 5, y + 3); px(x + 1, y + 5); px(x + 6, y + 6);
      break;
    case B_STONE:
      px(x + 2, y + 2); px(x + 5, y + 4); px(x + 3, y + 6);
      break;
    case B_COBBLE:
      px(x + 1, y + 1); px(x + 4, y + 2); px(x + 6, y + 1);
      px(x + 2, y + 4); px(x + 5, y + 5);
      break;
    case B_BEDROCK:
      px(x, y); px(x + 3, y + 1); px(x + 6, y); px(x + 1, y + 3);
      px(x + 5, y + 3); px(x + 2, y + 5); px(x + 7, y + 4); px(x + 4, y + 6);
      break;
    case B_LOG:
      pv(x + 2, y + 1, y + 6);
      pv(x + 5, y + 1, y + 6);
      break;
    case B_LEAVES:
      px(x + 1, y + 1); px(x + 4, y + 2); px(x + 6, y + 1);
      px(x + 2, y + 4); px(x + 5, y + 5); px(x + 3, y + 6);
      break;
    case B_PLANKS:
      px(x, y + 2); px(x + 2, y + 2); px(x + 4, y + 2); px(x + 6, y + 2);
      px(x, y + 5); px(x + 2, y + 5); px(x + 4, y + 5); px(x + 6, y + 5);
      break;
    case B_CRAFT:
      rect(x + 2, y + 2, 4, 4);
      break;
    case B_FURNACE:
      rect(x + 1, y + 1, 6, 6);
      ph(x + 2, x + 5, y + 4);
      if (g.smelt_t) px(x + 3, y + 5 + ((g.time >> 2) & 1));
      break;
    case B_SAND:
      px(x + 2, y + 2); px(x + 6, y + 3); px(x + 3, y + 5); px(x + 5, y + 6);
      break;
    case B_GRAVEL:
      px(x + 1, y + 2); px(x + 4, y + 1); px(x + 6, y + 4);
      px(x + 2, y + 5); px(x + 5, y + 6);
      break;
    case B_COAL:
      px(x + 3, y + 3); px(x + 4, y + 3); px(x + 3, y + 4);
      px(x + 4, y + 4); px(x + 5, y + 2);
      break;
    case B_IRON:
      px(x + 2, y + 2); px(x + 5, y + 3); px(x + 3, y + 5); px(x + 5, y + 5);
      break;
    case B_DIAMOND:
      px(x + 4, y + 2); px(x + 3, y + 3); px(x + 5, y + 3); px(x + 4, y + 4);
      break;
    case B_OBSIDIAN:
      px(x + 1, y + 1); px(x + 2, y + 2); px(x + 3, y + 3);
      px(x + 4, y + 4); px(x + 5, y + 5); px(x + 6, y + 6);
      px(x + 6, y + 1); px(x + 5, y + 2); px(x + 2, y + 5); px(x + 1, y + 6);
      break;
    case B_NETHERRACK:
      px(x + 1, y + 2); px(x + 4, y + 1); px(x + 6, y + 4);
      px(x + 2, y + 5); px(x + 5, y + 6);
      break;
    case B_NBRICK: case B_SBRICK:
      px(x, y + 3); px(x + 2, y + 3); px(x + 4, y + 3); px(x + 6, y + 3);
      pv(x + 4, y, y + 2);
      pv(x + 2, y + 4, y + 7);
      break;
    case B_SPAWNER:
      rect(x + 1, y + 1, 6, 6);
      px(x + 2, y + 2); px(x + 5, y + 2); px(x + 3, y + 4);
      px(x + 4, y + 4); px(x + 2, y + 5); px(x + 5, y + 5);
      break;
    case B_ENDSTONE:
      px(x + 2, y + 1); px(x + 5, y + 2); px(x + 1, y + 4);
      px(x + 4, y + 4); px(x + 6, y + 6); px(x + 3, y + 6);
      break;
    case B_FRAME:
      rect(x + 1, y + 2, 6, 5);
      px(x + 3, y + 4); px(x + 4, y + 4);
      break;
    case B_FRAME_EYE:
      rect(x + 1, y + 2, 6, 5);
      px(x + 3, y + 3); px(x + 4, y + 3);
      px(x + 3, y + 4); px(x + 4, y + 4);
      px(x + 2, y + 4); px(x + 5, y + 4);
      break;
    default: break;
  }
}
#endif

#ifdef PY_RISCV_GFX
static void draw_fluid(u8 id, int x, int y, u8 surf) { screen_fluid(id, x, y, surf, g.time); }
#else
static void draw_fluid(u8 id, int x, int y, u8 surf)
{
  u8 a = (u8)((g.time >> 2) & 3);
  if (id == B_WATER) {
    if (surf) {
      px(x + ((a + 0) & 7), y + 1);
      px(x + ((a + 1) & 7), y + 1);
      px(x + ((a + 4) & 7), y + 1);
      px(x + ((a + 5) & 7), y + 1);
    }
    px(x + 1, y + 4); px(x + 4, y + 5); px(x + 6, y + 3);
  } else if (id == B_LAVA) {
    if (surf) {
      ph(x + ((a + 0) & 7) - 1, x + ((a + 0) & 7), y + 1);
      ph(x + ((a + 4) & 7) - 1, x + ((a + 4) & 7), y + 1);
    }
    px(x + 1, y + 3); px(x + 5, y + 2); px(x + 3, y + 5); px(x + 6, y + 6);
  } else if (id == B_PORTAL) {
    px(x + 2, y + ((a + 0) & 7));
    px(x + 5, y + ((a + 4) & 7));
    px(x + 3, y + ((a + 6) & 7));
  } else if (id == B_ENDPORTAL) {
    if ((g.time & 7) < 5) px(x + 2, y + 2);
    if ((g.time & 7) < 3) px(x + 5, y + 5);
    px(x + 4, y + 3);
  }
}
#endif

static void draw_world(void)
{
#ifdef PY_RISCV_GFX
  screen_world(wmap, cam_x, cam_y, g.time, g.smelt_t);
#else
  int sx, sy;
  for (sy = 0; sy < VIEW_TY; sy++) {
    int wy = cam_y + sy;
    const unsigned char *row, *urow, *drow;
    int y0 = sy << 3;
    if (wy >= WORLD_H) continue;   /* below the world: blank */
    row = wmap + (wy << WORLD_SHIFT);
    urow = (sy > 0 && wy > 0) ? row - WORLD_W : 0;
    drow = (wy < WORLD_H - 1) ? row + WORLD_W : 0;
    for (sx = 0; sx < VIEW_TX; sx++) {
      int wx = cam_x + sx;
      u8 id = row[wx];
      u8 up, dn, lf, rt;
      u8 edge;
      int x0;
      if (id == B_AIR) continue;
      x0 = sx << 3;
      up = (u8)(urow ? urow[wx] : B_AIR);
      if (id == B_WATER || id == B_LAVA || id == B_PORTAL ||
          id == B_ENDPORTAL) {
        draw_fluid(id, x0, y0, (u8)(up != id));
        continue;
      }
      dn = (u8)(drow ? drow[wx] : B_AIR);
      lf = (u8)(wx > 0 ? row[wx - 1] : B_AIR);
      rt = (u8)(wx < WORLD_W - 1 ? row[wx + 1] : B_AIR);
      edge = 0;
      if (!solid_r[up]) { ph(x0, x0 + 7, y0); edge = 1; }
      if (!solid_r[dn]) { ph(x0, x0 + 7, y0 + 7); edge = 1; }
      if (!solid_r[lf]) { pv(x0, y0 + 1, y0 + 6); edge = 1; }
      if (!solid_r[rt]) { pv(x0 + 7, y0 + 1, y0 + 6); edge = 1; }
      /* ores (and gravel) show through solid rock so they can be found */
      if (edge || id == B_COAL || id == B_IRON || id == B_DIAMOND ||
          id == B_GRAVEL)
        draw_speckle(id, x0, y0);
    }
  }
#endif
  /* mining cracks */
  if (g.mtarget_valid && g.mprogress) {
    int cx = (g.mtx - cam_x) << 3, cy = (g.mty - cam_y) << 3;
    if (cx >= 0 && cx < SCR_W && cy >= 0 && cy < SCR_H) {
      px(cx + 3, cy + 3);
      if (g.mprogress > (g.mhard >> 1)) {
        px(cx + 4, cy + 4); px(cx + 2, cy + 4);
      }
    }
  }
}

/* dashed highlight box around the aim cursor tile */
static void draw_aim(void)
{
  int x = (g.aimx - cam_x) << 3, y = (g.aimy - cam_y) << 3;
  if (x < -8 || x >= SCR_W || y < -8 || y >= SCR_H) return;
  px(x, y); px(x + 1, y); px(x + 6, y); px(x + 7, y);
  px(x, y + 7); px(x + 1, y + 7); px(x + 6, y + 7); px(x + 7, y + 7);
  px(x, y + 3); px(x + 7, y + 3);
  px(x, y + 4); px(x + 7, y + 4);
  if ((g.time >> 2) & 1) { px(x + 3, y); px(x + 4, y + 7); }
}

/* ------------------------------------------------------------------ */
/* Entities                                                            */
/* ------------------------------------------------------------------ */
static void draw_player(void)
{
  int x = (g.px >> 1) - (cam_x << 3);
  int y = (g.py >> 1) - (cam_y << 3);
  u8 walk = (u8)(g.pvx != 0 ? ((g.time >> 1) & 1) : 0);

  if (g.invuln && (g.time & 1)) return;   /* damage blink */

  /* hair + head */
  ph(x + 1, x + 6, y);
  pv(x + 1, y, y + 5); pv(x + 6, y, y + 5);
  ph(x + 1, x + 6, y + 5);
  if (g.face) { px(x + 4, y + 2); px(x + 5, y + 2); }
  else        { px(x + 2, y + 2); px(x + 3, y + 2); }
  /* body */
  pv(x + 2, y + 6, y + 12); pv(x + 5, y + 6, y + 12);
  ph(x + 2, x + 5, y + 12);
  /* arm */
  if (g.face) pv(x + 6, y + 6, y + 10 - (walk << 1));
  else        pv(x + 1, y + 6, y + 10 - (walk << 1));
  /* legs */
  if (walk) {
    pv(x + 1, y + 13, y + 15);
    pv(x + 6, y + 13, y + 15);
  } else {
    pv(x + 2, y + 13, y + 15);
    pv(x + 5, y + 13, y + 15);
  }
}

static void draw_mob(const Mob *m)
{
  int x = (m->x >> 1) - (cam_x << 3);
  int y = (m->y >> 1) - (cam_y << 3);
  u8 f = (u8)((g.time >> 2) & 1);
  if (x < -34 || x > SCR_W || y < -24 || y > SCR_H) return;
  /* damage flash: blink once per rendered frame (2-tick parity so it
   * shows at the default 2 fps as well as high-fps mode) */
  if (m->ft && ((g.time >> 1) & 1)) return;

  switch (m->type) {
    case M_ZOMBIE:
      ph(x + 1, x + 6, y);
      pv(x + 1, y, y + 5); pv(x + 6, y, y + 5);
      ph(x + 1, x + 6, y + 5);
      px(x + 2, y + 2); px(x + 5, y + 2);
      pv(x + 2, y + 6, y + 12); pv(x + 5, y + 6, y + 12);
      ph(x + 2, x + 5, y + 12);
      /* arms stretched forward */
      if (m->vx >= 0) { ph(x + 6, x + 9, y + 7); ph(x + 6, x + 8, y + 8); }
      else            { ph(x - 2, x + 1, y + 7); ph(x - 1, x + 1, y + 8); }
      if (f) { pv(x + 1, y + 13, y + 15); pv(x + 6, y + 13, y + 15); }
      else   { pv(x + 2, y + 13, y + 15); pv(x + 5, y + 13, y + 15); }
      break;
    case M_ENDERMAN:
      ph(x + 1, x + 6, y);
      pv(x + 1, y, y + 4); pv(x + 6, y, y + 4);
      ph(x + 1, x + 6, y + 2);           /* glowing eyes band */
      ph(x + 1, x + 6, y + 4);
      pv(x + 3, y + 5, y + 13); pv(x + 4, y + 5, y + 13);
      pv(x + 2, y + 14, y + 22); pv(x + 5, y + 14, y + 22);
      break;
    case M_BLAZE:
      rect(x + 2, y + 2, 4, 4);
      px(x + 3, y + 3); px(x + 4, y + 3);
      /* four rods orbiting */
      {
        u8 fr = (u8)((g.time >> 2) & 3);
        static const i8 ox[4] = { 0, 5, 0, -3 };
        static const i8 oy[4] = { -4, 2, 6, 2 };
        u8 k;
        for (k = 0; k < 4; k++) {
          u8 p = (u8)((k + fr) & 3);
          pv(x + 3 + ox[p], y + 4 + oy[p], y + 6 + oy[p]);
        }
      }
      break;
    case M_DRAGON:
      {
        u8 dir = (u8)(m->vx >= 0);
        int hx = dir ? x + 22 : x - 2;
        /* body */
        rect(x + 6, y + 8, 16, 6);
        ph(x + 8, x + 19, y + 10);
        /* head + jaw */
        rect(hx, y + 2, 10, 6);
        ph(hx + 2, hx + 7, y + 8);
        px(dir ? hx + 7 : hx + 2, y + 4);
        /* wings */
        if (f) {
          pv(x + 9, y + 2, y + 8); pv(x + 17, y + 2, y + 8);
          ph(x + 9, x + 17, y + 2);
        } else {
          pv(x + 9, y + 8, y + 15); pv(x + 17, y + 8, y + 15);
          ph(x + 9, x + 17, y + 15);
        }
        /* tail */
        ph(x, y + 10, x + 6);
        px(x - 1, y + 9);
        /* legs while diving */
        if (m->t1) {
          pv(x + 10, y + 14, y + 17);
          pv(x + 16, y + 14, y + 17);
        }
      }
      break;
  }
}

static void draw_drops(void)
{
  u8 i;
  for (i = 0; i < MAXDROPS; i++) {
    if (drops[i].n) {
      int x = (drops[i].x >> 1) - (cam_x << 3);
      int y = (drops[i].y >> 1) - (cam_y << 3) - (int)((g.time >> 3) & 1);
      if (x < -8 || x > SCR_W || y < -8 || y > SCR_H) continue;
      icon(drops[i].id, x, y);
    }
  }
}

/* ------------------------------------------------------------------ */
/* HUD                                                                 */
/* ------------------------------------------------------------------ */
#ifdef PY_RISCV_GFX
static void draw_heart(int x, int y, u8 mode) { screen_heart(x, y, mode); }
#else
static void draw_heart(int x, int y, u8 mode)
{
  if (mode == 0) return;
  if (mode == 2) {
    px(x + 1, y); px(x + 3, y);
    ph(x, x + 4, y + 1);
    ph(x, x + 4, y + 2);
    ph(x + 1, x + 3, y + 3);
    px(x + 2, y + 4);
  } else {
    px(x + 1, y);
    ph(x, x + 2, y + 1);
    ph(x, x + 2, y + 2);
    px(x + 1, y + 3);
    px(x + 2, y + 4);
  }
}
#endif

static void draw_hud(void)
{
  u8 i;
  u16 day = (u16)((g.time & 1023) < 512);
  int selx;

  /* sun / moon */
  if (day) {
    rect(176, 6, 7, 7);
    px(179, 9); px(180, 8);
  } else {
    px(177, 6); px(176, 7); px(176, 8); px(176, 9); px(177, 10);
    px(178, 11); px(180, 10); px(181, 8); px(180, 6);
  }

  /* day counter + measured fps indicator */
  draw_str(160, 2, "D");
  draw_num(165, 2, g.days + 1);
  draw_num(164, 16, g.fps_now);
  draw_str(173, 16, "FPS");
  if (g.flying) draw_str(164, 26, "FLY");
  else if (g.creative) draw_str(164, 26, "CR");

  /* held item name */
  {
    const char *n = item_name(g.inv[g.sel].id);
    if (n[0]) {
      int l = str_len(n);
      draw_str((SCR_W - l * 4) >> 1, 139, n);
    }
  }

  /* hearts */
  for (i = 0; i < 10; i++) {
    int hpx = 51 + i * 9;
    if (g.hp >= (u8)(i * 2 + 2)) draw_heart(hpx, 146, 2);
    else if (g.hp == (u8)(i * 2 + 1)) draw_heart(hpx, 146, 1);
  }

  /* hotbar */
  ph(11, 180, 153);
  for (i = 0; i < MAXSLOTS; i++) {
    int x0 = 12 + i * 14;
    rect(x0, 154, 13, 12);
    if (g.inv[i].id) {
      if (g.inv[i].id < B_NB) {
        /* blocks: count only, no sprite */
        int w = g.inv[i].n >= 10 ? 8 : 4;
        draw_num(x0 + (13 - w) / 2, 158, g.inv[i].n);
      } else {
        /* clip the sprite's bottom two pixel rows */
        icon_clip = 161;
        icon(g.inv[i].id, x0 + 2, 155);
        icon_clip = SCR_H;
        if (g.inv[i].n > 1) draw_num(x0 + 7, 160, g.inv[i].n);
      }
    }
  }
  selx = 12 + g.sel * 14;
  rect(selx - 1, 153, 15, 14);
  px(selx + 6, 151); px(selx + 7, 150); px(selx + 8, 151);

  /* eye of ender direction arrow */
  if (g.dim == DIM_OVER && inv_count(I_EYE)) {
    int tx = (g.px + 6) >> 4;
    if (sh_x > tx + 2) draw_str(2, 2, "E>");
    else if (sh_x < tx - 2) draw_str(2, 2, "<E");
    else draw_str(2, 2, "EV");
  }

  /* dragon boss bar: 1 px of fill per hp point */
  if (g.dim == DIM_END) {
    for (i = 0; i < MAXMOBS; i++) {
      if (!mobs[i].dead && mobs[i].type == M_DRAGON) {
        rect(64, 8, 62, 7);
        if (mobs[i].hp) {
          ph(65, 65 + mobs[i].hp - 1, 10);
          ph(65, 65 + mobs[i].hp - 1, 11);
          ph(65, 65 + mobs[i].hp - 1, 12);
        }
        break;
      }
    }
  }

  /* message */
  if (g.msgt && g.msg[0]) {
    int len = str_len(g.msg);
    draw_str((SCR_W - len * 4) >> 1, 2, g.msg);
  }
}

/* ------------------------------------------------------------------ */
/* Menus                                                               */
/* ------------------------------------------------------------------ */
static void draw_craft_menu(void)
{
  u8 i;
  draw_str(4, 2, "CRAFT");
  if (!near_table()) draw_str(120, 2, "NO TABLE");
  for (i = 0; i < 8 && (u8)(g.menutop + i) < recipe_count; i++) {
    u8 r = (u8)(g.menutop + i);
    int y = 14 + i * 13;
    if (r == g.menusel) rect(2, y - 1, 188, 12);
    icon(recipes[r].out, 6, y);
    draw_char(16, y + 1, '>');
    icon(recipes[r].out, 22, y);
    draw_char(32, y + 1, 'X');
    draw_num(37, y + 1, recipes[r].n);
    /* requirements */
    {
      const u8 *p = recipes[r].need;
      int rx = 50;
      while (p[0]) {
        u8 have = inv_count(p[0]);
        icon(p[0], rx, y);
        draw_num(rx + 9, y + 1, p[1]);
        if (have < p[1]) {
          /* strike through the count */
          ph(rx + 8, rx + 14, y + 3);
        }
        rx += 18;
        p += 2;
      }
      if (recipes[r].table) draw_str(rx, y + 1, "T");
    }
  }
  {
    const char *n = item_name(recipes[g.menusel].out);
    draw_str(4, 122, n);
  }
  draw_str(4, 130, "Z CRAFT X CLOSE UP DN SELECT");
  if (recipes[g.menusel].table) draw_str(4, 138, "NEEDS CRAFT TABLE NEARBY");
}

static void draw_smelt_menu(void)
{
  draw_str(4, 2, "FURNACE");
  icon(B_IRON, 20, 20);
  draw_char(30, 22, '+');
  icon(I_COAL, 38, 20);
  draw_char(48, 22, '>');
  icon(I_IRONING, 56, 20);
  draw_str(20, 34, "ORE:");
  draw_num(40, 34, inv_count(B_IRON));
  draw_str(20, 42, "COAL:");
  draw_num(44, 42, inv_count(I_COAL));
  draw_str(20, 50, "INGOTS:");
  draw_num(52, 50, inv_count(I_IRONING));
  /* progress bar */
  rect(20, 62, 60, 6);
  if (g.smelt_t) ph(21, 21 + g.smelt_t * 4, 65);
  draw_str(4, 80, "HOLD Z TO SMELT");
  draw_str(4, 90, "X CLOSE");
}

static void draw_win(void)
{
  draw_str(56, 50, "YOU BEAT");
  draw_str(48, 60, "MINECRAFT!");
  /* dragon egg trophy */
  px(93, 74); px(94, 74); px(95, 74); px(96, 74);
  px(92, 75); px(97, 75);
  px(92, 76); px(97, 76);
  px(93, 77); px(96, 77);
  px(94, 78); px(95, 78);
  draw_str(40, 92, "DAYS SURVIVED:");
  draw_num(100, 92, g.days + 1);
  if ((g.time >> 3) & 1) draw_str(60, 110, "PRESS Q");
}

/* ------------------------------------------------------------------ */
/* Frame                                                               */
/* ------------------------------------------------------------------ */
void render_frame(void)
{
  int ptx = (g.px + 6) >> 4, pty = (g.py + 14) >> 4;
  cam_x = (i16)(ptx - VIEW_TX / 2);
  cam_y = (i16)(pty - VIEW_TY / 2);
  if (cam_x < 0) cam_x = 0;
  if (cam_y < 0) cam_y = 0;
  if (cam_x > WORLD_W - VIEW_TX) cam_x = WORLD_W - VIEW_TX;
  /* sink below the world bottom so the player stays above the HUD
   * when standing on bedrock */
  if (cam_y > WORLD_H - VIEW_TY + 4) cam_y = WORLD_H - VIEW_TY + 4;

  screen_clear();

  if (g.winscreen) { draw_win(); return; }

  if (g.menu == 1) { draw_craft_menu(); return; }
  if (g.menu == 2) { draw_smelt_menu(); return; }

  draw_world();
  draw_drops();
  /* fireballs */
  {
    u8 i;
    for (i = 0; i < MAXSHOTS; i++) {
      int x, y;
      if (!shots[i].ttl) continue;
      x = (shots[i].x >> 1) - (cam_x << 3);
      y = (shots[i].y >> 1) - (cam_y << 3);
      px(x, y); px(x + 1, y); px(x, y + 1); px(x + 1, y + 1);
      px(x - (shots[i].vx > 0 ? 2 : -2), y);
    }
  }
  /* mobs */
  {
    u8 i;
    for (i = 0; i < MAXMOBS; i++)
      if (!mobs[i].dead) draw_mob(&mobs[i]);
  }
  draw_player();
  if (g.aim) draw_aim();
  draw_hud();

  if (g.dead) {
    draw_str(70, 70, "YOU DIED");
  }
}
