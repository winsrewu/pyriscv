/* display_test.c -- headless test driver.
 *
 * Feeds a key script (MC_SCRIPT file, one key char per tick, same
 * mapping as display_ref.c) and prints a state report when the
 * script is exhausted.  Used by the verification suite; the real
 * target never links this file.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mc.h"
#include "display.h"

static unsigned char fb[SCR_W * SCR_H];
static unsigned char script[65536];
static long slen, spos;
static int idle, idle_max = 8;
static int key_down[K_COUNT], key_edge[K_COUNT];
static const char *pbm_prefix;
static int pbm_count;
static int want_map;
static int trace;

static long px_this_frame, px_max;

void screen_clear(void)
{
  memset(fb, 0, sizeof(fb));
  if (px_this_frame > px_max) px_max = px_this_frame;
  px_this_frame = 0;
}

void screen_set(int x, int y)
{
  if ((unsigned)x < SCR_W && (unsigned)y < SCR_H) {
    fb[y * SCR_W + x] = 1;
    px_this_frame++;
  }
}

static void press_l(int k) { key_edge[k] = 1; key_down[k] = 1; }

static void map(unsigned char c)
{
  switch (c) {
    case 'a': press_l(K_LEFT); break;
    case 'd': press_l(K_RIGHT); break;
    case 'w': case ' ': press_l(K_UP); break;
    case 'W': press_l(K_UP); press_l(K_RIGHT); break;  /* jump+right */
    case 'A': press_l(K_UP); press_l(K_LEFT); break;   /* jump+left  */
    case 's': case 'S': press_l(K_DOWN); break;
    case 'z': case 'Z': case 'j': press_l(K_ACT); break;
    case 'x': case 'X': case 'k': press_l(K_USE); break;
    case 'c': case 'C': case 'e': press_l(K_MENU); break;
    case 'q': case 'Q': press_l(K_DROP); break;
    case 'b': case 'B': press_l(K_CREATIVE); break;
    case ']': press_l(K_NEXT); break;
    case '[': press_l(K_PREV); break;
    case 'f': case 'F': case 9: press_l(K_AIM); break;
    case 'r': case 'R': press_l(K_FPS); break;
    case '1': press_l(K_D1); break;
    case '2': press_l(K_D2); break;
    case '3': press_l(K_D3); break;
    case '4': press_l(K_D4); break;
    case '5': press_l(K_D5); break;
    case '6': press_l(K_D6); break;
    case '7': press_l(K_D7); break;
    case '8': press_l(K_D8); break;
    case '9': press_l(K_D9); break;
    case '0': press_l(K_D0); break;
    case 'g': press_l(K_DBG_G); break;
    case 't': press_l(K_DBG_T); break;
    case 'h': press_l(K_DBG_H); break;
    case 'p': press_l(K_DBG_P); break;
    case 'v': press_l(K_DBG_L); break;
    case 'm': press_l(K_DBG_M); break;
    default: break;
  }
}

void display_init(void)
{
  const char *e = getenv("MC_SCRIPT");
  FILE *f;
  if (!e) { fprintf(stderr, "MC_SCRIPT required\n"); exit(1); }
  f = fopen(e, "rb");
  if (!f) { fprintf(stderr, "cannot open %s\n", e); exit(1); }
  slen = (long)fread(script, 1, sizeof(script), f);
  fclose(f);
  if ((e = getenv("MC_PBM"))) pbm_prefix = e;
  if ((e = getenv("MC_IDLE"))) idle_max = atoi(e);
  if ((e = getenv("MC_TRACE")) && *e == '1') trace = 1;
  if ((e = getenv("MC_MAP")) && *e == '1') want_map = 1;
}

void input_poll(void)
{
  int k;
  for (k = 0; k < K_COUNT; k++) { key_edge[k] = 0; key_down[k] = 0; }
  if (spos < slen) map(script[spos++]);
  else idle++;
}

int key(int k)      { return k >= 0 && k < K_COUNT ? key_down[k] : 0; }
int kpressed(int k) { return k >= 0 && k < K_COUNT ? key_edge[k] : 0; }

static void dump_pbm(void)
{
  char name[256];
  FILE *f;
  int x, y;
  sprintf(name, "%s_%04d.pbm", pbm_prefix, pbm_count++);
  f = fopen(name, "wb");
  if (!f) return;
  fprintf(f, "P4\n%d %d\n", SCR_W, SCR_H);
  for (y = 0; y < SCR_H; y++) {
    unsigned char byte = 0, bit = 0;
    for (x = 0; x < SCR_W; x++) {
      byte |= (unsigned char)(fb[y * SCR_W + x] << (7 - bit));
      if (++bit == 8) { fputc(byte, f); byte = 0; bit = 0; }
    }
    if (bit) fputc(byte, f);
  }
  fclose(f);
}

