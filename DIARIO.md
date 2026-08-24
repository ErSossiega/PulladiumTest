# Diario — imparare il C++ su Pulladium

**20–24 agosto 2026.** Quattro giorni, dal "so cosa sono i puntatori" a un hook piazzato in un
punto che ho trovato da solo.

Questo file esiste perché gli errori valgono più del codice che funziona. Il codice giusto lo
rileggi e dici "ok". Un errore che hai già fatto lo riconosci al volo la seconda volta.

---

## Punto di partenza

- C++ arrivato ai **puntatori**, saltando casting e un po' di funzioni
- **Due anni di Java a scuola**: classi, ereditarietà, eccezioni, generici, liste concatenate
- Zero esperienza sul motore

Da lì la scoperta che ha accorciato tutto: l'OOP c'era già. Mancava solo **la lista delle
differenze Java → C++**, che è corta.

| Cosa | Java | C++ |
|---|---|---|
| Oggetti | tutto è un riferimento | tre modi: valore, `*`, `&` |
| Accesso ai membri | sempre `.` | `.` su valori e riferimenti, `->` sui puntatori |
| `new` | il GC libera | **devi** fare `delete` |
| Distruttori | non esistono | `~Nome()` |
| File | una classe = un file | `.hpp` (dichiari) + `.cpp` (scrivi) |
| Import | `import` | `#include` + include guard |
| Interi | `int` e basta | `u8 u16 u32 s16 s32 f32` |
| `namespace` | `package` | uguale, con `::` |

E la parte da **non** studiare: tutta la STL. Nel motore non esiste `std::string`, non esiste
`vector`, non esiste `iostream`. Gira su una Wii con 24 MB di RAM.

---

## Giorno 1 — leggere il motore

### Cosa è stato costruito

Un **indicatore di carica del mini-turbo** accanto al tachimetro, in single player:
velocità a sinistra, carica MT a destra. Controllo proprio, classe propria.

File toccato: `PulsarEngine/UI/CtrlRaceBase/Speedometer.cpp` e `.hpp`.

### La riga che contiene tutto

```cpp
const Kart::Pointers& pointers = Kart::Manager::sInstance->players[this->GetPlayerId()]->pointers;
const Kart::Physics* physics = pointers.kartBody->kartPhysicsHolder->physics;
```

Novanta caratteri, e dentro c'è: membri statici, `->` su puntatori, indicizzazione, riferimenti,
`const`, `this`. Tutto il C++ che mancava, in due righe di codice vero.

### Concetti imparati

**La regola della stella** — si legge dalla dichiarazione, non si intuisce:

| Dichiarazione | Cos'è | Accesso |
|---|---|---|
| `Tipo x` | l'oggetto | `.` |
| `Tipo& x` | un riferimento | `.` |
| `Tipo* x` | un indirizzo | `->` |

`a->b` **è** `(*a).b`. Quel `*` è lo stesso di `*ptr_prova` in `puntatori.cpp`.

**`GameSource/` non si tocca.** Quelle struct sono la mappa della memoria del gioco, decisa da
Nintendo nel 2008. `size_assert` esiste per fermarti.

**Le tue classi in `PulsarEngine/` sì.** Anche quando ereditano da una classe del gioco: i campi
nuovi finiscono in coda, dopo la parte che il gioco conosce.

**Il contratto `Count()` / `Create()`.** Il motore chiede quanti slot servono e poi te li ripassa.
`count` significa *numero di controlli*, non *numero di giocatori* — finché coincidono nessuno se
ne accorge, e il giorno che li sdoppi salta tutto fuori.

**Il ciclo veloce vale più della teoria.** `BuildPulsar.py` ricompila solo i file toccati:
**4,7 secondi**. Modifica → build → guardo. Con un build lento si molla dopo una settimana.

### Gli errori del giorno 1

**Aggiungere un campo a `Kart::Pointers`**

```cpp
// in GameSource/MarioKartWii/Kart/KartPointers.hpp
Killer* kartKiller;   //0x60
s16 mtCharge;         //0x64   ← NO
```

Risultato: `static assert check 'Pointers' failed` in **60 file**.

> Non erano 60 errori: era **un errore solo, riportato 60 volte**. Guarda il primo, ignora l'eco.

**Cercare `mtCharge` nei posti sbagliati** — quattro tentativi, tutti lasciati a commento nel file:

```cpp
//speed = Kart::Movement::                     // pensavo servisse "collegare" la classe
//speed = pointers.mtcharge;                   // Pointers non ha quel membro
//speed = pointers.KartMovement->mtcharge;     // K maiuscola: quello è il TIPO, non il membro
```

La riga giusta era già nel file, **due righe sopra**, e l'avevo commentata:

```cpp
float speedCap = pointers.kartMovement->hardSpeedLimit;   // stessa identica strada
```

> Quando cerchi come arrivare a qualcosa, la prima mossa non è inventare: è **cercare chi ci è già
> arrivato** e copiare la strada.

**Il cast al contrario**

```cpp
speed = static_cast<s16>(pointers.kartMovement->mtCharge);   // mtCharge è GIÀ s16
```

Compila e funziona per caso. Ma:

> Si converte verso il tipo che **vuoi**, non verso quello che hai già.

