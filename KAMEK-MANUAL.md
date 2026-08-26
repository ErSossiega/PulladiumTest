# Kamek Manual

A practical reference for Pulladium / Pulsar on Mario Kart Wii: finding an address, picking the
right hook, working out why nothing happens.

Addresses verified on **Pulladium 2.1.1**, Mario Kart Wii **PAL (RMCP)**.

---

## 1. The three questions, in this order

Almost every lost afternoon comes from skipping one of them, or asking them in the wrong order.

### 1.1 Where is the thing I want to change?

In the headers under `GameSource/`: every function has its address in the comment. **That is the
map of Mario Kart Wii** — there is no other file to go and find, and no `.map` of the game.

```cpp
s32 GetMTMaxCharge() const;   //8057efe0
s16 driftState;               //0xfc
s16 mtCharge;                 //0xfe
```

**Watch out for namesakes.** `GetMTMaxCharge` exists both in `KartMovement.hpp` (`8057efe0`) and in
`KartLink.hpp` (`80591208`): same name, different functions, different addresses.

### 1.2 What is actually at that address?

Don't assume: **open it in the debugger and read the instructions.** A function that looks like it
decides something is often a three-line getter.

This is what separates two cases that get solved in opposite ways: *code* that computes something,
or *a variable* that somebody reads.

> **Hooking a getter changes things only for whoever calls the getter.** The code that runs every
> frame often reads the variable directly, and your hook never touches it. Everything works and
> nothing happens.

### 1.3 What kind of hook do I need?

The answer falls out of 1.2 on its own. The table in section 4 turns it into a macro.

---

## 2. Working out an address

PowerPC **cannot load a 32-bit address in a single instruction**. It always has to split it in two:
the high half first, then the low half as an offset.

```
8057efec   lis  r3, 0x808B        -> r3 = 0x808B0000
8057eff0   lha  r3, 0x5CC6 (r3)   -> reads at 0x808B0000 + 0x5CC6
8057eff4   blr                    -> returns the value in r3
```

> **address = (the `lis` value << 16) + the offset of the next instruction**
>
> `0x808B0000 + 0x5CC6 = 0x808B5CC6`. It's not trial and error, it's a sum.

`lis` = *Load Immediate Shifted*: it puts the value in the top 16 bits. The instructions that follow
(`lha`, `lwz`, `lfs`, `stw`…) add their own offset to it.

### The offset is signed

If it starts with **8, 9, A, B, C, D, E or F** it is negative and has to be **subtracted**:

```
lis  r3, 0x808C
lwz  r3, -0x4000 (r3)   -> 0x808C0000 - 0x4000 = 0x808BC000
```

Adding it instead of subtracting it puts you 64 KB off, in the middle of somebody else's data.

### Which binary they're in

| Range | Binary | What's there |
|---|---|---|
| `80004000` – `~80388000` | main.dol | system skeleton, libc |
| `~805102E0` – `~808D9A58` | StaticR.rel | race, UI, kart — nearly everything |
| `~809BD6E8` upwards | data / BSS | global variables, not code |

**Don't disassemble the `StaticR.rel` file.** It's relocatable: on disk the addresses aren't the
final ones and don't match the comments in the headers. Use Dolphin's debugger, which reads RAM with
the game running.

(If you need a static listing: **branch targets** survive in the file, the **immediates of
`lis`/`lha`** don't — they're zeroed, and the relocations fill them in at load time.)

---

## 3. The registers

Registers **have no relationship whatsoever to the game's concepts.** They are 32 generic slots.
The same `r3` holds a pointer to a kart one instant and a counter the next.

What does exist is a **convention**, which holds **at function call boundaries**:

| Register | Role |
|---|---|
| `r0` | scratch. In some instructions it means **the number zero**, not the register |
| `r1` | **stack pointer**. Don't touch it |
| `r2`, `r13` | pointers to the small data areas |
| **`r3`–`r10`** | **arguments**, in this order |
| **`r3`** | and also the **return value** |
| `r11`, `r12` | scratch |
| **`r14`–`r31`** | **preserved**: whoever uses them must save and restore them |
| `lr` | return address |
| `cr` | result of comparisons |

For floats, `f1`–`f8` as arguments and return value.

### Recognising the end of a function

```
80580634   lwz   r0, 0x14(r1)     <- fetches the return address back off the stack
80580638   lwz   r31, 0xc(r1)     <- restores r31, which is preserved
8058063c   mtlr  r0               <- puts it back into lr
80580640   addi  r1, r1, 0x10     <- frees the stack space
80580644   blr                    <- returns
```

Standard shape. Recognising it tells you at a glance where one function ends and the next begins.

### Recognising a function with no arguments

```
80790e3c   lis  r3, 0x809D    <- FIRST instruction: overwrites r3
```

> If the first instruction **writes** to `r3` instead of reading it, the function takes no arguments.

