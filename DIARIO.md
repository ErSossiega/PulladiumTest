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

**`GameSource/` non si allarga.** Quelle struct sono la mappa della memoria del gioco, decisa da
Nintendo nel 2008. `size_assert` esiste per fermarti.

*(Nota aggiunta dopo: "non si tocca" era detto male. **Aggiungere** una funzione che nel gioco esiste
già e che nessuno aveva documentato va benissimo — è solo documentazione, non cambia un byte.
Quello che rompe il patto è cambiare il **layout**: campi nuovi, array più grandi. Ed è quello che
avevo fatto io.)*

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
| `kmBranch` | `b` — vai *e basta* | sostituire **una funzione intera** \* |
| `kmWrite16/32` | un valore grezzo | cambiare **una variabile** o un'istruzione |
| `RaceLoadHook`, `RaceFrameHook`, `BootHook`, `SectionLoadHook` | — | **nessun indirizzo**: funzione `void`, una riga per registrarla |

> Prima di andare a caccia di un indirizzo, guarda se il momento che ti interessa **ha già un
> aggancio pronto**.

*\* Nota aggiunta dopo, da una correzione: `kmBranch` non serve solo a sostituire una funzione intera. `b` vuol dire "vai e non tornare", quindi messo sul **`blr`** di una funzione fa girare il tuo codice dopo che l'originale ha finito, tenendo il comportamento originale. Funziona; non è detto sia buona pratica.*


### Ricavare un indirizzo

Il PowerPC non può caricare 32 bit in un colpo: **spezza sempre in due**.

```
8057efec   lis  r3, 0x808B        → r3 = 0x808B0000
8057eff0   lha  r3, 0x5CC6 (r3)   → legge a 0x808B0000 + 0x5CC6 = 0x808B5CC6
8057eff4   blr
```

**indirizzo = (valore del `lis` << 16) + offset.** Aritmetica, non tentativi.

L'offset è **con segno** — 16 bit in complemento a due. Se il valore **grezzo** comincia con 8-F è
negativo: prima sottraici `0x10000`. Dolphin di solito quel passaggio lo fa già lui e te lo stampa
col segno, come `-0x4000`, quindi la regola della prima cifra serve davvero quando leggi
l'esadecimale grezzo.

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

