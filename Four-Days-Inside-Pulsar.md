# Four Days Inside Pulsar

**A beginner's diary of learning C++ and PowerPC by modding Mario Kart Wii.**
*20–24 August 2026.*

---

I want to be upfront about what this is. It's not a tutorial and I'm not qualified to write one. It's a log of four days I spent going from "I know what a pointer is" to placing a hook at an address I found myself, and of every single thing I got wrong along the way.

I'm posting it because when I started, what I couldn't find anywhere was an honest answer to *how bad is it going to be*. Every guide shows you working code. Nobody shows you the four hours you lose because your file was named `MUSHROOOOMS` instead of `MUSHROOOOMS.cpp`.

So: it's a hell, and it's genuinely fun. Here's what it actually looks like.

I kept the mistakes in on purpose. Code that works, you read once and go "ok." A mistake you've already made, you recognise instantly the second time.

---

## Where I started

- C++ up to **pointers**, having skipped casting and a fair chunk of functions
- **Two years of Java at school**: classes, inheritance, exceptions, generics, linked lists
- Zero experience with the engine

And then the discovery that shortened everything: **the OOP was already there.** All I was missing was the list of Java → C++ differences, and that list is short.

| Thing | Java | C++ |
|---|---|---|
| Objects | everything is a reference | three ways: by value, `*`, `&` |
| Member access | always `.` | `.` on values and references, `->` on pointers |
| `new` | GC frees it | **you** have to `delete` |
| Destructors | don't exist | `~Name()` |
| Files | one class = one file | `.hpp` (declare) + `.cpp` (write) |
| Imports | `import` | `#include` + include guard |
| Integers | `int` and that's it | `u8 u16 u32 s16 s32 f32` |
| `namespace` | `package` | same idea, with `::` |

And the part you should **not** study: the entire STL. There is no `std::string` in this engine. No `vector`. No `iostream`. It runs on a Wii with 24 MB of RAM.

That last point is worth sitting with if you're coming from modern C++ tutorials. Half of what they teach you is inapplicable here, and knowing that up front saves you weeks.

---

## Day 1 — reading the engine

### What I built

A **mini-turbo charge meter** next to the speedometer, in single player: speed on the left, MT charge on the right. My own control, my own class.

File: `PulsarEngine/UI/CtrlRaceBase/Speedometer.cpp` and `.hpp`.

### The line that contains everything

```cpp
const Kart::Pointers& pointers = Kart::Manager::sInstance->players[this->GetPlayerId()]->pointers;
const Kart::Physics* physics = pointers.kartBody->kartPhysicsHolder->physics;
```

Ninety characters, and inside them: static members, `->` on pointers, indexing, references, `const`, `this`. Every piece of C++ I was missing, in two lines of real code.

### What I learned

**The star rule** — you read it off the declaration, you don't guess it:

| Declaration | What it is | Access |
|---|---|---|
| `Type x` | the object | `.` |
| `Type& x` | a reference | `.` |
| `Type* x` | an address | `->` |

`a->b` **is** `(*a).b`. That `*` is the same `*` as in a textbook pointer exercise.

**You don't widen `GameSource/`.** Those structs are the game's memory map, decided by Nintendo in 2008. `size_assert` exists to stop you.

*(Added later: "don't touch it" was the wrong way to put it. **Adding** a declaration for a function that already exists in the game and nobody had documented is fine — it's documentation, it changes nothing at runtime. What breaks the contract is changing the **layout**: new fields, bigger arrays. Which is exactly what I'd done.)*

**Your own classes in `PulsarEngine/` are fair game.** Even when they inherit from a game class: your new fields land at the end, after the part the game knows about.

**The `Count()` / `Create()` contract.** The engine asks how many slots you need, then hands them back to you. `count` means *number of controls*, not *number of players* — as long as those two numbers match, nobody notices, and the day you split them everything falls out.

**A fast loop beats theory.** `BuildPulsar.py` only recompiles the files you touched: **4.7 seconds.** Edit → build → look. With a slow build you give up after a week.

### Day 1 mistakes

**Adding a field to `Kart::Pointers`**

```cpp
// in GameSource/MarioKartWii/Kart/KartPointers.hpp
Killer* kartKiller;   //0x60
s16 mtCharge;         //0x64   <-- NO
```

Result: `static assert check 'Pointers' failed` in **60 files**.

> Those weren't 60 errors. It was **one error, reported 60 times.** Read the first one, ignore the echo.

**Looking for `mtCharge` in the wrong places** — four attempts, all still in the file as comments:

```cpp
//speed = Kart::Movement::                     // thought I had to "connect" the class somehow
//speed = pointers.mtcharge;                   // Pointers has no such member
//speed = pointers.KartMovement->mtcharge;     // capital K: that's the TYPE, not the member
```

The correct line was already in the file, **two lines above**, and I had commented it out:

```cpp
float speedCap = pointers.kartMovement->hardSpeedLimit;   // the exact same road
```

> When you're looking for how to reach something, the first move isn't to invent it. It's to **find whoever already got there** and copy the road.

**The cast, backwards**

```cpp
speed = static_cast<s16>(pointers.kartMovement->mtCharge);   // mtCharge is ALREADY s16
```

Compiles, works by accident. But:

> You convert **towards the type you want**, not towards the one you already have.

**Shadowing** — why the compiler said nothing:

```cpp
u8 speedoType = (count == 3) ? 4 : count;      // the real one
if(...){
    u8 speedoType = (count == 3) ? 4 : count;  // a SECOND variable, dies at the }
}
```

Java rejects this. C++ allows it, silently.

**`=` instead of `==`**

```cpp
else if(count = 2) speedoType = 1;   // assigns 2, condition is ALWAYS true
```

And as a bonus it changes `count`, which controls the loop three lines further down.

> `=` writes. `==` asks.

**Deriving from ambiguous data.** `count == 2` can mean *one player doubled* or *two players*. That information isn't in the number — you have to get it from the source (`localPlayerCount`).

**Deciding before correcting.** In `Count()`, the test `if(localPlayerCount == 1)` sat **above** the two lines that fix the count for replays and spectator mode. It was deciding on data that wasn't ready yet.

And the disguised variant, right after:

```cpp
u32 localPlayerCount = scenario.localPlayerCount;   // the copy I then correct
...corrections applied to the copy...
if(scenario.localPlayerCount == 1){                 // but here I read the ORIGINAL
```