---

## 4. Which hook

| Macro | What it writes | When |
|---|---|---|
| `kmCall(addr, fn)` | `bl` — go **and come back** | hijack **one call** in the middle of a function |
| `kmBranch(addr, fn)` | `b` — go **and stay** | replace **an entire function**, from its first instruction |
| `kmWrite16/32(addr, val)` | a raw value | change **a variable** or a single instruction |
| `kmWriteNop(addr)` | `60000000` | delete an instruction |

### Hooks with no address

In `KamekInclude/kamek.hpp`, at the bottom. A `void` function with no arguments, one line to
register it:

```cpp
RaceLoadHook    reset(myFunction);       // when the race loads
RaceFrameHook   tick(otherFunction);     // every frame (60/s)
SectionLoadHook sect(thirdFunction);     // on section change
BootHook        boot(fourth, 0);         // at boot (the REL isn't loaded yet)
```

They are **types**, like `int`: two hooks = two variables of the same type, not two classes.

> Before you go hunting for an address, check whether the moment you care about **already has a
> hook waiting for you**.

### `kmCall` on the start of a function: the classic crash

`bl` records where to come back to. Your function does `blr` and re-enters **the middle of the
original function**, with `r3` full of your return value instead of the pointer that code expects.
Immediate DSI, with your number visible in `r3` in the dump.

### The signature is the register map

```cpp
// the game was calling: ApplyLightningEffect(frames, unk0, unk1)
void MegaTC(Kart::Movement& movement, int frames, int unk0, int unk1)
//          ^ r3               ^ r4        ^ r5      ^ r6
```

- A method's `this` becomes the **first explicit parameter**
- Declare **all** the original parameters, even the ones you ignore: they hold the slot
- Don't invent new ones: they'd read registers holding something else entirely

> A hook that doesn't call the original when its condition is false doesn't extend the game:
> **it breaks it.** That's the `else` branch in `MegaTC.cpp`.

### The addresses are PAL

- The ones passed to `kmCall` / `kmWrite` / `kmBranch` get **remapped** by Kamek using
  `versions.txt`
- The ones written **inside** your code, cast to a pointer, **don't**: they stay PAL, and on other
  regions they point at nothing

---

## 5. `symbols.txt`

It's there so you can **call** the game's functions from your own code — it's the `-externals` of
the link command. Hooks **don't** go through it: you give them the number and that's it.

```
mangled_name = 0xADDRESS
```

The `##RVL##`, `##EGG` and similar lines are just comments for humans.

### Reading a mangled name

```
UseItem__Q24Item9PlayerObjFb
```

| Piece | Meaning |
|---|---|
| `UseItem` | the function's name |
| `__` | the encoding starts here |
| `Q2` | a **qualified** name, **2** components |
| `4Item` | 4 characters -> `Item` |
| `9PlayerObj` | 9 characters -> `PlayerObj` |
| `F` | it's a function, the parameters follow |
| `b` | one `bool` |

= `Item::PlayerObj::UseItem(bool)`

The number before each name is its length. Recurring codes: `v` void · `b` bool · `i` int ·
`Uc` u8 · `Us` u16 · `Ui` u32 · `f` float · `P` pointer to… · `C` const · `R` reference to… ·
`e` ellipsis (`...`).

In the root of Pulladium there's **`demangler.py`** for the long ones.

### You don't have to write it by hand

Declare the function with the right namespace, class and signature. Call it. Compile. **The linker
will fail and print the exact string** it can't find: copy that.

If you get the signature wrong, the mangled name changes and the linker looks for a different symbol
from the one you put in the file. The error will look absurd ("but I *did* put it there!"). Compare
it **character by character**.

---

## 6. "It doesn't work": the chain of checks

> When a change has **no** effect — not a wrong effect, none at all — the first suspect is never the
> logic. It's the chain.

1. **Saved?** The build reads the file from disk, not from your editor.
2. **Compiled?** `ls -la build/YourFile.o` — it has to be **newer** than the `.cpp`.
3. **Linked?** If the prompt says *"No source or header files were modified"* right after you
   modified something, that isn't an offer: it's a warning. Answering `L` relinks the old objects.
4. **Copied?** Does `RIIVO` in `BuildPulsar.py` point at the pack you're actually launching?
5. **Reloaded?** Riivolution reads the files when the ISO starts, not while it's running.
6. **The right asset?** If the pack has per-language copies (`Language/ITA/Assets/`), the game might
   be opening the other one.

### The flaw in the build script

```python
def compile_cpp(cpp: str):
    subprocess.run(cmd, shell=True)   # the exit code is never checked
```

**A compile error is silent**: it stops nothing, the old object stays, the link succeeds, and it
hands you a `Code.pul` with a fresh timestamp and yesterday's code inside.

The compiler errors scrolling past in the output aren't noise. They're the only thing that matters.

### Checking what's actually in the binary

