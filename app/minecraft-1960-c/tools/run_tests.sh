#!/bin/sh
# Headless verification suite for Minecraft 1960.
# Drives the game through every progression stage with scripted keys
# (mctest build, one key per tick) and checks the final state report.
cd "$(dirname "$0")/.." || exit 1
make mctest >/dev/null 2>&1 || exit 1
pass=0; fail=0

rep() { # rep STRING COUNT
  i=0
  while [ "$i" -lt "$2" ]; do printf '%s' "$1"; i=$((i+1)); done
}

run() { # run NAME SCRIPT WANT
  name=$1; script=$(printf '%s' "$2" | tr -d ' '); want=$3
  out=$(printf '%s' "$script" > /tmp/mc1960_t.txt && \
        MC_SCRIPT=/tmp/mc1960_t.txt ./mctest 2>/dev/null)
  if printf '%s' "$out" | grep -q "$want"; then
    echo "PASS $name"; pass=$((pass+1))
  else
    echo "FAIL $name"; printf '%s\n' "$out" | sed 's/^/    /' | head -12
    fail=$((fail+1))
  fi
}

# --- early game: chop two logs, craft planks/sticks/table, place it
run "early-craft" \
  "$(rep a 22)$(rep z 8)$(rep a 24)$(rep z 8)aa c z sz sz x 3 x" \
  "stick"

# --- stronghold: smelt iron, place six eyes, fall into the End
# (step away from the furnace first: X interacts with it by priority)
run "stronghold-smelt-eyes-end" \
  "ght9xc$(rep z 14)x$(rep a 8)5$(rep x 4)$(rep d 4)x$(rep d 4)xwd" \
  "dim=2"

# --- build + light a nether portal, walk through
run "portal-to-nether" \
  "gh$(rep . 8)p$(rep d 16)" \
  "dim=1"

# --- nether: water on lava -> obsidian; kill blazes for rods
run "nether-obsidian-rods" \
  "ghttv4x2$(rep 'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzwd' 6)" \
  "rod"

# --- the End: slay the dragon, win the game
run "dragon-win" \
  "ghm2ttt$(rep z 2400)" \
  "won=1"

# --- drop key: one obsidian leaves the slot, then is re-collected
printf 'g8q' > /tmp/mc1960_t.txt
if MC_IDLE=5 MC_SCRIPT=/tmp/mc1960_t.txt ./mctest 2>/dev/null | grep -q 'obsidian x19'; then
  printf 'g8q%s........' "$(rep d 14)" > /tmp/mc1960_t.txt
  if MC_SCRIPT=/tmp/mc1960_t.txt ./mctest 2>/dev/null | grep -q 'obsidian x20'; then
    echo "PASS drop-key"; pass=$((pass+1))
  else
    echo "FAIL drop-key (re-collect)"; fail=$((fail+1))
  fi
else
  echo "FAIL drop-key (drop)"; fail=$((fail+1))
fi

# --- natural regen: hp recovers after damage (lava burn, then wait)
base="g$(rep . 8)v$(rep d 10)$(rep a 6)"
printf '%s%s' "$base" "$(rep . 16)" > /tmp/mc1960_t.txt
hpa=$(MC_SCRIPT=/tmp/mc1960_t.txt ./mctest 2>/dev/null | sed -n 's/^REPORT .* hp=\([0-9]*\) .*/\1/p')
printf '%s%s' "$base" "$(rep . 56)" > /tmp/mc1960_t.txt
hpb=$(MC_SCRIPT=/tmp/mc1960_t.txt ./mctest 2>/dev/null | sed -n 's/^REPORT .* hp=\([0-9]*\) .*/\1/p')
if [ -n "$hpa" ] && [ "$hpa" -lt 20 ] && [ "$hpb" -gt "$hpa" ]; then
  echo "PASS natural-regen"; pass=$((pass+1))
else
  echo "FAIL natural-regen (hpa=$hpa hpb=$hpb)"; fail=$((fail+1))
fi

# --- aim mode: place a step at the cursor, then hop onto it (y drops)
printf 'g..............................' > /tmp/mc1960_t.txt
y0=$(MC_SCRIPT=/tmp/mc1960_t.txt ./mctest 2>/dev/null | sed -n 's/^pos=[0-9]*,//p')
printf 'g........f8xfWWWW......' > /tmp/mc1960_t.txt
out=$(MC_SCRIPT=/tmp/mc1960_t.txt ./mctest 2>/dev/null)
y1=$(printf '%s' "$out" | sed -n 's/^pos=[0-9]*,//p')
if printf '%s' "$out" | grep -q 'obsidian x19' && [ -n "$y1" ] && [ "$y1" -lt "$y0" ]; then
  echo "PASS aim-place-and-hop"; pass=$((pass+1))
else
  echo "FAIL aim-place-and-hop (y0=$y0 y1=$y1)"; fail=$((fail+1))
fi

# --- creative mode: B toggles, double-tap W flies up (no gravity)
y0=$(printf 'g%s' "$(rep . 14)" > /tmp/mc1960_t.txt && \
     MC_SCRIPT=/tmp/mc1960_t.txt ./mctest 2>/dev/null | sed -n 's/^pos=[0-9]*,//p')
out=$(printf 'g%sb.w.wwwwww%s' "$(rep . 4)" "$(rep . 8)" > /tmp/mc1960_t.txt && \
      MC_SCRIPT=/tmp/mc1960_t.txt ./mctest 2>/dev/null)
y1=$(printf '%s' "$out" | sed -n 's/^pos=[0-9]*,//p')
if printf '%s' "$out" | grep -q 'flying=1' && [ -n "$y1" ] && [ "$y1" -lt "$y0" ]; then
  echo "PASS creative-flight"; pass=$((pass+1))
else
  echo "FAIL creative-flight (y0=$y0 y1=$y1)"; fail=$((fail+1))
fi

rm -f /tmp/mc1960_t.txt
echo "== $pass passed, $fail failed"
[ "$fail" = 0 ]
