/* display_riscv.c -- display backend for the pyriscv emulator.
 *
 * The screen lives inside the emulator: this file turns the game's
 * display.h contract into ecalls that the emulator's display device
 * handles.  There is no X11 or terminal code here -- the emulator owns
 * the framebuffer and renders it (see src/pydisplay.py).
 *
 * The ecall numbers must match PYRSISCV_ECALL_NUMBER in
 * src/pyriscv_riscv_def.py.  The emulated hardware clock ticks at 20 Hz
 * (50 ms); the game's logic tick is 250 ms = 5 clock ticks.
 */
#include "mc.h"
#include "display.h"

#define SYS_SCR_CLEAR    2000
#define SYS_SCR_SET      2001
#define SYS_DSP_INIT     2002
#define SYS_DSP_PRESENT  2003
#define SYS_DSP_SYNC     2004
#define SYS_DSP_MS       2005
#define SYS_DSP_SLEEP    2006
#define SYS_INPUT_POLL   2007
#define SYS_KEY_GET      2008
#define SYS_KEY_PRESSED  2009
#define SYS_SCR_HLINE    2010
#define SYS_SCR_VLINE    2011
#define SYS_SCR_RECT     2012
#define SYS_SCR_FILL     2013
#define SYS_SCR_CHAR     2014
#define SYS_SCR_TEXT     2015
#define SYS_SCR_NUM      2016
#define SYS_SCR_ICON     2017
#define SYS_SCR_SPECKLE  2018
#define SYS_SCR_HEART    2019
#define SYS_SCR_FLUID    2020
#define SYS_SCR_WORLD    2021

/* Measurement hook: when built with -DMAX_TICKS=N, exit after N logic
 * ticks (N/2 rendered frames) so the emulator prints its instruction
 * count.  Disabled by default. */
#ifdef MAX_TICKS
static int tick_count;
#endif

void screen_clear(void)
{
    register int a7 asm("a7") = SYS_SCR_CLEAR;
    asm volatile("ecall" : : "r"(a7) : "memory");
}

void screen_set(int x, int y)
{
    register int a0 asm("a0") = x;
    register int a1 asm("a1") = y;
    register int a7 asm("a7") = SYS_SCR_SET;
    asm volatile("ecall" : : "r"(a0), "r"(a1), "r"(a7) : "memory");
}

static void ecall3(int n, int a0, int a1, int a2)
{
    register int ra0 asm("a0") = a0;
    register int ra1 asm("a1") = a1;
    register int ra2 asm("a2") = a2;
    register int a7 asm("a7") = n;
    asm volatile("ecall" : : "r"(ra0), "r"(ra1), "r"(ra2), "r"(a7) : "memory");
}

static void ecall4(int n, int a0, int a1, int a2, int a3)
{
    register int ra0 asm("a0") = a0;
    register int ra1 asm("a1") = a1;
    register int ra2 asm("a2") = a2;
    register int ra3 asm("a3") = a3;
    register int a7 asm("a7") = n;
    asm volatile("ecall" : : "r"(ra0), "r"(ra1), "r"(ra2), "r"(ra3), "r"(a7) : "memory");
}

static void ecall5(int n, int a0, int a1, int a2, int a3, int a4)
{
    register int ra0 asm("a0") = a0;
    register int ra1 asm("a1") = a1;
    register int ra2 asm("a2") = a2;
    register int ra3 asm("a3") = a3;
    register int ra4 asm("a4") = a4;
    register int a7 asm("a7") = n;
    asm volatile("ecall" : : "r"(ra0), "r"(ra1), "r"(ra2), "r"(ra3), "r"(ra4), "r"(a7) : "memory");
}

void screen_hline(int x0, int x1, int y)
{
    ecall3(SYS_SCR_HLINE, x0, x1, y);
}

void screen_vline(int x, int y0, int y1)
{
    ecall3(SYS_SCR_VLINE, x, y0, y1);
}

void screen_rect(int x, int y, int w, int h)
{
    ecall4(SYS_SCR_RECT, x, y, w, h);
}

void screen_fill(int x, int y, int w, int h)
{
    ecall4(SYS_SCR_FILL, x, y, w, h);
}

void screen_char(int x, int y, int c)
{
    ecall3(SYS_SCR_CHAR, x, y, c);
}

void screen_text(int x, int y, const char *s)
{
    ecall3(SYS_SCR_TEXT, x, y, (int)s);
}

void screen_num(int x, int y, int v)
{
    ecall3(SYS_SCR_NUM, x, y, v);
}

void screen_icon(int id, int x, int y, int clip)
{
    ecall4(SYS_SCR_ICON, id, x, y, clip);
}

void screen_speckle(int id, int x, int y, int time, int smelt)
{
    ecall5(SYS_SCR_SPECKLE, id, x, y, time, smelt);
}

void screen_heart(int x, int y, int mode)
{
    ecall3(SYS_SCR_HEART, x, y, mode);
}

void screen_fluid(int id, int x, int y, int surf, int time)
{
    ecall5(SYS_SCR_FLUID, id, x, y, surf, time);
}

void screen_world(void *wmap, int cam_x, int cam_y, int time, int smelt)
{
    ecall5(SYS_SCR_WORLD, (int)wmap, cam_x, cam_y, time, smelt);
}

void display_init(void)
{
    register int a7 asm("a7") = SYS_DSP_INIT;
    asm volatile("ecall" : : "r"(a7) : "memory");
}

void display_present(void)
{
    register int a7 asm("a7") = SYS_DSP_PRESENT;
    asm volatile("ecall" : : "r"(a7) : "memory");
}

void display_sync(void)
{
    register int a7 asm("a7") = SYS_DSP_SYNC;
    asm volatile("ecall" : : "r"(a7) : "memory");
#ifdef MAX_TICKS
    if (++tick_count >= MAX_TICKS) {
        register int a0 asm("a0") = 0;
        register int a7e asm("a7") = 93;  /* SYS_exit */
        asm volatile("ecall" : : "r"(a0), "r"(a7e) : "memory");
    }
#endif
}

long display_ms(void)
{
    register int a0 asm("a0");
    register int a7 asm("a7") = SYS_DSP_MS;
    asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return a0;
}

void display_sleep_ms(int n)
{
    register int a0 asm("a0") = n;
    register int a7 asm("a7") = SYS_DSP_SLEEP;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}

void input_poll(void)
{
    register int a7 asm("a7") = SYS_INPUT_POLL;
    asm volatile("ecall" : : "r"(a7) : "memory");
}

int key(int k)
{
    register int a0 asm("a0") = k;
    register int a7 asm("a7") = SYS_KEY_GET;
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

int kpressed(int k)
{
    register int a0 asm("a0") = k;
    register int a7 asm("a7") = SYS_KEY_PRESSED;
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}
