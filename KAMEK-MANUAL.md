# Kamek Manual

A practical reference for Kamek on Mario Kart Wii: how to find an address, how to pick the right
hook, and what to do when the thing you wrote does absolutely nothing.

It's ordered the way you actually hit these problems, not the way a tutorial would teach them. Skim
the headings and jump to whichever one you're stuck on.

Addresses verified on **Pulladium 2.1.1**, Mario Kart Wii **PAL (RMCP)**.

---

## 1. The three questions, in this order

Almost every afternoon you're going to lose comes from skipping one of these, or from asking them
out of order. They look obvious written down. They're much less obvious at 2am.

### 1.1 Where is the thing I want to change?

In the headers under `GameSource/`. Every function has its address sitting right there in the
comment, and that's it — that **is** the map of Mario Kart Wii. There's no other file to go hunting
for, no `.map` of the game somewhere, nothing to download.

```cpp
s32 GetMTMaxCharge() const;   //8057efe0
s16 driftState;               //0xfc
s16 mtCharge;                 //0xfe
```

One thing to watch: **namesakes**. `GetMTMaxCharge` exists in `KartMovement.hpp` at `8057efe0` and
again in `KartLink.hpp` at `80591208`. Same name, different function, different address. Searching
by name and grabbing the first hit is how you end up hooking something you've never heard of.

### 1.2 What's actually at that address?

Don't assume — open it in the debugger and read the instructions. It takes twenty seconds and it
saves you the whole afternoon.

The reason it matters: a function that sounds like it decides something is very often a
three-instruction getter that just reads a variable. Those two cases look identical from the header
and get fixed in completely opposite ways.

> **Hooking a getter only changes things for whoever calls the getter.** And the code that actually
> runs every frame usually reads the variable directly, never going through it. So your hook is
> perfectly correct, perfectly installed, and nothing happens.

### 1.3 What kind of hook do I need?

You don't really choose this one — it falls out of the answer to 1.2. Section 4 turns that answer
into a macro.

---

## 2. Working out an address

PowerPC can't load a 32-bit address in one instruction. There isn't room in the encoding, so it
always splits the job in two: the top half first, then the bottom half as an offset on the
instruction after.

```
8057efec   lis  r3, 0x808B        -> r3 = 0x808B0000
8057eff0   lha  r3, 0x5CC6 (r3)   -> reads at 0x808B0000 + 0x5CC6
8057eff4   blr                    -> hands the value back in r3
```

> **address = (the `lis` value << 16) + the offset on the next instruction**
>
> `0x808B0000 + 0x5CC6 = 0x808B5CC6`. You're not guessing, you're adding.

`lis` is *Load Immediate Shifted* — it drops the value into the top 16 bits and leaves the bottom
half at zero. Whatever comes next (`lha`, `lwz`, `lfs`, `stw`, take your pick) adds its own offset
on top.

### Careful: the offset is signed

It's a **16-bit two's complement** field, which is a fancy way of saying the top half of its range
is really negative numbers. If the **raw** value starts with **8, 9, A, B, C, D, E or F**, subtract
`0x10000` from it and use that instead:

```
lis  r3, 0x808C
lwz  r3, 0xC000 (r3)    raw 0xC000 -> starts with C -> 0xC000 - 0x10000 = -0x4000
                        -> 0x808C0000 - 0x4000 = 0x808BC000
```

In practice you'll rarely do this by hand, because Dolphin and most disassemblers have already done
it for you and print the offset **with the sign already applied**, as `-0x4000`. So the
leading-digit rule is really for when you're reading raw hex out of the bytes. If there's a minus
in front of it in the listing, that's the whole story — just subtract and move on.

Get this backwards and you land 64 KB away from where you meant to be, right in the middle of
somebody else's data. It'll even look plausible.

### Which binary you're in

| Range | Binary | What lives there |
|---|---|---|
| `80004000` – `~80388000` | main.dol | system skeleton, libc |
| `~805102E0` – `~808D9A58` | StaticR.rel | race, UI, kart — nearly everything you care about |
| `~809BD6E8` upwards | data / BSS | global variables, not code |

One warning that will save you a lot of confusion: **don't try to disassemble the `StaticR.rel`
file directly.** It's relocatable, so the addresses sitting in it on disk aren't the final ones and
won't line up with the comments in the headers. Use Dolphin's debugger instead, which reads RAM with
the game actually running.

