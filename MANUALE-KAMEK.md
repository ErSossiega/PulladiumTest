# Manuale Kamek

Riferimento pratico per Pulladium / Pulsar su Mario Kart Wii: trovare un indirizzo, scegliere
l'hook giusto, capire perché non funziona.

Indirizzi verificati su **Pulladium 2.1.1**, Mario Kart Wii **PAL (RMCP)**.

---

## 1. Le tre domande, in quest'ordine

Quasi tutti i pomeriggi persi nascono dal saltarne una, o dal farsele nell'ordine sbagliato.

### 1.1 Dove sta la cosa che voglio cambiare?

Negli header sotto `GameSource/`: ogni funzione ha il suo indirizzo nel commento. **Quella è la
mappa di Mario Kart Wii** — non esiste nessun altro file da procurarsi, e nessun `.map` del gioco.

```cpp
s32 GetMTMaxCharge() const;   //8057efe0
s16 driftState;               //0xfc
s16 mtCharge;                 //0xfe
```

**Attenzione agli omonimi.** `GetMTMaxCharge` esiste sia in `KartMovement.hpp` (`8057efe0`) sia in
`KartLink.hpp` (`80591208`): stesso nome, funzioni diverse, indirizzi diversi.

### 1.2 Cosa c'è davvero a quell'indirizzo?

Non darlo per scontato: **aprilo nel debugger e leggi le istruzioni.** Una funzione che sembra
decidere qualcosa spesso è solo un getter di tre righe.

Serve a distinguere due casi che si risolvono in modi opposti: *codice* che calcola, oppure
*una variabile* che qualcuno legge.

> **Agganciare un getter cambia le cose solo per chi chiama il getter.** Il codice che gira ogni
> frame spesso legge la variabile direttamente, e il tuo hook non lo sfiora. Funziona tutto e non
> succede niente.

### 1.3 Che tipo di hook serve?

La risposta viene da sola dal punto 1.2. La tabella della sezione 4 la traduce in una macro.

---

## 2. Ricavare un indirizzo

Il PowerPC **non può caricare un indirizzo a 32 bit in una sola istruzione**. Deve sempre
spezzarlo in due: prima la metà alta, poi la metà bassa come offset.

```
8057efec   lis  r3, 0x808B        → r3 = 0x808B0000
8057eff0   lha  r3, 0x5CC6 (r3)   → legge a 0x808B0000 + 0x5CC6
8057eff4   blr                    → ritorna il valore in r3
```

> **indirizzo = (valore del `lis` << 16) + offset dell'istruzione dopo**
>
> `0x808B0000 + 0x5CC6 = 0x808B5CC6`. Non è a tentativi, è una somma.

`lis` = *Load Immediate Shifted*: mette il valore nei 16 bit alti. Le istruzioni che seguono
(`lha`, `lwz`, `lfs`, `stw`…) ci sommano il loro offset.

### Attenzione: l'offset è con segno

È un campo a **16 bit in complemento a due**, che detto semplice vuol dire che la metà alta del suo
intervallo sono in realtà numeri negativi. Se il valore **grezzo** comincia con **8, 9, A, B, C, D,
E o F**, sottraici `0x10000` e usa quello:

```
lis  r3, 0x808C
lwz  r3, 0xC000 (r3)    grezzo 0xC000 → comincia con C → 0xC000 − 0x10000 = −0x4000
                        → 0x808C0000 − 0x4000 = 0x808BC000
```

In pratica a mano lo farai di rado, perché Dolphin e quasi tutti i disassemblatori quel passaggio
l'hanno già fatto per te e ti stampano l'offset **con il segno già applicato**, come `-0x4000`. La
regola della prima cifra serve quindi quando leggi l'esadecimale grezzo dai byte: se nel listato c'è
già il meno davanti, è tutto lì — sottrai e vai avanti.

Sbagliare verso ti manda 64 KB più in là, in mezzo ai dati di qualcun altro. E sembrerà pure
plausibile.

### In che binario sono

