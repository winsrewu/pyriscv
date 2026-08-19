# Minecraft 1960

A complete, **beatable** 2D Minecraft that runs on a 20 KIPS, 1960s-class
CPU, drawn on a 192×168 one-bit display whose only primitives are
*clear all* and *set a bit*.  No mouse, no heap, no floating point, no
division in hot loops — plain C89.

```
make        # builds ./mc (terminal), ./mctest (headless), ./mcx (X11 window)
./mcx       # play in a real window: crisp scaled 1-bit pixels
sh tools/run_tests.sh           # end-to-end verification of every stage
```

## Run on the pyriscv emulator

This tree can also be compiled for the bare-metal RISC-V emulator in
`../../src`.  The display contract (`display.h`) is satisfied by
`display_riscv.c`, which turns every screen call into an *ecall* handled
by the emulator's display device (`src/pydisplay.py`) — a 1-bit
framebuffer shown in a **pygame** window (crisp nearest-neighbour scaled
pixels, keyboard input).  Requires `pygame` (`pip install pygame`).

```
./build_riscv.sh                          # rv32im, bare-metal -> app.mem
python3 ../../src/pyriscv.py app.mem      # play in a pygame window
python3 ../../src/pyriscv.py app.mem --scale 6   # larger window
python3 ../../src/pyriscv.py app.mem --no-display   # headless (no window)
```

The emulated hardware clock ticks at 20 Hz (50 ms); the game's 250 ms
logic tick is 5 clock ticks.  Keys: WASD/arrows move, Z/J act, X/K use,
C/E craft, F aim, R high-fps, Q drop, B creative — same as the reference
driver.  A daemon thread pumps SDL events, so the window stays responsive
(move / close / keyboard) even while the emulator spends the first ~40 s
running one-time world generation.

The build uses `-O3 -flto` so `screen_set`/`screen_clear` inline across
translation units (the single load-bearing optimization for a software
display, per the notes below).

`./mcx` opens a 768×672 window (MC_SCALE=n to resize) showing the panel
phosphor-style — light pixels on black, optional pixel seams with
MC_GRID=1, MC_WINSHOT=1 + MC_PBM=prefix to capture what the server
composites.  `./mc` keeps the terminal fallback.

## The game is beatable — full progression

punch trees → planks → crafting table → wooden tools → stone →
furnace + coal →
iron tools → diamonds (deep) → diamond pickaxe → water on lava =
obsidian → flint & steel → **nether portal** → fortress blazes → rods →
enderman pearls → eyes of ender → **stronghold** portal → **the End** →
kill the **ender dragon** → win screen.

Verified end-to-end by `tools/run_tests.sh` (scripted keyboard, headless):

```
PASS early-craft                  logs -> planks/sticks/table, placed
PASS stronghold-smelt-eyes-end    smelting, 6 eyes, end portal, travel
PASS portal-to-nether             obsidian frame, flint&steel, travel
PASS nether-obsidian-rods         water+lava->obsidian, blaze rods
PASS dragon-win                   won=1
```

## Controls (reference driver)

| key | action |
|-----|--------|
| A/D or ←/→ | walk |
| W/↑/space | jump / swim up |
| S/↓ | descend |
| Z or J | attack / mine (hold) |
| X or K | interact (tables/furnaces win) / place / use held item / close |
| C or E | crafting (or furnace when near one) |
| F or Tab | **aim mode**: WASD moves a highlighted cursor; Z/X act on it |
| R | **high-fps mode**: render as fast as the display can show (~60), logic stays 4 Hz |
| Q | drop one of the held item |
| B | toggle **creative mode** |
| Esc | back (also closes the win screen) |
| 1-9,0 / [ ] | select hotbar slot |

Without aim mode, mining/placing auto-targets a small set of tiles in
front of you (no mouse needed).  With aim mode on, WASD steers a dashed
highlight box (±4 tiles) and Z/X mine/place exactly there — build
bridges, light portals, fill eyes precisely.  The held item's name and
the selected recipe's name are shown in text (3×5 pixel font).
Jumping clears a full block, so you can hop onto steps.

Health regenerates naturally: +1 heart every 5 s, starting 5 s after
the last damage (no food system).  Mobs, shots and lava deal half
damage (rounded down); falls hit 4× as hard (net double the old
values — a max-speed fall costs 3 hearts).

Creative mode (B): no damage, and double-tapping W toggles flight
(W/S move up/down).  The inventory stays survival — there is no
creative item menu.  B again returns to survival (flight stops).
A `CR`/`FLY` tag appears under the fps readout while active.

Ores (coal/iron/diamond) and gravel show their speckle pattern even
when fully buried in rock, so veins can be spotted from caves or while
digging.  A measured fps readout sits under the day counter; R toggles
high-fps rendering on modern hardware (the 20 KIPS budget assumes the
default 2 fps).

## Getting obsidian

1. smelt iron (furnace + coal), craft a **bucket** (3 iron ingots),
2. scoop water (X on a lake), pour it on lava (X) — lava turns to
   obsidian where the water touches it.  Overworld lava occurs as
   open-topped pools filling carved basins from y60 down (plus the
   stronghold moat and the nether lava sea),
3. mine it with a **diamond pickaxe** (only that tier breaks it),
4. build the doorway frame (10 obsidian: back column + lintel + floor
   around a 2×3 air gap) and light it with flint & steel.
The hint chain walks you through these steps in game.