**Lo shadowing** — perché il compilatore non ha protestato:

```cpp
u8 speedoType = (count == 3) ? 4 : count;      // quella vera
if(...){
    u8 speedoType = (count == 3) ? 4 : count;  // una SECONDA variabile, muore alla }
}
```

In Java il compilatore lo rifiuta. In C++ è legale e silenzioso.

**`=` invece di `==`**

```cpp
else if(count = 2) speedoType = 1;   // assegna 2, la condizione è SEMPRE vera
```

E come effetto collaterale cambia `count`, che governa il ciclo tre righe dopo.

> `=` scrive. `==` chiede.

**Derivare da un dato ambiguo.** `count == 2` può voler dire *un giocatore raddoppiato* oppure
*due giocatori*. Da quel numero l'informazione non si ricava: va presa dalla fonte
(`localPlayerCount`).

**Decidere prima di correggere.** In `Count()` il test `if(localPlayerCount == 1)` stava **sopra**
le due righe che sistemano il conteggio per replay e spettatore. Decideva su un dato non ancora
pronto.

E la variante travestita, subito dopo:

```cpp
u32 localPlayerCount = scenario.localPlayerCount;   // la copia, che poi correggo
...correzioni sulla copia...
if(scenario.localPlayerCount == 1){                 // ma qui rileggo l'ORIGINALE
```

> Se ti prendi una copia per correggerla, da quel momento **la copia è la verità**.

---

## Giorno 2 — agganciarsi al gioco

### Cosa è stato costruito

**Due hook, di due tipi diversi.**

`PulsarEngine/Race/GetUMTValues.cpp` — partito per sostituire una funzione, finito a scrivere
direttamente in memoria:

```cpp
kmWrite16(0x808B5CC2, 1);   // la carica MT non è calcolata: è una variabile
```

`PulsarEngine/Race/TTItems.cpp` — una stella in Time Trial, data **una volta a gara**, con
`RaceLoadHook` che azzera e `RaceFrameHook` che controlla.

### Come funziona un hook

A `0x80580630` il gioco ha **una sola istruzione**: una `bl` verso `ApplyLightningEffect`.
`kmCall(0x80580630, MegaTC)` riscrive quella `bl` perché punti alla tua funzione. **Nient'altro.**
I registri restano com'erano.

```cpp
void MegaTC(Kart::Movement& movement, int frames, int unk0, int unk1)
//          ↑ r3               ↑ r4        ↑ r5      ↑ r6
```

> La lista dei parametri **non è una scelta tua: è la mappa dei registri.**
> Dichiari tutti quelli originali, nell'ordine, anche quelli che ignori. E non ne inventi di nuovi.

Il `this` di un metodo diventa il primo parametro esplicito. E il ramo `else` che richiama
l'originale non è pignoleria:

> Un hook che non richiama l'originale quando la condizione è falsa non estende il gioco: **lo rompe.**

### I tipi di hook

| Macro | Cosa scrive | Quando |
|---|---|---|
| `kmCall` | `bl` — vai *e torna* | dirottare **una chiamata** in mezzo al codice |
| `kmBranch` | `b` — vai *e basta* | sostituire **una funzione intera** |
| `kmWrite16/32` | un valore grezzo | cambiare **una variabile** o un'istruzione |
| `RaceLoadHook`, `RaceFrameHook`, `BootHook`, `SectionLoadHook` | — | **nessun indirizzo**: funzione `void`, una riga per registrarla |

> Prima di andare a caccia di un indirizzo, guarda se il momento che ti interessa **ha già un
> aggancio pronto**.

### Ricavare un indirizzo

Il PowerPC non può caricare 32 bit in un colpo: **spezza sempre in due**.

```
8057efec   lis  r3, 0x808B        → r3 = 0x808B0000
8057eff0   lha  r3, 0x5CC6 (r3)   → legge a 0x808B0000 + 0x5CC6 = 0x808B5CC6
8057eff4   blr
```

**indirizzo = (valore del `lis` << 16) + offset.** Aritmetica, non tentativi.

L'offset è **con segno**: se comincia con 8-F è negativo e va sottratto.

| Intervallo | Binario |
|---|---|
| `80004000` – `~80388000` | main.dol |
| `~805102E0` – `~808D9A58` | StaticR.rel |
| `~809BD6E8` in su | dati e variabili |

`StaticR.rel` è rilocabile: **non si disassembla da file**, gli indirizzi non corrispondono.
Si usa il debugger di Dolphin, che legge la RAM a gioco avviato.

### Leggere un crash

`Crash.pul`, **offset 12** = il tipo. `2` = DSI · `3` = ISI · `7` = virgola mobile · `8` = FPE

| | Cosa ha provato a fare | Dove guardare |
|---|---|---|
| **DSI** | leggere/scrivere **dati** a un indirizzo che non esiste | `srr0` è un'istruzione vera; l'indirizzo marcio è **nei registri** |
| **ISI** | eseguire **codice** dove non c'è | `srr0` **è** la spazzatura; guarda **`lr`**, che dice da dove è partito il salto |

### Gli errori del giorno 2

**`kmCall` su un inizio di funzione**

