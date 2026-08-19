/* display_x11.c -- window driver: real pixels instead of ASCII.
 *
 * Opens an X11 window and shows the 192x168 one-bit framebuffer as a
 * phosphor-style panel: light pixels on black, integer nearest-
 * neighbour scaling (MC_SCALE, default 4), optional pixel grid
 * (MC_GRID=1).  Same 7-function contract as display_ref.c -- the game
 * core is untouched.
 *
 * Env: MC_SCALE=n  MC_GRID=1  MC_FAST=1  MC_PBM=prefix  MC_FRAMES=n
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "mc.h"
#include "display.h"

static unsigned char fb[SCR_W * SCR_H];
static int key_down[K_COUNT], key_edge[K_COUNT];

static Display *dpy;
static Window win;
static XImage *ximg;
static Atom wm_delete;
static int scale = 4, grid = 0;
static int fast_mode, winshot;
static const char *pbm_prefix;
static int pbm_count, frame_max;
static struct timespec deadline;
static unsigned long px_on, px_off;

/* ------------------------------------------------------------------ */
/* Screen functions: the only video calls the game makes               */
/* ------------------------------------------------------------------ */
void screen_clear(void) { memset(fb, 0, sizeof(fb)); }

void screen_set(int x, int y)
{
  if ((unsigned)x < SCR_W && (unsigned)y < SCR_H) fb[y * SCR_W + x] = 1;
}

/* ------------------------------------------------------------------ */
/* Blit the logical 1-bit buffer into the scaled window image          */
/* ------------------------------------------------------------------ */
static void blit(void)
{
  int x, y, sx, sy;
  /* XImage is 4 bytes per pixel on 24/32-bit visuals */
  unsigned int *p = (unsigned int *)ximg->data;
  int stride = ximg->bytes_per_line / 4;
  for (y = 0; y < SCR_H; y++)
    for (x = 0; x < SCR_W; x++) {
      unsigned int c = fb[y * SCR_W + x] ? (unsigned int)px_on
                                         : (unsigned int)px_off;
      for (sy = 0; sy < scale; sy++) {
        unsigned int *row = p + (y * scale + sy) * stride + x * scale;
        for (sx = 0; sx < scale; sx++) row[sx] = c;
      }
    }
  if (grid) {
    /* 1px dark seams between logical pixels */
    for (x = 0; x < SCR_W; x++)
      for (sy = 0; sy < SCR_H * scale; sy++)
        p[sy * stride + x * scale] = 0x202020u;
    for (y = 0; y < SCR_H; y++)
      for (sx = 0; sx < SCR_W * scale; sx++)
        p[(y * scale) * stride + sx] = 0x202020u;
  }
}

/* capture what the server actually shows in the window */
static void dump_winshot(void)
{
  char name[256];
  FILE *f;
  XImage *si;
  int x, y;
  si = XGetImage(dpy, win, 0, 0, (unsigned)(SCR_W * scale),
                 (unsigned)(SCR_H * scale), AllPlanes, ZPixmap);
  if (!si) return;
  sprintf(name, "%s_%04d.pbm", pbm_prefix, pbm_count);
  f = fopen(name, "wb");
  if (f) {
    fprintf(f, "P4\n%d %d\n", SCR_W, SCR_H);
    for (y = 0; y < SCR_H; y++) {
      unsigned char byte = 0, bit = 0;
      for (x = 0; x < SCR_W; x++) {
        unsigned long c = XGetPixel(si, x * scale + scale / 2,
                                    y * scale + scale / 2);
        if (c != px_off) byte |= (unsigned char)(1 << (7 - bit));
        if (++bit == 8) { fputc(byte, f); byte = 0; bit = 0; }
      }
      if (bit) fputc(byte, f);
    }
    fclose(f);
  }
  XDestroyImage(si);
  pbm_count++;
  if (frame_max && pbm_count >= frame_max) exit(0);
}

