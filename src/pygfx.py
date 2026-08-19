"""pygfx -- game-specific 1-bit drawing, executed natively in Python.

The C renderer (app/minecraft-1960-c/render.c) calls into this code
through the display ecalls, so the per-pixel loops and glyph/icon/speckle
switches run at Python speed instead of consuming guest instructions on
the emulated CPU.  Every drawing rule mirrors render.c exactly.

The mixin draws into ``self.fb`` (a bytearray, 0/1 per pixel) with
``self.w``/``self.h`` dimensions, and clips vertically at ``self._clip``
(mirrors render.c's ``icon_clip``).
"""

# Block ids (mc.h)
B_AIR = 0
B_GRASS, B_DIRT, B_STONE, B_COBBLE, B_BEDROCK = 1, 2, 3, 4, 5
B_LOG, B_LEAVES, B_PLANKS, B_CRAFT, B_FURNACE = 6, 7, 8, 9, 10
B_SAND, B_GRAVEL, B_COAL, B_IRON, B_DIAMOND = 11, 12, 13, 14, 15
B_WATER, B_LAVA, B_OBSIDIAN, B_PORTAL = 16, 17, 18, 19
B_NETHERRACK, B_NBRICK, B_SPAWNER, B_ENDSTONE = 20, 21, 22, 23
B_SBRICK, B_FRAME, B_FRAME_EYE, B_ENDPORTAL = 24, 25, 26, 27

I_STICK, I_COAL, I_FLINT, I_IRONING, I_DIAMOND = 40, 41, 42, 43, 44
I_BUCKET, I_BUCKET_W, I_BUCKET_L, I_FSTEEL, I_ROD = 45, 46, 47, 48, 49
I_POWDER, I_PEARL, I_EYE = 50, 51, 52
I_WPICK, I_WAXE, I_WSWORD = 53, 54, 55
I_SPICK, I_SAXE, I_SSWORD = 56, 57, 58
I_IPICK, I_IAXE, I_ISWORD = 59, 60, 61
I_DPICK, I_DAXE, I_DSWORD = 62, 63, 64

# 3x5 font, 48 glyphs, same order as render.c's font[]
_FONT = (
    7, 5, 5, 5, 7,    # 0
    2, 6, 2, 2, 7,    # 1
    6, 1, 2, 4, 7,    # 2
    6, 1, 2, 1, 6,    # 3
    5, 5, 7, 1, 1,    # 4
    7, 4, 6, 1, 6,    # 5
    3, 4, 7, 5, 7,    # 6
    7, 1, 2, 2, 2,    # 7
    7, 5, 7, 5, 7,    # 8
    7, 5, 7, 1, 6,    # 9
    2, 5, 7, 5, 5,    # A
    6, 5, 6, 5, 6,    # B
    3, 4, 4, 4, 3,    # C
    6, 5, 5, 5, 6,    # D
    7, 4, 6, 4, 7,    # E
    7, 4, 6, 4, 4,    # F
    3, 4, 5, 5, 3,    # G
    5, 5, 7, 5, 5,    # H
    7, 2, 2, 2, 7,    # I
    1, 1, 1, 5, 2,    # J
    5, 5, 6, 5, 5,    # K
    4, 4, 4, 4, 7,    # L
    5, 7, 7, 5, 5,    # M
    6, 5, 5, 5, 5,    # N
    2, 5, 5, 5, 2,    # O
    6, 5, 6, 4, 4,    # P
    2, 5, 5, 6, 1,    # Q
    6, 5, 6, 5, 5,    # R
    3, 4, 2, 1, 6,    # S
    7, 2, 2, 2, 2,    # T
    5, 5, 5, 5, 7,    # U
    5, 5, 5, 5, 2,    # V
    5, 5, 7, 7, 5,    # W
    5, 5, 2, 5, 5,    # X
    5, 5, 2, 2, 2,    # Y
    7, 1, 2, 4, 7,    # Z
    0, 0, 0, 0, 0,    # space
    0, 0, 0, 0, 2,    # .
    0, 2, 0, 2, 0,    # :
    2, 2, 2, 0, 2,    # !
    6, 1, 2, 0, 2,    # ?
    0, 0, 7, 0, 0,    # -
    0, 2, 7, 2, 0,    # +
    4, 2, 1, 2, 4,    # >
    1, 2, 4, 2, 1,    # <
    1, 1, 2, 4, 4,    # /
    2, 2, 0, 0, 0,    # '
    0, 0, 0, 2, 4,    # ,
)