```cpp
int umt100(Kart::Movement& movement, int unk0, int unk1) { return 100; }
kmCall(0x8057efe0, umt100);   // 0x8057efe0 è l'INIZIO di GetMTMaxCharge
```

Crash. E il dump raccontava tutto:

```
error = 2 (DSI)
srr0   = 0x8057efe4    ← 4 byte dopo l'inizio
lr     = 0x8057efe4
r3     = 0x00000064    ← 100. Il MIO valore.
```

`bl` va **e torna**: la funzione rientrava a metà dell'originale con `r3` pieno del valore di
ritorno invece del puntatore. L'istruzione dopo faceva `0x64 + 0x5CC2 = 0x5D26`, che non è memoria.

Più due parametri inventati (`unk0`, `unk1`) che non esistevano.

**L'include sbagliato — l'errore più insidioso dei due giorni**

```cpp
#include <KamekInclude/hooks.hpp>   // il path di ricerca È GIÀ ./KamekInclude
```

Errore **fatale**, non avviso. Ma:

```python
def compile_cpp(cpp: str):
    subprocess.run(cmd, shell=True)   # ← nessun controllo del codice di uscita
```

Il build script **non guarda se la compilazione è riuscita**. Linka l'`.o` vecchio e consegna un
`Code.pul` con timestamp fresco e dentro il codice di ieri. Il crash era identico byte per byte, e
la prova stava nel binario:

```
40 ff ff fe   80 57 ef dc   ...    ← kmBranch (b)
41 ff ff fe   80 57 ef e0   ...    ← il mio hook: ancora kmCall (bl)
```

> In questo build system **un errore di compilazione è silenzioso.** Gli errori che scorrono
> nell'output non sono rumore: sono l'unica cosa che conta.

**Scrivere in mezzo a un'istruzione**

```cpp
//kmWrite16(0x8057efe0,1);   // indirizzo giusto, ma di CODICE: riscrive il lis
```

`kmWrite` non sa se stai puntando a un'istruzione o a una variabile. Quella distinzione la tieni tu.

**Lo `0x` mancante**

```cpp
//kmWrite16(80591208,1);   // decimale → 0x04CDF3A8. Non un indirizzo sbagliato: un ALTRO numero.
```

Crash al boot, perché `kmWrite` agisce al caricamento.

**Agganciare la funzione sbagliata**

```cpp
kmBranch(0x80790e3c, setStartOnTT);   // 0x80790e3c = u16 GetKMPObjectsCount()
```

Volevo una stella in TT e ho sostituito **la funzione che conta gli oggetti del KMP**. Firma
incompatibile in due modi: l'originale non prende parametri e **ritorna** un `u16`, la mia
prendeva un parametro e ritornava `void`.

E si vedeva dal disassemblato:

```
80790e3c   lis  r3, 0x809D    ← PRIMA istruzione: sovrascrive r3
```

> Se la prima istruzione **scrive** in `r3` invece di leggerlo, la funzione non ha argomenti.

**Inventare un tipo di hook**

```cpp
// TTItems.hpp
class cancelStarAfterUse{ cancelStarAfterUse(void *func); };
```

`RaceFrameHook` **è un tipo**, come `int`. Due hook = due variabili dello stesso tipo, non due
classi:

```cpp
RaceFrameHook star(setStarOnTT);
RaceFrameHook removeStar(removeStarAfterUse);
```

**Catena incompleta sul manager**

```cpp
Item::Manager::players.setItem(STAR, true);
```

Quattro cose insieme: `players` non è statico (serve `sInstance->`), manca l'indice, `SetItem` è
maiuscola, e **non appartiene a `Player`** ma al suo `inventory`. La catena vera:

```
Item::Manager::sInstance   →  Manager*            →  ->
   ->players               →  Player*  (array)    →  [indice]
   [indice]                →  un Player OGGETTO   →  .
   .inventory              →  oggetto             →  .
   .SetItem(STAR, true)
```

Una stella in meno rispetto a `Kart::Manager` (`Player**` contro `Player*`) e cambia il simbolo.

**Comando scambiato per domanda**

```cpp
if(Item::Manager::sInstance->players[0].UseStar() == true && isTT == true)
```

`void UseStar()` non ritorna niente. Ma il problema vero è più profondo: mettendola nell'`if` non
stavo *chiedendo* se la stella era stata usata — **la stavo facendo partire**, sessanta volte al
secondo.

> Se una funzione ritorna `void`, **fa** qualcosa. Non può rispondere, quindi non può stare in un `if`.
> Leggere è gratis, chiamare no.

**Un taccuino che nessuno consulta**

```cpp
if(isTT == true){                    // ← nessun controllo su givenStar
    ...SetItem...
    givenStar = true;
}
if(gamemode == MODE_TIME_TRIAL && givenStar == true){
    ...UseItem... RemoveItems...
    givenStar = false;               // cancellato nello STESSO frame in cui l'ho scritto
}
```

| | in ingresso | primo `if` | secondo `if` | in uscita |
|---|---|---|---|---|
| frame 1 | `false` | dà → `true` | usa → `false` | `false` |
| frame 2 | `false` | dà → `true` | usa → `false` | `false` |