| Intervallo | Binario | Cosa c'è |
|---|---|---|
| `80004000` – `~80388000` | main.dol | ossatura di sistema, libc |
| `~805102E0` – `~808D9A58` | StaticR.rel | gara, UI, kart — quasi tutto |
| `~809BD6E8` in su | dati / BSS | variabili globali, non codice |

**Non disassemblare il file `StaticR.rel`.** È rilocabile: sul disco gli indirizzi non sono quelli
finali e non corrispondono ai commenti negli header. Usa il debugger di Dolphin, che legge la RAM
a gioco avviato.

(Se ti serve un listato statico: i **target dei salti** sopravvivono nel file, gli **immediati di
`lis`/`lha`** no — sono azzerati e li riempiono le rilocazioni al caricamento.)

---

## 3. I registri

I registri **non hanno nessun rapporto con i concetti del gioco.** Sono 32 caselle generiche.
Lo stesso `r3` un istante contiene un puntatore a un kart, quello dopo un contatore.

Quello che esiste è una **convenzione**, valida **al momento delle chiamate di funzione**:

| Registro | Ruolo |
|---|---|
| `r0` | scratch. In certe istruzioni significa **il numero zero**, non il registro |
| `r1` | **stack pointer**. Non si tocca |
| `r2`, `r13` | puntatori alle aree dati piccole |
| **`r3`–`r10`** | **argomenti**, in quest'ordine |
| **`r3`** | e anche il **valore di ritorno** |
| `r11`, `r12` | scratch |
| **`r14`–`r31`** | **preservati**: chi li usa deve salvarli e ripristinarli |
| `lr` | indirizzo di ritorno |
| `cr` | risultato dei confronti |

Per i float, `f1`–`f8` come argomenti e ritorno.

### Riconoscere la fine di una funzione

```
80580634   lwz   r0, 0x14(r1)     ← ripesca l'indirizzo di ritorno dallo stack
80580638   lwz   r31, 0xc(r1)     ← ripristina r31, che è preservato
8058063c   mtlr  r0               ← lo rimette in lr
80580640   addi  r1, r1, 0x10     ← libera lo spazio sullo stack
80580644   blr                    ← torna
```

Schema standard. Riconoscerlo ti dice a colpo d'occhio dove finisce una funzione e comincia quella
dopo.

### Riconoscere una funzione senza argomenti

```
80790e3c   lis  r3, 0x809D    ← PRIMA istruzione: sovrascrive r3
```

> Se la prima istruzione **scrive** in `r3` invece di leggerlo, la funzione non ha argomenti.

---

## 4. Quale hook

| Macro | Cosa scrive | Quando |
|---|---|---|
| `kmCall(addr, fn)` | `bl` — vai **e torna** | dirottare **una chiamata** in mezzo a una funzione |
| `kmBranch(addr, fn)` | `b` — vai **e basta** | saltare via e non tornare — di solito per sostituire una funzione dalla prima istruzione |
| `kmWrite16/32(addr, val)` | un valore grezzo | cambiare **una variabile** o una singola istruzione |
| `kmWriteNop(addr)` | `60000000` | cancellare un'istruzione |

### `kmBranch` non serve solo a sostituire una funzione intera

`b` vuol dire "vai e non tornare", e vuol dire solo quello. Messo sulla prima istruzione di una
funzione la sostituisce — è l'uso comune. Ma messo sul **`blr`** della funzione, il tuo codice gira
*dopo* che l'originale ha fatto il suo lavoro, tenendo intatto il comportamento originale.

Merito di chi me l'ha fatto notare sul server di Pulsar. Vale la pena tenere anche la loro
precisazione: funziona, non è detto sia buona pratica.

### Hook senza indirizzo

In `KamekInclude/kamek.hpp`, in fondo. Funzione `void` senza argomenti, una riga per registrarla:

```cpp
RaceLoadHook    reset(miaFunzione);      // al caricamento della gara
RaceFrameHook   tick(altraFunzione);     // a ogni frame (60/s)
SectionLoadHook sez(terzaFunzione);      // al cambio sezione
BootHook        boot(quarta, 0);         // all'avvio (il REL non è ancora caricato)
```

