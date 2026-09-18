# Notes — item randomizer

Notes from a read-only session: no code written, nothing verified at runtime.
PAL (RMCP) addresses, taken from the comments in the `GameSource/` headers.

**The idea:** you get a bullet, the icon says bullet, you press the button, and you drop a banana.
A deliberate mismatch between **icon** and **effect**.

---

## The piece that matters

`GameSource/MarioKartWii/Item/ItemBehaviour.hpp:26`

```cpp
struct Behavior {
    static Behavior behaviourTable[19];   //809c36a0, index item id
    u8  unknkown_0x0;
    u8  unknkown_0x1;
    u8  padding[2];
    ItemObjId objId;                      //0x4
    u32 numberOfItems;                    //0x8
    u32 unknown_0xc;
    u8  unknown_0x10;
    u8  padding2[3];
    UseType useType;                      //0x14
    void (*useFunction)(Player& player);  //0x18  <-- null = the item is draggable
}; // Total size 0x1c
```

Also in `GameSource/symbols.txt:1726`:

```
behaviourTable__Q24Item8Behavior = 0x809c36a0
```

**An array of 19 rows indexed by `ItemId`, with a function pointer inside each.**
There aren't 19 scattered roads: the game has one, and it goes through here.

`0x809c36a0` sits above `809BD6E8` -> the data range. **It's a variable, not code.** You can write
to it.

`ItemId` lives in `GameSource/MarioKartWii/System/Identifiers.hpp:115` (0x0 green shell … 0xF bullet
… 0x12 triple banana, 0x14 none).

---

## Why not do it by hand with a `switch`

Because for half the items **there is no function to call.** The game has two families:

| Family | How it works | Examples |
|---|---|---|
| self-buff | there's a function | mushroom, bullet, star, mega, TC, lightning, POW, blooper |
| objects in the world | spawned from the `ObjHolder`, dragged or thrown; `useFunction` is **null** | banana, shells, FIB, bob-omb |

Everything that exists, verified:

```
Kart::Movement          ActivateMushroom 8057f3d8 · ActivateTc 80581a28 ·
                        ActivateBullet 805858ac · ActivateZipperBoost 8057f96c ·
                        ActivateStar (virtual, vtable 0x18, 80580268) ·
                        ActivateMega (virtual, vtable 0x1c, 80580b14)

Item::Player            UseBlooper 807a81b4 · UsePow 807b1b2c · UseBullet 807a9afc ·
                        UseMushroom 807a9d3c · UseMegaMushroom 807a9e50 ·
                        UseTC 807af1bc · UseStar 807b706c · UseThunder 807b7b7c
```

There's no `ActivateBanana`. There's no `UseGreenShell`. **They don't exist**, and the banana is
precisely the example the whole idea starts from.

> The table, on the other hand, already has that distinction inside it — in `useType` and in the null
> `useFunction`. Move the `0x1c`-byte row and you bring the *how it behaves* along with it.

---

## Variant A — one permutation per race

At race load, shuffle the 19 rows. The map stays **fixed** for the whole race: bullet -> banana,
banana -> mega, and so on. Get two bullets in the same race, you drop two bananas. Next race, new
map.

There are **no probabilities** here: it's a permutation, not a lottery. There's a map to discover by
playing, which is more interesting than pure randomness — after three races pure randomness is just
noise.

- Cost: no debugger. The table is already located.
- **`RaceLoadHook`, not `BootHook`**: the header has `void InitAllBehavior();`, so the table gets
  populated at some point. Write before that and it gets overwritten. (Same problem as day 3, in
  another shape.)
- Move **whole rows**, not just `useFunction`: otherwise you cross the two families and get
  inconsistent rows (a trailing `useType` with a "just use it" function).

**First test, before the real permutation — the absurd value:** point *every* row's `useFunction` at
the same one, something you'll recognise instantly. If every item does the same thing, the table is
live and you're in. If nothing changes, you aren't writing where you think, and that's a different
problem.

---

## Variant B — a roll per use

Bullet -> banana now, bullet -> star two minutes later, in the same race. No map to learn.

**This can't be done with the table.** The table is read *at the moment of use*, so to change it per
use you have to intervene there. That means finding where the game reads `behaviourTable[id]`:
breakpoint, read `lr`, `lr − 4`. **Not done.**

Weights go on top of B.

---

## "So that even I don't know the probabilities"

Strictly speaking, no: I write the generator, so the distribution is a fact about my code. Uniform
means I know it's 1/19.

What's actually achievable is one step below, and that's enough: at `RaceLoadHook` don't draw the
item, draw **the weights**. 19 numbers from the RNG, normalised, and that's the race's distribution.
I know the weights are random; I don't know that *in this race* the bullet gives a banana 80% of the
time.

RNG available — `MarioKartWii/System/Random.hpp`:

```
Random::NextLimited(int limit)        805555cc
DriverManager::GetRaceinfoRandom()    807bd718
```

> **The debug `OS::Report` has to come out.** It's needed while developing, but the day it stays in,
> you open the log and you know the table: the feature is dead. Deleting that log is the final step
> of the work, not the tidy-up. Worth a comment in the file, or in two months you'll put it back to
> debug something else.

**Online:** if the weights aren't seeded identically on every client, everyone plays a different
game. (`PulsarEngine/Extensions/LECODE/XPF.cpp:43` does `Random random(seed)` with an explicit seed
for exactly this reason.)

---

## Dead ends already ruled out

**`ItemSlotData` probabilities** (`Item/ItemSlot.hpp`) — changes **what comes out of the box**, not
what happens when you use it. Roulette, HUD and effect all stay consistent with each other. It's a
different feature, not this one.

**`RandomizeRouletteItem` `807baed4`** — that's the spinning-wheel animation. The struct says
`//visual only`. And more importantly it's **inlined**: the body is copied into the callers, so
there's no `bl` to hijack, there are N possibly-different copies, and changing it would mean finding
them all. Useful for the opposite effect (a wheel that bluffs), and low risk since it's declared
visual only.

---

## Other useful addresses picked up along the way

```
Item::PlayerObj::UseItem(bool)              80791910
Item::PlayerObj::PrepareHandlers[5]         808d18d0   (one per useType)
Item::PlayerObj::UpdateHandlers[5]          808d1900
Item::PlayerInventory::SetItem              807bc940   (currentItemId at 0x4)
Item::ItemSlotData::DecideItem              807bb42c   <- the real verdict
Item::ItemSlotData::DecideRouletteItem      807bb8d0   <- the sequence displayed
Item::Manager::itemObjHolders[0xF]          (offset 0x48, one per objId)
```

---

## Next time

1. Variant A, with the "all the same" test first
2. If it works: the real permutation with `Random`
3. Only if I want B: breakpoint to find who reads `behaviourTable[id]`