Hooks end up in `Code.pul` as recognisable commands — the first byte says the type:

```
20 ff ff fe   80 57 ef 38   41 82 00 a4   kmWrite32
40 ff ff fe   80 57 ef dc   00 01 83 e4   kmBranch  (b)
41 ff ff fe   80 58 2f dc   00 01 84 2c   kmCall    (bl)
```

If the source says `kmBranch` and the binary says `41`, the change was never compiled.

---

## 7. Dolphin's debugger

You turn it on in settings, interface section. **Code**, **Registers**, **Breakpoints** and
**Memory** appear.

**A breakpoint answers yes or no.** It fires -> the function **is** being called. It never fires ->
you're hooking the wrong place, and no amount of reasoning about the code would have told you.

**Reading a return value**: breakpoint on the `blr`, then look at **`r3`**. It works in reverse too —
in a crash, an `r3` holding a number of *yours* says your code has just been through there.

**Looking at memory**: the Memory panel, with emulation **stopped on a breakpoint**. With the game
off there's nothing to read. The symbol name will say `unk`: normal, there's no map loaded — you want
the contents, not the name.

> **Before you open the debugger, try an absurd value.** A `1` in place of a `270` gives an effect
> you cannot mistake for anything else. If nothing changes even with that, the hook isn't firing —
> and that's a different problem, with a different solution.

Separating *"doesn't fire"* from *"fires but the value doesn't matter"* is the move that unblocks
things most often.

---

## 8. Reading a crash

`Crash.pul`, **offset 12** = the exception type.

```bash
python -c "import io,struct; d=io.open(r'PATH/Crash.pul','rb').read(); print('error =', struct.unpack('>I', d[12:16])[0])"
```

**2** = DSI · **3** = ISI · **7** = floating point · **8** = FPE

| | What the CPU tried to do | Where to look |
|---|---|---|
| **DSI** | read or write **data** at an address that doesn't exist | `srr0` is a **real** instruction (a load/store); the rotten address is **in a register** |
| **ISI** | execute **code** where there isn't any | `srr0` **is** the garbage, useless; look at **`lr`**, which tells you where the jump came from |

DSI = wrong pointer. ISI = wrong jump.

The rest of the dump: `srr0` at offset 16, then `srr1`, `msr`, `cr`, `lr`, the 32 GPRs, the 32 FPRs
and finally ten stack frames with `sp`/`lr` — the chain of callers.

---

## 9. Language traps

### The `0x` isn't decoration

```cpp
kmWrite16(80591208, 1);     // decimal -> 0x04CDF3A8. Not a wrong address: a DIFFERENT number.
kmWrite16(0x80591208, 1);   // this is the address
```

### `=` writes, `==` asks

```cpp
if (count = 2)    // assigns 2, the condition is always true, and count has changed
if (count == 2)   // compares
```

In Java the compiler rejected this. In C++ it's legal, because any non-zero number counts as *true*.

### `.` or `->`: look for the star

| Declaration | What it is | Access |
|---|---|---|
| `Type x` | the object | `.` |
| `Type& x` | a reference | `.` |
| `Type* x` | an address | `->` |

`a->b` is exactly `(*a).b`. `this` is **always** a pointer, so always `->`.
Getting it wrong isn't dangerous: it won't compile.

Mind the number of stars:

```cpp
Kart::Player** players;   // players[i] is a POINTER -> ->
Item::Player*  players;   // players[i] is the OBJECT -> .
```

### `void` means command

> If a function returns `void`, it **does** something. It can't answer anything, so it can't live in
> an `if`.

Putting `UseStar()` inside a condition doesn't ask whether the star has been used: **it fires it.**
You ask questions of whoever has an answer — a field, a getter. **Reading is free, calling isn't.**

### The three `static`s

| Where | What it means |
|---|---|
| in a **class** | a single copy shared by every instance |
| **inside a function** | the variable **survives** between one call and the next |
| **outside everything**, in the file | visible only in this file |

For a class member, `static` is written **only in the declaration** (`.hpp`). In the `.cpp` you
define it qualified with the class name and **without** repeating `static`.

### `GameSource/` is off limits

Those structs are the game's memory map. Adding a field changes `sizeof`, and `size_assert` stops
compilation in dozens of files — which is **one single error, reported many times**: read the first
one, ignore the echo.

Your own classes in `PulsarEngine/`, on the other hand, you can extend freely, even when they inherit
from a game class: the new fields land **at the end**, after the part the game knows about.

> You can extend a game class **at the end**, by inheriting from it. You can never change it
> **in the middle**.

---

## 10. Things to keep in mind

- The absurd value before the debugger
- The first error, not the echo
- The corrected copy is the truth, not the original
- Whoever computes isn't whoever decides: the getter may not be on the road
- Command or question: `void` doesn't answer
- When the effect is *none*, look at the chain, not the logic