Sono **tipi**, come `int`: due hook = due variabili dello stesso tipo, non due classi.

> Prima di andare a caccia di un indirizzo, guarda se il momento che ti interessa **ha già un
> aggancio pronto**.

### `kmCall` su un inizio di funzione: il crash classico

La `bl` si segna dove tornare. La tua funzione fa `blr` e rientra **a metà della funzione
originale**, con `r3` pieno del tuo valore di ritorno invece del puntatore che quel codice si
aspetta. DSI immediato, con il tuo numero visibile in `r3` nel dump.

### La firma è la mappa dei registri

```cpp
// il gioco chiamava: ApplyLightningEffect(frames, unk0, unk1)
void MegaTC(Kart::Movement& movement, int frames, int unk0, int unk1)
//          ↑ r3               ↑ r4        ↑ r5      ↑ r6
```

- Il `this` di un metodo diventa il **primo parametro esplicito**
- Dichiara **tutti** i parametri originali, anche quelli che ignori: tengono il posto
- Non inventarne di nuovi: leggerebbero registri che contengono altro

> Un hook che non richiama l'originale quando la sua condizione è falsa non estende il gioco:
> **lo rompe.** È il ramo `else` di `MegaTC.cpp`.

### Gli indirizzi sono PAL

- Quelli passati a `kmCall` / `kmWrite` / `kmBranch` vengono **rimappati** da Kamek con
  `versions.txt`
- Quelli scritti **dentro** il codice, castati a puntatore, **no**: restano PAL e su altre regioni
  puntano nel vuoto

---

## 5. `symbols.txt`

Serve a **chiamare** funzioni del gioco dal tuo codice — è il `-externals` del comando di link.
Gli hook **non** ci passano: a loro dai il numero e basta.

```
nome_mangled = 0xINDIRIZZO
```

Le righe `##RVL##`, `##EGG` e simili sono solo commenti per gli umani.

### Leggere un nome mangled

```
UseItem__Q24Item9PlayerObjFb
```

| Pezzo | Significato |
|---|---|
| `UseItem` | nome della funzione |
| `__` | da qui comincia la codifica |
| `Q2` | nome **qualificato**, **2** componenti |
| `4Item` | 4 caratteri → `Item` |
| `9PlayerObj` | 9 caratteri → `PlayerObj` |
| `F` | è una funzione, seguono i parametri |
| `b` | un `bool` |

= `Item::PlayerObj::UseItem(bool)`

Il numero prima di ogni nome è la sua lunghezza. Codici ricorrenti: `v` void · `b` bool · `i` int ·
`Uc` u8 · `Us` u16 · `Ui` u32 · `f` float · `P` puntatore a… · `C` const · `R` riferimento a… ·
`e` ellipsis (`...`).

Nella radice di Pulladium c'è **`demangler.py`** per i nomi lunghi.

### Non serve scriverlo a mano

Dichiara la funzione con namespace, classe e firma corretti. Chiamala. Compila. **Il linker
fallirà stampandoti la stringa esatta** che non trova: quella copi.

Se sbagli la firma, il nome mangled cambia e il linker cerca un simbolo diverso da quello che hai
messo nel file. L'errore sembrerà assurdo ("ma io ce l'ho messo!"). Confronta **carattere per
carattere**.

---

## 6. "Non funziona": la catena delle verifiche

> Quando una modifica non ha **nessun** effetto — non un effetto sbagliato, proprio nessuno — il
> primo sospetto non è mai la logica. È la catena.

1. **Salvato?** Il build legge il file dal disco, non dall'editor.
2. **Compilato?** `ls -la build/TuoFile.o` — deve essere **più recente** del `.cpp`.
3. **Linkato?** Se il prompt dice *"No source or header files were modified"* subito dopo che hai
   modificato qualcosa, non è un'offerta: è un avvertimento. Rispondere `L` rilinka gli oggetti
   vecchi.