static const char *iname(u8 id)
{
  switch (id) {
    case 0: return "-";
    case B_LOG: return "log";       case B_PLANKS: return "planks";
    case B_COBBLE: return "cobble"; case B_CRAFT: return "table";
    case B_FURNACE: return "furnace"; case B_OBSIDIAN: return "obsidian";
    case B_DIRT: return "dirt";     case B_IRON: return "ironore";
    case I_STICK: return "stick";   case I_COAL: return "coal";
    case I_FLINT: return "flint";   case I_IRONING: return "ingot";
    case I_DIAMOND: return "diamond"; case I_BUCKET: return "bucket";
    case I_BUCKET_W: return "wbucket"; case I_BUCKET_L: return "lbucket";
    case I_FSTEEL: return "fsteel"; case I_ROD: return "rod";
    case I_POWDER: return "powder"; case I_PEARL: return "pearl";
    case I_EYE: return "eye";
    case I_WPICK: return "wpick";   case I_WAXE: return "waxe";
    case I_WSWORD: return "wsword"; case I_SPICK: return "spick";
    case I_SAXE: return "saxe";     case I_SSWORD: return "ssword";
    case I_IPICK: return "ipick";   case I_IAXE: return "iaxe";
    case I_ISWORD: return "isword"; case I_DPICK: return "dpick";
    case I_DAXE: return "daxe";     case I_DSWORD: return "dsword";
    default: return "?";
  }
}

static void dump_map(void)
{
  int x0 = (g.px >> 4) - 14, y0 = (g.py >> 4) - 12;
  int x, y;
  for (y = y0; y < y0 + 24; y++) {
    for (x = x0; x < x0 + 28; x++) {
      u8 t = get_t(x, y);
      char c = '.';
      if (t == B_AIR) c = ' ';
      else if (t == B_GRASS) c = '"';
      else if (t == B_DIRT) c = ',';
      else if (t == B_STONE) c = '#';
      else if (t == B_LOG) c = '|';
      else if (t == B_LEAVES) c = '*';
      else if (t == B_WATER) c = '~';
      else if (t == B_LAVA) c = '%';
      else if (t == B_COAL) c = 'c';
      else if (t == B_IRON) c = 'i';
      else if (t == B_DIAMOND) c = 'd';
      else if (t == B_COBBLE) c = 'o';
      else if (t == B_OBSIDIAN) c = 'O';
      else if (t == B_PORTAL) c = 'P';
      else if (t == B_FRAME) c = 'F';
      else if (t == B_FRAME_EYE) c = 'G';
      else if (t == B_ENDPORTAL) c = 'E';
      else if (t == B_NETHERRACK) c = 'n';
      else if (t == B_NBRICK) c = 'b';
      else if (t == B_SBRICK) c = 'k';
      else if (t == B_SPAWNER) c = 'X';
      else if (t == B_ENDSTONE) c = 'e';
      else c = '?';
      putchar(c);
    }
    putchar('\n');
  }
}

long display_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

void display_sleep_ms(int n) { (void)n; }

void display_present(void) { }

void display_sync(void)
{
  static int t;
  t++;
  if (trace && (t & 3) == 0)
    printf("t=%d pos=%d,%d face=%d\n", t, g.px >> 4, g.py >> 4, g.face);
  if (pbm_prefix && (t & 1) == 0) dump_pbm();
  if (idle >= idle_max) {
    int i;
    printf("REPORT dim=%d won=%d hp=%d days=%d time=%u creative=%d flying=%d\n",
           g.dim, g.won, g.hp, g.days, g.time, g.creative, g.flying);
    printf("pos=%d,%d\n", g.px >> 4, g.py >> 4);
    printf("max_pixels_per_frame=%ld\n", px_max);
    for (i = 0; i < MAXSLOTS; i++)
      if (g.inv[i].id)
        printf("inv %d %s x%d\n", i, iname(g.inv[i].id), g.inv[i].n);
    for (i = 0; i < MAXMOBS; i++)
      if (!mobs[i].dead)
        printf("mob %d hp=%d at %d,%d\n", mobs[i].type, mobs[i].hp,
               mobs[i].x >> 4, mobs[i].y >> 4);
    if (want_map) dump_map();
    exit(0);
  }
}
