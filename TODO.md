# TODO

What's left to do, in the order it makes sense to do it.
Every entry says **what I need to know first** and **what makes it hard**, so when I pick it back up
I don't have to rebuild the context.

Status: none started. Research done where noted.

---

## Now

### 1. Item randomizer — first version

- **Research:** done, notes kept locally
- **Needs:** nothing I don't already have
- **Difficulty:** low. No debugger, testable alone, the effect shows on the first lap

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

Extend the randomizer beyond items.

- **Research:** done, notes kept locally. Easier than I expected
- **Difficulty:** medium, but not for the reason it looks. The writing is easy — **the problem is
  *when***: after the selection screens, before loading. Finding that moment is the real work
- **Note:** offline. Online is a different thing, see below

### 4. Item randomizer as a setting

A **setting** first, not a mode. I've already walked this road with the mushroom/star/mega in TT on
day 3: redoing it takes an afternoon and gives me the playable thing.

- **Depends on:** 1
- **Difficulty:** low, it's repeating something already done

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

Promote 4 to a real mode only **if, playing it as a setting, it turns out to deserve one.**

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
- Item randomizer research (kept locally)
- To publish: the thread about the diary — text ready, just needs pasting
