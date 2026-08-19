/* craft.c -- recipe table + crafting/smelt helpers.
 *
 * Recipes are icon-only in the UI (no text), which keeps menu frames
 * cheap enough for the 1-bit display budget.
 */
#include "mc.h"

const Recipe recipes[] = {
  /* 0 */ { B_PLANKS, 4, { B_LOG, 1, 0, 0, 0, 0 }, 0 },
  /* 1 */ { I_STICK,  4, { B_PLANKS, 2, 0, 0, 0, 0 }, 0 },
  /* 2 */ { B_CRAFT,  1, { B_PLANKS, 4, 0, 0, 0, 0 }, 0 },
  /* 3 */ { I_WPICK,  1, { B_PLANKS, 3, I_STICK, 2, 0, 0 }, 1 },
  /* 4 */ { I_WAXE,   1, { B_PLANKS, 3, I_STICK, 2, 0, 0 }, 1 },
  /* 5 */ { I_WSWORD, 1, { B_PLANKS, 2, I_STICK, 1, 0, 0 }, 1 },
  /* 6 */ { B_FURNACE,1, { B_COBBLE, 8, 0, 0, 0, 0 }, 1 },
  /* 7 */ { I_SPICK,  1, { B_COBBLE, 3, I_STICK, 2, 0, 0 }, 1 },
  /* 8 */ { I_SAXE,   1, { B_COBBLE, 3, I_STICK, 2, 0, 0 }, 1 },
  /* 9 */ { I_SSWORD, 1, { B_COBBLE, 2, I_STICK, 1, 0, 0 }, 1 },
  /*10 */ { I_IPICK,  1, { I_IRONING, 3, I_STICK, 2, 0, 0 }, 1 },
  /*11 */ { I_IAXE,   1, { I_IRONING, 3, I_STICK, 2, 0, 0 }, 1 },
  /*12 */ { I_ISWORD, 1, { I_IRONING, 2, I_STICK, 1, 0, 0 }, 1 },
  /*13 */ { I_DPICK,  1, { I_DIAMOND, 3, I_STICK, 2, 0, 0 }, 1 },
  /*14 */ { I_DAXE,   1, { I_DIAMOND, 3, I_STICK, 2, 0, 0 }, 1 },
  /*15 */ { I_DSWORD, 1, { I_DIAMOND, 2, I_STICK, 1, 0, 0 }, 1 },
  /*16 */ { I_BUCKET, 1, { I_IRONING, 3, 0, 0, 0, 0 }, 1 },
  /*17 */ { I_FSTEEL, 1, { I_IRONING, 1, I_FLINT, 1, 0, 0 }, 0 },
  /*18 */ { I_POWDER, 2, { I_ROD, 1, 0, 0, 0, 0 }, 0 },
  /*19 */ { I_EYE,    1, { I_POWDER, 1, I_PEARL, 1, 0, 0 }, 0 },
};
const u8 recipe_count = sizeof(recipes) / sizeof(recipes[0]);

u8 can_craft(u8 r)
{
  const Recipe *rc = &recipes[r];
  const u8 *p = rc->need;
  if (rc->table && !near_table()) return 0;
  while (p[0]) {
    if (inv_count(p[0]) < p[1]) return 0;
    p += 2;
  }
  return 1;
}

void do_craft(u8 r)
{
  const Recipe *rc = &recipes[r];
  const u8 *p = rc->need;
  if (!can_craft(r)) return;
  while (p[0]) { inv_remove(p[0], p[1]); p += 2; }
  inv_add(rc->out, rc->n);
}

/* scan a small neighbourhood of the player for a block */
static u8 near_block(u8 id)
{
  int tx = (g.px + 8) >> 4, ty = (g.py + 14) >> 4;
  int dx, dy;
  for (dy = -3; dy <= 3; dy++)
    for (dx = -3; dx <= 3; dx++)
      if (get_t(tx + dx, ty + dy) == id) return 1;
  return 0;
}

u8 near_table(void)   { return near_block(B_CRAFT); }
u8 near_furnace(void) { return near_block(B_FURNACE); }

const char *item_name(u8 id)
{
  switch (id) {
    case B_GRASS: return "GRASS";       case B_DIRT: return "DIRT";
    case B_STONE: return "STONE";       case B_COBBLE: return "COBBLE";
    case B_LOG: return "LOG";           case B_LEAVES: return "LEAVES";
    case B_PLANKS: return "PLANKS";     case B_CRAFT: return "CRAFT TABLE";
    case B_FURNACE: return "FURNACE";   case B_SAND: return "SAND";
    case B_GRAVEL: return "GRAVEL";     case B_COAL: return "COAL ORE";
    case B_IRON: return "IRON ORE";     case B_DIAMOND: return "DIAMOND ORE";
    case B_OBSIDIAN: return "OBSIDIAN"; case B_NETHERRACK: return "NETHERRACK";
    case B_NBRICK: return "NETHER BRICK"; case B_ENDSTONE: return "END STONE";
    case B_SBRICK: return "STONE BRICK"; case B_FRAME: return "END FRAME";
    case I_STICK: return "STICK";       case I_COAL: return "COAL";
    case I_FLINT: return "FLINT";       case I_IRONING: return "IRON INGOT";
    case I_DIAMOND: return "DIAMOND";   case I_BUCKET: return "BUCKET";
    case I_BUCKET_W: return "WATER BUCKET";
    case I_BUCKET_L: return "LAVA BUCKET";
    case I_FSTEEL: return "FLINT+STEEL";
    case I_ROD: return "BLAZE ROD";     case I_POWDER: return "BLAZE POWDER";
    case I_PEARL: return "ENDER PEARL"; case I_EYE: return "EYE OF ENDER";
    case I_WPICK: return "WOOD PICKAXE";  case I_WAXE: return "WOOD AXE";
    case I_WSWORD: return "WOOD SWORD";   case I_SPICK: return "STONE PICKAXE";
    case I_SAXE: return "STONE AXE";      case I_SSWORD: return "STONE SWORD";
    case I_IPICK: return "IRON PICKAXE";  case I_IAXE: return "IRON AXE";
    case I_ISWORD: return "IRON SWORD";   case I_DPICK: return "DIAMOND PICKAXE";
    case I_DAXE: return "DIAMOND AXE";    case I_DSWORD: return "DIAMOND SWORD";
    default: return "";
  }
}