> If you take a copy in order to fix it, from that moment on **the copy is the truth**.

---

## Day 2 — hooking into the game

### What I built

**Two hooks, of two different kinds.**

`PulsarEngine/Race/GetUMTValues.cpp` — started out trying to replace a function, ended up writing straight into memory:

```cpp
kmWrite16(0x808B5CC2, 1);   // MT charge isn't computed: it's a variable
```

`PulsarEngine/Race/TTItems.cpp` — a star in Time Trial, granted **once per race**, with `RaceLoadHook` resetting and `RaceFrameHook` checking.

### How a hook actually works

At `0x80580630` the game has **one single instruction**: a `bl` to `ApplyLightningEffect`. `kmCall(0x80580630, MegaTC)` rewrites that `bl` so it points at your function. **Nothing else.** The registers stay exactly as they were.

```cpp
void MegaTC(Kart::Movement& movement, int frames, int unk0, int unk1)
//          ^ r3               ^ r4        ^ r5      ^ r6
```

> The parameter list **is not your choice: it's the register map.** You declare all the original ones, in order, including the ones you ignore. And you don't invent new ones.

A method's `this` becomes the first explicit parameter. And the `else` branch that calls the original isn't pedantry:

> A hook that doesn't call the original when its condition is false doesn't extend the game. **It breaks it.**

### The kinds of hook

| Macro | What it writes | When |
|---|---|---|
| `kmCall` | `bl` — go *and come back* | hijack **one call** in the middle of the code |
| `kmBranch` | `b` — go *and stay* | replace **an entire function** \* |
| `kmWrite16/32` | a raw value | change **a variable** or an instruction |
| `RaceLoadHook`, `RaceFrameHook`, `BootHook`, `SectionLoadHook` | — | **no address at all**: a `void` function, one line to register it |

> Before you go hunting for an address, check whether the moment you care about **already has a hook waiting for you**.

*\* Added later, from a correction: `kmBranch` isn't only for replacing a whole function. `b` just means "go and don't come back", so putting one on a function's **`blr`** runs your code after the original has finished, keeping the original behaviour. Works; not necessarily good practice.*

### Working out an address

PowerPC can't load 32 bits in one instruction. **It always splits it in two.**

```
8057efec   lis  r3, 0x808B        -> r3 = 0x808B0000
8057eff0   lha  r3, 0x5CC6 (r3)   -> reads at 0x808B0000 + 0x5CC6 = 0x808B5CC6
8057eff4   blr
```

**address = (the `lis` value << 16) + offset.** Arithmetic, not guesswork.

The offset is **signed** — 16-bit two's complement. If the **raw** value starts with 8–F it's
negative: subtract `0x10000` from it first. Dolphin usually does that step for you and prints it
already signed, as `-0x4000`, so the leading-digit rule is really for when you're reading raw hex.

| Range | Binary |
|---|---|
| `80004000` – `~80388000` | main.dol |
| `~805102E0` – `~808D9A58` | StaticR.rel |
| `~809BD6E8` upwards | data and variables |

`StaticR.rel` is relocatable: **you can't disassemble it from the file**, the addresses won't match. You use Dolphin's debugger, which reads RAM with the game running.

### Reading a crash

`Crash.pul`, **offset 12** = the type. `2` = DSI · `3` = ISI · `7` = floating point · `8` = FPE

| | What it tried to do | Where to look |
|---|---|---|
| **DSI** | read/write **data** at an address that doesn't exist | `srr0` is a real instruction; the rotten address is **in the registers** |
| **ISI** | execute **code** where there isn't any | `srr0` **is** the garbage; look at **`lr`**, which tells you where the jump came from |

### Day 2 mistakes

**`kmCall` on the start of a function**

```cpp
int umt100(Kart::Movement& movement, int unk0, int unk1) { return 100; }
kmCall(0x8057efe0, umt100);   // 0x8057efe0 is the START of GetMTMaxCharge
```

Crash. And the dump told the whole story:

```
error = 2 (DSI)
srr0   = 0x8057efe4    <-- 4 bytes past the start
lr     = 0x8057efe4
r3     = 0x00000064    <-- 100. MY value.
```

`bl` goes **and comes back**: my function returned into the middle of the original with `r3` full of a return value instead of a pointer. The next instruction did `0x64 + 0x5CC2 = 0x5D26`, which isn't memory.

Plus two invented parameters (`unk0`, `unk1`) that didn't exist.

**The wrong include — the nastiest mistake of the whole four days**

```cpp
#include <KamekInclude/hooks.hpp>   // the search path IS ALREADY ./KamekInclude
```

A **fatal** error, not a warning. But:

```python
def compile_cpp(cpp: str):
    subprocess.run(cmd, shell=True)   # <-- exit code never checked
```

The build script **doesn't look at whether the compile succeeded.** It links the old `.o` and hands you a `Code.pul` with a fresh timestamp and yesterday's code inside. The crash was byte-for-byte identical, and the proof was in the binary:

```
40 ff ff fe   80 57 ef dc   ...    <-- kmBranch (b)
41 ff ff fe   80 57 ef e0   ...    <-- my hook: still kmCall (bl)
```

> In this build system **a compile error is silent.** Those errors scrolling past in the output aren't noise — they're the only thing that matters.

*(Day 4 note: I fixed this. See the last section.)*

**Writing into the middle of an instruction**

```cpp
//kmWrite16(0x8057efe0,1);   // right address, but it's CODE: it overwrites the lis
```

`kmWrite` has no idea whether you're pointing at an instruction or a variable. That distinction is yours to keep.

**The missing `0x`**

```cpp
//kmWrite16(80591208,1);   // decimal -> 0x04CDF3A8. Not a wrong address: a DIFFERENT NUMBER.
```

Crash on boot, because `kmWrite` acts at load time.

**Hooking the wrong function**

```cpp
kmBranch(0x80790e3c, setStartOnTT);   // 0x80790e3c = u16 GetKMPObjectsCount()
```

I wanted a star in TT and I replaced **the function that counts KMP objects**. Incompatible signature in two ways: the original takes no parameters and **returns** a `u16`, mine took one and returned `void`.

And you could see it in the disassembly:

```
80790e3c   lis  r3, 0x809D    <-- FIRST instruction: overwrites r3
```

> If the first instruction **writes** to `r3` instead of reading it, the function takes no arguments.

**Inventing a kind of hook**

```cpp
// TTItems.hpp
class cancelStarAfterUse{ cancelStarAfterUse(void *func); };
```

