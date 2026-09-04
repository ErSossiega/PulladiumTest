# Recap

Everything I've learned so far, from the beginning up to `inline`, in one place.

This isn't the manual — that one is for looking something up while you work. This is for getting my
head back in gear after a break, so it's ordered the way I ran into things rather than
alphabetically, and each rule carries the reason I learned it.

Addresses are PAL (RMCP).

---

## 1. Where I started from

C++ up to pointers, two years of Java at school, zero experience with the engine. And the discovery
that shortened everything: **the OOP was already there.** All that was missing was the list of
differences, and it's short.

| Thing | Java | C++ |
|---|---|---|
| Objects | everything is a reference | three ways: by value, `*`, `&` |
| Access | always `.` | `.` on values and references, `->` on pointers |
| `new` | the GC frees it | **you** have to `delete` |
| Destructors | don't exist | `~Name()` |
| Files | one class = one file | `.hpp` (declare) + `.cpp` (write) |
| Imports | `import` | `#include` + include guard |
| Integers | `int` and that's it | `u8 u16 u32 s16 s32 f32` |
| `namespace` | `package` | same idea, with `::` |

And the part not to study: the whole STL. There's no `std::string` here, no `vector`, no `iostream`.
It runs on a Wii with 24 MB of RAM.

---

## 2. The language traps that got me

I've put them in one list because they're nearly all the same family: **legal and silent.** The
compiler says nothing and the game does something other than what you meant.

### You read the star, you don't guess it

| Declaration | What it is | Access |
|---|---|---|
| `Type x` | the object | `.` |
| `Type& x` | a reference | `.` |
| `Type* x` | an address | `->` |

`a->b` **is** `(*a).b`. `this` is always a pointer, so always `->`.
Getting this wrong isn't dangerous: it won't compile.

But count **how many** stars:

```cpp
Kart::Player** players;   // players[i] is a POINTER -> ->
Item::Player*  players;   // players[i] is the OBJECT -> .
```

### `=` writes, `==` asks

```cpp
if(count = 2)    // assigns 2, the condition is ALWAYS true, and count has changed
```

In Java the compiler rejected this. In C++ it's legal, because any non-zero number counts as *true*.
And as a bonus it changes the variable controlling the loop three lines down.

### `void` means command

> If a function returns `void`, it **does** something. It can't answer, so it can't live in an `if`.

Putting `UseStar()` inside a condition doesn't ask whether the star was used: **it fires it**, sixty
times a second. Ask questions of something that has an answer — a field, a getter.

**Reading is free. Calling isn't.**

### Shadowing

```cpp
u8 speedoType = ...;      // the real one
if(...){
    u8 speedoType = ...;  // a SECOND variable, dies at the }
}
```

Java rejects it. C++ allows it, silently.

### The variable that initialises itself

```cpp
RaceLoadHook restartStar(restartStar);
```

The name you're declaring is **already in scope inside its own parentheses**. That `restartStar`
isn't the function: it's the variable I'm creating right now.

Same family: `void MyMushroom(Kart::Movement& Movement)` — from there on `Movement` is the variable,
not the class.

### Namespaces are open

```cpp
namespace pulsar { ... }   // NOT Pulsar
```

Getting the capital wrong isn't an error the compiler rejects: **it creates a new, empty one.** Name
lookup walks outwards, and `Pulsar::Settings` never gets looked at.

### The three `static`s

| Where | What it means |
|---|---|
| in a **class** | one copy shared by every instance |
| **inside a function** | the variable **survives** between calls |
| **outside everything**, in the file | visible only in this file |

For a class member, `static` goes **only in the declaration** in the `.hpp`. In the `.cpp` you define
it qualified with the class name and **without** repeating `static`.

*(`static` variables are born zeroed, guaranteed: `static bool x;` is already `false`.)*

### `switch`: `break` isn't optional

Without it, execution **falls through** into the next case, and the compiler won't warn you.
`default` covers everything else — and often solves the "none" case for free.

### Declaration order matters

If a function is defined **below** whoever calls it, that's an error. The compiler reads top to
bottom and doesn't know where to go looking.

---

## 3. How the project is laid out

### `GameSource/`: two different operations

It isn't "don't touch it". Two unrelated things hide under that phrase.

**A — adding documentation.** Declaring a function that **already exists** in the game but nobody
had written into the header, fixing a name, adding a comment, noting an address you found yourself
in the debugger:

```cpp
+ void SetItemWithCount(ItemId id, int count, bool isForced);  //807bc908
```

