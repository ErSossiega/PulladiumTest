# Note — item randomizer

Appunti da una sessione di sola lettura (nessun codice scritto, niente verificato a runtime).
Indirizzi PAL (RMCP), presi dai commenti negli header di `GameSource/`.

**L'idea:** trovi un bullet, l'icona è il bullet, premi, e lasci una banana.
Cioè una discrepanza voluta fra **icona** ed **effetto**.

---

## Il pezzo che serve

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
    void (*useFunction)(Player& player);  //0x18  <-- nulla = l'item si trascina
}; // Total size 0x1c
```

Anche in `GameSource/symbols.txt:1726`:

```
behaviourTable__Q24Item8Behavior = 0x809c36a0
```

**Un array di 19 righe indicizzato per `ItemId`, con dentro un puntatore a funzione.**
Non ci sono 19 strade sparse: il gioco ne ha una che passa di qui.

`0x809c36a0` sta sopra `809BD6E8` → zona dati. **È una variabile, non codice.** Ci si può scrivere.

`ItemId` sta in `GameSource/MarioKartWii/System/Identifiers.hpp:115` (0x0 green shell … 0xF bullet
… 0x12 triple banana, 0x14 none).

---

## Perché non farlo a mano con un `switch`

Perché per metà degli item **non esiste nessuna funzione da chiamare.** Il gioco ha due famiglie:

| Famiglia | Come funziona | Esempi |
|---|---|---|
| buff su te stesso | c'è una funzione | fungo, bullet, star, mega, TC, fulmine, POW, blooper |
| oggetti nel mondo | spawn dall'`ObjHolder`, trascinati o lanciati; `useFunction` **nulla** | banana, gusci, FIB, bob-omba |

Tutto quello che esiste, verificato:

```
Kart::Movement          ActivateMushroom 8057f3d8 · ActivateTc 80581a28 ·
                        ActivateBullet 805858ac · ActivateZipperBoost 8057f96c ·
                        ActivateStar (virtual, vtable 0x18, 80580268) ·
                        ActivateMega (virtual, vtable 0x1c, 80580b14)

Item::Player            UseBlooper 807a81b4 · UsePow 807b1b2c · UseBullet 807a9afc ·
                        UseMushroom 807a9d3c · UseMegaMushroom 807a9e50 ·
                        UseTC 807af1bc · UseStar 807b706c · UseThunder 807b7b7c
```

Non c'è `ActivateBanana`. Non c'è `UseGreenShell`. **Non esistono**, e la banana è proprio
l'esempio da cui parte l'idea.

> La tabella invece quella distinzione ce l'ha già dentro, in `useType` e nella `useFunction` nulla.
> Sposti la riga da `0x1c` byte e ti porti dietro anche il *come si comporta*.

---

## Variante A — permutazione, una volta a gara

Al caricamento gara rimescoli le 19 righe. La mappa resta **fissa** per tutta la gara:
bullet → banana, banana → mega, e così via. Prendi due bullet nella stessa gara, esce due volte
banana. Gara dopo, mappa nuova.

Qui **non ci sono probabilità**: è una permutazione, non una lotteria. C'è una mappa da scoprire
giocando, il che è più interessante del caso puro — dopo tre gare il caso puro è solo rumore.

- Costo: nessun debugger. La tabella è già localizzata.
- **`RaceLoadHook`, non `BootHook`**: nell'header c'è `void InitAllBehavior();`, quindi la tabella
  viene popolata a un certo punto. Scrivi prima → te la sovrascrivono. (È il problema del giorno 3
  in un'altra forma.)
- Sposta **righe intere**, non solo `useFunction`: altrimenti incroci le due famiglie e ottieni
  righe incoerenti (un `useType` da trailing con una funzione da "usa e basta").

**Primo test, prima della permutazione vera — il valore assurdo:** punta la `useFunction` di *tutte*
le righe alla stessa, una che riconosci al volo. Se ogni item fa la stessa cosa, la tabella è viva
e sei dentro. Se non cambia niente, non stai scrivendo dove credi, ed è un problema diverso.

---

## Variante B — tiro a ogni uso

Bullet → banana adesso, bullet → stella fra due minuti, nella stessa gara. Nessuna mappa da
imparare.

**Non si fa con la tabella.** La tabella viene letta *nel momento dell'uso*, quindi per cambiarla a
ogni uso devi intervenire lì. Serve trovare il punto in cui il gioco legge `behaviourTable[id]`:
breakpoint, leggi `lr`, `lr − 4`. **Non fatto.**

Sopra la B ci puoi mettere i pesi.

---

## "Che neanche io sappia le probabilità"

In senso stretto no: il generatore lo scrivi tu, quindi la distribuzione è un fatto sul tuo codice.
Uniforme = sai che è 1/19.

Quello che si fa è un gradino sotto, e basta: al `RaceLoadHook` non estrai l'item, estrai **i pesi**.
19 numeri dall'RNG, normalizzati, e quella è la distribuzione della gara. Sai che i pesi sono
casuali; non sai che *in questa gara* il bullet dà banana l'80% delle volte.

RNG disponibile — `MarioKartWii/System/Random.hpp`:

```
Random::NextLimited(int limit)        805555cc
DriverManager::GetRaceinfoRandom()    807bd718
```

> **Il `OS::Report` di debug va tolto.** Serve mentre sviluppi, ma il giorno che resta dentro apri
> il log e sai la tabella: la feature è morta. Cancellare quel log è la parte finale del lavoro,
> non la pulizia. Scriverlo in un commento nel file, o fra due mesi lo rimetti per debuggare altro.

**Online:** se i pesi non sono seedati uguali su tutti i client, ognuno gioca a un gioco diverso.
(`PulsarEngine/Extensions/LECODE/XPF.cpp:43` fa `Random random(seed)` con seed esplicito proprio
per questo.)

---

## Vicoli ciechi già esclusi

**`ItemSlotData` probabilities** (`Item/ItemSlot.hpp`) — cambia **cosa esce dalla box**, non cosa
succede quando usi. Roulette, HUD ed effetto restano coerenti fra loro. È un'altra feature, non
questa.

**`RandomizeRouletteItem` `807baed4`** — è l'animazione della ruota che gira. La struct dice
`//visual only`. E soprattutto è **inlined**: il corpo è copiato dentro ai chiamanti, quindi non
c'è nessun `bl` da dirottare, ne esistono N copie possibilmente diverse fra loro, e per cambiarlo
andrebbero trovate tutte. Utile semmai per l'effetto opposto (una ruota che bluffa), a basso
rischio perché è dichiarato visual only.

---

## Altri indirizzi utili trovati per strada

```
Item::PlayerObj::UseItem(bool)              80791910
Item::PlayerObj::PrepareHandlers[5]         808d18d0   (uno per useType)
Item::PlayerObj::UpdateHandlers[5]          808d1900
Item::PlayerInventory::SetItem              807bc940   (currentItemId a 0x4)
Item::ItemSlotData::DecideItem              807bb42c   <- il vero verdetto
Item::ItemSlotData::DecideRouletteItem      807bb8d0   <- la sequenza mostrata
Item::Manager::itemObjHolders[0xF]          (offset 0x48, uno per objId)
```

---

## Da fare quando torno

1. Variante A, con prima il test "tutte uguali"
2. Se funziona: permutazione vera con `Random`
3. Solo se voglio la B: breakpoint per trovare chi legge `behaviourTable[id]`