Fra un frame e l'altro non cambiava niente. La variabile veniva scritta e cancellata dentro lo
stesso frame, e **il blocco che dava la stella non la guardava mai.**

**Log invisibili**

```cpp
OS::Report("[TEST LOG ...]PulsarEngine: Giving player 1 a star for TT", 0);
```

I log c'erano da sempre. Ma `OS::Report` **non va a capo**: senza `\n` centinaia di messaggi si
attaccano in una riga sola, sepolta sotto migliaia di righe di boot.

---

## Il filo che lega quasi tutti gli errori

Quattro volte in quattro giorni, sempre la stessa forma:

| | La modifica non arrivava perché |
|---|---|
| il `pos` del tachimetro | stava su una riga morta (`if(count == 1)` con `count` ormai 2) |
| il `kmBranch` | il `.o` non era stato ricompilato |
| la stella | il taccuino veniva cancellato nello stesso frame |
| l'hook sul fungo | il file si chiamava `MUSHROOOOMS`, senza `.cpp` |

> **Quando l'effetto è *nessuno* invece che *sbagliato*, il problema non è quasi mai nella logica.**
> È nella catena: salvato → compilato → linkato → copiato → ricaricato.

### La catena delle cinque verifiche

1. **Salvato?** Il build legge dal disco, non dall'editor.
2. **Compilato?** `ls -la build/TuoFile.o` — deve essere più recente del `.cpp`.
3. **Linkato?** Se il prompt dice *"No source or header files were modified"* subito dopo che hai
   modificato qualcosa, non è un'offerta: è un avvertimento.
4. **Copiato?** `RIIVO` punta al pack che stai davvero avviando?
5. **Ricaricato?** Riivolution legge i file all'avvio della ISO.

### E l'altra abitudine che ha risolto più cose

> **Prova un valore assurdo.** Un `1` al posto di un `270` dà un effetto impossibile da confondere.
> Se non cambia niente nemmeno con quello, l'hook non scatta — ed è un problema diverso con una
> soluzione diversa.

Separare "non scatta" da "scatta ma il valore non conta" è quello che ha sbloccato la giornata due.

---

## Il debugger di Dolphin

Si attiva dalle impostazioni, sezione interfaccia. Tre mosse:

1. **Il breakpoint risponde sì o no.** Scatta → la funzione viene chiamata. Non scatta → stai
   agganciando il posto sbagliato, e nessun ragionamento sul codice te lo direbbe.
2. **Breakpoint sul `blr`, poi `r3`.** È dove il PowerPC tiene i valori di ritorno. Vale anche al
   contrario: in un crash, un `r3` con dentro un numero tuo dice che il tuo codice è passato di lì.
3. **Memoria** con l'emulazione **ferma su un breakpoint**. Il simbolo dirà `unk`: normale, non c'è
   nessuna mappa caricata. Serve il contenuto, non il nome.

È così che si è scoperto che `GetMTMaxCharge` non calcola niente — è un getter di tre istruzioni
su una variabile a `0x808B5CC2`. E che il codice che conta legge quella variabile **direttamente**,
senza passare dal getter.

> Agganciare un getter cambia le cose **solo per chi chiama il getter**.

---

## `symbols.txt` in breve

Serve a **chiamare** funzioni del gioco dal tuo codice (`-externals` nel link). Formato:

```
nome_mangled = 0xINDIRIZZO
```

Il nome mangled si smonta:

```
UseItem__Q24Item9PlayerObjFb
  UseItem  __  Q2  4Item  9PlayerObj  F  b
  nome         2 livelli qualificati   funzione, un bool
= Item::PlayerObj::UseItem(bool)
```

Il numero prima di ogni nome è la sua lunghezza. `v` void · `b` bool · `i` int · `Uc` u8 ·
`Us` u16 · `f` float · `P` puntatore · `C` const · `R` riferimento · `e` ellipsis.

**Non serve scriverlo a mano:** dichiara la funzione, chiamala, e il linker fallirà stampandoti la
stringa esatta. Se sbagli la firma, il nome cambia e cerca un simbolo diverso.

Gli hook **non** passano da `symbols.txt`: a loro dai il numero e basta.

### Gli indirizzi sono PAL, e le altre regioni vengono gratis

Quello che scrivo in `symbols.txt` è un indirizzo **PAL (RMCP)**, perché PAL è la versione base.
Le altre le sistema `versions.txt`, che il linker usa insieme a `symbols.txt`:

```
[P]
#Base version: MKWii PAL
00000000-*: +0x0          <- PAL è la base: nessuno spostamento
[E]
...
8054fb2c-80550547: +0xd9c
80550548-805537cb: -0x5f58
```

Non è una tabella **per simbolo**: è una tabella di **intervalli**. Un indirizzo PAL cade dentro un
intervallo e prende quel delta. Quindi per le mie due righe non ho dovuto aggiungere niente da
nessuna parte — vengono tradotte da sole:

| indirizzo PAL | | E | J | K | D |
|---|---|---|---|---|---|
| `0x8057f3d8` | `ActivateMushroom` | −0x6864 | −0x680 | −0x11fa8 | −0x9d8 |
| `0x805858ac` | `ActivateBullet` | −0x6824 | −0x680 | −0x11fa8 | −0x9d8 |
| `0x80798664` | il mio `kmCall` | −0x900c | −0x994 | −0x11c40 | +0x594 |