`RaceFrameHook` **is a type**, like `int`. Two hooks = two variables of the same type, not two classes:

```cpp
RaceFrameHook star(setStarOnTT);
RaceFrameHook removeStar(removeStarAfterUse);
```

**Incomplete chain on the manager**

```cpp
Item::Manager::players.setItem(STAR, true);
```

Four things at once: `players` isn't static (needs `sInstance->`), the index is missing, `SetItem` is capitalised, and it **doesn't belong to `Player`** but to its `inventory`. The real chain:

```
Item::Manager::sInstance   ->  Manager*            ->  ->
   ->players               ->  Player*  (array)    ->  [index]
   [index]                 ->  a Player OBJECT     ->  .
   .inventory              ->  object              ->  .
   .SetItem(STAR, true)
```

One star fewer than `Kart::Manager` (`Player**` vs `Player*`) and the symbol changes.

**A command mistaken for a question**

```cpp
if(Item::Manager::sInstance->players[0].UseStar() == true && isTT == true)
```

`void UseStar()` doesn't return anything. But the real problem runs deeper: by putting it in the `if` I wasn't *asking* whether the star had been used — **I was firing it**, sixty times a second.

> If a function returns `void`, it **does** something. It can't answer, so it can't live in an `if`. Reading is free. Calling isn't.

**A notebook nobody reads**

```cpp
if(isTT == true){                    // <-- no check on givenStar
    ...SetItem...
    givenStar = true;
}
if(gamemode == MODE_TIME_TRIAL && givenStar == true){
    ...UseItem... RemoveItems...
    givenStar = false;               // cleared in the SAME frame it was written
}
```

| | in | first `if` | second `if` | out |
|---|---|---|---|---|
| frame 1 | `false` | grants -> `true` | uses -> `false` | `false` |
| frame 2 | `false` | grants -> `true` | uses -> `false` | `false` |

Nothing changed between frames. The variable was written and erased inside the same frame, and **the block that granted the star never looked at it.**

**Invisible logs**

```cpp
OS::Report("[TEST LOG ...]PulsarEngine: Giving player 1 a star for TT", 0);
```

The logs had been there the whole time. But `OS::Report` **doesn't add a newline**: without `\n`, hundreds of messages fuse into one line, buried under thousands of lines of boot output.

---

## Day 3 — making a feature configurable

### What I built

The Time Trial item becomes **a setting**: triple mushrooms, star or mega, chosen from the Pulsar menu. First feature that isn't a hardcoded value, but something the person playing decides.

Files: `PulsarEngine/Race/TTItems.cpp` plus new values in `Settings/SettingsParam.hpp` (`SETTINGTT_RADIO_ITEM`, `TTSETTING_ITEM_STAR`, …).

### The underlying mistake: where the condition goes

My first attempt was three functions — `setStar`, `setMega`, `set3Shrooms` — each reading the setting and, if it matched, registering its own hooks:

```cpp
static void setStar(){
    const bool isEnabled = ...GetSettingValue(...) == TTSETTING_ITEM_STAR;
    if(isEnabled){
        RaceLoadHook restartStar(restartStar);   // <-- LOCAL variable
        RaceFrameHook star(setStarOnTT);         // <-- LOCAL variable
    }
}
```

Three things wrong at once:

1. **Nobody ever called `setStar`, `setMega` or `set3Shrooms`.** Three dead functions.
2. A hook **is an object whose constructor adds it to a list**. Declared inside a function, it's born on every call and **dies when the function ends**, leaving a pointer to memory that no longer exists in the list.
3. And above all: hooks register **at boot, once**, before any setting means anything. And the player can change the setting **mid-session**.

> **You can't make the registration conditional. You make the behaviour conditional.**

The fix merged three functions and three flags into **one function and one flag**, with the setting read inside it, and the two hooks at global scope where they belong. Welcome side effect: changing the setting between races now works immediately, because the value is re-read every frame.

### Other day 3 mistakes

**Declaration order, again.**

```
Error: undefined identifier 'restartMega'   (line 86)
Error: undefined identifier 'setMegaOnTT'   (line 87)
```

`setMega()` was calling two functions defined **below** it. Which is literally what I'd written for myself months earlier in a practice file: *"if it's placed below where it's called it'll error, because the compiler is dumb and doesn't know where to find it."*

**The variable that initialises itself.**

```cpp
RaceLoadHook restartStar(restartStar);
// Warning: variable 'restartStar' is not initialized before being used
```

I gave **the variable the same name as the function**. In C++ the name you're declaring is already in scope **inside its own parentheses**: that `restartStar` isn't the function, it's the variable I'm creating right now. It initialises itself with itself.

The day before it had worked only because I'd used different names: `RaceLoadHook restart(restartStar);`.

**The silent build — third time in three days.** The file wasn't compiling, the old `.o` stayed linked, and the game kept granting the star. The log said one thing and the source said another.

**A name that lies.** The option called `TTSETTING_ITEM_DISABLED` granted three mushrooms. The code did exactly what I told it. The name didn't.

### The switch, and the "decide then act" shape

The `else if` chain read the setting **three times** and repeated `SetItem` + flag + log in every branch. With a `switch` that picks **only the value**:

```cpp
const u32 choice = ...GetSettingValue(...);
ItemId item;
switch(choice){
    case ...: item = TRIPLE_MUSHROOM; break;
    case ...: item = STAR;            break;
    case ...: item = MEGA_MUSHROOM;   break;
    default:  return;                        // no item
}
...SetItem(item, true);
isGivenItem = true;
```

Same idea as the ternary in `Load()` on day 1: **decide a value first, then act once.** And `default` solves the "no item" case for free.

Two things about `switch` in C++: **`break` is not optional** — without it execution falls through into the next case, and the compiler won't warn you. And `default` covers everything else.

*(`static` variables in C++ are born zeroed, guaranteed: `static bool isGivenItem;` is already `false`.)*

---

## Day 4 — placing a hook exactly where I wanted it

### What I built

The mushroom that turns into a **bullet**: a `kmCall` at an address that isn't written in any header. I found it myself, with the debugger.

Plus the build script that stops lying: `BuildPulsar.py` now **halts** when a compile fails.

### Finding the address of a `bl`

Up to now I'd used all three hook types, but with a difference that matters:

| Hook | Where the address came from |
|---|---|
| `kmBranch` | a comment in the header — handed to me |
| `kmWrite16` | `lis` + offset in the debugger — worked out |
| `kmCall` | **never done from scratch** |

