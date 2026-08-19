/* display_ref.c -- REFERENCE display driver for modern POSIX boxes.
 *
 * Swap this file out for your native implementation: the game only
 * uses screen_clear()/screen_set() for video, plus input_poll/key/
 * kpressed for the keyboard and display_sync for the 250 ms tick.
 *
 * Environment variables (reference driver only):
 *   MC_FAST=1         run without pacing (for testing)
 *   MC_PBM=prefix     dump each frame to prefix_%04d.pbm
 *   MC_PBM_EVERY=n    dump every n-th frame (default 1)
 *   MC_FRAMES=n       exit after n frames dumped (default: run forever)
 *   MC_SCALE=n        terminal scale 2 or 4 (default 4)
 *   MC_NOTTY=1        do not draw to the terminal
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include "mc.h"
#include "display.h"

static unsigned char fb[SCR_W * SCR_H];
static struct termios saved_tio;
static int tty_mode;
static int fast_mode, notty_mode;
static const char *pbm_prefix;
static int pbm_every = 1, pbm_count = 0, frame_max = 0;
static int term_scale = 4;
static long tick_count;
static struct timespec deadline;

static int key_down[K_COUNT], key_edge[K_COUNT];

/* ------------------------------------------------------------------ */
/* Screen functions: the only video calls the game makes               */
/* ------------------------------------------------------------------ */
void screen_clear(void)
{
  memset(fb, 0, sizeof(fb));
}

void screen_set(int x, int y)
{
  if ((unsigned)x < SCR_W && (unsigned)y < SCR_H)
    fb[y * SCR_W + x] = 1;
}