4. **Copiato?** `RIIVO` in `BuildPulsar.py` punta al pack che stai davvero avviando?
5. **Ricaricato?** Riivolution legge i file all'avvio della ISO, non a caldo.
6. **L'asset giusto?** Se il pack ha copie per lingua (`Language/ITA/Assets/`), il gioco potrebbe
   aprire l'altra.

### Il difetto del build script

```python
def compile_cpp(cpp: str):
    subprocess.run(cmd, shell=True)   # nessun controllo del codice di uscita
```

**Un errore di compilazione è silenzioso**: non ferma niente, l'oggetto vecchio resta, il link
riesce, e ti consegna un `Code.pul` con timestamp fresco e dentro il codice di ieri.

Gli errori del compilatore che scorrono nell'output non sono rumore. Sono l'unica cosa che conta.

### Verificare cosa c'è davvero nel binario

Gli hook finiscono nel `Code.pul` come comandi riconoscibili — il primo byte dice il tipo:

```
20 ff ff fe   80 57 ef 38   41 82 00 a4   kmWrite32
40 ff ff fe   80 57 ef dc   00 01 83 e4   kmBranch  (b)
41 ff ff fe   80 58 2f dc   00 01 84 2c   kmCall    (bl)
```

Se il sorgente dice `kmBranch` e il binario dice `41`, la modifica non è mai stata compilata.

---

## 7. Il debugger di Dolphin

Si attiva dalle impostazioni, sezione interfaccia. Compaiono **Code**, **Registers**,
**Breakpoints** e **Memory**.

**Il breakpoint risponde sì o no.** Scatta → la funzione **viene** chiamata. Non scatta mai → stai
agganciando il posto sbagliato, e nessuna quantità di ragionamento sul codice te lo direbbe.

**Leggere un valore di ritorno**: breakpoint sul `blr`, poi guarda **`r3`**. Vale anche al
contrario — in un crash, un `r3` che contiene un numero tuo dice che il tuo codice è appena passato
di lì.

**Guardare la memoria**: pannello Memory, con l'emulazione **ferma su un breakpoint**. A gioco
spento non c'è niente da leggere. Il nome del simbolo sarà `unk`: normale, non c'è nessuna mappa
caricata — ti serve il contenuto, non il nome.

> **Prima di aprire il debugger, prova un valore assurdo.** Un `1` al posto di un `270` dà un
> effetto impossibile da confondere. Se non cambia niente nemmeno con quello, l'hook non scatta —
> ed è un problema diverso, con una soluzione diversa.

Separare *"non scatta"* da *"scatta ma il valore non conta"* è la mossa che sblocca più spesso.

---

## 8. Leggere un crash

`Crash.pul`, **offset 12** = il tipo dell'eccezione.

```bash
python -c "import io,struct; d=io.open(r'PERCORSO/Crash.pul','rb').read(); print('error =', struct.unpack('>I', d[12:16])[0])"
```

**2** = DSI · **3** = ISI · **7** = virgola mobile · **8** = FPE

| | Cosa ha provato a fare la CPU | Dove guardare |
|---|---|---|
| **DSI** | leggere o scrivere **dati** a un indirizzo che non esiste | `srr0` è un'istruzione **vera** (una load/store); l'indirizzo marcio è **in un registro** |
| **ISI** | eseguire **codice** dove non c'è | `srr0` **è** la spazzatura, inutile; guarda **`lr`**, che dice da dove è partito il salto |

DSI = puntatore sbagliato. ISI = salto sbagliato.

Il resto del dump: `srr0` a offset 16, poi `srr1`, `msr`, `cr`, `lr`, i 32 GPR, i 32 FPR e infine
dieci stack frame con `sp`/`lr` — la catena dei chiamanti.

---

## 9. Trappole del linguaggio

### Lo `0x` non è decorativo

```cpp
kmWrite16(80591208, 1);     // decimale → 0x04CDF3A8. Non un indirizzo sbagliato: un ALTRO numero.
kmWrite16(0x80591208, 1);   // questo è l'indirizzo
```

### `=` scrive, `==` chiede

