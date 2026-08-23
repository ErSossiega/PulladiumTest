# Diario — imparare il C++ su Pulladium

**20–21 agosto 2026.** Due giorni, dal "so cosa sono i puntatori" a "ho scritto i miei hook".

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

Tre volte in due giorni, sempre la stessa forma:

| | La modifica non arrivava perché |
|---|---|
| il `pos` del tachimetro | stava su una riga morta (`if(count == 1)` con `count` ormai 2) |
| il `kmBranch` | il `.o` non era stato ricompilato |
| la stella | il taccuino veniva cancellato nello stesso frame |

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

---

## Cosa resta aperto

- Due gare di fila, per verificare che `RaceLoadHook` azzeri davvero anche col *riprova*
- Il `\n` nei log, e messaggi diversi in punti diversi
- L'impostazione destra/sinistra del tachimetro: ancora una riga morta
- Il kit per il telefono: VPS per compilare, Dolphin Android per provare, listato disassemblato
  per gli indirizzi, `OS::Report` al posto del debugger
- Un `kmCall` chirurgico in un punto trovato da me, invece che preso da un header

---

## Il conto dei due giorni

Da `int prova = 5` a due hook che girano su Wii. In mezzo: `.` contro `->`, riferimenti, membri
statici, il contratto fra due funzioni, scope e shadowing, `=` contro `==`, il ternario, `this`,
conteggio contro indice, le tre `static`, comando contro domanda, `bl` contro `b`, i registri come
firma, DSI contro ISI, `lis` + offset, i nomi mangled, e un debugger aperto per la prima volta.

E soprattutto: **smettere di indovinare.** Gli ultimi problemi li ho risolti ragionando, e su
qualcuno avevo già la risposta prima della conferma.