static void dump_pbm(void)
{
  char name[256];
  FILE *f;
  int x, y;
  sprintf(name, "%s_%04d.pbm", pbm_prefix, pbm_count);
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
  pbm_count++;
  if (frame_max && pbm_count >= frame_max) exit(0);
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */
static void setkey(int k, int down)
{
  if (k < 0 || k >= K_COUNT) return;
  if (down && !key_down[k]) key_edge[k] = 1;
  key_down[k] = down;
}

static int sym_key(KeySym s)
{
  switch (s) {
    case XK_Left:  case XK_a: case XK_A: return K_LEFT;
    case XK_Right: case XK_d: case XK_D: return K_RIGHT;
    case XK_Up:    case XK_w: case XK_W: case XK_space: return K_UP;
    case XK_Down:  case XK_s: case XK_S: return K_DOWN;
    case XK_z: case XK_Z: case XK_j: case XK_J: return K_ACT;
    case XK_x: case XK_X: case XK_k: case XK_K: return K_USE;
    case XK_c: case XK_C: case XK_e: case XK_E: return K_MENU;
    case XK_Escape: return K_BACK;
    case XK_q: case XK_Q: return K_DROP;
    case XK_b: case XK_B: return K_CREATIVE;
    case XK_bracketright: return K_NEXT;
    case XK_bracketleft:  return K_PREV;
    case XK_f: case XK_F: case XK_Tab: return K_AIM;
    case XK_r: case XK_R: return K_FPS;
    case XK_1: return K_D1;  case XK_2: return K_D2;  case XK_3: return K_D3;
    case XK_4: return K_D4;  case XK_5: return K_D5;  case XK_6: return K_D6;
    case XK_7: return K_D7;  case XK_8: return K_D8;  case XK_9: return K_D9;
    case XK_0: return K_D0;
    case XK_g: case XK_G: return K_DBG_G;
    case XK_t: case XK_T: return K_DBG_T;
    case XK_h: case XK_H: return K_DBG_H;
    case XK_p: case XK_P: return K_DBG_P;
    case XK_v: case XK_V: return K_DBG_L;
    case XK_m: case XK_M: return K_DBG_M;
    default: return -1;
  }
}

static void events(void)
{
  XEvent ev;
  while (XPending(dpy)) {
    XNextEvent(dpy, &ev);
    switch (ev.type) {
      case KeyPress:
        setkey(sym_key(XLookupKeysym(&ev.xkey, 0)), 1);
        break;
      case KeyRelease:
        setkey(sym_key(XLookupKeysym(&ev.xkey, 0)), 0);
        break;
      case ClientMessage:
        if ((Atom)ev.xclient.data.l[0] == wm_delete) exit(0);
        break;
      default: break;
    }
  }
}

void input_poll(void)
{
  int k;
  for (k = 0; k < K_COUNT; k++) key_edge[k] = 0;
  events();
}

int key(int k)      { return k >= 0 && k < K_COUNT ? key_down[k] : 0; }
int kpressed(int k) { return k >= 0 && k < K_COUNT ? key_edge[k] : 0; }

/* ------------------------------------------------------------------ */
/* Init + pacing                                                       */
/* ------------------------------------------------------------------ */
void display_init(void)
{
  const char *e;
  int w, h;
  if ((e = getenv("MC_SCALE")) && atoi(e) > 0) scale = atoi(e);
  if ((e = getenv("MC_GRID")) && *e == '1') grid = 1;
  if ((e = getenv("MC_FAST")) && *e == '1') fast_mode = 1;
  if ((e = getenv("MC_PBM"))) pbm_prefix = e;
  if ((e = getenv("MC_WINSHOT")) && *e == '1') winshot = 1;
  if ((e = getenv("MC_FRAMES"))) frame_max = atoi(e);

  dpy = XOpenDisplay(NULL);
  if (!dpy) {
    fprintf(stderr, "mcx: cannot open X display\n");
    exit(1);
  }
  w = SCR_W * scale; h = SCR_H * scale;
  win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
                            (unsigned)w, (unsigned)h, 0,
                            BlackPixel(dpy, DefaultScreen(dpy)),
                            BlackPixel(dpy, DefaultScreen(dpy)));
  XStoreName(dpy, win, "MINECRAFT 1960 - 192x168x1");
  XSelectInput(dpy, win, KeyPressMask | KeyReleaseMask | ExposureMask);
  wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(dpy, win, &wm_delete, 1);
  XMapWindow(dpy, win);

  ximg = XCreateImage(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
                      (unsigned)DefaultDepth(dpy, DefaultScreen(dpy)),
                      ZPixmap, 0, NULL, (unsigned)w, (unsigned)h, 32, 0);
  if (!ximg) { fprintf(stderr, "mcx: XCreateImage failed\n"); exit(1); }
  ximg->data = (char *)malloc((size_t)ximg->bytes_per_line * (size_t)h);
  if (!ximg->data) { fprintf(stderr, "mcx: no memory\n"); exit(1); }

  /* phosphor on black */
  px_on  = WhitePixel(dpy, DefaultScreen(dpy));
  px_off = BlackPixel(dpy, DefaultScreen(dpy));

  clock_gettime(CLOCK_MONOTONIC, &deadline);
}

long display_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

void display_sleep_ms(int n)
{
  struct timespec ts;
  /* the high-fps loop never calls display_sync; snapshot there too */
  static int sl;
  if (pbm_prefix && (sl++ & 15) == 0) {
    if (winshot) dump_winshot();
    else dump_pbm();
  }
  if (n <= 0) return;
  ts.tv_sec = n / 1000;
  ts.tv_nsec = (long)(n % 1000) * 1000000L;
  nanosleep(&ts, 0);
}

void display_present(void)
{
  blit();
  XPutImage(dpy, win, DefaultGC(dpy, DefaultScreen(dpy)), ximg, 0, 0, 0, 0,
            (unsigned)(SCR_W * scale), (unsigned)(SCR_H * scale));
  XFlush(dpy);
}

void display_sync(void)
{
  static int t;
  t++;
  events();
  display_present();
  if (pbm_prefix && (t & 1) == 0) {
    if (winshot) dump_winshot();
    else dump_pbm();
  }

  if (!fast_mode) {
    struct timespec now, rem;
    deadline.tv_nsec += 250000000L;
    if (deadline.tv_nsec >= 1000000000L) {
      deadline.tv_nsec -= 1000000000L;
      deadline.tv_sec++;
    }
    clock_gettime(CLOCK_MONOTONIC, &now);
    rem.tv_sec = deadline.tv_sec - now.tv_sec;
    rem.tv_nsec = deadline.tv_nsec - now.tv_nsec;
    if (rem.tv_nsec < 0) { rem.tv_nsec += 1000000000L; rem.tv_sec--; }
    if (rem.tv_sec > 0 || rem.tv_nsec > 0) nanosleep(&rem, 0);
  }
}