E si vede che **non è un unico scostamento globale**: due funzioni a `0x6000` di distanza hanno
delta diversi in NTSC-U. Il codice è stato ricompilato, non spostato in blocco.

**Come si rompe:** se un indirizzo PAL finisce in un **buco** della tabella — un intervallo che
nessuna riga copre — il linker lo lascia **com'è**. Nessun errore, nessun avviso: sull'altra
regione il gioco chiama semplicemente un indirizzo diverso, che lì è un'altra cosa.

> Se qualcosa funziona in PAL e crasha solo su NTSC, il primo posto da guardare è `versions.txt`.

È lo stesso genere di silenzio del build che linka il `.o` vecchio: il sistema non ha modo di
sapere che l'indirizzo non era stato tradotto.

---

## Giorno 3 — rendere una feature configurabile

### Cosa è stato costruito

L'oggetto in Time Trial diventa **un'impostazione**: tripli funghi, stella o mega, scelti dal menu
di Pulsar. Prima feature che non è un valore fisso nel codice, ma qualcosa che decide chi gioca.

File: `PulsarEngine/Race/TTItems.cpp` più i valori aggiunti in `Settings/SettingsParam.hpp`
(`SETTINGTT_RADIO_ITEM`, `TTSETTING_ITEM_STAR`, …).

### L'errore di fondo: dove va la condizione

Il primo tentativo erano tre funzioni — `setStar`, `setMega`, `set3Shrooms` — ognuna che leggeva
l'impostazione e, se corrispondeva, registrava i suoi hook:

```cpp
static void setStar(){
    const bool isEnabled = ...GetSettingValue(...) == TTSETTING_ITEM_STAR;
    if(isEnabled){
        RaceLoadHook restartStar(restartStar);   // ← variabile LOCALE
        RaceFrameHook star(setStarOnTT);         // ← variabile LOCALE
    }
}
```

Tre cose sbagliate insieme:

1. **`setStar`, `setMega` e `set3Shrooms` non le chiamava nessuno.** Tre funzioni morte.
2. Un hook **è un oggetto il cui costruttore lo aggiunge a una lista**. Dichiarato dentro una
   funzione, nasce a ogni chiamata e **muore quando la funzione finisce**, lasciando nella lista un
   puntatore a memoria che non esiste più.
3. E soprattutto: gli hook si registrano **all'accensione, una volta sola**, prima che qualsiasi
   impostazione abbia senso. E il giocatore può cambiarla **mentre gioca**.

> **Non puoi rendere condizionale la registrazione. Devi rendere condizionale il comportamento.**

La correzione ha fuso tre funzioni e tre flag in **una funzione e un flag**, con l'impostazione
letta dentro, e i due hook a livello globale dove devono stare. Effetto collaterale gradito: se
cambi impostazione fra una gara e l'altra funziona subito, perché il valore lo rileggi ogni frame.

### Gli altri errori del giorno 3

**L'ordine di dichiarazione, di nuovo.**

```
Error: undefined identifier 'restartMega'   (riga 86)
Error: undefined identifier 'setMegaOnTT'   (riga 87)
```

`setMega()` chiamava due funzioni definite **sotto** di lei. È letteralmente quello che avevo
scritto io mesi fa in `funzioni.cpp`: *"se va messo al di sotto di dove viene chiamata andrà in
errore perché il compilatore è scemo e non sa dove trovarla"*.

**La variabile che inizializza sé stessa.**

```cpp
RaceLoadHook restartStar(restartStar);
// Warning: variable 'restartStar' is not initialized before being used
```

Ho dato **alla variabile lo stesso nome della funzione**. In C++ il nome che stai dichiarando è già
in scope **dentro le sue stesse parentesi**: quel `restartStar` non è la funzione, è la variabile
che sto creando in quel momento. Si inizializza con sé stessa.

Il giorno prima funzionava solo perché avevo usato nomi diversi: `RaceLoadHook restart(restartStar);`.

**Il build silenzioso — terza volta in tre giorni.** Il file non compilava, il vecchio `.o` restava
linkato, e il gioco continuava a dare la stella. Il log diceva una cosa e il sorgente un'altra.

**Un nome che mente.** L'opzione chiamata `TTSETTING_ITEM_DISABLED` dava tre funghi. Il codice
faceva quello che gli avevo detto, ma il nome no.

### Lo switch, e la forma "decidi poi agisci"

La catena di `else if` leggeva l'impostazione **tre volte** e ripeteva `SetItem` + flag + log in
ogni ramo. Con uno `switch` che sceglie **solo il valore**:

```cpp
const u32 scelta = ...GetSettingValue(...);
ItemId item;
switch(scelta){
    case ...: item = TRIPLE_MUSHROOM; break;
    case ...: item = STAR;            break;
    case ...: item = MEGA_MUSHROOM;   break;
    default:  return;                        // niente oggetto
}
...SetItem(item, true);
isGivenItem = true;
```

È la stessa idea del `? :` in `Load()` del giorno 1: **decidi prima un valore, poi agisci una volta
sola.** E il `default` risolve gratis il caso "nessun oggetto".