That changes nothing at runtime. You're only telling my compiler what lives at that address.
**Zero risk, always allowed**, and it's how the headers grow.

And sometimes it's necessary, because `GameSource/` is hand-written documentation (by melg) rather
than an official artefact, and it isn't the same thing as the decompilation project: **classes can be
missing and names can be wrong.** People have had to add whole groups of functions, and fix a
mis-named variable, before their own code would work. So when something I need isn't there, the
answer may be that nobody has written it yet.

**B — changing the layout.** A new field, a bigger array. *That* changes `sizeof`, and `size_assert`
stops the build across dozens of files.

> Those aren't 60 errors. It's **one error, reported 60 times.** Read the first one, ignore the echo.

The alarm is there for a precise reason: the game's **compiled** code reaches fields at fixed
offsets, decided in 2008. Widen a struct the game itself allocates and indexes, and that code carries
on using the old offsets — the field you moved lands where the game isn't looking, and the game
writes where you aren't expecting. It doesn't crash immediately: it does wrong things in unrelated
places.

> B **can be done** — `mkw-item-expansion` widens `ItemId items[19]` to `[27]` and switches off seven
> `size_assert`s to manage it. But alongside the headers it takes ~3700 lines of new code in
> `PulsarEngine/` to hold them up. It isn't a header edit: it's a project of which the headers are
> line zero.

My `s16 mtCharge;` in `Kart::Pointers` was a B done believing it was an A.

**`PulsarEngine/` is mine**, extend it as much as I like — even when it inherits from a game class:
new fields land **at the end**, after the part the game knows about.

> You can extend a game class **at the end**, by inheriting from it. You can never change it
> **in the middle**.

**The `Count()` / `Create()` contract.** The engine asks how many slots you need, then hands them
back. `count` means *number of controls*, not *number of players* — as long as those two numbers
match nobody notices, and the day you split them everything falls out.

And the general rule that came out of it: **decide first, act once.** A `switch` that picks only the
value and then does a single `SetItem` beats three branches repeating the same thing.

---

## 4. Working out an address

PowerPC **can't load 32 bits in a single instruction.** It always splits them in two.

```
8057efec   lis  r3, 0x808B        -> r3 = 0x808B0000
8057eff0   lha  r3, 0x5CC6 (r3)   -> reads at 0x808B0000 + 0x5CC6
8057eff4   blr
```

> **address = (the `lis` value << 16) + the offset on the next instruction**

`lis` = *Load Immediate Shifted*, it puts the value in the top 16 bits. What follows adds its own
offset.

### The offset is signed (the correct version)

It's a **16-bit two's complement** field. If the **raw** value starts with **8–F** it's negative:
subtract `0x10000`.

```
lwz  r3, 0xC000 (r3)    raw 0xC000 -> 0xC000 - 0x10000 = -0x4000
```

But Dolphin has **already done** that step and prints `-0x4000`. So the leading-digit rule only
applies to raw hex: if the listing already shows a minus, subtract and move on.

### The three binaries

| Range | Binary |
|---|---|
| `80004000` – `~80388000` | main.dol |
| `~805102E0` – `~808D9A58` | StaticR.rel — race, UI, kart |
| `~809BD6E8` upwards | data and variables, not code |

**Don't disassemble the `StaticR.rel` file**: it's relocatable, the addresses on disk aren't the
final ones. You need the debugger, which reads RAM with the game running.

---

## 5. The registers

They have **no** relationship to the game's concepts. They're 32 generic slots. What does exist is a
convention, valid **at call boundaries**:

| Register | Role |
|---|---|
| `r1` | stack pointer, leave it alone |
| **`r3`–`r10`** | **arguments**, in that order |
| **`r3`** | and the **return value** too |
| `r0`, `r11`, `r12` | scratch |
| `r14`–`r31` | preserved: whoever uses them puts them back |
| `lr` | return address |

**End of a function** — fixed shape: `lwz r0, ...(r1)` · `mtlr r0` · `addi r1, r1, ...` · `blr`.
Spotting it tells you where one function ends and the next begins.

**Function with no arguments**: if the **first** instruction *writes* to `r3` instead of reading it,
nobody passed anything in.

---

## 6. The hooks

| Macro | What it writes | When |
|---|---|---|
| `kmCall` | `bl` — go **and come back** | hijack **one call** in the middle of the code |
| `kmBranch` | `b` — go **and stay** | jump away and don't come back (usually: replace a function) |
| `kmWrite16/32` | a raw value | change **a variable** or an instruction |
| `kmWriteNop` | `60000000` | delete an instruction |
| `RaceLoadHook`, `RaceFrameHook`, `BootHook`, `SectionLoadHook` | — | **no address at all** |

