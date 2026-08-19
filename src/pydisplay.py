"""pyriscv display device: a 1-bit framebuffer shown in a pygame window.

Screen geometry and key codes match the game's display.h / mc.h contract.
The emulator drives this through its display ecalls (src/pyriscv.py):
clear / set_pixel / present / init / poll_input / key / kpressed.

A daemon thread pumps SDL events so the window stays responsive (move,
close, keyboard) even while the slow emulator spends tens of seconds
inside world generation without issuing any ecall.
"""
import os
import threading
import time

# Avoid ALSA noise on headless machines: we only need video + keyboard.
os.environ.setdefault("SDL_AUDIODRIVER", "dummy")
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pygame

from pygfx import Gfx

SCR_W = 192
SCR_H = 168

# Key codes must match the K_* enum in app/minecraft-1960-c/mc.h.
K_LEFT, K_RIGHT, K_UP, K_DOWN = 0, 1, 2, 3
K_ACT, K_USE, K_MENU, K_BACK = 4, 5, 6, 7
K_NEXT, K_PREV, K_AIM, K_FPS = 8, 9, 10, 11
K_DROP, K_CREATIVE = 12, 13
K_D1, K_D2, K_D3, K_D4, K_D5 = 14, 15, 16, 17, 18
K_D6, K_D7, K_D8, K_D9, K_D0 = 19, 20, 21, 22, 23
K_DBG_G, K_DBG_T, K_DBG_H, K_DBG_P, K_DBG_L, K_DBG_M = 24, 25, 26, 27, 28, 29
K_COUNT = 30


def _build_keymap():
    return {
        pygame.K_LEFT: K_LEFT, pygame.K_a: K_LEFT,
        pygame.K_RIGHT: K_RIGHT, pygame.K_d: K_RIGHT,
        pygame.K_UP: K_UP, pygame.K_w: K_UP, pygame.K_SPACE: K_UP,
        pygame.K_DOWN: K_DOWN, pygame.K_s: K_DOWN,
        pygame.K_z: K_ACT, pygame.K_j: K_ACT,
        pygame.K_x: K_USE, pygame.K_k: K_USE,
        pygame.K_c: K_MENU, pygame.K_e: K_MENU,
        pygame.K_ESCAPE: K_BACK,
        pygame.K_q: K_DROP,
        pygame.K_b: K_CREATIVE,
        pygame.K_RIGHTBRACKET: K_NEXT,
        pygame.K_LEFTBRACKET: K_PREV,
        pygame.K_f: K_AIM, pygame.K_TAB: K_AIM,
        pygame.K_r: K_FPS,
        pygame.K_1: K_D1, pygame.K_2: K_D2, pygame.K_3: K_D3,
        pygame.K_4: K_D4, pygame.K_5: K_D5, pygame.K_6: K_D6,
        pygame.K_7: K_D7, pygame.K_8: K_D8, pygame.K_9: K_D9,
        pygame.K_0: K_D0,
        pygame.K_g: K_DBG_G, pygame.K_t: K_DBG_T, pygame.K_h: K_DBG_H,
        pygame.K_p: K_DBG_P, pygame.K_v: K_DBG_L, pygame.K_m: K_DBG_M,
    }


_KEYMAP = _build_keymap()


class PyDisplay(Gfx):
    def __init__(self, width=SCR_W, height=SCR_H, scale=4):
        self.w = width
        self.h = height
        self.scale = max(1, scale)
        self.fb = bytearray(width * height)
        self._clip = height
        self.key_down = [False] * K_COUNT
        self.key_edge = [False] * K_COUNT
        self._edge_snapshot = [False] * K_COUNT
        self._quit = False
        self._inited = False
        self._surface = None
        self._small = None
        self._lock = threading.Lock()
        self._thread = None

    # ---- screen ------------------------------------------------------
    def clear(self):
        self.fb[:] = b"\x00" * (self.w * self.h)

    def set_pixel(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.fb[y * self.w + x] = 1

    def present(self):
        if not self._inited:
            return
        white = (255, 255, 255)
        small = self._small
        small.fill(0)
        set_at = small.set_at
        fb = self.fb
        i = 0
        for y in range(self.h):
            for x in range(self.w):
                if fb[i]:
                    set_at((x, y), white)
                i += 1
        scaled = pygame.transform.scale(
            small, (self.w * self.scale, self.h * self.scale)
        )
        self._surface.blit(scaled, (0, 0))
        pygame.display.flip()

    # ---- input -------------------------------------------------------
    def poll_input(self):
        # Latch the edges that accumulated since the last poll, then reset
        # for the next tick.  Without this, a key press landing during the
        # 250 ms display_sync window is wiped here before the game reads it.
        with self._lock:
            self._edge_snapshot = self.key_edge
            self.key_edge = [False] * K_COUNT

    def key(self, k):
        with self._lock:
            return 1 if 0 <= k < K_COUNT and self.key_down[k] else 0

    def kpressed(self, k):
        with self._lock:
            return 1 if 0 <= k < K_COUNT and self._edge_snapshot[k] else 0

    def should_quit(self):
        return self._quit

    # ---- lifecycle ---------------------------------------------------
    def init(self):
        if self._inited:
            return
        pygame.display.init()
        pygame.font.init()
        pygame.display.set_caption("pyriscv — Minecraft 1960")
        self._surface = pygame.display.set_mode(
            (self.w * self.scale, self.h * self.scale)
        )
        self._small = pygame.Surface((self.w, self.h))
        self._inited = True

        # Show a "generating" screen so the blank window isn't mistaken
        # for a hang while the emulator runs the one-time world gen.
        self._surface.fill((0, 0, 0))
        font = pygame.font.SysFont(None, 24)
        label = font.render("generating world ...", True, (120, 120, 120))
        self._surface.blit(
            label,
            (
                (self._surface.get_width() - label.get_width()) // 2,
                (self._surface.get_height() - label.get_height()) // 2,
            ),
        )
        pygame.display.flip()

        self._thread = threading.Thread(target=self._event_loop, daemon=True)
        self._thread.start()

    def _event_loop(self):
        keymap = _KEYMAP
        while not self._quit:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self._quit = True
                elif event.type == pygame.KEYDOWN:
                    k = keymap.get(event.key)
                    if k is not None:
                        with self._lock:
                            if not self.key_down[k]:
                                self.key_edge[k] = True
                            self.key_down[k] = True
                elif event.type == pygame.KEYUP:
                    k = keymap.get(event.key)
                    if k is not None:
                        with self._lock:
                            self.key_down[k] = False
            time.sleep(0.02)

    def close(self):
        self._quit = True
        if self._thread is not None:
            self._thread.join(timeout=1.0)
            self._thread = None
        if self._inited:
            pygame.quit()
            self._inited = False