*(Nota aggiunta dopo: stavo facendo tutto questo **senza una mappa dei simboli**, e non sapevo si potesse. Con `RMCP01.map` caricata, Dolphin mostra i nomi veri e demangled su tutto StaticR.rel, e quasi tutto l'orientarsi a mano qui sopra non serve più. La call tree di Dolphin trova anche i punti di chiamata più a monte, e con il progetto di decomp di MKW puoi usare Ghidra per elencare ogni riferimento a una funzione.)*

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

## Giorno 5 — il randomizzatore degli oggetti

Due settimane di pausa in mezzo. Riprendere è costato più di quanto pensassi, e non per la
sintassi: quello che si era perso era l'abitudine a **far girare il codice nella testa**. La prima
mezz'ora non ricordavo nemmeno cosa fosse un'inizializzazione, e gli array me li sono dovuti
riprendere da zero.

### Cosa è stato costruito

`PulsarEngine/Race/ItemRandomizer.cpp`. Prendi un oggetto, l'icona dice la verità, premi il tasto,
e succede altro. La discrepanza voluta fra **icona** ed **effetto**.

Stato a fine giornata: il meccanismo di copia e riscrittura della tabella **funziona**, lo
scorrimento fisso funziona, il sorteggio funziona. Manca il mescolamento vero, e l'ultima versione
crasha per un indice fuori dall'array. Non è finito, ma ogni pezzo è stato verificato da solo prima
di costruirci sopra il successivo.

### Il pezzo che regge tutto

`GameSource/MarioKartWii/Item/ItemBehaviour.hpp:25`

```cpp
static Behavior behaviourTable[19];   //809c36a0, index item id
```

Diciannove righe, una per `ItemId`, e in ogni riga `objId`, `numberOfItems`, `useType` e un
puntatore a funzione. Quando premi il tasto, il gioco va nella riga dell'oggetto che hai e chiama
quello che ci trova scritto. Non c'è nessuno `switch` gigante: la strada è una, e passa di lì.

È un **membro statico con un simbolo in `symbols.txt`**, quindi si raggiunge scrivendo
`Item::Behavior::behaviourTable[i]`. Nessun indirizzo scritto a mano: includi l'header e il linker
fa il resto. E `0x809c36a0` sta sopra `809BD6E8`, cioè in zona dati: è una variabile, si può
scrivere.

### Leggere un puntatore a funzione

```cpp
void (*useFunction)(Player& player);   //0x18, null = l'oggetto è trascinabile
```

Si legge partendo dal nome e andando verso l'esterno. `useFunction` è avvolto da `(* … )`, quindi è
un puntatore. Subito dopo c'è una lista di parametri, quindi è un puntatore **a funzione**. Il
`void` iniziale è quello che quella funzione restituisce.

Le parentesi attorno alla stella non sono decorative: `void *useFunction(Player&)` sarebbe una
funzione che restituisce un puntatore, cosa completamente diversa.

Due conseguenze pratiche, sbagliate entrambe prima di capirle:

- il **nome di una funzione è già il suo indirizzo**, quindi si assegna senza parentesi. Con le
  parentesi la chiami adesso, che è l'opposto di quello che vuoi: è la differenza fra dare a
  qualcuno il tuo numero e chiamarlo mentre glielo dai;
- non è un caso che la firma sia `(Player&)`. Le funzioni già presenti nella tabella sono i metodi
  `Use...` di `Item::Player`. Un metodo riceve l'oggetto come primo argomento nascosto, quindi
  `void Player::UseBullet()` e `void f(Player&)`, a livello di macchina, sono la stessa forma.

### Il valore assurdo: tutte e 19 le righe alla stessa funzione

Primo test, prima di qualunque permutazione: puntare la `useFunction` di **tutte** le righe a una
funzione mia che chiama `player.UseBullet()`.

In gara, qualunque cosa uscisse dalla cassa dava il bullet. Tre risposte in una gara sola: la
tabella è viva, è scrivibile a `RaceLoadHook`, e nessuno la sovrascrive dopo di me.

**Perché non filtrare le righe da riempire:** metà tabella ha `useFunction` nulla — banane e gusci,
quelli che si trascinano. Se avessi scritto solo dove era già pieno e poi in gara mi fosse uscita
una banana senza effetti, non avrei saputo distinguere "quella riga l'ho saltata" da "il codice non
funziona". Riempiendo tutto, il risultato atteso è netto e la risposta è una sola.

### La scoperta vera: `useFunction` non è tutto

Con la tabella riempita, gli oggetti lanciabili facevano **due cose insieme**: la banana partiva
lanciata *e* partiva il bullet.

Quindi il lancio non passa da `useFunction`. Se ci passasse, avendola io sovrascritta, la banana
non sarebbe più partita. Il lancio passa da un'altra strada, scelta in base a `useType` — nelle note
avevo già trovato `PrepareHandlers[5]` e `UpdateHandlers[5]`, cinque come i cinque valori di
`useType`. E l'oggetto lanciato era quello **giusto**, quindi quella strada legge `objId` dalla
stessa riga.

> Il comportamento di un oggetto non sta in un campo solo: sta in `useType`, `objId` e
> `useFunction` insieme. Toccarne uno e lasciare gli altri dà due comportamenti sovrapposti.

È la ragione per cui la versione buona sposta **righe intere da 0x1c byte**, non puntatori. E la
buona notizia che ne viene: siccome `objId` viaggia con la riga, quando prendi il bullet e dentro
c'è la riga della banana, esce una banana vera, col suo modello e il suo comportamento.

### La copia di sicurezza, e il test il cui risultato atteso è "niente"

Per mescolare serve un originale intatto. Se scrivi nella tabella mentre la leggi distruggi i dati
man mano, e alla gara dopo lavoreresti su una tabella già mescolata: dopo tre gare, poltiglia.

Quindi un array mio di 19 righe, riempito **una volta sola** alla prima gara, con un `bool` che si
ricorda che è fatto. Poi, a ogni gara, un secondo ciclo che riscrive la tabella leggendo dalla copia.

Il passo intermedio è quello che mi ha dato più fiducia di tutti: il secondo ciclo che copia la riga
`i` nella riga `i`, cioè **che non cambia niente**. Se il meccanismo funziona, il gioco deve
comportarsi esattamente come al solito. Se invece esplode, il problema è nel copiare, e lo scopri
prima di averci costruito sopra il resto.

Il log lo ha confermato meglio della gara: diciannove righe di copia alla prima gara, **zero alla
seconda**, e diciannove righe di riscrittura in entrambe. Il `bool` faceva il suo mestiere.

### Lo scorrimento fisso, prima del caso

Prima di mettere in mezzo l'RNG, uno scorrimento di uno: la riga `i` prende la riga `i + 1`, e
l'ultima riprende la prima con `(i + 1) % 19`.

Deterministico, quindi le previsioni si scrivono prima di avviare: guscio verde → guscio rosso,
banana → fake item box, fungo → triplo fungo, tripla banana → guscio verde. In gara è tornato tutto.

Sull'indice: `i + 1` da solo, all'ultimo giro, leggerebbe la casella 19 di un array che arriva alla
18. Il resto della divisione serve a chiudere il cerchio.

### Il seme

`Random` non è casuale: è una sequenza calcolata che parte da un numero, il seme. **Stesso seme,
stessa sequenza, sempre.** Se il seme non cambia, ogni gara ha la stessa mappa.

Serve un numero che cambi da solo, e la console ne ha uno pronto: `OS::GetTick()`, il contatore che
sale da quando hai acceso.

`XPF.cpp` fa lo stesso ma gli gira attorno i bit, scambiando metà alta e metà bassa, perché i bit
bassi del contatore corrono velocissimi e quelli alti quasi non si muovono. Offline il tick liscio
basta. Online no, e non per eleganza: se il seme non è identico su tutti i client, ognuno gioca una
partita diversa. Per questo XPF, nelle stanze private, il seme se lo fa dare dalla stanza invece
che dall'orologio.

### Pescare con reimmissione non è mescolare

Con `NextLimited(19)` chiamato diciannove volte il codice girava, e in gara sembrava tutto a posto.
Il log diceva un'altra cosa. Prima gara, numeri estratti:

```
16, 0, 9, 6, 11, 12, 15, 13, 8, 14, 15, 5, 5, 17, 9, 15, 11, 17, 2
```

Il 15 tre volte. Il 5, il 9, l'11 e il 17 due volte. E mai usciti: 1, 3, 4, 7, 10, 18.

Tradotto in gara: **sei comportamenti su diciannove non esistevano in quella partita**, e tre oggetti
diversi facevano la stessa identica cosa. Non è ripetizione fra una gara e l'altra, è la mappa di
*quella* gara a essere storta.

Il motivo è che ogni chiamata non sa niente delle precedenti. È distribuire carte rimettendo ogni
volta la carta nel mazzo: qualcuno riceve tre assi e sei carte non escono mai. La soluzione non è
pescare meglio, è **cambiare gesto**: mescolare il mazzo e poi distribuire in ordine, così ogni riga
va a esattamente una posizione e tutte vengono usate.

E c'è il motivo per cui giocando non si vedeva: in una gara raccogli una manciata di oggetti, non
diciannove, quindi è facile che la coppia doppia non ti capiti tutta e due le volte sotto mano.

> Quando un effetto è troppo raro o troppo sparso per vederlo giocando, si smette di giocare e si
> stampa.

### Il crash, e il posto dove **non** è la colpa

Ultima versione della giornata. Un bot prende una cassa e:

```
Error: DSI
SRR0: 0x80797500  Item::ObjHolder::GetTotalItemCount
LR:   0x807BB7D4  Item::SlotData::DecideItem
R03:  0xA544F4D4
```

La catena delle chiamate: `Itembox::OnCollision` → `Player::DecideItem` →
`PlayerRoulette::DecideItem` → `SlotData::DecideItem` → `GetTotalItemCount`, e lì un indirizzo che
non è un indirizzo.

La colpa era mia, in questa riga:

```cpp
randomItemArray[i + random.NextLimited(19-1)]
```

`i` arriva a 18, la pescata arriva a 17, la somma arriva a 35, e l'array ha 19 caselle. Per buona
parte dei giri stavo **leggendo memoria che non mi appartiene** e scrivendo quella spazzatura nella
tabella del gioco come se fosse una riga di comportamento. Quella riga finta contiene un `objId` che
non è un oggetto; il gioco lo usa per cercare il contenitore corrispondente e si ritrova in mano
`0xA544F4D4`.

> **Il luogo del crash non è il luogo della colpa.** Nel dump non compare una sola riga di codice
> mio. Io ho lasciato la mina al caricamento della gara, a calpestarla è stato il gioco minuti dopo.
> Gli errori di memoria si trovano solo risalendo la catena delle chiamate e poi chiedendosi chi ha
> scritto dati sbagliati là dentro.

### Gli errori del giorno 5

**L'inizializzazione fuori da tutte le funzioni.**

```cpp
bool isVSRace = DriverMgr::isVSRace;   // a livello di namespace
```

La parte a destra dell'uguale viene letta **una volta sola**, quando quella variabile nasce, cioè
all'avvio del gioco. In quel momento non c'è nessuna gara, quindi il valore è falso e resta falso
per sempre. L'`if` non era mai vero e il log non usciva mai: il log funzionava, non veniva mai
raggiunto.

La stessa riga **dentro** la funzione dell'hook viene eseguita a ogni caricamento gara e legge il
valore vero. Stessa riga, due posti, due comportamenti.

Su Kamek c'è un motivo in più per non fidarsi: gli inizializzatori dinamici a livello di namespace
potrebbero non essere eseguiti affatto. Le globali si dichiarano; non si inizializzano con valori
presi dal gioco. Con una costante, tipo `static bool isRandom = false;`, invece va benissimo: quel
valore finisce scritto nel binario e non c'è niente da eseguire.

**Le due funzioni confuse per una.** Il lavoro è fatto di due funzioni con mestieri opposti, e per
mezz'ora ho provato a scriverle come se fossero la stessa:

| | quando gira | cosa fa | firma |
|---|---|---|---|
| `randomItem` | al caricamento gara, una volta | riscrive la tabella | `void()` |
| `randomAHMoment` | in gara, quando premi il tasto | l'effetto nuovo | `void(Item::Player&)` |

L'indice e la tabella servono nella prima. Quando parte la seconda, la ricerca nella tabella
**l'ha già fatta il gioco**, altrimenti non sarebbe finito lì dentro. Dentro la seconda non c'è
niente da cercare, c'è solo da fare.

**`getBehaviorIndex()`.** Non esiste. Se l'è inventato l'autocomplete dell'editor. Cercato in tutto
`GameSource`: zero occorrenze, con qualsiasi maiuscola.

> Se un nome non lo trovi cercandolo dentro `GameSource`, quel nome non esiste, per quanto
> ragionevole sembri. Gli header sono documentazione scritta a mano da più persone, e i nomi non
> seguono nessuna convenzione prevedibile.

**`Item::Player::UseBullet();`**

```
a nonstatic member reference must be relative to a specific object
```

`::` raggiunge qualcosa che esiste in copia unica: uno statico, un namespace. `.` raggiunge qualcosa
che appartiene a un oggetto preciso, e quindi un oggetto lo devi avere. Il bullet non esiste in
astratto: esiste quello **di un giocatore**, che consuma il suo oggetto e muove il suo kart. Quel
giocatore ce l'avevo già in mano, era il parametro.

**`Random random();`** — non crea un oggetto. Per il compilatore dichiara una **funzione** che si
chiama `random`, non prende parametri e restituisce un `Random`. Quando non passi argomenti,
l'oggetto si crea senza parentesi. È lo stesso scherzo delle parentesi del puntatore a funzione, in
un'altra veste.

**`Item::Behavior& behavior;`** — una referenza è un soprannome, e va inizializzata nel momento in
cui nasce. Non si crea vuota e si riempie dopo come un `bool`.

Il rovescio è peggio, perché è silenzioso: **dimenticare la `&`**.

```cpp
Item::Behavior behavior = Item::Behavior::behaviourTable[i];   // copia!
```

Compila, nessun warning, il gioco parte, e in gara non succede niente: hai modificato una copia che
muore alla fine del giro di ciclo. Legale e silenzioso, di nuovo.

**Le quadre vogliono dire due cose diverse.**

```cpp
Item::Behavior copia[19];   // DICHIARAZIONE: creami 19 caselle
copia[3]                    // USO: dammi la casella numero 3
```

Nella dichiarazione il numero è *quante*, nell'uso è *quale*. Ecco perché `[19]` è giusto in una e
fuori dall'array nell'altra. Questa confusione mi ha fatto scrivere `behaviourTable[19]` come indice
più di una volta.

**L'`if` dentro il `for` invece che attorno.** La copia di sicurezza va fatta una volta sola, e avevo
messo il controllo dentro il ciclo:

- giro con `i` a 0: il bool è falso, copio la riga 0, metto il bool a vero
- giro con `i` a 1: il bool adesso è vero, salto
- giri da 2 a 18: salto, salto, salto

Una riga copiata e diciotto caselle vuote. Il `for` è la ripetizione; la domanda "l'ho già fatto?"
si fa una volta, prima, quindi sta fuori. Prima si decide se fare il lavoro, poi si fa tutto il
lavoro, poi ci si segna che è stato fatto.

Il trucco che ha trovato il bug costa dieci secondi: **recitare il primo giro, il secondo e
l'ultimo.** Gli errori sui cicli non sono quasi mai errori di scrittura — il codice è giusto come
frase e sbagliato come storia, e la storia si sente meglio a voce che a schermo.

**Il seme non inizializzato.**

```cpp
s32 seed;              // dichiarato e mai assegnato
Random random(seed);   // ...e passato così
```

Il seme era quello che capitava di trovare in quella zona di stack. È **esattamente** il bug che
avevo trovato in un file altrui mezz'ora prima, rifatto nel mio.

**Il log che stampava l'oggetto invece del numero.** Passavo `random` al posto del numero pescato, e
uscivano diciannove righe con lo stesso valore, `-2143711312`, che è un indirizzo di stack e che
cambiava fra una gara e l'altra.

La correzione non è cosmetica: **il numero pescato va messo in una variabile.** `NextLimited` non è
una domanda, è un'azione: ogni chiamata fa avanzare la sequenza. Chiamandola una volta dentro le
quadre e una nel log, il log racconterebbe un numero diverso da quello davvero usato.

E in C non esistono le stringhe da sommare col `+`: si scrivono segnaposto nel testo e si passano i
valori dopo, in ordine — `%d` decimale, `%x` esadecimale, `%s` testo. Il numero di segnaposto deve
corrispondere ai valori passati: due `%d` e un valore solo, e il secondo se lo pesca dalla memoria a
caso.

**Due log con lo stesso testo.** Quando ne usciva uno non sapevo quale dei due fosse. Vanno scritti
diversi, e ne va messo uno **prima** dell'`if`, non dentro: così distingui "la funzione non è mai
partita" da "è partita ma la condizione era falsa". Sono due bug diversi e si cercano in due posti
diversi.

**Due cicli annidati invece di due cicli in fila.** Mescolare e distribuire sono lavori separati, e
infilandoli uno dentro l'altro facevo 361 scambi invece di 19, con la scrittura nella tabella in
mezzo al mescolamento. La forma giusta sono due `for` uno sotto l'altro.

E un classico dentro il classico: `for (int j = 0; j < 19; i++)`, che fa avanzare `i` invece di `j`.
Il compilatore non dice niente, perché è una frase legale.

### Due simboli mancanti, stessa procedura

`isVSRace` e `__vt__6Random` non erano in `symbols.txt`. Il commento nell'header dà l'indirizzo, ma
**il commento non è il linker**: se il simbolo non è nella lista, il link fallisce.

```
isVSRace__9DriverMgr = 0x809c38ba
__vt__6Random        = 0x808b42e0
```

Il secondo è la vtable. `Random` ha un distruttore virtuale, quindi ogni oggetto si porta dietro un
puntatore alla tabella delle funzioni virtuali, e chi ce lo scrive è il costruttore: creare un
`Random` significa fare riferimento a quell'indirizzo.

Il gesto ormai è meccanico: **l'errore dà il nome, l'header dà l'indirizzo, la lista li unisce.**

### Un bug trovato in casa d'altri

`PulsarEngine/Extensions/LECODE/XPF.cpp`, la funzione che decide lo scenario casuale:

```cpp
s32 seed;                          // riga 29
if(stanza privata) {
    ...
    u32 seed;                      // riga 33 — stesso nome
    ...
    seed = ...;                    // scrive in QUESTA
}
else {
    seed = ...tick...;             // scrive in quella di fuori
}
Random random(seed);               // legge quella di fuori
```

Due variabili con lo stesso nome: quella interna **copre** l'esterna per tutta la durata del blocco.
È l'ombra, *shadowing*. Nel ramo delle stanze private si scrive nella variabile interna, che muore
alla parentesi chiusa, e il costruttore legge quella esterna, mai assegnata.

Quindi nelle stanze private quel seme è spazzatura, e il risultato è proprio quello che la funzione
vuole evitare. Da segnalare a chi mantiene Pulsar, insieme alla patch del build script.

### Cosa manca al randomizzatore

**Il mescolamento vero**, cioè scambiare le righe a due a due invece di pescare, e con il sacchetto
che si restringe: il compagno di scambio si sorteggia solo fra le posizioni non ancora sistemate,
`i + NextLimited(19 - i)`. È quello che impedisce ai doppioni di nascere.

Lo scambio vuole una variabile d'appoggio: `a = b` seguito da `b = a` non scambia niente, perché il
primo assegnamento ha già cancellato `a` e restano due copie di `b`.

E l'ultimo passo, che non è pulizia ma parte del lavoro: **togliere il log.** Un log che stampa la
mappa rende inutile la feature, perché chi apre la console sa in anticipo cosa fa ogni oggetto. Al
suo posto ci va un commento, o fra due mesi lo rimetto per debuggare altro e non me ne accorgo.

### Cosa mi porto dietro dalla pausa

Che a tornare non si perde la sintassi, si perde il **metodo**. Le cose che mi hanno sbloccato oggi
non erano nozioni di C++: erano recitare i giri del ciclo a voce, stampare invece di giocare,
tenere separati due lavori in due cicli, e fare il test neutro il cui risultato atteso è che non
cambi niente.

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