(If you really do need a static listing: **branch targets** survive in the file just fine, but the
**immediates on `lis`/`lha`** don't — they're zeroed out, and the relocations fill them in at load
time. Which is exactly the half you needed.)

---

## 3. The registers

First thing to get straight: registers have **nothing to do with the game's concepts.** They're 32
generic slots. The same `r3` holds a pointer to a kart one instant and a loop counter the next.
Don't go looking for meaning in them.

What does exist is a **convention**, and it holds **at function call boundaries**:

| Register | Role |
|---|---|
| `r0` | scratch. In some instructions it literally means **the number zero**, not the register |
| `r1` | **stack pointer**. Leave it alone |
| `r2`, `r13` | pointers to the small data areas |
| **`r3`–`r10`** | **arguments**, in that order |
| **`r3`** | and the **return value** as well |
| `r11`, `r12` | scratch |
| **`r14`–`r31`** | **preserved** — if a function uses them it has to put them back |
| `lr` | return address |
| `cr` | where comparisons leave their result |

Floats use `f1`–`f8` for arguments and the return value.

### Spotting the end of a function

```
80580634   lwz   r0, 0x14(r1)     <- grabs the return address back off the stack
80580638   lwz   r31, 0xc(r1)     <- restores r31, which it promised to preserve
8058063c   mtlr  r0               <- puts it back in lr
80580640   addi  r1, r1, 0x10     <- gives back the stack space
80580644   blr                    <- and returns
```

It's always this shape. Once you can spot it you can tell at a glance where one function stops and
the next one starts, which is most of what you need to navigate a listing with no symbol names.

### Spotting a function that takes no arguments

```
80790e3c   lis  r3, 0x809D    <- FIRST instruction, and it overwrites r3
```

> If the very first instruction **writes** to `r3` instead of reading it, nobody passed anything in.
> The function takes no arguments.

---

## 4. Which hook to use

| Macro | What it writes | Use it to |
|---|---|---|
| `kmCall(addr, fn)` | `bl` — go **and come back** | hijack **one call** in the middle of a function |
| `kmBranch(addr, fn)` | `b` — go **and stay** | replace **a whole function**, from its first instruction |
| `kmWrite16/32(addr, val)` | a raw value | change **a variable**, or a single instruction |
| `kmWriteNop(addr)` | `60000000` | delete an instruction |

### The ones that don't need an address at all

These live at the bottom of `KamekInclude/kamek.hpp`. Write a `void` function with no arguments,
then one line to register it:

```cpp
RaceLoadHook    reset(myFunction);       // when the race loads
RaceFrameHook   tick(otherFunction);     // every frame, 60 times a second
SectionLoadHook sect(thirdFunction);     // on section change
BootHook        boot(fourth, 0);         // at boot, before the REL is even loaded
```

Worth saying out loud, because it trips people up: these are **types**, exactly like `int`. Two
hooks means two variables of the same type — not two classes, not two functions.

> So before you go address-hunting, check whether the moment you care about **already has a hook
> waiting for you.** It's a free afternoon if it does.

### The classic crash: `kmCall` on the start of a function

This one gets everybody once. `bl` writes down where to come back to. Your function does its thing
and hits `blr`, which drops you back **into the middle of the original function** — with `r3` full
of your return value instead of the pointer that code was expecting to find there.

Instant DSI, and your own number sitting in `r3` in the dump, which is at least a nice clear
signature.

### Your function's signature is really the register map

```cpp
// the game was calling: ApplyLightningEffect(frames, unk0, unk1)
void MegaTC(Kart::Movement& movement, int frames, int unk0, int unk1)
//          ^ r3               ^ r4        ^ r5      ^ r6
```

Three rules, and they're not negotiable:

- A method's `this` becomes the **first explicit parameter**
- Declare **all** the original parameters, even the ones you'll never touch — they're holding a slot
- Don't invent extra ones. They'd read registers that are holding something else entirely

> And if your condition is false, **call the original.** A hook that doesn't isn't extending the
> game, it's deleting a piece of it. That `else` branch isn't pedantry.

### These addresses are PAL

- Anything you hand to `kmCall` / `kmWrite` / `kmBranch` gets **remapped** by Kamek through
  `versions.txt`, so other regions come for free
- Anything you write **inside** your own code and cast to a pointer does **not**. It stays PAL, and
  on another region it points somewhere meaningless

If something works on PAL and only dies on NTSC, that's the first place to look.

