/* gfx-saver -- 1-bit demo for the standardized screen ecall.
 *
 * Contract (geometry must match pyriscv src/pyscreen.py, and the
 * riscvmc2 screen config):
 *   - screen is SCR_W x SCR_H (192 x 168 now), row-major, origin at the
 *     top-left;
 *   - framebuffer is int[SCR_W*SCR_H] in data memory: 0 = black, else white;
 *   - present it with ONE ecall: a7 = 2000 (SCR_DRAW), a0 = fb pointer.
 * The framebuffer lives in the reserved .screenfb window at the fixed
 * base _screen_fb (see app/c-common/link.ld, currently 0x30000000).
 *
 * Timing & input (game tick clock, 1 gt = 1/20 s):
 *   a7 = 2001 (GT_GET)   a0 = current game tick
 *   a7 = 2002 (GT_WAIT)  pause until the next game tick
 *   a7 = 2003 (KEY_GET)  a0 = key number -> a0 = 1 if held.  The launch
 *                        configuration maps numbers to real keys, e.g.
 *                        pyriscv: --key 0=w --key 1=s
 *
 * This demo paces itself on the game tick clock: it renders at most once
 * per "target interval" of game ticks (1..20 gt per frame; default 10,
 * i.e. 2 frames/s), shows target/measured intervals in the top-left
 * corner ("T##" target, "F##" measured), and key 0 / key 1 raise / lower
 * the target (bind e.g. --key 0=w --key 1=s: w = larger interval =
 * slower, s = smaller interval = faster).  The static card is painted
 * once; the HUD and the moving clock hand / bouncing block are redrawn
 * every rendered frame.
 *
 * Run with the emulator, e.g. from app/gfx-saver/:
 *   python3 ../../src/pyriscv.py app.mem --screen --key 0=w --key 1=s
 *   python3 ../../src/pyriscv.py app.mem --frames 2 --dump out.ppm
 */
#include <stdint.h>

#define SCR_W 192
#define SCR_H 168
#define SCR_PX (SCR_W * SCR_H)

#define SYS_SCR_DRAW 2000
#define SYS_GT_GET 2001
#define SYS_GT_WAIT 2002
#define SYS_KEY_GET 2003

#define GT_MIN 1
#define GT_MAX 20
#define GT_DEFAULT 10

static int fb[SCR_PX] __attribute__((section(".screenfb"), aligned(4)));

static void draw_screen(void)
{
    register int a0 asm("a0") = (int)fb;
    register int a7 asm("a7") = SYS_SCR_DRAW;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}

static long gt_now(void)
{
    register int a0 asm("a0");
    register int a7 asm("a7") = SYS_GT_GET;
    asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return a0;
}

static void gt_wait(void)
{
    register int a7 asm("a7") = SYS_GT_WAIT;
    asm volatile("ecall" : : "r"(a7) : "memory");
}