Due cose sullo `switch` in C++: il **`break` non è opzionale** — senza, l'esecuzione prosegue nel
caso successivo (*fallthrough*) e il compilatore non avvisa. E `default` copre tutto il resto.

(Le variabili `static` in C++ nascono azzerate, garantito: `static bool isGivenItem;` è già `false`.)

---

## Giorno 4 — il primo hook piazzato dove volevo io

### Cosa è stato costruito

Il fungo che diventa un **bullet**. `PulsarEngine/Race/MUSHROOOOMS.cpp`: un `kmCall` su un
indirizzo che non era scritto in nessun header — l'ho trovato io col debugger.

E il build che smette di mentire: `BuildPulsar.py` ora **si ferma** se una compilazione fallisce.

### Trovare l'indirizzo di una `bl`

| | Mossa | Risultato |
|---|---|---|
| 1 | Breakpoint su `0x8057f3d8` — inizio di `Kart::Movement::ActivateMushroom`, preso dal commento nell'header | scatta → la funzione viene chiamata davvero |
| 2 | Fungo in gara | il breakpoint scatta |
| 3 | Leggo `lr` | `0x80798668` |
| 4 | `lr − 4` | **`0x80798664`** ← l'indirizzo per il `kmCall` |

> Gli header elencano gli **inizi** delle funzioni, e possono farlo perché un inizio è unico.
> Una *chiamata* no: la stessa funzione può essere chiamata da dieci punti, e nessuno dei dieci ha
> un nome. Per questo l'indirizzo di un `kmCall` non è documentato da nessuna parte.

### Il vicinato dice dove sei finito

`0x80798664` non è un numero qualsiasi: sta in mezzo a simboli di **`Item::Player`**.

| Indirizzo | Funzione |
|---|---|
| `0x80797928` | `Item::Player::Update()` |
| **`0x80798664`** | la mia `bl` |
| `0x807986b4` | `Item::Player::ActivateMegaMushroom()` |

Cioè sono nel codice **dell'oggetto fungo**, non della guida. Nell'header manca la sorella
`Item::Player::ActivateMushroom()`, che sta lì subito prima e finisce chiamando quella di
`Kart::Movement`.

Per sapere dove comincia davvero la funzione che ti contiene, si scorre il disassemblato
all'indietro fino al **prologo**:

```
mflr  r0                 ← salva il return address: questa funzione ne chiama altre
stwu  r1, -0x??(r1)      ← apre lo stack frame
```

Sopra al prologo c'è il `blr` della funzione precedente. È l'unico modo di orientarsi in un
listato senza mappa dei simboli — cioè quando il debugger non ce l'hai.

### La prova che non ha dato il risultato che mi aspettavo

Ho tenuto il breakpoint e sono passato su un **pannello turbo**. Non è scattato.

Non era un bug: il turbo del fungo e quello del pannello sono boost di **tipo diverso**, con
funzioni diverse.

```cpp
void ActivateMushroom();     //8057f3d8   ← la mia
void ActivateZipperBoost();  //8057f96c   ← rampe e zipper
void TryStartJumpPad();      //8057fd18   ← jump pad
```

> Un breakpoint che **non** scatta è un'informazione, non un fallimento. Mi ha detto una cosa vera
> sul motore che non avrei ricavato leggendo il codice.

E come effetto pratico: ho un call site solo, che scatta esattamente quando voglio io.

### Virtuale o no: chi ha bisogno di `symbols.txt`

In `MegaTC.cpp` chiamo `movement.ActivateMega()` e il link passa. Chiamo
`movement.ActivateMushroom()` e il link fallisce. La differenza è nell'header:

```cpp
void ActivateMushroom();            //8057f3d8   ← metodo normale
virtual void ActivateMega();        //0x1c       ← virtuale
```

> Se la chiamata passa dalla **vtable**, l'indirizzo lo trova il gioco a runtime e a me non serve
> nessun simbolo. Se è una `bl` a indirizzo fisso, quell'indirizzo lo devo dare io al linker.

Il commento accanto a un metodo virtuale non è nemmeno un indirizzo: è l'**offset nella vtable**
(`0x1c`). Due numeri che sembrano uguali e non lo sono.

### `symbols.txt`: le due metà di una riga

Il pezzo che mi mancava era banale. Una riga viene da due posti diversi:

| Metà | Da dove |
|---|---|
| `ActivateBullet__Q24Kart8MovementFUc` | dal linker che fallisce, o derivato a mano dalle regole |
| `= 0x805858ac` | dal **commento nell'header**, con lo `0x` davanti |

```cpp
void ActivateBullet(u8 itemPoint); //805858ac
```

Non c'è niente da calcolare: qualcuno ha già documentato dove comincia ogni funzione.

> `symbols.txt` è un **elenco del telefono**: nome ↔ numero. Ci aggiungi la riga di chi vuoi
> chiamare.

E si incastra col `kmCall`: l'**inizio** di una funzione è documentato, la **chiamata** no. La
metà facile è `symbols.txt`.

(Il nome mangled l'ho derivato a mano e il linker l'ha accettato al primo colpo. Le sezioni
`#KartMovement` sono solo commenti: le mie due righe sono finite sotto `#ITEMHandler` e funziona
lo stesso. Ordinarle serve a me, non al linker.)