/* ------------------------------------------------------------------ */
/* PBM snapshot                                                        */
/* ------------------------------------------------------------------ */
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
/* Terminal rendering                                                  */
/* ------------------------------------------------------------------ */
static void draw_term(void)
{
  static char buf[(SCR_W / 2 + 2) * (SCR_H / 2 + 2)];
  int x, y, i = 0, s = term_scale;
  buf[i++] = '\033'; buf[i++] = '['; buf[i++] = 'H';
  for (y = 0; y < SCR_H; y += s) {
    for (x = 0; x < SCR_W; x += s) {
      int n = 0, dy, dx;
      for (dy = 0; dy < s; dy++)
        for (dx = 0; dx < s; dx++)
          n += fb[(y + dy) * SCR_W + (x + dx)];
      buf[i++] = n == 0 ? ' ' : n < s * s / 3 ? '.' :
                 n < (s * s * 2) / 3 ? ':' : '#';
    }
    buf[i++] = '\r'; buf[i++] = '\n';
  }
  if (write(1, buf, (size_t)i) < 0) i = 0;
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */
static void press(int k)
{
  if (k >= 0 && k < K_COUNT) {
    if (!key_down[k]) key_edge[k] = 1;
    key_down[k] = 1;
  }
}

static void handle_byte(unsigned char c)
{
  switch (c) {
    case 'a': case 'A': press(K_LEFT); break;
    case 'd': case 'D': press(K_RIGHT); break;
    case 'w': case 'W': press(K_UP); break;
    case 's': case 'S': press(K_DOWN); break;
    case ' ': press(K_UP); break;
    case 'z': case 'Z': case 'j': case 'J': press(K_ACT); break;
    case 'x': case 'X': case 'k': case 'K': press(K_USE); break;
    case 'c': case 'C': case 'e': case 'E': press(K_MENU); break;
    case 27: press(K_BACK); break;
    case 'q': case 'Q': press(K_DROP); break;
    case 'b': case 'B': press(K_CREATIVE); break;
    case ']': press(K_NEXT); break;
    case '[': press(K_PREV); break;
    case 'f': case 'F': case 9: press(K_AIM); break;
    case 'r': case 'R': press(K_FPS); break;
    case '1': press(K_D1); break;
    case '2': press(K_D2); break;
    case '3': press(K_D3); break;
    case '4': press(K_D4); break;
    case '5': press(K_D5); break;
    case '6': press(K_D6); break;
    case '7': press(K_D7); break;
    case '8': press(K_D8); break;
    case '9': press(K_D9); break;
    case '0': press(K_D0); break;
    case 'g': press(K_DBG_G); break;
    case 't': press(K_DBG_T); break;
    case 'h': press(K_DBG_H); break;
    case 'p': press(K_DBG_P); break;
    case 'v': press(K_DBG_L); break;
    case 'm': press(K_DBG_M); break;
    default: break;
  }
}

/* One key event is consumed per tick: at 4 Hz that matches a brisk
 * typist and makes piped input scripts deterministic. */
static unsigned char q[8192];
static int qh, qt;

void input_poll(void)
{
  unsigned char buf[256];
  ssize_t n;
  int k;
  for (k = 0; k < K_COUNT; k++) key_edge[k] = 0;
  /* no release events from a pipe: keys held for exactly one tick */
  if (!tty_mode) {
    for (k = 0; k < K_COUNT; k++) key_down[k] = 0;
  }
  n = read(0, buf, sizeof(buf));
  if (n > 0) {
    if (qt + n > (int)sizeof(q)) n = (ssize_t)(sizeof(q) - qt);
    for (k = 0; k < n; k++) q[qt++] = buf[k];
  }
  /* pop one event */
  if (qh < qt) {
    unsigned char c = q[qh++];
    if (c == 27 && qt - qh >= 2 && q[qh] == '[') {
      qh++;
      switch (q[qh++]) {
        case 'A': press(K_UP); break;
        case 'B': press(K_DOWN); break;
        case 'C': press(K_RIGHT); break;
        case 'D': press(K_LEFT); break;
        default: break;
      }
    } else if (c != 27) {
      handle_byte(c);
    }
  }
}

int key(int k)     { return k >= 0 && k < K_COUNT ? key_down[k] : 0; }
int kpressed(int k){ return k >= 0 && k < K_COUNT ? key_edge[k] : 0; }

/* ------------------------------------------------------------------ */
/* Init + pacing                                                       */
/* ------------------------------------------------------------------ */
static void timeout_reset(void)
{
  tcsetattr(0, TCSANOW, &saved_tio);
}

void display_init(void)
{
  const char *e;
  if ((e = getenv("MC_FAST")) && *e == '1') fast_mode = 1;
  if ((e = getenv("MC_NOTTY")) && *e == '1') notty_mode = 1;
  if ((e = getenv("MC_PBM"))) pbm_prefix = e;
  if ((e = getenv("MC_PBM_EVERY"))) pbm_every = atoi(e) > 0 ? atoi(e) : 1;
  if ((e = getenv("MC_FRAMES"))) frame_max = atoi(e);
  if ((e = getenv("MC_SCALE"))) term_scale = atoi(e) == 2 ? 2 : 4;

  if (isatty(0)) {
    struct termios tio;
    tty_mode = 1;
    tcgetattr(0, &saved_tio);
    tio = saved_tio;
    tio.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &tio);
    atexit(timeout_reset);
  }
  if (isatty(1) && !notty_mode) {
    if (write(1, "\033[2J\033[?25l", 10) < 0) tty_mode = 0;
  }
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
  if (n <= 0) return;
  ts.tv_sec = n / 1000;
  ts.tv_nsec = (long)(n % 1000) * 1000000L;
  nanosleep(&ts, 0);
}

void display_present(void)
{
  if (tty_mode && isatty(1) && !notty_mode) draw_term();
}

void display_sync(void)
{
  static int since_frame;
  tick_count++;
  since_frame++;

  if (pbm_prefix && since_frame >= 2) {
    /* frames land every 2 ticks; dump once per frame */
    static int dumped;
    since_frame = 0;
    dumped++;
    if (dumped % pbm_every == 0) dump_pbm();
  }

  display_present();

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