`kmCall` wants the address of a **`bl`**, i.e. of a call site inside the game's code. That isn't written anywhere: headers list the **starts** of functions, not the points where they're called from.

The technique:

| | Move | Result |
|---|---|---|
| 1 | Breakpoint at `0x8057f3d8` — start of `Kart::Movement::ActivateMushroom`, taken from the header comment | it fires -> the function really is called |
| 2 | Use a mushroom in a race | breakpoint fires |
| 3 | Read `lr` | `0x80798668` |
| 4 | `lr − 4` | **`0x80798664`** <- the address for the `kmCall` |

The reason is mechanical: when the CPU executes a `bl` at address A, it puts **A + 4** into `lr` — where to come back to — and jumps. So `lr` is the instruction *after* the call.

> Headers can list the **starts** of functions because a start is unique. A *call* isn't: the same function can be called from ten places, and none of those ten has a name.

### The neighbourhood tells you where you landed

`0x80798664` isn't just any number: it sits among **`Item::Player`** symbols.

| Address | Function |
|---|---|
| `0x80797928` | `Item::Player::Update()` |
| **`0x80798664`** | my `bl` |
| `0x807986b4` | `Item::Player::ActivateMegaMushroom()` |

So I'm in the code of the **item**, not of the driving. The header is missing the sibling `Item::Player::ActivateMushroom()`, which sits just before and ends by calling the `Kart::Movement` one.

To find where the enclosing function actually starts, scroll the disassembly backwards until you hit the **prologue**:

```
mflr  r0                 <-- saves the return address: this function calls others
stwu  r1, -0x??(r1)      <-- opens the stack frame
```

Above the prologue is the previous function's `blr`. It's the only way to orient yourself in a listing with no symbol map — i.e. when you don't have the debugger.

### The test that didn't do what I expected

I kept the breakpoint and drove over a **boost panel**. It didn't fire.

That wasn't a bug: the mushroom boost and the panel boost are boosts of **different types**, with different functions.

```cpp
void ActivateMushroom();     //8057f3d8   <-- mine
void ActivateZipperBoost();  //8057f96c   <-- ramps and zippers
void TryStartJumpPad();      //8057fd18   <-- jump pads
```

> A breakpoint that **doesn't** fire is information, not a failure. It told me something true about the engine that I wouldn't have got from reading the code.

Practical upshot: I have exactly one call site, and it fires exactly when I want it to.

**Careful:** a function can be called from many places. The breakpoint tells you where it came from *this time*. If you need one specific site, make it fire in the right context.

### Virtual or not: who needs `symbols.txt`

In `MegaTC.cpp` I call `movement.ActivateMega()` and it links fine. I call `movement.ActivateMushroom()` and the link fails. The difference is in the header:

```cpp
void ActivateMushroom();            //8057f3d8   <-- ordinary method
virtual void ActivateMega();        //0x1c       <-- virtual
```

> If the call goes through the **vtable**, the game resolves the address at runtime and I need no symbol at all. If it's a `bl` to a fixed address, I'm the one who has to give that address to the linker.

The comment next to a virtual method isn't even an address: it's the **vtable offset** (`0x1c`). Two numbers that look identical and aren't.

### `symbols.txt`: the two halves of a line

The piece I was missing turned out to be trivial. One line comes from two different places:

| Half | Where from |
|---|---|
| `ActivateBullet__Q24Kart8MovementFUc` | from the linker failing, or derived by hand from the rules |
| `= 0x805858ac` | from the **comment in the header**, with `0x` in front |

```cpp
void ActivateBullet(u8 itemPoint); //805858ac
```

There's nothing to compute: somebody already documented where every function starts.

> `symbols.txt` is a **phone book**: name <-> number. You add the line for whoever you want to call.

And it dovetails with `kmCall`: the **start** of a function is documented, the **call** isn't. `symbols.txt` is the easy half.

If you'd rather not write mangled names by hand, you don't have to: declare the function, call it, and the linker will fail and print the exact string at you. The rules, if you want to read them:

```
UseItem__Q24Item9PlayerObjFb
  UseItem  __  Q2  4Item  9PlayerObj  F  b
  name         2 qualified levels      function, one bool
= Item::PlayerObj::UseItem(bool)
```

The number before each name is its length. `v` void · `b` bool · `i` int · `Uc` u8 · `Us` u16 · `f` float · `P` pointer · `C` const · `R` reference · `e` ellipsis.

Hooks do **not** go through `symbols.txt`: you give them the number and that's it.

### The addresses are PAL, and the other regions come for free

What I write in `symbols.txt` is a **PAL (RMCP)** address, because PAL is the base version. The other regions are handled by `versions.txt`, which the linker uses alongside `symbols.txt`:

```
[P]
#Base version: MKWii PAL
00000000-*: +0x0          <- PAL is the base: no shift
[E]
...
8054fb2c-80550547: +0xd9c
80550548-805537cb: -0x5f58
```

It isn't a **per-symbol** table: it's a table of **ranges**. A PAL address falls inside a range and picks up that delta. So for my two lines I didn't have to add anything anywhere — they get translated on their own:

| PAL address | | E | J | K | D |
|---|---|---|---|---|---|
| `0x8057f3d8` | `ActivateMushroom` | −0x6864 | −0x680 | −0x11fa8 | −0x9d8 |
| `0x805858ac` | `ActivateBullet` | −0x6824 | −0x680 | −0x11fa8 | −0x9d8 |
| `0x80798664` | my `kmCall` | −0x900c | −0x994 | −0x11c40 | +0x594 |

And you can see it **isn't one global shift**: two functions `0x6000` apart get different deltas in NTSC-U. The code was recompiled, not moved as a block.

**How it breaks:** if a PAL address lands in a **hole** in the table — a range no line covers — the linker leaves it **as is**. No error, no warning: on the other region the game simply calls a different address, which over there is something else entirely.

> If something works on PAL and only crashes on NTSC, the first place to look is `versions.txt`.

It's the same flavour of silence as the build linking yesterday's `.o`: nothing in the system knows the address was never translated.

### Day 4 mistakes

**The lowercase namespace**

```cpp
namespace pulsar{
    namespace race{
```

```
Error: name followed by '::' must be a class or namespace name
```

Name lookup walks **outwards**: `Settings` is looked for in `pulsar::race`, then in `pulsar`, then in `::`. And `Pulsar::Settings` is never looked at, because `pulsar` and `Pulsar` are two **different** namespaces.