### Gli errori del giorno 4

**La namespace minuscola**

```cpp
namespace pulsar{
    namespace race{
```

```
Error: name followed by '::' must be a class or namespace name
```

La ricerca dei nomi esce **verso l'esterno**: `Settings` cercato in `pulsar::race`, poi in
`pulsar`, poi in `::`. E `Pulsar::Settings` non lo guarda mai, perché `pulsar` e `Pulsar` sono due
namespace **diverse**.

> Le namespace in C++ sono **aperte**: scriverne il nome sbagliato non è un errore che il
> compilatore rifiuta — **ne crea una nuova, vuota.**

Terza della stessa famiglia in quattro giorni, dopo lo shadowing e
`RaceLoadHook restartStar(restartStar)`: **legale e silenzioso.**

**Il file senza estensione — la quarta volta che la modifica non arriva**

```
PulsarEngine/Race/MUSHROOOOMS
```

```python
cpp_files = glob.glob(f"{PULSAR}/**/*.cpp", recursive=True)
```

Non è `*.cpp`. **Il build non lo vede.** E la cattiveria è che il fix appena fatto al build script
non serve a niente qui: non c'è nessuna compilazione che fallisce, semplicemente non ne parte
nessuna. Il build dice "tutto ok" e ha ragione.

> La catena delle cinque verifiche ha uno scalino sopra al primo: **il build sa che il file
> esiste?**

Con un dettaglio che mi ha ingannato: l'errore che leggevo veniva **dall'editor**, non dal
compilatore. Sembrano la stessa cosa e non lo sono — uno controlla mentre scrivi, l'altro solo
quando il file entra nel build.

**La variabile chiamata come il tipo**

```cpp
void MyMushroom(Kart::Movement& Movement)
```

Compila, ma da lì in poi dentro la funzione `Movement` è la variabile, non la classe. È il ritorno
di `RaceLoadHook restartStar(restartStar)` del giorno 3, e della `K` maiuscola del giorno 1.

**Il parametro che non sapevo cosa fosse**

`ActivateBullet(u8 itemPoint)` — cosa ci metto? La risposta era in un header accanto,
`KartKiller.hpp`, dentro un commento:

```cpp
void Activate(u8 itemPoint); //8059b7b8 if itemPoint == 0xFF, gets item point from Item::Player
```

`0xFF` = **"arrangiati tu"**: il gioco va a prendersi l'item point da solo.

Terza volta in quattro giorni che la risposta era già scritta nel repo e sono andato a cercarla
altrove.

### Il build non mente più

Tre modifiche a `BuildPulsar.py`:

1. `compile_cpp` **ritorna** `(file, returncode)` invece di buttarlo via
2. `executor.map` viene **raccolto**: prima i risultati finivano nel vuoto, quindi anche
   controllando il codice di uscita non sarebbe bastato
3. Se anche un solo file fallisce: elenco dei file e `sys.exit(1)`. **Non linka e non copia.**

Più una quarta cosa, che è quella che chiude il caso del giorno 2: se un file non compila, il suo
`.o` vecchio viene **cancellato**. Così il `.o` di ieri dentro un `Code.pul` con timestamp fresco
diventa impossibile, anche premendo `L` per rilinkare.

> Il build script è **codice tuo come il resto.** Se ti dice bugie, si aggiusta.

---

## Cosa resta aperto

- Due gare di fila, per verificare che `RaceLoadHook` azzeri davvero anche col *riprova*
- L'impostazione destra/sinistra del tachimetro: ancora una riga morta dal giorno 1
- Il kit per il telefono: Winlator compila (il compilatore a 32 bit gira), il linker .NET no —
  resta `Kamek` ARM64 nativo in Termux, oppure farsi linkare i `.o` da qualcuno
- Il listato disassemblato di `StaticR.rel` come sostituto del debugger in viaggio

---

## Il prossimo argomento: gli operatori bit a bit

Ho usato tutti e tre i tipi di hook, e l'ultimo l'ho piazzato dove volevo io. Il buco che resta
non è sul motore: è sul C++.

In quattro giorni non ho mai scritto un `&`, un `|`, un `<<`. E non è un dettaglio accademico — in
un motore senza `vector` e senza `set`, **i bit sono la struttura dati**. Sono già dappertutto
nelle cose che ho toccato senza accorgermene:

```cpp
u16 bitfield; /* 0xc
1 = 0x2:  has inventory item
2 = 0x4: is releasing dragged item     ← ItemPlayer.hpp
```

E sono il prerequisito diretto del compito dopo.

### L'esercizio: rimappare i comandi

`Input::State::buttonActions` è già astratto dal controller fisico: sono **cinque bit da
permutare**. Un punto solo, si prova in Time Trial, e l'effetto o c'è o non c'è.

Attenzione a non rimappare anche `GhostController` e `AIController`, che leggono la stessa
struttura.

### L'altro pezzo di PPC che mi manca

Il **prologo/epilogo** l'ho incontrato il giorno 4, ma solo per riconoscerlo. Mi manca il resto
dell'ABI: quali registri sono argomenti (`r3`–`r10`), quali sono liberi (`r0`, `r11`, `r12`),
quali una funzione deve restituire com'erano (`r14` in su), e come si legge lo stack frame da
`r1`.

