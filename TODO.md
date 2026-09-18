# TODO

What's left to do, in the order it makes sense to do it.
Every entry says **what I need to know first** and **what makes it hard**, so when I pick it back up
I don't have to rebuild the context.

Status: 1 and 4 done (item randomizer, as a setting). Research done where noted.

---

## Now

### 2. Control remapping

`Input::State::buttonActions` (`InputState.hpp:27`), a `u16` with five documented bits:
`0x1` accelerate · `0x2` brake · `0x4` use item · `0x8` hop/drift · `0x20` rear-view.

- **Needs:** bitwise operators (`&`, `|`, `<<`), which I've never written. **This is the exercise for
  learning them**
- **Difficulty:** low, and it's testable in Time Trial where the effect either happens or it doesn't
- **Known trap:** `GhostController` and `AIController` read the same struct

---

## Next

### 3. Full randomizer — character, vehicle, drift, track

**Research done, and it's easier than expected.** In `UI/Section/SectionParams.hpp` it's all sitting
together in the same struct:

```cpp
CourseId    vsTracks[32];   //0x78   offline
CharacterId characters[4];  //0x12c
KartId      karts[4];       //0x13c
u32         driftType[4];   //0x164
void RandomizeVSTracks();   //805e32ec   <- the game already has this
```

Three of the four things are **adjacent arrays of 4**, and for tracks there's already a
randomisation function in the game. No pointer tables, no dispatch: it's a matter of writing values
before the race starts.

Ranges: `CharacterId` 0x00–0x2F (48 entries, Miis and bikers included), `KartId` 0x00–0x23 (36).

- **Difficulty:** medium, but not for the reason it looks. Writing is easy — **the problem is
  *when***: after the selection screens, before loading. Finding that moment is the real work
- **To check:** that Miis and bikers (`PEACH_BIKER` 0x2D…) don't break anything when drawn at random,
  and that the character+vehicle combination is always valid (weight classes have constraints in MKW)
- **Note:** offline. Online is a different thing, see below

### 4. Item randomizer online — same map for everyone

The randomizer works offline and as a setting. Online, every console seeds with its own
`OS::GetTick()`, so every player gets a different map. Everyone would be playing a different game.

- **Model already in the repo:** `PulsarEngine/Extensions/LECODE/XPF.cpp`, around line 30. The host
  uses its own `selectId`, clients read it from the host's `RACEHEADER1` packet, the tick is only the
  offline fallback. Same problem, already solved
- **Needs first:** `randomItemArray` has to restart from `copyItemArray` at every race, before the
  Fisher-Yates. Today it's filled once and reshuffled on top of itself. Offline that's invisible,
  but with a shared seed it means two players get different maps depending on how many races they
  played before joining
- **Then, last:** remove the `OS::Report` logs. Not before: comparing the log of two consoles is
  the only way to check the maps match. After: a log left in means anyone can read the map
- **Difficulty:** medium. Few lines, but like 9 it can't be tested alone: it needs two clients
- **Open question:** TT stays vanilla even though the snapshot is taken at every race load. The
  likely reason is that the game rebuilds `behaviourTable` itself before the hook runs
  (`Item::InitAllBehavior`). Worth confirming with a breakpoint, because if it's true the `else`
  branch that restores the table is doing nothing

### 5. Upstream the build-script fix to Pulladium

`BuildPulsar.py` silently ignored the compiler's exit code, which cost me three of my four
"impossible" bugs. I fixed it locally: collect the results, fail the build, and delete the stale `.o`
so a failed compile can't leave yesterday's code inside a fresh `Code.pul`.

Someone on the Pulsar server suggested opening a pull request with it, on the grounds that more
people would benefit. They use `BuildPulsar.bat` themselves precisely because it shows the compiler
and linker output, which is the same problem seen from the other side.

- **Difficulty:** low as code — it's already written and it already works for me
- **What's actually needed:** checking it doesn't break the flow for people who build differently,
  and writing the description so the reasoning is clear
- **Note:** this would be my first contribution back rather than a feature for myself

### 6. The speedometer left/right setting

The dead line from day 1, never fixed. Not exciting, but it's the only entry on this list that's
already **broken** rather than **to be built**.

- **Difficulty:** low

---

## Further out

### 7. A new item — ice / fire flower

- **Prior art to study:** `LannyCF/mkw-item-expansion` has already done exactly this (Boo, Feather,
  Shroom Star, fused items). Reading it before starting is worth weeks
- **Difficulty: high, and it's the biggest entry on the list.** A new item means widening `ItemId`
  past `ITEM_NONE`, which means widening the game's structs, which means switching off the
  `size_assert`s — and from that point every place the game touches those structs is mine to keep
  correct. Their tally: 10 headers modified, seven `size_assert`s off, **~3700 lines** of new code in
  `PulsarEngine/`, plus network packet expansion
- **Needs first:** having properly understood the difference between *adding to* and *widening*
  `GameSource/` (see `RECAP.md` §3), and having done at least 1 and 3
- **Licence note:** their "Shroom Star" needs the author's permission (ImZeraora on Discord). The
  rest of the repo is MIT

### 8. The randomizer as its own mode

Promote the setting to a real mode only **if, playing it as a setting, it turns out to deserve one.**

- **Prior art in-house:** `PulsarEngine/Gamemodes/KO` and `OnlineTT`
- **Difficulty:** high. Menus, system contexts (`PULSAR_MODE_KO` as the model), probably networking

### 9. Blocking vanilla tracks online

Looks like the smallest one. **It's the riskiest.**

`CupsConfig::RandomizeTrack()` isn't a quiet corner: it sits inside `ExpSELECTHandler::DecideTrack`
in `PulsarEngine/Network/PulSELECT.cpp`, the code that decides **the track for the whole room**.
Around it are host-wins, KO mode, aid patching.

Three bad properties at once:
- **You can't test it alone** — you need other people, or two clients
- **Getting it wrong doesn't break your game, it breaks other people's room.** The only one where the
  failure is social
- **The loop is glacial** — the 4.7-second edit-build-look doesn't exist there, and the absurd-value
  trick can't be used

It's probably a few lines. It's the surroundings that are expensive.

---

## Undecided

- Bitwise and the PowerPC ABI as study in their own right, or learn them by doing 2? (leaning
  towards 2)
- The full randomizer online: does it make sense, or does it stay offline?
- 24 players — the north star. Not an entry on this list; it's what I use to choose what to study.
  First question to answer: **how much memory does one player cost today.** Until there's a number
  there, it isn't a project.

---

## Done

- Diary of the first four days (`Four-Days-Inside-Pulsar.md`)
- Kamek manual (`KAMEK-MANUAL.md`)
- Recap (`RECAP.md`)
- Item randomizer research (`NOTES-ITEM-RANDOMIZER.md`)
- Item randomizer, variant A: Fisher-Yates on `behaviourTable` at race load, VS only
- Item randomizer as a setting, with vanilla restored outside VS or when switched off
- To publish: the thread about the diary — text ready, just needs pasting
