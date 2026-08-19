/* main.c -- init and main loop.
 *
 * Logic ticks at 4 Hz, the display is redrawn every second tick, so
 * the screen updates at 2 fps.  On the 1960s target, display_sync()
 * is provided by the native code (timer interrupt / vblank / etc).
 */
#include "mc.h"
#include "display.h"

int main(void)
{
  u8 tick = 0;
  u16 seed = 1960;
  static int frames;
  static long sec_end;

  display_init();

#ifdef PY_RISCV_GFX
  /* Loading screen — world generation (gen_overworld) is slow on the
   * emulated target, so show this before game_init() blocks on it. */
  screen_clear();
  screen_text(82, 78, "LOADING");
  screen_text(64, 88, "GENERATING WORLD");
  display_present();
#endif

  game_init(seed);

  for (;;) {
    input_poll();
    game_tick();               /* always 4 Hz: one tick per loop */

    if (g.hifps) {
      /* logic rate unchanged; render as fast as the display can show */
      long end = display_ms() + 250;
      for (;;) {
        long now = display_ms();
        if (now >= end) break;
        render_frame();
        display_present();
        frames++;
        display_sleep_ms(16);
      }
    } else {
      if (++tick == FRAME_EVERY) {
        tick = 0;
        render_frame();
        frames++;
      }
      display_sync();
    }

    if (display_ms() >= sec_end) {
      g.fps_now = (u8)(frames > 99 ? 99 : frames);
      frames = 0;
      sec_end = display_ms() + 1000;
    }
  }
  return 0;
}
