"""pyscreen -- optional display device for the pyriscv emulator.

Screen contract (guest apps must match; the C test app is
app/gfx-saver/main.c):
  * geometry comes from the caller: pyriscv.py passes --width/--height
    (default 192x168); width x height pixels, row-major, origin at the
    top-left.
  * The guest keeps an int[width * height] framebuffer in its data
    memory (one 32-bit little-endian word per pixel): 0 = black,
    nonzero = white.  The buffer sits in the reserved .screenfb window
    (base _screen_fb, see app/c-common/link.ld) but the device only
    needs the pointer.
  * A single ecall presents it: PYRSISCV_ECALL_NUMBER.SCR_DRAW (2000),
    with the word-aligned framebuffer pointer in a0.  Screen placement,
    timing and input are deliberately outside this call.

The screen is an optional module: the core emulator never imports pygame.
Attach it through pyriscv.py's CLI:

  python3 src/pyriscv.py app.mem --screen             # pygame window
  python3 src/pyriscv.py app.mem --dump out.ppm       # headless, last frame
                       [--frames N] [--scale N]

pyriscv.py stops when the window is closed (ESC / close button) or when
--frames N frames have been presented.
"""

import struct

_BLACK = (0, 0, 0)
_WHITE = (255, 255, 255)
_FPS = 20


class Screen:
    """Draws the guest framebuffer; draw() returns False to stop."""

    def __init__(self, mode="window", dump_path=None, max_frames=0, scale=4,
                 width=None, height=None, keymap=None):
        if width is None or height is None:
            raise ValueError("Screen needs width/height (pyriscv.py "
                             "passes --width/--height)")
        if mode not in ("window", "headless"):
            raise ValueError(f"unknown screen mode: {mode}")
        self.w = width
        self.h = height
        # Key number -> pygame key code (KEY_GET ecall).  Values may be a
        # pygame key constant, a single character ("w"), or a key name
        # ("up", "space", ...); resolved lazily once pygame is available.
        self._keymap = dict(keymap or {})
        self._key_codes = None
        # Press latch for short taps: a KEYDOWN sets _press[n]=1 and it is
        # read-and-cleared by key_down().  Combined with the live held
        # state this gives a "sticky level": a held key reads 1 every
        # frame, and a tap shorter than a frame still reads 1 once.
        self._press = {}
        self._num_of_code = {}
        self._quit = False
        # Current frame as 0/1 bytes per pixel, top-left origin.
        self.fb = bytearray(self.w * self.h)
        self._mode = mode
        self._frames = 0
        self._max_frames = max_frames
        self._scale = max(1, scale)
        self._dump_path = dump_path
        self._word_fmt = "<%dI" % (self.w * self.h)
        self._pygame = None
        self._surface = None
        self._clock = None

        if mode == "window":
            import os

            # No audio needed; without a sound card SDL/ALSA spams errors.
            # Keep the pygame banner quiet too.
            os.environ.setdefault("SDL_AUDIODRIVER", "dummy")
            os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

            import pygame  # lazy: pygame is only needed for a window

            pygame.init()
            self._pygame = pygame
            self._key_codes = {
                n: self._resolve_key(pygame, v)
                for n, v in self._keymap.items()
            }
            self._num_of_code = {code: n for n, code in self._key_codes.items()}
            self._surface = pygame.display.set_mode(
                (self.w * self._scale, self.h * self._scale)
            )
            pygame.display.set_caption("pyriscv screen")
            self._clock = pygame.time.Clock()
        else:
            if max_frames <= 0:
                self._max_frames = 1
            if not dump_path:
                raise ValueError("headless mode needs --dump <file>")

    @staticmethod
    def _resolve_key(pygame, value):
        """Map a key name/number to a pygame key constant."""
        if isinstance(value, int):
            return value
        s = str(value).lower()
        if len(s) == 1:
            code = pygame.key.key_code(s)
            if code is not None:
                return code
        return getattr(pygame, "K_" + s.upper(), None)

    def key_down(self, n):
        """Sticky level: 1 if key number n is held down OR was pressed
        since the last read.

        The held state is live (pygame.key.get_pressed), so holding a
        key reads 1 every frame (long-press).  A KEYDOWN also latches a
        one-shot flag, so a tap shorter than a frame interval still
        reads 1 once instead of being lost.  The latch is cleared by
        this read."""
        if self._mode != "window" or not self._key_codes:
            return 0
        code = self._key_codes.get(n)
        if code is None:
            return 0
        self._drain_events()
        held = 1 if self._pygame.key.get_pressed()[code] else 0
        pressed = self._press.get(n, 0)
        self._press[n] = 0
        return 1 if (held or pressed) else 0

    def _drain_events(self):
        """Pump the pygame event queue into the press latch + quit flag.

        Idempotent: the queue is empty after this until new events
        arrive.  pygame refreshes key.get_pressed() whenever the queue
        is pumped, so the live held view is also kept current here."""
        for event in self._pygame.event.get():
            if event.type == self._pygame.QUIT:
                self._quit = True
            elif event.type == self._pygame.KEYDOWN:
                if event.key == self._pygame.K_ESCAPE:
                    self._quit = True
                else:
                    n = self._num_of_code.get(event.key)
                    if n is not None:
                        self._press[n] = 1

    def draw(self, dmem, fb_ptr):
        """Read the guest framebuffer at fb_ptr and present it.

        Returns False when the emulator should stop (window closed or
        --frames reached), True otherwise.
        """
        if fb_ptr % 4:
            raise ValueError(f"screen fb pointer not word-aligned: {fb_ptr:#x}")

        words = struct.unpack(
            self._word_fmt, dmem.read_bytes(fb_ptr, 4 * self.w * self.h)
        )
        fb = self.fb
        for i, w in enumerate(words):
            fb[i] = 1 if w else 0

        self._frames += 1
        if self._mode == "window":
            return self._present_window()
        return self._present_headless()

    def _present_window(self):
        pygame = self._pygame
        self._drain_events()
        if self._quit:
            return False

        surf = pygame.image.frombuffer(self.fb, (self.w, self.h), "P")
        surf.set_palette([_BLACK, _WHITE])
        big = pygame.transform.scale(
            surf, (self.w * self._scale, self.h * self._scale)
        )
        self._surface.blit(big, (0, 0))
        pygame.display.flip()
        self._clock.tick(_FPS)

        if self._max_frames and self._frames >= self._max_frames:
            return False
        return True

    def _present_headless(self):
        if self._frames >= self._max_frames:
            self._dump()
            return False
        return True

    def _dump(self):
        path = self._dump_path
        if path.endswith(".pbm"):
            self._dump_pbm(path)
        else:
            self._dump_ppm(path)

    def _dump_ppm(self, path):
        """P6 (binary RGB) -- simple to read back for verification."""
        w, h, fb = self.w, self.h, self.fb
        out = bytearray(w * h * 3)
        i = 0
        for v in fb:
            if v:
                out[i] = out[i + 1] = out[i + 2] = 255
            i += 3
        with open(path, "wb") as f:
            f.write(f"P6\n{w} {h}\n255\n".encode())
            f.write(out)

    def _dump_pbm(self, path):
        """P4 (raw bitmap), 1 bit per pixel, MSB first per row."""
        w, h, fb = self.w, self.h, self.fb
        row_bytes = (w + 7) // 8
        with open(path, "wb") as f:
            f.write(f"P4\n{w} {h}\n".encode())
            for y in range(h):
                row = bytearray(row_bytes)
                base = y * w
                for x in range(w):
                    if fb[base + x]:
                        row[x >> 3] |= 0x80 >> (x & 7)
                f.write(row)