Debug keys (also on the real target, harmless): `g` kit, `h` god,
`t` teleport cycle (spawn→stronghold→nether→end), `p` build+light a
portal ahead, `v` lava pool ahead, `m` mining off.

## Architecture — swapping in your native display

The game core **never** touches hardware.  `display.h` is the whole
contract:

```c
void screen_clear(void);        /* blank the 192x168 panel   */
void screen_set(int x,int y);   /* set one pixel             */
void display_init(void);        /* one-time setup            */
void input_poll(void);          /* refresh key state (once per tick) */
int  key(int k);                /* key held (K_xxx, mc.h)    */
int  kpressed(int k);           /* key pressed since last tick */
void display_sync(void);        /* present + wait for next 250 ms tick */
void display_present(void);     /* present now (high-fps loop) */
long display_ms(void);          /* monotonic milliseconds    */
void display_sleep_ms(int n);   /* sleep ~n ms               */
```

Replace `display_ref.c` (terminal) with your native implementation of
those functions and link `main.o game.o world.o craft.o render.o`
against it.  `display_test.c` is a second, headless implementation used
by the test suite — a worked example of a swap.

Timing: logic ticks at 4 Hz; a frame is rendered every 2nd tick
(2 fps).  On the target, `display_sync` is your timer interrupt /
vblank wait.

Environment variables of the reference driver: `MC_FAST`, `MC_PBM`
(frame snapshots), `MC_PBM_EVERY`, `MC_FRAMES`, `MC_SCALE`, `MC_NOTTY`.
`tools/pbm2png.py` converts snapshots to PNG for inspection.

## How 2 fps fits in 20 KIPS

Budget: 20 000 instr/s ÷ 2 frames/s = **10 000 instructions per frame**.
A naive full-screen redraw (32 256 px) is ~100× over budget, so the
renderer never fills: blocks are drawn as **outlines** (only faces
exposed to air) plus 2–6 identifying speckle pixels.  Measured worst
case (`mctest` reports `max_pixels_per_frame`):

| scene        | set-bit calls |
|--------------|---------------|
| overworld    | 2 598 |
| nether       | 3 440 |
| the End      | 3 228 |
| menus        | 1 561 |

With `screen_set` implemented as a tight native bit-set (1–2
instructions — e.g. one store-OR into video memory, or a hardware
bit-set), rendering costs ≈ 3.5K × 2 ≈ 7K instructions; the 4 Hz logic
(whole world sim per frame ≈ 2–3K) brings a worst frame to ≈ 9.5–10K —
2 fps holds.  If your `screen_set` is an expensive subroutine call,
inline it (macro) at port time; that is the single load-bearing
optimization.

Other 1960s-friendly techniques used throughout:

- fixed point only (16 units/tile, shifts for /16, /8, /2)
- world width 256 → tile index = `x + (y<<8)`, no multiply
- xorshift RNG (shifts + xors, no division)
- no heap: two static 256×80 world buffers (overworld persistent;
  buffer 2 is regenerated per nether/end entry), static entity arrays
- menus replace the world instead of overlaying it (cheap frames)
- no lighting simulation; day/night is a counter that gates spawning

## Memory footprint

- world buffers: 2 × 256 × 80 B = 40 KB
- code + tables + font + stack ≈ 15–20 KB
- entities/HUD state < 1 KB

≈ 60 KB total — PDP-6/CDC-class core memory, or a drum-backed PDP-8.

## World & simulation notes

- Overworld: heightmap terrain, lakes, caves (drunken walkers), coal /
  iron / diamond veins, gravel (flint), trees you can walk under, a
  buried stronghold with a 6-frame end portal over lava.
- Nether: netherrack caverns, lava sea, nether-brick fortress with a
  blaze spawner.  Regenerated each visit; your overworld portal is
  rebuilt at the matching coordinates.
- End: floating endstone island, obsidian pillars, diving dragon with
  a boss bar showing its health, exit portal appears on kill.
- Mobs: zombies (night/caves), endermen (roam day and night, 2 pearls,
  turn hostile when hit; a few live on the End island), blazes (2 rods),
  dragon.  Mobs deal no contact damage — only blaze/dragon shots hurt —
  and flash when hit.  Hearts, fall damage, lava, swimming,
  respawn-at-spawn keeping inventory.
- Portals are side-view doorways (back column + lintel + floor of
  obsidian, open front); 10 obsidian via water bucket on lava.

## Deliberate simplifications vs. real Minecraft

9+3-slot inventory only (12 hotbar), no hunger, no block physics except
falling sand/gravel, no torches/lighting, crafting is a recipe list
(not a grid), smelting is instant-per-item with a short hold, no item
durability, no chests, end portal needs 6 eyes, dragon is melee-only.

## Files

| file | purpose |
|------|---------|
| `mc.h` | constants, ids, shared state |
| `display.h` | the 7-function hardware contract |
| `world.c` | generation for all 3 dimensions, xorshift |
| `game.c` | 4 Hz simulation: physics, mining, combat, portals, mobs |
| `craft.c` | recipe table + craft/smelt helpers |
| `render.c` | 1-bit outline renderer, sprites, HUD, menus, 3×5 font |
| `main.c` | init + loop (tick 4 Hz, render 2 fps) |
| `display_ref.c` | reference POSIX driver (terminal video, pacing) |
| `display_x11.c` | window driver (Xlib, scaled 1-bit panel, MC_WINSHOT) |
| `display_test.c` | headless scripted driver + state report |
| `tools/run_tests.sh` | progression verification suite |
| `tools/pbm2png.py` | snapshot viewer helper |