---

## 5. `symbols.txt`

This is what lets you **call** the game's own functions from your code — it's the `-externals` of
the link command. Hooks don't go through it at all; those just take the number directly.

```
mangled_name = 0xADDRESS
```

The `##RVL##`, `##EGG` and friends are just comments for humans reading the file.

### Reading a mangled name

They look like line noise, but they're completely mechanical:

```
UseItem__Q24Item9PlayerObjFb
```

| Piece | What it means |
|---|---|
| `UseItem` | the function name |
| `__` | encoding starts here |
| `Q2` | it's a **qualified** name with **2** components |
| `4Item` | 4 characters -> `Item` |
| `9PlayerObj` | 9 characters -> `PlayerObj` |
| `F` | it's a function; parameters follow |
| `b` | one `bool` |

Which reads back as `Item::PlayerObj::UseItem(bool)`.

The number in front of each name is just its length. The type codes you'll keep seeing: `v` void ·
`b` bool · `i` int · `Uc` u8 · `Us` u16 · `Ui` u32 · `f` float · `P` pointer to… · `C` const ·
`R` reference to… · `e` ellipsis (`...`).

There's a **`demangler.py`** in the root for the long horrible ones.

### But you don't have to write them by hand

Much easier: declare the function with the right namespace, class and signature, call it, and
compile. **The linker will fail and print the exact string it couldn't find.** Copy that.

And if you get the signature wrong, the mangled name changes, so the linker goes looking for a
symbol that isn't the one you carefully added to the file. The error looks completely insane at that
point — *"but it's right there!"* — so compare the two strings **character by character**. It's
usually one letter.

---

## 6. "It just doesn't do anything"

> When a change has **no** effect — not a wrong effect, genuinely nothing — the logic is almost
> never the problem. It's the chain between your editor and the running game.

Walk it in order, it takes two minutes:

1. **Saved?** The build reads from disk, not from your editor's buffer.
2. **Compiled?** `ls -la build/YourFile.o` — it has to be **newer** than the `.cpp`.
3. **Linked?** If the prompt says *"No source or header files were modified"* right after you
   modified something, that isn't an offer. It's a warning, and answering to relink just links the
   old objects again.
4. **Copied?** Does the folder your build copies into actually match the pack you're launching?
5. **Reloaded?** Riivolution reads the files when the ISO starts. Not while it's running.

### Check that your build actually stops on an error

Worth verifying once, whatever you build with, because this one is genuinely nasty:

```python
subprocess.run(cmd, shell=True)   # the exit code goes nowhere
```

If nothing checks the compiler's exit code, then **a compile error is completely silent.** Nothing
stops. The stale `.o` from last time is still sitting in `build/`, so the link succeeds, and you get
a `Code.pul` with a fresh timestamp and yesterday's code inside it. Every single symptom tells you
the change didn't work, and not one of them tells you why.

If that's your situation, those compiler errors scrolling past in the output aren't noise. They're
the only thing on screen that matters.

### Checking what's really in the binary

Hooks land in `Code.pul` as recognisable commands, and the first byte gives away the type:

```
20 ff ff fe   80 57 ef 38   41 82 00 a4   kmWrite32
40 ff ff fe   80 57 ef dc   00 01 83 e4   kmBranch  (b)
41 ff ff fe   80 58 2f dc   00 01 84 2c   kmCall    (bl)
```

So if your source says `kmBranch` and the binary says `41`, you've got your answer: that file never
got compiled.

---

## 7. Dolphin's debugger

You turn it on in settings, under interface. **Code**, **Registers**, **Breakpoints** and
**Memory** show up.

You only need three moves to get real value out of it.

**A breakpoint answers yes or no.** It fires, so the function *is* being called. It never fires, so
you're hooking the wrong place — and no amount of staring at the code was ever going to tell you
that.

**To catch a return value**, put a breakpoint on the `blr` and look at **`r3`**. This works in
reverse too, which is the useful part: in a crash dump, an `r3` holding a number that's recognisably
*yours* means your code went through there.

**To read memory**, use the Memory panel with emulation **stopped on a breakpoint**. With the game
not running there's nothing there to look at. The symbol will say `unk` — that's normal, there's no
map loaded, and you want the contents anyway, not the name.

> One thing before you open the debugger at all: **try an absurd value.** Put a `1` where a `270`
> should be. If nothing changes even then, your hook isn't firing, and that's a completely different
> problem with a completely different fix.