> Before hunting for an address, check whether the moment you care about **already has a hook waiting
> for you**.

The no-address hooks are **types**, like `int`: two hooks = two variables of the same type, not two
classes.

### The signature is the register map

```cpp
void MegaTC(Kart::Movement& movement, int frames, int unk0, int unk1)
//          ^ r3               ^ r4        ^ r5      ^ r6
```

- A method's `this` becomes the **first explicit parameter**
- Declare **all** the originals, even the ones you ignore: they hold the slot
- Don't invent new ones: they'd read registers holding something else

> A hook that doesn't call the original when its condition is false doesn't extend the game:
> **it breaks it.**

### `kmCall` on the start of a function = crash

`bl` notes down where to return. Your function does `blr` and re-enters **the middle of the
original**, with `r3` full of your return value instead of the pointer that code expects. Immediate
DSI, with your number visible in `r3` in the dump.

### And the day-3 lesson

> **You can't make the registration conditional. You make the behaviour conditional.**

Hooks register **at boot, once**, before any setting means anything. And a hook declared inside a
function dies at the `}`, leaving a pointer to memory that no longer exists in the list.

Welcome side effect: if you read the setting **inside**, changing it between races works immediately.

---

## 7. Finding an address yourself

Headers list the **starts** of functions, because a start is unique. A *call* isn't: the same
function can be called from ten places, and none of the ten has a name.

For `kmCall` you need the address of a **`bl`**. So:

| | Move | Result |
|---|---|---|
| 1 | Breakpoint on the function's start (from the header comment) | it fires -> it really is called |
| 2 | Make the thing happen in a race | the breakpoint fires |
| 3 | Read `lr` | e.g. `0x80798668` |
| 4 | **`lr − 4`** | `0x80798664` <- the address for the `kmCall` |

The reason is mechanical: executing a `bl` at address A, the CPU puts **A + 4** in `lr` — where to
come back to — and jumps. So `lr` is the instruction *after* the call.

**The neighbourhood tells you where you landed**: if your address sits among `Item::Player` symbols,
you're in the item code, not the driving. And to find where the enclosing function starts, scroll
back to the **prologue** (`mflr r0` / `stwu r1, -0x??(r1)`).

**Careful:** the breakpoint tells you where it came from *this time*. If you need one specific site,
make it fire in the right context.

---

## 8. `symbols.txt`

It's there to **call** the game's functions from your code. Hooks don't go through it.

```
mangled_name = 0xADDRESS
```

One line comes from two places: the name from the linker failing, the number **from the header
comment** (with `0x` in front). There's nothing to compute.

> `symbols.txt` is a **phone book**: name <-> number.

**Don't write the names by hand.** Declare the function, call it, compile: the linker fails and
prints the exact string. If you get the signature wrong the name changes, and the error looks
absurd — compare it **character by character**.

The rules, if you want to read them:

```
UseItem__Q24Item9PlayerObjFb
  UseItem  __  Q2  4Item  9PlayerObj  F  b
= Item::PlayerObj::UseItem(bool)
```

The number before each name is its length. `v` void · `b` bool · `i` int · `Uc` u8 · `Us` u16 ·
`f` float · `P` pointer · `C` const · `R` reference.

### Virtual or not: who needs the symbol

```cpp
void ActivateMushroom();       //8057f3d8   <- ordinary method: the symbol IS needed
virtual void ActivateMega();   //0x1c       <- virtual: it isn't
```

> If the call goes through the **vtable**, the game resolves the address at runtime. If it's a `bl`
> to a fixed address, you're the one who has to supply it.

And watch out: the comment next to a virtual method **isn't an address**, it's the vtable offset.
Two numbers that look alike and aren't the same thing.

---

## 9. The regions

What you write in `symbols.txt` is a **PAL (RMCP)** address, the base version. The others are handled
by `versions.txt`, which isn't a per-symbol table but a table of **ranges**: a PAL address falls in a
range and picks up that delta. So the translation is free.

And it isn't one global shift: two nearby functions can get different deltas, because the code was
**recompiled**, not moved as a block.

**How it breaks:** if a PAL address lands in a **hole** in the table, the linker leaves it as is. No
error, no warning — on the other region the game calls an address that over there is something else
entirely.

> If it works on PAL and only crashes on NTSC, the first place to look is `versions.txt`.

Note: this applies to addresses handed to hooks. Ones written **inside** your own code and cast to a
pointer are **not** remapped.

---

## 10. Reading a crash

