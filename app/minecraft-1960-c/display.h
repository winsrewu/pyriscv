/* display.h -- the hardware abstraction for the 1960s target.
 *
 * The SCREEN is described by exactly two functions:
 *
 *   void screen_clear(void);       blank the whole 192x168 display
 *   void screen_set(int x,int y);  set one pixel (0<=x<192, 0<=y<168)
 *
 * Replace display_ref.c with your own implementation (native display
 * code, real hardware, etc).  The game never touches the screen any
 * other way.
 *
 * The reference driver additionally provides input + pacing, which on
 * a real machine would come from the keyboard controller and a timer
 * interrupt.  Those hooks are:
 *
 *   void display_init(void);
 *   void input_poll(void);          refresh key state (once per logic tick)
 *   int  key(int k);                key held down      (K_xxx codes)
 *   int  kpressed(int k);           key pressed since last tick
 *   void display_sync(void);        present + wait for the next 250 ms tick
 *   void display_present(void);     present now, without waiting
 *   long display_ms(void);          monotonic milliseconds
 *   void display_sleep_ms(int n);   sleep ~n ms
 */
#ifndef DISPLAY_H
#define DISPLAY_H

void screen_clear(void);
void screen_set(int x, int y);

/* High-level drawing primitives.  A backend may implement these natively
 * (the pyriscv port turns them into ecalls handled in Python); otherwise
 * render.c falls back to screen_set loops via the PY_RISCV_GFX flag. */
void screen_hline(int x0, int x1, int y);
void screen_vline(int x, int y0, int y1);
void screen_rect(int x, int y, int w, int h);
void screen_fill(int x, int y, int w, int h);
void screen_char(int x, int y, int c);
void screen_text(int x, int y, const char *s);
void screen_num(int x, int y, int v);
void screen_icon(int id, int x, int y, int clip);
void screen_speckle(int id, int x, int y, int time, int smelt);
void screen_heart(int x, int y, int mode);
void screen_fluid(int id, int x, int y, int surf, int time);
void screen_world(void *wmap, int cam_x, int cam_y, int time, int smelt);

void display_init(void);
void input_poll(void);
int  key(int k);
int  kpressed(int k);
void display_sync(void);
void display_present(void);
long display_ms(void);
void display_sleep_ms(int n);

#endif