È quello che trasforma il listato di `StaticR.rel` in qualcosa di leggibile senza debugger —
cioè in viaggio, che è metà del tempo in cui potrei lavorarci.

### E poi

- **Il blocco parametri a `0x808B5xxx`**: soglie e moltiplicatori della guida, si toccano con
  `kmWrite`, e un errore si *vede* invece di crashare. Adatto a lavorare senza debugger.
- **Nascondere le piste originali dall'online**: `CupsConfig::RandomizeTrack()` ha già il ramo
  giusto scritto (`else` quando `hasRegs` è falso). Id `< 0x100` = originali, `>= 0x100` = custom.
- **24 giocatori** — la stella polare. Ha una sezione sua qui sotto, perché non è un compito: è
  un elenco di cose che non so.

---

## La stella polare: 24 giocatori

Non è il prossimo compito e non lo sarà per un pezzo. È la cosa verso cui punto, e la tengo scritta
qui perché mi serve a scegliere: ogni argomento nuovo lo giudico anche per quanto mi avvicina a
questo.

Il punto è che **non so se sia possibile**, e metà del lavoro è scoprirlo. Quindi invece di un
piano, qui ci sono le domande.

### Quello che credevo di sapere, e non so

**"Ci sono 78 array `[12]`."** È il risultato di un `grep`, non un inventario. Dice una cosa molto
più debole di come suona:

| | |
|---|---|
| 78 | `[12]` negli header di `GameSource/` (39 file) |
| +36 | altri in `PulsarEngine/`, cioè nel codice del motore, che è il mio |
| ? | quanti di quei 78 sono davvero *un elemento per giocatore*, e quanti sono un buffer da 12 byte che non c'entra niente |
| ? | quanti sono scritti in un altro modo e il grep non li vede — `[0xC]` nel repo c'è già, e un array dimensionato da una costante non lo trovo cercando `12` |

E soprattutto la categoria che **nessun grep può vedere**: i 12 scritti dentro le istruzioni. Un
`cmpwi r3, 12` o un `li r0, 12` nel binario del gioco non sta in nessun header. Quelli si trovano
solo disassemblando, e non ho idea di quanti siano.

> Il numero vero non è 78. Non so quale sia, e per saperlo va guardato uno a uno.

**"Il vincolo vero è la memoria."** Lo ripeto, ma non l'ho misurato. Non so:

- quanto occupa **oggi** un giocatore, tutto compreso — kart, Mii, oggetti, fisica, AI
- cosa scala davvero col numero di giocatori e cosa invece è fisso
- quanta memoria libera resta in una gara pesante a 12
- se il limite è MEM1 o se c'è spazio da qualche altra parte

Finché non ho un numero per la prima riga, "24 giocatori" non è un progetto: è un desiderio. È la
**prima domanda a cui rispondere**, perché se lì la risposta è no, tutto il resto non conta.

### Le altre domande aperte

**Cosa si rompe per primo?** Memoria, framerate, o qualcosa di strutturale che non ho ancora visto
— tipo un indice giocatore salvato in mezzo byte, che a 24 semplicemente non ci sta. Non so
nemmeno in che ordine scoprirlo.

**Offline è davvero il gradino più facile?** Isola dalla rete, quello è certo. Ma 24 CPU che
calcolano percorsi potrebbero costare più di 24 umani che arrivano via rete. Non so se l'AI scali
peggio del resto, e se sia lei il muro vero.

**E la UI?** Minimappa, classifica in gara, schermata dei risultati: sono disegnate per 12. Non so
se sia un problema di layout, di array, o di tutti e due.

**L'online lo escludo dall'inizio** — il formato dei pacchetti è pensato per 12, e quello non è
codice mio.

### Perché la tengo come stella polare

Perché è l'unico obiettivo che ho che **non si risolve con un hook**. Tocca memoria, strutture
dati, disassemblato, e mi costringe a misurare invece di provare. Le cose che sto imparando adesso
— i bit, l'ABI, leggere un listato senza debugger — servono tutte a questo, ed è avere la meta
scritta che mi fa scegliere cosa studiare dopo.

E se la risposta finale è "non ci sta in memoria", va bene lo stesso: ci sarò arrivato **misurando,
non indovinando.**

---

## Il conto dei quattro giorni

Da `int prova = 5` a un hook piazzato in un punto che ho trovato da solo. In mezzo: `.` contro
`->`, riferimenti, membri statici, il contratto fra due funzioni, scope e shadowing, `=` contro
`==`, il ternario, `this`, conteggio contro indice, le tre `static`, comando contro domanda,
namespace aperte, virtuale contro non virtuale, `bl` contro `b`, i registri come firma della
funzione, DSI contro ISI, `lis` + offset, `lr − 4`, il prologo, i nomi mangled, `switch` e
fallthrough, quando si registrano gli hook, un debugger aperto per la prima volta, e un build
script sistemato perché smettesse di mentire.

E soprattutto: **smettere di indovinare.** Gli ultimi problemi li ho risolti ragionando, e su
qualcuno avevo già la risposta prima della conferma.