Don't decode it by hand: the **Pack Creator** has a crash window, you drop the `Crash.pul` on it and
it prints everything — the exception type already named, `SRR0` and `LR` **with symbols resolved**,
every register, and ten stack frames.

| | What the CPU tried to do | Where to look |
|---|---|---|
| **DSI** | read/write **data** at an address that doesn't exist | `srr0` is a real instruction; the rotten address is **in a register** |
| **ISI** | execute **code** where there isn't any | `srr0` **is** the garbage; look at **`lr`** |

**DSI = bad pointer. ISI = bad jump.**

And in both cases, a register holding a number that's recognisably **yours** says your code has just
been through there.

---

## 11. The debugger, in three moves

1. **A breakpoint answers yes or no.** It fires -> the function is being called. It never fires ->
   you're hooking the wrong place, and no amount of reasoning about the code would have told you.
2. **Breakpoint on the `blr`, then read `r3`.** That's where PowerPC keeps return values.
3. **Memory view** with emulation **stopped on a breakpoint**. Without a symbol map every name reads
   `unk` and you navigate by address alone — so **load `RMCP01.map` first**: Dolphin then shows real
   demangled names across StaticR.rel, and most of the manual orienting stops being necessary.

> And **before** opening the debugger: **try an absurd value.** A `1` in place of a `270` gives an
> effect you cannot mistake for anything else. If nothing changes even then, the hook isn't firing —
> and that's a different problem.

Separating *"doesn't fire"* from *"fires but the value doesn't matter"* is the move that unblocks
things most often.

---

## 12. When NOTHING happens

> When the effect is **none** rather than **wrong**, the problem is almost never in the logic. It's
> in the chain.

0. **Does the build even know this file exists?** (`.cpp` extension, in a folder the glob covers)
1. **Saved?** The build reads from disk, not from your editor.
2. **Compiled?** `ls -la build/YourFile.o` — it has to be newer than the `.cpp`.
3. **Linked?** *"No source or header files were modified"* right after you modified something isn't
   an offer: it's a warning.
4. **Copied?** Is the destination folder the pack you're actually launching?
5. **Reloaded?** Riivolution reads the files when the ISO starts.

And the flaw that caught me three times out of four: **a compile error can be silent.** If nothing
checks the compiler's exit code, the old object stays, the link succeeds, and you end up with a
`Code.pul` with a fresh timestamp and yesterday's code inside.

The four cases, all the same shape:

| | The change didn't arrive because |
|---|---|
| the speedometer's `pos` | it was on a dead line (`if(count == 1)` with `count` already 2) |
| the `kmBranch` | the `.o` hadn't been recompiled |
| the star | the notebook was erased in the same frame |
| the mushroom hook | the file was called `MUSHROOOOMS`, with no `.cpp` |

---

## 13. `inline` — the new bit

In the headers you find comments like:

```cpp
static ItemId RandomizeRouletteItem(RouletteItems*, ItemId prev); //807baed4 inlined
```

**Inlined does NOT mean it's never called.** It means the opposite: it runs all right, but the *call*
doesn't exist.

When the function is small, instead of emitting a `bl` to it the compiler **copies its body into
whoever uses it**:

```
Not inlined                      Inlined
─────────────                    ────────
caller:                          caller:
  bl  Function        <-->         [ the body, copied in here ]
```

It does this for speed: a call costs something (saving `lr`, opening the stack frame, jumping,
returning, closing back up), and for a three-line function that ceremony costs more than the actual
work.

**The two consequences:**

1. **There are N copies, not one** — one per place it was called. There's no longer a single point to
   get in the way of: to change that behaviour you have to find and modify all of them.
2. **And the copies can differ from each other.** Once pasted, the compiler optimises the body *in
   that caller's context*: if a parameter is always `0` there, the branches depending on it vanish.
   Two copies of the same function can have different lengths and different instructions.

Put another way: it isn't "phoning a friend and asking them to do something", it's "copying their
instructions into your notebook and doing it yourself". The work gets done either way, but there's no
phone call to intercept.

C++ does have an `inline` keyword, but **it's only a hint**: the compiler decides, and it inlines
things you never marked — typically the little functions defined inside headers.

*(Whether the address a header lists actually holds a reachable copy is something only a breakpoint
answers. If it never fires, you have your answer.)*

---

## Where the rest is

| File | What it's for |
|---|---|
| `KAMEK-MANUAL.md` | the reference to consult while working |
| `Four-Days-Inside-Pulsar.md` | how I got here, with every mistake |
| `TODO.md` | what's next, and in what order |