> Namespaces in C++ are **open**: getting the name wrong isn't an error the compiler rejects — **it creates a new, empty one.**

Third member of the same family in four days, after shadowing and `RaceLoadHook restartStar(restartStar)`: **legal and silent.**

**The file with no extension — the fourth time a change didn't arrive**

```
PulsarEngine/Race/MUSHROOOOMS
```

```python
cpp_files = glob.glob(f"{PULSAR}/**/*.cpp", recursive=True)
```

Not `*.cpp`. **The build doesn't see it.** And the cruel part is that the fix I'd just made to the build script is useless here: no compile fails, there simply isn't one. The build says "all good" and it's telling the truth.

> The chain of five checks has a step above the first one: **does the build know the file exists?**

With a detail that fooled me: the error I was reading came from **the editor**, not the compiler. They look like the same thing and they aren't — one checks as you type, the other only when the file enters the build.

**The variable named after the type**

```cpp
void MyMushroom(Kart::Movement& Movement)
```

It compiles, but from there on inside the function `Movement` is the variable, not the class. It's `RaceLoadHook restartStar(restartStar)` coming back, and the capital `K` from day 1.

**The parameter I didn't know what to do with**

`ActivateBullet(u8 itemPoint)` — what do I pass? The answer was in a neighbouring header, `KartKiller.hpp`, inside a comment:

```cpp
void Activate(u8 itemPoint); //8059b7b8 if itemPoint == 0xFF, gets item point from Item::Player
```

`0xFF` = **"you figure it out"**: the game goes and fetches the item point itself.

Third time in four days that the answer was already written in the repo and I went looking for it somewhere else.

### The build doesn't lie any more

Three changes to `BuildPulsar.py`:

1. `compile_cpp` now **returns** `(file, returncode)` instead of throwing it away
2. `executor.map` gets **collected**: previously the results went nowhere, so even checking the exit code wouldn't have been enough
3. If even one file fails: list the files and `sys.exit(1)`. **No link, no copy.**

Plus a fourth thing, which closes the day-2 case for good: if a file doesn't compile, its old `.o` gets **deleted**. That makes yesterday's `.o` inside a freshly-timestamped `Code.pul` impossible, even if you hit `L` to relink.

> The build script is **your code too.** If it lies to you, you fix it.

---

## The thread that ties almost every mistake together

Four times in four days, always the same shape:

| | The change didn't arrive because |
|---|---|
| the speedometer's `pos` | it was on a dead line (`if(count == 1)` with `count` already 2) |
| the `kmBranch` | the `.o` hadn't been recompiled |
| the star | the notebook was erased in the same frame |
| the mushroom hook | the file was called `MUSHROOOOMS`, with no `.cpp` |

> **When the effect is *nothing* rather than *wrong*, the problem is almost never in the logic.** It's in the chain: saved → compiled → linked → copied → reloaded.

### The chain of five checks

0. **Does the build even know this file exists?** (`.cpp` extension, inside a folder the glob covers)
1. **Saved?** The build reads from disk, not from your editor.
2. **Compiled?** `ls -la build/YourFile.o` — it has to be newer than the `.cpp`.
3. **Linked?** If the prompt says *"No source or header files were modified"* right after you modified something, that isn't an offer. It's a warning.
4. **Copied?** Does `RIIVO` point at the pack you're actually launching?
5. **Reloaded?** Riivolution reads the files when the ISO starts.

### And the other habit that solved the most

> **Try an absurd value.** A `1` in place of a `270` gives an effect you cannot mistake for anything else. If nothing changes even with that, the hook isn't firing — and that's a different problem with a different solution.

Separating "doesn't fire" from "fires but the value doesn't matter" is what unblocked day 2.

---

## Dolphin's debugger, in three moves

You turn it on in settings, interface section.

1. **A breakpoint answers yes or no.** It fires → the function is being called. It doesn't → you're hooking the wrong place, and no amount of reasoning about the code would have told you.
2. **Breakpoint on the `blr`, then read `r3`.** That's where PowerPC keeps return values. It works in reverse too: in a crash, an `r3` holding a number of *yours* says your code went through there.
3. **Memory view** with emulation **stopped on a breakpoint**. The symbol will say `unk`: normal, there's no map loaded. You want the contents, not the name.