# solid_r: is the block solid (0 for air/water/lava/portal/endportal)
_SOLID = (
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0,
)


def _glyph_of(c):
    o = ord(c)
    if 48 <= o <= 57:
        return o - 48
    if 65 <= o <= 90:
        return o - 65 + 10
    if 97 <= o <= 122:
        return o - 97 + 10
    if c == ".":
        return 37
    if c == ":":
        return 38
    if c == "!":
        return 39
    if c == "?":
        return 40
    if c == "-":
        return 41
    if c == "+":
        return 42
    if c == ">":
        return 43
    if c == "<":
        return 44
    if c == "/":
        return 45
    if c == "'":
        return 46
    if c == ",":
        return 47
    return 36


def _tool_tier(id):
    return (id - I_WPICK) // 3 + 1 if I_WPICK <= id <= I_DSWORD else 0


def _tool_kind(id):
    return (id - I_WPICK) % 3


class Gfx:
    """Drawing mixin; expects self.fb, self.w, self.h, self._clip."""

    # ---- primitives -------------------------------------------------
    def gfx_px(self, x, y):
        if 0 <= x < self.w and 0 <= y < self._clip:
            self.fb[y * self.w + x] = 1

    def gfx_hline(self, x0, x1, y):
        if y < 0 or y >= self._clip:
            return
        if x0 < 0:
            x0 = 0
        if x1 >= self.w:
            x1 = self.w - 1
        if x0 <= x1:
            base = y * self.w
            for x in range(x0, x1 + 1):
                self.fb[base + x] = 1

    def gfx_vline(self, x, y0, y1):
        if x < 0 or x >= self.w:
            return
        if y0 < 0:
            y0 = 0
        if y1 >= self._clip:
            y1 = self._clip - 1
        if y0 <= y1:
            for y in range(y0, y1 + 1):
                self.fb[y * self.w + x] = 1

    def gfx_rect(self, x, y, w, h):
        self.gfx_hline(x, x + w - 1, y)
        self.gfx_hline(x, x + w - 1, y + h - 1)
        self.gfx_vline(x, y + 1, y + h - 2)
        self.gfx_vline(x + w - 1, y + 1, y + h - 2)

    def gfx_fill(self, x, y, w, h):
        x1 = min(x + w, self.w)
        y1 = min(y + h, self._clip)
        for yy in range(max(0, y), y1):
            base = yy * self.w
            for xx in range(max(0, x), x1):
                self.fb[base + xx] = 1

    # ---- text -------------------------------------------------------
    def gfx_char(self, x, y, c):
        g = _glyph_of(c) * 5
        for r in range(5):
            row = _FONT[g + r]
            for b in range(3):
                if (row >> (2 - b)) & 1:
                    self.gfx_px(x + b, y + r)

    def gfx_text(self, x, y, s):
        for c in s:
            self.gfx_char(x, y, c)
            x += 4

    def gfx_num(self, x, y, v):
        if v < 0:
            v = 0
        if v > 9999:
            v = 9999
        self.gfx_text(x, y, str(v))

    # ---- hearts -----------------------------------------------------
    def gfx_heart(self, x, y, mode):
        if mode == 0:
            return
        if mode == 2:
            self.gfx_px(x + 1, y)
            self.gfx_px(x + 3, y)
            self.gfx_hline(x, x + 4, y + 1)
            self.gfx_hline(x, x + 4, y + 2)
            self.gfx_hline(x + 1, x + 3, y + 3)
            self.gfx_px(x + 2, y + 4)
        else:
            self.gfx_px(x + 1, y)
            self.gfx_hline(x, x + 2, y + 1)
            self.gfx_hline(x, x + 2, y + 2)
            self.gfx_px(x + 1, y + 3)
            self.gfx_px(x + 2, y + 4)

    # ---- block speckle ----------------------------------------------
    def gfx_speckle(self, id, x, y, time, smelt):
        if id == B_GRASS:
            self.gfx_px(x + 1, y + 1); self.gfx_px(x + 4, y + 1)
            self.gfx_px(x + 2, y + 4); self.gfx_px(x + 5, y + 5)
        elif id == B_DIRT:
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 3); self.gfx_px(x + 1, y + 5); self.gfx_px(x + 6, y + 6)
        elif id == B_STONE:
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 4); self.gfx_px(x + 3, y + 6)
        elif id == B_COBBLE:
            self.gfx_px(x + 1, y + 1); self.gfx_px(x + 4, y + 2); self.gfx_px(x + 6, y + 1)
            self.gfx_px(x + 2, y + 4); self.gfx_px(x + 5, y + 5)
        elif id == B_BEDROCK:
            self.gfx_px(x, y); self.gfx_px(x + 3, y + 1); self.gfx_px(x + 6, y); self.gfx_px(x + 1, y + 3)
            self.gfx_px(x + 5, y + 3); self.gfx_px(x + 2, y + 5); self.gfx_px(x + 7, y + 4); self.gfx_px(x + 4, y + 6)
        elif id == B_LOG:
            self.gfx_vline(x + 2, y + 1, y + 6)
            self.gfx_vline(x + 5, y + 1, y + 6)
        elif id == B_LEAVES:
            self.gfx_px(x + 1, y + 1); self.gfx_px(x + 4, y + 2); self.gfx_px(x + 6, y + 1)
            self.gfx_px(x + 2, y + 4); self.gfx_px(x + 5, y + 5); self.gfx_px(x + 3, y + 6)
        elif id == B_PLANKS:
            self.gfx_px(x, y + 2); self.gfx_px(x + 2, y + 2); self.gfx_px(x + 4, y + 2); self.gfx_px(x + 6, y + 2)
            self.gfx_px(x, y + 5); self.gfx_px(x + 2, y + 5); self.gfx_px(x + 4, y + 5); self.gfx_px(x + 6, y + 5)
        elif id == B_CRAFT:
            self.gfx_rect(x + 2, y + 2, 4, 4)
        elif id == B_FURNACE:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_hline(x + 2, x + 5, y + 4)
            if smelt:
                self.gfx_px(x + 3, y + 5 + ((time >> 2) & 1))
        elif id == B_SAND:
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 6, y + 3); self.gfx_px(x + 3, y + 5); self.gfx_px(x + 5, y + 6)
        elif id == B_GRAVEL:
            self.gfx_px(x + 1, y + 2); self.gfx_px(x + 4, y + 1); self.gfx_px(x + 6, y + 4)
            self.gfx_px(x + 2, y + 5); self.gfx_px(x + 5, y + 6)
        elif id == B_COAL:
            self.gfx_px(x + 3, y + 3); self.gfx_px(x + 4, y + 3); self.gfx_px(x + 3, y + 4)
            self.gfx_px(x + 4, y + 4); self.gfx_px(x + 5, y + 2)
        elif id == B_IRON:
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 3); self.gfx_px(x + 3, y + 5); self.gfx_px(x + 5, y + 5)
        elif id == B_DIAMOND:
            self.gfx_px(x + 4, y + 2); self.gfx_px(x + 3, y + 3); self.gfx_px(x + 5, y + 3); self.gfx_px(x + 4, y + 4)
        elif id == B_OBSIDIAN:
            self.gfx_px(x + 1, y + 1); self.gfx_px(x + 2, y + 2); self.gfx_px(x + 3, y + 3)
            self.gfx_px(x + 4, y + 4); self.gfx_px(x + 5, y + 5); self.gfx_px(x + 6, y + 6)
            self.gfx_px(x + 6, y + 1); self.gfx_px(x + 5, y + 2); self.gfx_px(x + 2, y + 5); self.gfx_px(x + 1, y + 6)
        elif id == B_NETHERRACK:
            self.gfx_px(x + 1, y + 2); self.gfx_px(x + 4, y + 1); self.gfx_px(x + 6, y + 4)
            self.gfx_px(x + 2, y + 5); self.gfx_px(x + 5, y + 6)
        elif id in (B_NBRICK, B_SBRICK):
            self.gfx_px(x, y + 3); self.gfx_px(x + 2, y + 3); self.gfx_px(x + 4, y + 3); self.gfx_px(x + 6, y + 3)
            self.gfx_vline(x + 4, y, y + 2)
            self.gfx_vline(x + 2, y + 4, y + 7)
        elif id == B_SPAWNER:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 2); self.gfx_px(x + 3, y + 4)
            self.gfx_px(x + 4, y + 4); self.gfx_px(x + 2, y + 5); self.gfx_px(x + 5, y + 5)
        elif id == B_ENDSTONE:
            self.gfx_px(x + 2, y + 1); self.gfx_px(x + 5, y + 2); self.gfx_px(x + 1, y + 4)
            self.gfx_px(x + 4, y + 4); self.gfx_px(x + 6, y + 6); self.gfx_px(x + 3, y + 6)
        elif id == B_FRAME:
            self.gfx_rect(x + 1, y + 2, 6, 5)
            self.gfx_px(x + 3, y + 4); self.gfx_px(x + 4, y + 4)
        elif id == B_FRAME_EYE:
            self.gfx_rect(x + 1, y + 2, 6, 5)
            self.gfx_px(x + 3, y + 3); self.gfx_px(x + 4, y + 3)
            self.gfx_px(x + 3, y + 4); self.gfx_px(x + 4, y + 4)
            self.gfx_px(x + 2, y + 4); self.gfx_px(x + 5, y + 4)

    # ---- fluid ------------------------------------------------------
    def gfx_fluid(self, id, x, y, surf, time):
        a = (time >> 2) & 3
        if id == B_WATER:
            if surf:
                self.gfx_px(x + ((a + 0) & 7), y + 1)
                self.gfx_px(x + ((a + 1) & 7), y + 1)
                self.gfx_px(x + ((a + 4) & 7), y + 1)
                self.gfx_px(x + ((a + 5) & 7), y + 1)
            self.gfx_px(x + 1, y + 4); self.gfx_px(x + 4, y + 5); self.gfx_px(x + 6, y + 3)
        elif id == B_LAVA:
            if surf:
                self.gfx_hline(x + ((a + 0) & 7) - 1, x + ((a + 0) & 7), y + 1)
                self.gfx_hline(x + ((a + 4) & 7) - 1, x + ((a + 4) & 7), y + 1)
            self.gfx_px(x + 1, y + 3); self.gfx_px(x + 5, y + 2); self.gfx_px(x + 3, y + 5); self.gfx_px(x + 6, y + 6)
        elif id == B_PORTAL:
            self.gfx_px(x + 2, y + ((a + 0) & 7))
            self.gfx_px(x + 5, y + ((a + 4) & 7))
            self.gfx_px(x + 3, y + ((a + 6) & 7))
        elif id == B_ENDPORTAL:
            if (time & 7) < 5:
                self.gfx_px(x + 2, y + 2)
            if (time & 7) < 3:
                self.gfx_px(x + 5, y + 5)
            self.gfx_px(x + 4, y + 3)

    # ---- item / block icons ----------------------------------------
    def gfx_icon(self, id, x, y, clip):
        old = self._clip
        self._clip = clip
        if id == B_GRASS:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 2, y); self.gfx_px(x + 4, y); self.gfx_px(x + 6, y)
            self.gfx_px(x + 3, y + 4); self.gfx_px(x + 5, y + 5)
        elif id == B_DIRT:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 3, y + 3); self.gfx_px(x + 5, y + 4); self.gfx_px(x + 2, y + 5)
        elif id == B_STONE:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 3, y + 3); self.gfx_px(x + 5, y + 3); self.gfx_px(x + 4, y + 5)
        elif id == B_COBBLE:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 2); self.gfx_px(x + 3, y + 4); self.gfx_px(x + 5, y + 5)
        elif id == B_LOG:
            self.gfx_vline(x + 2, y + 1, y + 6); self.gfx_vline(x + 5, y + 1, y + 6)
            self.gfx_hline(x + 2, x + 5, y); self.gfx_hline(x + 2, x + 5, y + 7)
        elif id == B_LEAVES:
            self.gfx_px(x + 1, y + 1); self.gfx_px(x + 4, y + 2); self.gfx_px(x + 6, y + 1)
            self.gfx_px(x + 2, y + 4); self.gfx_px(x + 5, y + 5); self.gfx_px(x + 3, y + 6)
        elif id == B_PLANKS:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 2, y + 3); self.gfx_px(x + 4, y + 3); self.gfx_px(x + 6, y + 3)
            self.gfx_px(x + 1, y + 5); self.gfx_px(x + 3, y + 5); self.gfx_px(x + 5, y + 5)
        elif id == B_CRAFT:
            self.gfx_rect(x + 1, y + 2, 6, 5)
            self.gfx_hline(x + 1, x + 6, y + 1)
            self.gfx_px(x + 3, y + 4); self.gfx_px(x + 4, y + 4); self.gfx_px(x + 2, y + 5); self.gfx_px(x + 5, y + 5)
        elif id == B_FURNACE:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_hline(x + 3, x + 5, y + 4)
            self.gfx_px(x + 3, y + 5); self.gfx_px(x + 5, y + 5)
        elif id == B_SAND:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 3); self.gfx_px(x + 3, y + 5)
        elif id == B_GRAVEL:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 2); self.gfx_px(x + 3, y + 4)
            self.gfx_px(x + 2, y + 5); self.gfx_px(x + 5, y + 5)
        elif id == B_COAL:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 3, y + 3); self.gfx_px(x + 4, y + 3); self.gfx_px(x + 3, y + 4); self.gfx_px(x + 4, y + 4)
        elif id == B_IRON:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 3); self.gfx_px(x + 3, y + 5); self.gfx_px(x + 5, y + 5)
        elif id == B_DIAMOND:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 4, y + 2); self.gfx_px(x + 3, y + 3); self.gfx_px(x + 5, y + 3); self.gfx_px(x + 4, y + 4)
        elif id == B_OBSIDIAN:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 3, y + 3); self.gfx_px(x + 4, y + 4); self.gfx_px(x + 5, y + 5)
            self.gfx_px(x + 5, y + 2); self.gfx_px(x + 2, y + 5)
        elif id == B_NETHERRACK:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 3); self.gfx_px(x + 3, y + 5)
        elif id in (B_NBRICK, B_SBRICK):
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_hline(x + 1, x + 6, y + 4)
            self.gfx_vline(x + 3, y + 1, y + 3); self.gfx_vline(x + 5, y + 5, y + 6)
        elif id == B_ENDSTONE:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 2); self.gfx_px(x + 3, y + 4); self.gfx_px(x + 5, y + 5)
        elif id == B_BEDROCK:
            self.gfx_rect(x + 1, y + 1, 6, 6)
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 2); self.gfx_px(x + 3, y + 3)
            self.gfx_px(x + 2, y + 5); self.gfx_px(x + 5, y + 5); self.gfx_px(x + 4, y + 6)
        elif id == I_STICK:
            self.gfx_px(x + 2, y + 6); self.gfx_px(x + 3, y + 5); self.gfx_px(x + 4, y + 4)
            self.gfx_px(x + 5, y + 3); self.gfx_px(x + 6, y + 2)
        elif id == I_COAL:
            self.gfx_px(x + 3, y + 3); self.gfx_px(x + 4, y + 3); self.gfx_px(x + 3, y + 4)
            self.gfx_px(x + 4, y + 4); self.gfx_px(x + 5, y + 5)
        elif id == I_FLINT:
            self.gfx_px(x + 4, y + 2); self.gfx_px(x + 3, y + 3); self.gfx_px(x + 4, y + 3); self.gfx_px(x + 5, y + 3)
            self.gfx_px(x + 2, y + 4); self.gfx_px(x + 3, y + 4); self.gfx_px(x + 4, y + 4)
            self.gfx_px(x + 3, y + 5)
        elif id == I_IRONING:
            self.gfx_rect(x + 1, y + 3, 6, 3)
            self.gfx_px(x + 3, y + 4); self.gfx_px(x + 4, y + 4)
        elif id == I_DIAMOND:
            self.gfx_px(x + 3, y + 1); self.gfx_px(x + 4, y + 1)
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 2)
            self.gfx_px(x + 1, y + 3); self.gfx_px(x + 6, y + 3)
            self.gfx_px(x + 2, y + 4); self.gfx_px(x + 5, y + 4)
            self.gfx_px(x + 3, y + 5); self.gfx_px(x + 4, y + 5)
        elif id == I_BUCKET:
            self.gfx_hline(x + 2, x + 5, y + 2)
            self.gfx_vline(x + 2, y + 3, y + 6); self.gfx_vline(x + 5, y + 3, y + 6)
            self.gfx_hline(x + 2, x + 5, y + 6)
            self.gfx_px(x + 3, y + 1); self.gfx_px(x + 4, y + 1)
        elif id == I_BUCKET_W:
            self.gfx_icon(I_BUCKET, x, y, clip)
            self.gfx_px(x + 3, y + 4); self.gfx_px(x + 4, y + 5)
        elif id == I_BUCKET_L:
            self.gfx_icon(I_BUCKET, x, y, clip)
            self.gfx_px(x + 3, y + 4); self.gfx_px(x + 4, y + 4); self.gfx_px(x + 3, y + 5); self.gfx_px(x + 4, y + 5)
        elif id == I_FSTEEL:
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 2, y + 3); self.gfx_px(x + 2, y + 4)
            self.gfx_px(x + 3, y + 5); self.gfx_px(x + 4, y + 5); self.gfx_px(x + 4, y + 4)
            self.gfx_px(x + 6, y + 2); self.gfx_px(x + 5, y + 1)
        elif id == I_ROD:
            self.gfx_vline(x + 4, y + 2, y + 6)
            self.gfx_px(x + 3, y + 1); self.gfx_px(x + 5, y + 1); self.gfx_px(x + 4, y)
        elif id == I_POWDER:
            self.gfx_px(x + 2, y + 5); self.gfx_px(x + 4, y + 5); self.gfx_px(x + 6, y + 5)
            self.gfx_px(x + 3, y + 6); self.gfx_px(x + 5, y + 6); self.gfx_px(x + 3, y + 3)
        elif id == I_PEARL:
            self.gfx_px(x + 3, y + 1); self.gfx_px(x + 4, y + 1)
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 2)
            self.gfx_vline(x + 1, y + 3, y + 4); self.gfx_vline(x + 6, y + 3, y + 4)
            self.gfx_px(x + 2, y + 5); self.gfx_px(x + 5, y + 5)
            self.gfx_px(x + 3, y + 6); self.gfx_px(x + 4, y + 6)
            self.gfx_px(x + 4, y + 4)
        elif id == I_EYE:
            self.gfx_px(x + 3, y + 1); self.gfx_px(x + 4, y + 1)
            self.gfx_px(x + 2, y + 2); self.gfx_px(x + 5, y + 2)
            self.gfx_vline(x + 1, y + 3, y + 4); self.gfx_vline(x + 6, y + 3, y + 4)
            self.gfx_px(x + 2, y + 5); self.gfx_px(x + 5, y + 5)
            self.gfx_px(x + 3, y + 6); self.gfx_px(x + 4, y + 6)
            self.gfx_px(x + 3, y + 3); self.gfx_px(x + 4, y + 3); self.gfx_px(x + 3, y + 4); self.gfx_px(x + 4, y + 4)
        elif I_WPICK <= id <= I_DSWORD:
            t = _tool_kind(id)
            if t == 0:  # pickaxe
                self.gfx_px(x + 1, y + 3); self.gfx_px(x + 2, y + 2); self.gfx_px(x + 3, y + 1)
                self.gfx_px(x + 4, y + 1); self.gfx_px(x + 5, y + 1); self.gfx_px(x + 6, y + 2)
                self.gfx_px(x + 6, y + 3)
                self.gfx_px(x + 3, y + 3); self.gfx_px(x + 3, y + 4); self.gfx_px(x + 2, y + 5)
                self.gfx_px(x + 1, y + 6)
            elif t == 1:  # axe
                self.gfx_px(x + 2, y + 1); self.gfx_px(x + 3, y + 1); self.gfx_px(x + 4, y + 1)
                self.gfx_px(x + 2, y + 2); self.gfx_px(x + 3, y + 2); self.gfx_px(x + 4, y + 2)
                self.gfx_px(x + 2, y + 3); self.gfx_px(x + 3, y + 3)
                self.gfx_px(x + 4, y + 4); self.gfx_px(x + 3, y + 5); self.gfx_px(x + 2, y + 6)
            else:  # sword
                self.gfx_px(x + 6, y + 1); self.gfx_px(x + 5, y + 2); self.gfx_px(x + 4, y + 3)
                self.gfx_px(x + 3, y + 4)
                self.gfx_px(x + 2, y + 3); self.gfx_px(x + 4, y + 5)
                self.gfx_px(x + 1, y + 5); self.gfx_px(x + 2, y + 6); self.gfx_px(x + 1, y + 6)
            tier = _tool_tier(id)
            for i in range(tier):
                self.gfx_px(x + 4 + i, y + 7)
        self._clip = old

    # ---- world tile renderer (offload of render.c draw_world) -------
    # Reads tiles straight from guest memory (dmem is the emulator's
    # PyMEM; wmap is the base address of the current world buffer).
    def gfx_world(self, dmem, wmap, cam_x, cam_y, time, smelt):
        W, H = 256, 80
        VTX, VTY = 24, 21
        SOLID = _SOLID
        fluid_ids = (B_WATER, B_LAVA, B_PORTAL, B_ENDPORTAL)
        ore_ids = (B_COAL, B_IRON, B_DIAMOND, B_GRAVEL)
        for sy in range(VTY):
            wy = cam_y + sy
            if wy >= H:
                continue
            y0 = sy << 3
            row = wmap + (wy << 8)
            urow = row - W if (sy > 0 and wy > 0) else None
            drow = row + W if wy < H - 1 else None
            for sx in range(VTX):
                wx = cam_x + sx
                idx = row + wx
                bid = dmem[idx]
                if bid == B_AIR:
                    continue
                x0 = sx << 3
                up = dmem[urow + wx] if urow is not None else B_AIR
                if bid in fluid_ids:
                    self.gfx_fluid(bid, x0, y0, 1 if up != bid else 0, time)
                    continue
                dn = dmem[drow + wx] if drow is not None else B_AIR
                lf = dmem[row + wx - 1] if wx > 0 else B_AIR
                rt = dmem[row + wx + 1] if wx < W - 1 else B_AIR
                edge = 0
                if not SOLID[up]:
                    self.gfx_hline(x0, x0 + 7, y0)
                    edge = 1
                if not SOLID[dn]:
                    self.gfx_hline(x0, x0 + 7, y0 + 7)
                    edge = 1
                if not SOLID[lf]:
                    self.gfx_vline(x0, y0 + 1, y0 + 6)
                    edge = 1
                if not SOLID[rt]:
                    self.gfx_vline(x0 + 7, y0 + 1, y0 + 6)
                    edge = 1
                if edge or bid in ore_ids:
                    self.gfx_speckle(bid, x0, y0, time, smelt)