```cpp
if (count = 2)    // assegna 2, la condizione è sempre vera, e count è cambiato
if (count == 2)   // confronta
```

In Java il compilatore lo rifiutava. In C++ è legale, perché qualsiasi numero diverso da zero
vale *vero*.

### `.` oppure `->`: cerca la stella

| Dichiarazione | Cos'è | Accesso |
|---|---|---|
| `Tipo x` | l'oggetto | `.` |
| `Tipo& x` | un riferimento | `.` |
| `Tipo* x` | un indirizzo | `->` |

`a->b` è esattamente `(*a).b`. `this` è **sempre** un puntatore, quindi sempre `->`.
Sbagliare non è pericoloso: non compila.

Occhio al numero di stelle:

```cpp
Kart::Player** players;   // players[i] è un PUNTATORE → ->
Item::Player*  players;   // players[i] è l'OGGETTO    → .
```

### `void` vuol dire comando

> Se una funzione ritorna `void`, **fa** qualcosa. Non può rispondere a niente, quindi non può
> stare in un `if`.

Mettere `UseStar()` dentro una condizione non chiede se la stella è stata usata: **la fa partire**.
Le domande si fanno a chi ha una risposta — un campo, un getter. **Leggere è gratis, chiamare no.**

### Le tre `static`

| Dove | Cosa significa |
|---|---|
| in una **classe** | una copia sola condivisa da tutte le istanze |
| **dentro una funzione** | la variabile **sopravvive** fra una chiamata e l'altra |
| **fuori da tutto**, nel file | visibile solo in questo file |

Per un membro di classe, `static` si scrive **solo nella dichiarazione** (`.hpp`). Nel `.cpp` la
definisci qualificata col nome della classe e **senza** ripetere `static`.

### `GameSource/`: aggiungere sì, allargare è un'altra storia

Sono due operazioni diverse, e vale la pena non confonderle.

**Aggiungere documentazione** — dichiarare una funzione che nel gioco esiste già ma che nessuno
aveva scritto, correggere un nome, annotare un indirizzo trovato col debugger — non cambia niente a
runtime: dici solo al compilatore cosa c'è a quell'indirizzo. Sempre permesso.

E a volte necessario. `GameSource/` è documentazione scritta a mano (da melg), non un artefatto
ufficiale e non la stessa cosa della decompilazione in corso: **possono mancare classi intere e i
nomi possono essere sbagliati.** C'è chi, per far funzionare la propria feature, ha dovuto aggiungere
gruppi di funzioni e correggere il nome di una variabile. Quindi se quello che ti serve non c'è, la
risposta può essere che nessuno l'ha ancora scritto — non che stai cercando nel posto sbagliato.

**Cambiare il layout** — un campo nuovo, un array più grande — cambia `sizeof`, e `size_assert`
ferma la compilazione in decine di file, che sono **un errore solo, riportato molte volte**: leggi
il primo, ignora l'eco.

L'assert non è pedanteria: il codice compilato del gioco raggiunge quei campi per **offset fissi**.
Allarghi la struct, il gioco continua a usare gli offset vecchi, e da lì in poi scrive dove tu non
guardi. Si può fare — l'item expansion esiste — ma da quel momento sei tu il responsabile di ogni
punto in cui il gioco tocca quella struct, e sono tante righe di codice, non una modifica a un
header.

Le tue classi in `PulsarEngine/` invece puoi allungarle liberamente, anche quando ereditano da una
classe del gioco: i campi nuovi finiscono **in coda**, dopo la parte che il gioco conosce.

> Puoi allungare una classe del gioco **in fondo**, ereditandola. Non puoi mai cambiarla **in mezzo**.

---

## 10. Promemoria da tenere a mente

- Il valore assurdo prima del debugger
- Il primo errore, non l'eco
- La copia corretta è la verità, non l'originale
- Chi calcola non è chi decide: il getter può non essere sulla strada
- Comando o domanda: `void` non risponde
- Quando l'effetto è *nessuno*, guarda la catena, non la logica