*(Added later: I was doing all of this **without a symbol map**, which I didn't know was an option. With `RMCP01.map` loaded, Dolphin shows real demangled names across StaticR.rel and most of the orienting-by-hand above simply isn't needed. Dolphin's call tree also finds earlier call sites, and with the mkw decomp project you can use Ghidra to list every reference to a function.)*

That's how I found out `GetMTMaxCharge` doesn't compute anything — it's a three-instruction getter on a variable at `0x808B5CC2`. And that the code that matters reads that variable **directly**, without going through the getter.

> Hooking a getter changes things **only for whoever calls the getter.**

---

## Day 5 — the item randomizer

Two weeks off in between. Coming back cost more than I expected, and not because of syntax: what I'd lost was the habit of **running the code in my head**. For the first half hour I couldn't remember what an initialisation was, and I had to pick arrays up from scratch.

### What I built

`PulsarEngine/Race/ItemRandomizer.cpp`. You pick up an item, the icon tells the truth, you press the button, and something else happens. A deliberate mismatch between **icon** and **effect**.

State at the end of the day: the copy-and-rewrite machinery **works**, the fixed shift works, the random draw works. The real shuffle is missing, and the last version crashes on an out-of-bounds index. Not finished, but every piece was verified on its own before the next one was built on top of it.

### The piece everything rests on

`GameSource/MarioKartWii/Item/ItemBehaviour.hpp:25`

```cpp
static Behavior behaviourTable[19];   //809c36a0, index item id
```

Nineteen rows, one per `ItemId`, and inside each row `objId`, `numberOfItems`, `useType` and a function pointer. When you press the button, the game goes to the row of the item you're holding and calls whatever is written there. There's no giant `switch`: there's one road, and it goes through here.

It's a **static member with a symbol in `symbols.txt`**, so you reach it by writing `Item::Behavior::behaviourTable[i]`. No hand-written addresses: include the header and the linker does the rest. And `0x809c36a0` sits above `809BD6E8`, i.e. in the data range: it's a variable, you can write to it.

### Reading a function pointer

```cpp
void (*useFunction)(Player& player);   //0x18, null means the item is draggable
```

You read it starting from the name and working outwards. `useFunction` is wrapped in `(* … )`, so it's a pointer. Immediately after comes a parameter list, so it's a pointer **to a function**. The leading `void` is what that function returns.

The parentheses around the star aren't decoration: `void *useFunction(Player&)` would be a function returning a pointer, which is a completely different thing.

Two practical consequences, both of which I got wrong before I understood them:

- **a function's name is already its address**, so you assign it without parentheses. With parentheses you call it right now, which is the opposite of what you want: it's the difference between giving someone your phone number and dialling it while you hand it over;
- the signature being `(Player&)` is no accident. The functions already in the table are `Item::Player`'s `Use...` methods. A method receives its object as a hidden first argument, so `void Player::UseBullet()` and `void f(Player&)` are, at machine level, the same shape.

### The absurd value: all 19 rows pointed at one function

First test, before any permutation: point **every** row's `useFunction` at a function of mine that calls `player.UseBullet()`.

In a race, whatever came out of the box gave me the bullet. Three answers from one race: the table is live, it's writable at `RaceLoadHook`, and nobody overwrites it after me.

**Why not filter which rows to fill:** half the table has a null `useFunction` — bananas and shells, the ones you drag behind you. If I'd only written where it was already filled, and then a banana came out with no effect, I couldn't have told "I skipped that row" from "my code doesn't work". Filling everything makes the expected result unambiguous.

### The real discovery: `useFunction` isn't the whole story

With the table filled, throwable items did **two things at once**: the banana got thrown *and* the bullet fired.

So the throw doesn't go through `useFunction`. If it did, having overwritten it, the banana wouldn't have come out at all. The throw goes down another road, chosen by `useType` — my notes already had `PrepareHandlers[5]` and `UpdateHandlers[5]`, five of them, like the five values of `useType`. And the thrown object was the **right** one, so that road reads `objId` from the same row.

> An item's behaviour doesn't live in one field. It lives in `useType`, `objId` and `useFunction` together. Touch one and leave the others and you get two behaviours stacked on top of each other.

That's why the good version moves **whole 0x1c-byte rows**, not pointers. And the good news that follows: since `objId` travels with the row, when you pick up a bullet and the banana's row is inside it, what comes out is a real banana, with its own model and its own behaviour.

### The backup, and the test whose expected result is "nothing"

To shuffle you need an intact original. If you write into the table while reading it you destroy the data as you go, and next race you'd be working on an already-shuffled table: after three races, mush.

So: an array of my own, 19 rows, filled **once** on the first race, with a `bool` that remembers it's done. Then, every race, a second loop that rewrites the table reading from the copy.

The intermediate step is the one that gave me the most confidence of all: the second loop copying row `i` into row `i`, i.e. **changing nothing**. If the machinery works, the game has to behave exactly as usual. If instead it blows up, the problem is in the copying, and you find that out before building anything else on top.

The log confirmed it better than the race did: nineteen copy lines on the first race, **zero on the second**, and nineteen rewrite lines on both. The `bool` was doing its job.

### The fixed shift, before the randomness

Before bringing the RNG in, a shift of one: row `i` takes row `i + 1`, and the last one wraps back to the first with `(i + 1) % 19`.

Deterministic, so the predictions get written down before launching: green shell → red shell, banana → fake item box, mushroom → triple mushroom, triple banana → green shell. In the race they all came out right.

On the index: `i + 1` on its own would, on the last iteration, read slot 19 of an array that stops at 18. The remainder is what closes the circle.

### The seed

`Random` isn't random: it's a computed sequence starting from one number, the seed. **Same seed, same sequence, always.** If the seed doesn't change, every race gets the same map.

You need a number that changes by itself, and the console has one ready: `OS::GetTick()`, the counter that climbs from power-on.

`XPF.cpp` does the same but shuffles the bits around, swapping the top and bottom halves, because the counter's low bits race and the high bits barely move. Offline the plain tick is enough. Online it isn't, and not for elegance: if the seed isn't identical on every client, everyone plays a different game. That's why XPF, in private rooms, takes the seed from the room instead of the clock.

### Drawing with replacement isn't shuffling

With `NextLimited(19)` called nineteen times the code ran, and in a race it looked fine. The log said otherwise. First race, the numbers drawn:

```
16, 0, 9, 6, 11, 12, 15, 13, 8, 14, 15, 5, 5, 17, 9, 15, 11, 17, 2
```

15 three times. 5, 9, 11 and 17 twice each. And never drawn at all: 1, 3, 4, 7, 10, 18.

Translated into the race: **six behaviours out of nineteen didn't exist in that game**, and three different items did exactly the same thing. This isn't repetition between races, it's *that* race's map being lopsided.

The reason is that each call knows nothing about the previous ones. It's dealing cards and putting each card back in the deck every time: somebody gets three aces and six cards never come out. The fix isn't to draw better, it's to **change the gesture**: shuffle the deck and then deal in order, so each row goes to exactly one position and all of them get used.

And there's the reason it wasn't visible while playing: in one race you pick up a handful of items, not nineteen, so it's easy for both halves of a duplicated pair never to land in your hands.

> When an effect is too rare or too scattered to see by playing, stop playing and start printing.

### The crash, and the place where the fault **isn't**

Last version of the day. A bot hits an item box and:

```
Error: DSI
SRR0: 0x80797500  Item::ObjHolder::GetTotalItemCount
LR:   0x807BB7D4  Item::SlotData::DecideItem
R03:  0xA544F4D4
```

The call chain: `Itembox::OnCollision` → `Player::DecideItem` → `PlayerRoulette::DecideItem` → `SlotData::DecideItem` → `GetTotalItemCount`, and there an address that isn't an address.

The fault was mine, on this line:

```cpp
randomItemArray[i + random.NextLimited(19-1)]
```

`i` goes up to 18, the draw goes up to 17, the sum reaches 35, and the array has 19 slots. For a good share of the iterations I was **reading memory that isn't mine** and writing that garbage into the game's table as if it were a behaviour row. That fake row contains an `objId` that isn't an object; the game uses it to look up the matching holder and ends up holding `0xA544F4D4`.

> **The crash site is not the fault site.** Not one line of my code appears in that dump. I left the mine at race load; the game stepped on it minutes later. Memory bugs are only found by walking back up the call chain and then asking who wrote bad data in there.

### Day 5 mistakes

**The initialisation outside every function.**

```cpp
bool isVSRace = DriverMgr::isVSRace;   // at namespace scope
```

The right-hand side is read **once**, when that variable is born, i.e. when the game boots. At that moment there's no race, so the value is false and stays false forever. The `if` was never true and the log never came out: the log worked fine, it was simply never reached.

The same line **inside** the hook's function runs on every race load and reads the real value. Same line, two places, two behaviours.

On Kamek there's one more reason not to trust it: dynamic initialisers at namespace scope may not run at all. Globals get declared; they don't get initialised with values taken from the game. With a constant, like `static bool isRandom = false;`, it's fine: that value ends up baked into the binary and there's nothing to execute.

**Two functions mistaken for one.** The job is made of two functions with opposite trades, and for half an hour I kept trying to write them as if they were the same one:

| | when it runs | what it does | signature |
|---|---|---|---|
| `randomItem` | at race load, once | rewrites the table | `void()` |
| `randomAHMoment` | in the race, on button press | the new effect | `void(Item::Player&)` |

The index and the table belong in the first. By the time the second runs, **the game has already done the table lookup** — otherwise it wouldn't have ended up in there. Inside the second there's nothing to look up, there's only something to do.

**`getBehaviorIndex()`.** Doesn't exist. The editor's autocomplete invented it. Searched across all of `GameSource`: zero hits, in any capitalisation.

> If you can't find a name by searching `GameSource`, that name doesn't exist, however reasonable it looks. The headers are documentation written by hand by several people, and the names follow no predictable convention.

**`Item::Player::UseBullet();`**

```
a nonstatic member reference must be relative to a specific object
```

`::` reaches something that exists in a single copy: a static, a namespace. `.` reaches something belonging to a specific object, and so you need to have an object. A bullet doesn't exist in the abstract: what exists is **a player's** bullet, which consumes their item and moves their kart. I already had that player in my hands — it was the parameter.

**`Random random();`** — doesn't create an object. To the compiler that declares a **function** called `random` taking nothing and returning a `Random`. When you pass no arguments, an object is created without parentheses. It's the same parentheses joke as the function pointer, wearing a different coat.

**`Item::Behavior& behavior;`** — a reference is a nickname, and it must be initialised the moment it's born. You can't create it empty and fill it later like a `bool`.

The opposite slip is worse, because it's silent: **forgetting the `&`**.

```cpp
Item::Behavior behavior = Item::Behavior::behaviourTable[i];   // a copy!
```

Compiles, no warning, the game boots, and in the race nothing happens: you modified a copy that dies at the end of the iteration. Legal and silent, again.

**Square brackets mean two different things.**

```cpp
Item::Behavior copy[19];   // DECLARATION: make me 19 slots
copy[3]                    // USE: give me slot number 3
```

In a declaration the number is *how many*, in a use it's *which one*. That's why `[19]` is right in one and off the end of the array in the other. This confusion had me writing `behaviourTable[19]` as an index more than once.

**The `if` inside the `for` instead of around it.** The backup has to happen once, and I'd put the check inside the loop:

- iteration with `i` at 0: the bool is false, copy row 0, set the bool to true
- iteration with `i` at 1: the bool is now true, skip
- iterations 2 to 18: skip, skip, skip

One row copied and eighteen empty slots. The `for` is the repetition; the question "have I done this already?" is asked once, up front, so it belongs outside. First decide whether to do the work, then do all of the work, then record that it's done.

The trick that found the bug costs ten seconds: **recite the first iteration, the second, and the last.** Loop bugs are almost never writing errors — the code is right as a sentence and wrong as a story, and a story is easier to hear out loud than to see on screen.

**The uninitialised seed.**

```cpp
s32 seed;              // declared and never assigned
Random random(seed);   // ...and passed like that
```

The seed was whatever happened to be lying in that bit of stack. It is **exactly** the bug I'd found in someone else's file half an hour earlier, reproduced in mine.

**The log printing the object instead of the number.** I was passing `random` in place of the drawn number, and out came nineteen lines with the same value, `-2143711312`, which is a stack address, and which changed between races.

The fix isn't cosmetic: **the drawn number has to go in a variable.** `NextLimited` isn't a question, it's an action: every call advances the sequence. Calling it once inside the brackets and once in the log would make the log report a different number from the one actually used.

And in C there are no strings to add with `+`: you write placeholders in the text and pass the values after, in order — `%d` decimal, `%x` hex, `%s` text. The number of placeholders has to match the values passed: two `%d` and one value, and the second one gets picked out of random memory.

**Two logs with the same text.** When one came out I couldn't tell which of the two it was. They have to read differently, and one belongs **before** the `if`, not inside it: that separates "the function never ran" from "it ran but the condition was false". Two different bugs, found in two different places.

**Two nested loops instead of two loops in a row.** Shuffling and dealing are separate jobs, and by nesting them I was doing 361 swaps instead of 19, with the write into the table sitting in the middle of the shuffle. The right shape is two `for` loops one under the other.

And a classic inside the classic: `for (int j = 0; j < 19; i++)`, which advances `i` instead of `j`. The compiler says nothing, because it's a legal sentence.

### Two missing symbols, same procedure

`isVSRace` and `__vt__6Random` weren't in `symbols.txt`. The header comment gives you the address, but **a comment is not the linker**: if the symbol isn't in the list, the link fails.

```
isVSRace__9DriverMgr = 0x809c38ba
__vt__6Random        = 0x808b42e0
```

The second one is the vtable. `Random` has a virtual destructor, so every object carries a pointer to the virtual function table, and the constructor is what writes it in: creating a `Random` means referring to that address.

The move is mechanical by now: **the error gives you the name, the header gives you the address, the list joins them.**

### A bug found in someone else's house

`PulsarEngine/Extensions/LECODE/XPF.cpp`, the function that picks the random scenario:

```cpp
s32 seed;                          // line 29
if(private room) {
    ...
    u32 seed;                      // line 33 — same name
    ...
    seed = ...;                    // writes into THIS one
}
else {
    seed = ...tick...;             // writes into the outer one
}
Random random(seed);               // reads the outer one
```

Two variables with the same name: the inner one **shadows** the outer one for the whole block. In the private-room branch the write goes into the inner variable, which dies at the closing brace, and the constructor reads the outer one, never assigned.

So in private rooms that seed is garbage, which is precisely what the function is trying to avoid. One to report to whoever maintains Pulsar, along with the build script patch.

### What the randomizer still needs

**The real shuffle**, i.e. swapping rows pairwise instead of drawing, with the bag shrinking as you go: the swap partner is drawn only from the positions not yet settled, `i + NextLimited(19 - i)`. That's what stops duplicates from being born in the first place.

A swap needs a holding variable: `a = b` followed by `b = a` swaps nothing, because the first assignment has already destroyed `a` and you're left with two copies of `b`.

And the last step, which is part of the work rather than tidying up: **remove the log.** A log that prints the map makes the whole feature pointless, because anyone who opens the console knows in advance what each item does. A comment goes in its place, or in two months I'll put it back to debug something else and never notice.

### What I took away from the break

That coming back doesn't cost you the syntax, it costs you the **method**. The things that unblocked me today weren't C++ facts: they were reciting loop iterations out loud, printing instead of playing, keeping two jobs in two separate loops, and running the neutral test whose expected result is that nothing changes.

---

## What's still open

- Testing the mushroom→bullet hook **in a race**: it links, but I haven't watched it fire yet
- Two races back to back, to check `RaceLoadHook` really resets on *retry* too
- The speedometer's left/right setting: still a dead line from day 1
- The phone kit: Winlator compiles (the 32-bit compiler runs), the .NET linker doesn't — so it's native ARM64 `Kamek` in Termux, or getting someone else to link the `.o` files
- A disassembly listing of `StaticR.rel` as a debugger substitute while travelling

## What's next

**Bitwise operators.** In four days I never wrote a `&`, a `|`, a `<<`. That's not academic: in an engine with no `vector` and no `set`, **bits are the data structure.** They were already everywhere in things I'd touched without noticing:

```cpp
u16 bitfield; /* 0xc
1 = 0x2:  has inventory item
2 = 0x4: is releasing dragged item     <-- ItemPlayer.hpp
```

Then, as the exercise: **remapping the controls.** `Input::State::buttonActions` is already abstracted from the physical controller — five bits to permute. One single place, testable in Time Trial, and the effect either happens or it doesn't. (Careful not to remap `GhostController` and `AIController` too, which read the same struct.)

And the rest of the PowerPC ABI: which registers are arguments (`r3`–`r10`), which are scratch (`r0`, `r11`, `r12`), which a function must give back untouched (`r14` up), and how to read a stack frame from `r1`. That's what turns a `StaticR.rel` listing into something readable without a debugger.

Further out: the driving parameter block at `0x808B5xxx` (thresholds and multipliers, poked with `kmWrite`, where a mistake is *visible* instead of crashing), and hiding the original tracks from online (`CupsConfig::RandomizeTrack()` already has the right branch written).

---

## The north star: 24 players

This isn't my next task and it won't be for a long time. It's the thing I'm aiming at, and I keep it written down because it's what I use to choose: every new topic gets judged partly on how much closer it gets me to this.

The thing is, **I don't know whether it's possible**, and half the work is finding out. So instead of a plan, what follows is the questions.

### What I thought I knew, and don't

**"There are 78 `[12]` arrays."** That's the output of a `grep`, not an inventory. It says something much weaker than it sounds:

| | |
|---|---|
| 78 | `[12]` in the `GameSource/` headers (across 39 files) |
| +36 | more in `PulsarEngine/`, i.e. in the engine code, which is ours |
| ? | how many of those 78 are genuinely *one element per player*, and how many are a 12-byte buffer that has nothing to do with it |
| ? | how many are spelled some other way that grep misses — `[0xC]` already exists in the repo, and an array sized by a named constant won't turn up when you search for `12` |

And above all, the category **no grep can see**: the 12s written inside instructions. A `cmpwi r3, 12` or a `li r0, 12` in the game's binary isn't in any header. Those only turn up by disassembling, and I have no idea how many there are.

> The real number isn't 78. I don't know what it is, and finding out means going through them one by one.

**"The real constraint is memory."** I keep saying it, but I've never measured it. I don't know:

- how much a single player costs **today**, all in — kart, Mii, items, physics, AI
- what actually scales with player count and what's fixed
- how much free memory is left in a heavy 12-player race
- whether the ceiling is MEM1 or whether there's room somewhere else

Until I have a number for the first line, "24 players" isn't a project, it's a wish. It's the **first question to answer**, because if the answer there is no, nothing else matters.

### The other open questions

**What breaks first?** Memory, framerate, or something structural I haven't seen yet — a player index stored in half a byte, say, which simply doesn't fit 24. I don't even know what order to find out in.

**Is offline really the easier step?** It isolates the problem from the network, that much is certain. But 24 CPUs computing routes might cost more than 24 humans arriving over the wire. I don't know whether the AI scales worse than everything else, and whether it's the real wall.

**And the UI?** Minimap, in-race standings, results screen: all drawn for 12. I don't know whether that's a layout problem, an array problem, or both.

**Online I'm ruling out from the start** — the packet format is built around 12, and that isn't my code.

### Why I keep it as the north star

Because it's the only goal I have that **can't be solved with a hook**. It touches memory, data structures and disassembly, and it forces me to measure instead of trying things. Everything I'm learning right now — bits, the ABI, reading a listing without a debugger — feeds into it, and having the destination written down is what makes me pick what to study next.

And if the final answer is "it doesn't fit in memory", that's fine too: I'll have got there **by measuring, not by guessing.**

---

## The tally

From `int test = 5` to a hook placed at an address I found myself. In between: `.` vs `->`, references, static members, the contract between two functions, scope and shadowing, `=` vs `==`, the ternary, `this`, count vs index, the three meanings of `static`, command vs question, open namespaces, virtual vs non-virtual, `bl` vs `b`, registers as a function signature, DSI vs ISI, `lis` + offset, `lr − 4`, the prologue, mangled names, `switch` and fallthrough, when hooks get registered, a debugger opened for the first time, and a build script fixed so it would stop lying.

And above all: **I stopped guessing.** The last problems I solved by reasoning, and on some of them I already had the answer before the confirmation came.

---

## If you're about to start

Four things I'd tell myself on day 0:

1. **The OOP you already know transfers.** If you've done any Java, you're much closer than you think. What's missing is a short list of differences, not a new way of thinking.
2. **Don't learn the STL for this.** It isn't here.
3. **Read the errors that scroll past.** In this build system they're the only thing that matters, and they're easy to miss.
4. **When nothing happens, don't debug the logic.** Debug the chain. Four out of my four "impossible" bugs were a change that never reached the game.

It's a hell. It's a very fun hell. Come in.