static int key_down(int k)
{
    register int a0 asm("a0") = k;
    register int a7 asm("a7") = SYS_KEY_GET;
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

static inline void px(int x, int y, int on)
{
    if ((unsigned)x < SCR_W && (unsigned)y < SCR_H)
        fb[y * SCR_W + x] = on;
}

/* ---- static test card: outer + inner frame ----------------------- */

static void hline(int x0, int x1, int y)
{
    int x;
    if ((unsigned)y >= SCR_H)
        return;
    if (x0 < 0)
        x0 = 0;
    if (x1 >= SCR_W)
        x1 = SCR_W - 1;
    for (x = x0; x <= x1; x++)
        px(x, y, 1);
}

static void vline(int x, int y0, int y1)
{
    int y;
    if ((unsigned)x >= SCR_W)
        return;
    if (y0 < 0)
        y0 = 0;
    if (y1 >= SCR_H)
        y1 = SCR_H - 1;
    for (y = y0; y <= y1; y++)
        px(x, y, 1);
}

static void card(void)
{
    hline(0, SCR_W - 1, 0);
    hline(0, SCR_W - 1, SCR_H - 1);
    vline(0, 0, SCR_H - 1);
    vline(SCR_W - 1, 0, SCR_H - 1);
    hline(4, SCR_W - 5, 4);
    hline(4, SCR_W - 5, SCR_H - 5);
    vline(4, 4, SCR_H - 5);
    vline(SCR_W - 5, 4, SCR_H - 5);
}

/* ---- bouncing 3x3 block (erases its own old position) ------------ */

static int blk_x, blk_y;
static int blk_vx = 2, blk_vy = 1;

static void erase_block(void)
{
    int x, y;
    for (y = 0; y < 3; y++)
        for (x = 0; x < 3; x++)
            px(blk_x + x, blk_y + y, 0);
}

static void draw_block(void)
{
    int x, y;
    for (y = 0; y < 3; y++)
        for (x = 0; x < 3; x++)
            px(blk_x + x, blk_y + y, 1);
}

static void step_block(void)
{
    erase_block();
    blk_x += blk_vx;
    blk_y += blk_vy;
    if (blk_x <= 6 || blk_x + 2 >= SCR_W - 7)
        blk_vx = -blk_vx;
    if (blk_y <= 6 || blk_y + 2 >= SCR_H - 7)
        blk_vy = -blk_vy;
    draw_block();
}

/* ---- rotating clock hand, 30 px long (erases the old one) -------- */

/* 32-step unit circle *32, index & 31 */
static const char cs32[32] = {
    32,
    31,
    29,
    25,
    19,
    13,
    6,
    -1,
    -6,
    -13,
    -19,
    -25,
    -29,
    -31,
    -32,
    -31,
    -29,
    -25,
    -19,
    -13,
    -6,
    1,
    6,
    13,
    19,
    25,
    29,
    31,
    32,
    31,
    29,
    25,
};
static const char sn32[32] = {
    0,
    6,
    13,
    19,
    25,
    29,
    32,
    31,
    29,
    25,
    19,
    13,
    6,
    1,
    0,
    -1,
    -6,
    -13,
    -19,
    -25,
    -29,
    -32,
    -31,
    -29,
    -25,
    -19,
    -13,
    -6,
    -1,
    0,
    1,
    6,
};

static int hand_a; /* angle step 0..31, advancing every 2 frames */

static void hand_line(int a, int on)
{
    int i;
    for (i = 0; i < 30; i += 2)
    {
        px(SCR_W / 2 + (cs32[a] * i) / 32, SCR_H / 2 + (sn32[a] * i) / 32, on);
    }
}

static void step_hand(int t)
{
    if ((t & 1) == 0)
        return; /* advance every other frame */
    hand_line(hand_a, 0);
    hand_a = (hand_a + 1) & 31;
    hand_line(hand_a, 1);
}

/* ---- 3x5 font for the HUD ---------------------------------------- */

/* glyph rows, 3 px wide; index: 0-9 digits, 10 ':', 11 'T', 12 'F',
 * 13 space.  Bit 2 = leftmost pixel. */
static const unsigned char font[14][5] = {
    {7, 5, 5, 5, 7}, /* 0 */
    {2, 6, 2, 2, 7}, /* 1 */
    {7, 1, 7, 4, 7}, /* 2 */
    {7, 1, 7, 1, 7}, /* 3 */
    {5, 5, 7, 1, 1}, /* 4 */
    {7, 4, 7, 1, 7}, /* 5 */
    {7, 4, 7, 5, 7}, /* 6 */
    {7, 1, 1, 1, 1}, /* 7 */
    {7, 5, 7, 5, 7}, /* 8 */
    {7, 5, 7, 1, 7}, /* 9 */
    {0, 2, 0, 2, 0}, /* : */
    {7, 2, 2, 2, 2}, /* T */
    {7, 4, 7, 4, 4}, /* F */
    {0, 0, 0, 0, 0}, /* space */
};

static void draw_char(int x, int y, char c)
{
    int g, r, xx;
    if (c >= '0' && c <= '9')
        g = c - '0';
    else if (c == ':')
        g = 10;
    else if (c == 'T' || c == 't')
        g = 11;
    else if (c == 'F' || c == 'f')
        g = 12;
    else
        g = 13;
    for (r = 0; r < 5; r++)
    {
        unsigned char row = font[g][r];
        for (xx = 0; xx < 3; xx++)
            px(x + xx, y + r, (row >> (2 - xx)) & 1);
    }
}

static void draw_str(int x, int y, const char *s)
{
    while (*s)
    {
        draw_char(x, y, *s);
        x += 4;
        s++;
    }
}

static void erase_box(int x0, int y0, int x1, int y1)
{
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            px(x, y, 0);
}

/* "T##" target interval + "F##" measured interval, top-left corner.
 * The letters T/F are drawn once by the caller; only the two digit
 * cells are redrawn here, and only when a digit actually changed, so an
 * idle frame costs almost nothing. */
static char hud_t[3] = {' ', ' '};
static char hud_f[3] = {' ', ' '};

static void hud_row(int y, char *prev, int val)
{
    char d0 = (char)('0' + (val / 10) % 10);
    char d1 = (char)('0' + val % 10);
    if (prev[0] == d0 && prev[1] == d1)
        return;
    erase_box(10, y, 18, y + 4); /* the two digit cells of one row */
    prev[0] = d0;
    prev[1] = d1;
    draw_char(11, y, d0);
    draw_char(15, y, d1);
}

static void hud(int tgt, int act)
{
    hud_row(7, hud_t, tgt);
    hud_row(14, hud_f, act);
}

int main(void)
{
    uint32_t f = 0;
    int target = GT_DEFAULT;
    long next_gt = 0;
    long last_gt = 0;
    int have_last = 0;

    blk_x = SCR_W / 2 - 1;
    blk_y = 40;
    hand_a = 0;
    card();
    hand_line(hand_a, 1);
    draw_block();
    /* HUD labels, drawn once (digits change via hud()) */
    draw_char(7, 7, 'T');
    draw_char(7, 14, 'F');

    for (;;)
    {
        long t = gt_now();
        if (t < next_gt)
        {
            gt_wait();
            continue;
        }

        /* key 0 (w): raise the target interval; key 1 (s): lower it.
         * Press edges only; keys are registered at launch, e.g.
         * pyriscv: --key 0=w --key 1=s */
        {
            int k0 = key_down(0), k1 = key_down(1);
            if (k0 && target < GT_MAX)
                target++;
            if (k1 && target > GT_MIN)
                target--;
        }

        hud(target, have_last ? (int)(t - last_gt) : 0);

        step_block();
        step_hand((int)f);
        f++;

        draw_screen();

        last_gt = t;
        have_last = 1;
        next_gt = t + target;
    }
    return 0;
}