Separating *"it isn't firing"* from *"it fires but the value doesn't matter"* is the single move
that unblocks things most often. Do it first.

---

## 8. Reading a crash

When the game dies it writes a `Crash.pul`. Don't decode it by hand — the Pulsar Pack Creator has a
crash window, and you just drop the file on it. It lays the whole dump out for you:

| | |
|---|---|
| **Error** | `DSI`, `ISI` or `Float` — the exception type, already named |
| **Region** | which version it crashed on |
| **SRR0** / **LR** | the two addresses that matter, **with symbol names resolved** where it knows them |
| **GPRs** / **FPRs** | all 32 of each |
| **Stack Frame** | ten frames of `SP` / `LR`, symbols resolved — the chain of callers |

Those resolved names are the whole reason to use it instead of a hex editor. An `LR` landing in a
function you recognise turns a wall of numbers into an actual sentence.

Then read it like this:

| | What the CPU was trying to do | Where to look |
|---|---|---|
| **DSI** | read or write **data** at an address that doesn't exist | `srr0` is a **real** instruction, some load or store; the rotten address is **in a register** |
| **ISI** | execute **code** where there isn't any | `srr0` **is** the garbage and tells you nothing; look at **`lr`** instead, which says where the jump came from |

Short version: DSI is a bad pointer, ISI is a bad jump.

And in both cases, a register holding a number that's recognisably yours — a return value, a
constant you typed — is your code putting its hand up.

---

## 9. Language traps

The ones that cost real time, roughly in order of how much.

### The `0x` isn't decoration

```cpp
kmWrite16(80591208, 1);     // decimal -> 0x04CDF3A8. Not a wrong address: a DIFFERENT number.
kmWrite16(0x80591208, 1);   // this is the address
```

### `=` writes, `==` asks

```cpp
if (count = 2)    // assigns 2, condition is always true, and count is now different
if (count == 2)   // compares
```

If you're coming from Java, this is one the compiler used to catch for you. In C++ it's perfectly
legal, because any non-zero number counts as true — so it compiles clean and lies to you at runtime.

### `.` or `->`: look for the star

You don't have to remember this, you read it off the declaration:

| Declaration | What it is | Access |
|---|---|---|
| `Type x` | the object | `.` |
| `Type& x` | a reference | `.` |
| `Type* x` | an address | `->` |

`a->b` is literally `(*a).b`, same `*` as in any textbook pointer exercise. `this` is **always** a
pointer, so it's always `->`. And getting it wrong is harmless — it just won't compile.

Do count the stars, though:

```cpp
Kart::Player** players;   // players[i] is a POINTER -> use ->
Item::Player*  players;   // players[i] is the OBJECT -> use .
```

### `void` means it's a command

> If a function returns `void`, it **does** something. It can't answer a question, so it can't go in
> an `if`.

Sticking `UseStar()` inside a condition doesn't ask whether the star got used — **it fires the
star**, sixty times a second. Ask your questions of something that has an answer: a field, a getter.

**Reading is free. Calling isn't.**

### The three meanings of `static`

Same keyword, three unrelated jobs:

| Where you wrote it | What it means |
|---|---|
| in a **class** | one single copy, shared by every instance |
| **inside a function** | the variable **survives** between calls |
| **outside everything**, in the file | visible only in this file |

For a class member, `static` goes **only in the declaration** in the `.hpp`. In the `.cpp` you define
it qualified with the class name and **without** repeating `static`.

### `GameSource/` is off limits

Those structs are the game's memory map, decided by Nintendo in 2008. Add a field and `sizeof`
changes, and `size_assert` will stop the build across dozens of files — which is **one error
reported many times**, so read the first one and ignore the echo.

Your own classes under `PulsarEngine/` are a different story: extend those as much as you like, even
when they inherit from a game class. Your new fields land **at the end**, after the part the game
knows about, so nothing shifts.

> You can extend a game class **at the end**, by inheriting from it. You can never change it
> **in the middle**.

---

## 10. The short version

If you remember nothing else:

- Try the absurd value before you open the debugger
- Read the first error, ignore the echo
- If you took a copy in order to fix it, the copy is now the truth — not the original
- Whoever computes isn't always whoever decides: the getter might not be on the road at all
- Command or question? `void` doesn't answer
- And when the effect is *nothing at all*, go check the chain, not the logic
