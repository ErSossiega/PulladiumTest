# Riassunto

Ripasso di tutto quello che hai imparato finora, dall'inizio fino a `inline`.

Non è il manuale: quello serve a **cercare** una cosa mentre lavori. Questo serve a **rimettere in
moto la testa** dopo una pausa. Leggilo di fila, poi vai in fondo e prova a rispondere alle domande
prima di riguardare le risposte.

Indirizzi PAL (RMCP).

---

## 1. Da dove sei partito

C++ fino ai puntatori, due anni di Java a scuola, zero engine. E la scoperta che ha accorciato
tutto: **l'OOP ce l'avevi già.** Mancava solo la lista delle differenze, che è corta.

| Cosa | Java | C++ |
|---|---|---|
| Oggetti | tutto è un riferimento | tre modi: valore, `*`, `&` |
| Accesso | sempre `.` | `.` su valori e riferimenti, `->` sui puntatori |
| `new` | il GC libera | **tu** devi fare `delete` |
| Distruttori | non esistono | `~Nome()` |
| File | una classe = un file | `.hpp` (dichiara) + `.cpp` (scrivi) |
| Import | `import` | `#include` + include guard |
| Interi | `int` e basta | `u8 u16 u32 s16 s32 f32` |
| `namespace` | `package` | stessa idea, con `::` |

E la cosa da **non** studiare: tutta la STL. Qui non c'è `std::string`, non c'è `vector`, non c'è
`iostream`. Gira su una Wii con 24 MB di RAM.

---

## 2. Le regole del linguaggio che ti hanno fregato

Le metto in fila perché sono quasi tutte della stessa famiglia: **legali e silenziose.** Il
compilatore non dice niente, il gioco fa una cosa diversa da quella che pensavi.

### La stella la leggi, non la indovini

| Dichiarazione | Cos'è | Accesso |
|---|---|---|
| `Tipo x` | l'oggetto | `.` |
| `Tipo& x` | un riferimento | `.` |
| `Tipo* x` | un indirizzo | `->` |

`a->b` **è** `(*a).b`. `this` è sempre un puntatore, quindi sempre `->`.
Sbagliare qui non è pericoloso: non compila.

Ma conta **quante** stelle:

```cpp
Kart::Player** players;   // players[i] è un PUNTATORE → ->
Item::Player*  players;   // players[i] è l'OGGETTO    → .
```

### `=` scrive, `==` chiede

```cpp
if(count = 2)    // assegna 2, la condizione è SEMPRE vera, e count è cambiato
```

In Java il compilatore lo rifiutava. In C++ è legale, perché qualsiasi numero diverso da zero vale
*vero*. E come bonus ti cambia la variabile che controlla il ciclo tre righe sotto.

### `void` vuol dire comando

> Se una funzione ritorna `void`, **fa** qualcosa. Non può rispondere, quindi non può stare in un
> `if`.

Mettere `UseStar()` dentro una condizione non chiede se la stella è stata usata: **la fa partire**,
sessanta volte al secondo. Le domande si fanno a chi ha una risposta — un campo, un getter.

**Leggere è gratis, chiamare no.**

### Lo shadowing

```cpp
u8 speedoType = ...;      // quella vera
if(...){
    u8 speedoType = ...;  // una SECONDA variabile, muore alla }
}
```

Java lo rifiuta. C++ lo permette, in silenzio.

### La variabile che si inizializza con sé stessa

```cpp
RaceLoadHook restartStar(restartStar);
```

Il nome che stai dichiarando è **già in scope dentro le sue stesse parentesi**. Quel `restartStar`
non è la funzione: è la variabile che stai creando in questo momento.

Stessa famiglia: `void MyMushroom(Kart::Movement& Movement)` — da lì in poi `Movement` è la
variabile, non la classe.

### I namespace sono aperti

```cpp
namespace pulsar { ... }   // NON è Pulsar
```

Sbagliare la maiuscola non è un errore che il compilatore rifiuta: **te ne crea uno nuovo, vuoto.**
Il lookup dei nomi va verso l'esterno, e `Pulsar::Settings` non lo guarda mai.

### Le tre `static`

| Dove | Cosa significa |
|---|---|
| in una **classe** | una copia sola condivisa da tutte le istanze |
| **dentro una funzione** | la variabile **sopravvive** fra una chiamata e l'altra |
| **fuori da tutto**, nel file | visibile solo in questo file |

Per un membro di classe, `static` va **solo nella dichiarazione** nell'`.hpp`. Nel `.cpp` la
definisci qualificata col nome della classe e **senza** ripetere `static`.

*(Le variabili `static` nascono azzerate, garantito: `static bool x;` è già `false`.)*

### `switch`: `break` non è opzionale

Senza, l'esecuzione **cade dentro** al caso successivo, e il compilatore non ti avverte. `default`
copre tutto il resto — e spesso risolve gratis il caso "nessuno".

### L'ordine di dichiarazione conta

Se una funzione è definita **sotto** a chi la chiama, errore. Il compilatore legge dall'alto in
basso e non sa dove andare a cercare.

---

## 3. Com'è fatto il progetto

### `GameSource/`: due operazioni diverse

Non è "non si tocca". Sotto quella frase ci stanno due cose che non c'entrano niente fra loro.

**A — aggiungere documentazione.** Dichiarare una funzione che nel gioco **esiste già** ma che
nessuno aveva scritto nell'header, correggere un nome, aggiungere un commento, mettere un indirizzo
che hai scoperto tu col debugger:

```cpp
+ void SetItemWithCount(ItemId id, int count, bool isForced);  //807bc908
```

Non cambia un byte a runtime. Stai solo dicendo al tuo compilatore che a quell'indirizzo c'è quella
roba. **Rischio zero, sempre permesso**, ed è così che gli header crescono.

E a volte è necessario: `GameSource/` è documentazione scritta a mano (da melg), non un artefatto
ufficiale, e non è la stessa cosa della decompilazione. **Possono mancare classi intere e i nomi
possono essere sbagliati.** C'è chi ha dovuto aggiungere gruppi di funzioni e correggere una
variabile mal chiamata prima di far funzionare il proprio codice. Quindi se quello che ti serve non
c'è, può darsi che nessuno l'abbia ancora scritto.

**B — cambiare il layout.** Un campo nuovo, un array più grande. *Questo* cambia `sizeof`, e
`size_assert` ferma la compilazione in decine di file.

> Quelli non sono 60 errori. È **un errore solo, riportato 60 volte.** Leggi il primo, ignora l'eco.

L'allarme c'è per un motivo preciso: il codice **compilato** del gioco raggiunge i campi per offset
fissi, decisi nel 2008. Se allarghi una struct che il gioco stesso alloca e indicizza, quel codice
continua a usare gli offset vecchi — il campo che hai spostato finisce dove il gioco non guarda, e
il gioco scrive dove tu non ti aspetti. Non crasha subito: fa cose sbagliate in posti scollegati.

> La B **si può fare** — `mkw-item-expansion` allarga `ItemId items[19]` a `[27]` e disattiva sette
> `size_assert` per riuscirci. Ma insieme agli header servono ~3700 righe di codice nuovo in
> `PulsarEngine/` per reggerli. Non è una modifica agli header: è un progetto di cui gli header sono
> la riga zero.

Il tuo `s16 mtCharge;` in `Kart::Pointers` era una B fatta credendo fosse una A.

**`PulsarEngine/` è roba tua**, allungala quanto vuoi — anche quando eredita da una classe del
gioco: i campi nuovi finiscono **in coda**, dopo la parte che il gioco conosce.

> Puoi allungare una classe del gioco **in fondo**, ereditandola. Non puoi mai cambiarla **in mezzo**.

**Il contratto `Count()` / `Create()`.** L'engine chiede quanti slot ti servono, poi te li ridà.
`count` significa *numero di controlli*, non *numero di giocatori* — finché i due numeri coincidono
nessuno se ne accorge, e il giorno che li separi crolla tutto.

E la regola generale che ne è uscita: **decidi prima, agisci dopo.** Il `switch` che sceglie solo il
valore e poi fa una `SetItem` sola batte tre rami che ripetono la stessa cosa.

---

## 4. Ricavare un indirizzo

Il PowerPC **non può caricare 32 bit in una istruzione sola.** Li spezza sempre in due.

```
8057efec   lis  r3, 0x808B        → r3 = 0x808B0000
8057eff0   lha  r3, 0x5CC6 (r3)   → legge a 0x808B0000 + 0x5CC6
8057eff4   blr
```

> **indirizzo = (valore del `lis` << 16) + offset dell'istruzione dopo**

`lis` = *Load Immediate Shifted*, mette il valore nei 16 bit alti. Quello che segue ci somma il suo
offset.

### L'offset è con segno (la versione giusta)

È un campo a **16 bit in complemento a due**. Se il valore **grezzo** comincia con **8–F** è
negativo: sottraici `0x10000`.

```
lwz  r3, 0xC000 (r3)    grezzo 0xC000 → 0xC000 − 0x10000 = −0x4000
```

Ma Dolphin quel passaggio **l'ha già fatto** e ti stampa `-0x4000`. Quindi la regola della prima
cifra serve solo sull'esadecimale grezzo: se nel listato c'è già il meno, sottrai e vai avanti.

### I tre binari

| Intervallo | Binario |
|---|---|
| `80004000` – `~80388000` | main.dol |
| `~805102E0` – `~808D9A58` | StaticR.rel — gara, UI, kart |
| `~809BD6E8` in su | dati e variabili, non codice |

**Non disassemblare il file `StaticR.rel`**: è rilocabile, sul disco gli indirizzi non sono quelli
finali. Serve il debugger, che legge la RAM a gioco avviato.

---

## 5. I registri

Non hanno **nessun** rapporto con i concetti del gioco. Sono 32 caselle generiche. Quello che esiste
è una convenzione, valida **al momento delle chiamate**:

| Registro | Ruolo |
|---|---|
| `r1` | stack pointer, non si tocca |
| **`r3`–`r10`** | **argomenti**, in quest'ordine |
| **`r3`** | e anche il **valore di ritorno** |
| `r0`, `r11`, `r12` | scratch |
| `r14`–`r31` | preservati: chi li usa li rimette a posto |
| `lr` | indirizzo di ritorno |

**Fine di una funzione** — schema fisso: `lwz r0, ...(r1)` · `mtlr r0` · `addi r1, r1, ...` · `blr`.
Riconoscerlo ti dice dove finisce una funzione e comincia la dopo.

**Funzione senza argomenti**: se la **prima** istruzione *scrive* in `r3` invece di leggerlo,
nessuno le ha passato niente.

---

## 6. Gli hook

| Macro | Cosa scrive | Quando |
|---|---|---|
| `kmCall` | `bl` — vai **e torna** | dirottare **una chiamata** in mezzo al codice |
| `kmBranch` | `b` — vai **e basta** | vai e non tornare (di solito: sostituire una funzione) |
| `kmWrite16/32` | un valore grezzo | cambiare **una variabile** o un'istruzione |
| `kmWriteNop` | `60000000` | cancellare un'istruzione |
| `RaceLoadHook`, `RaceFrameHook`, `BootHook`, `SectionLoadHook` | — | **nessun indirizzo** |

> Prima di andare a caccia di un indirizzo, guarda se il momento che ti interessa **ha già un
> aggancio pronto**.

Gli hook senza indirizzo sono **tipi**, come `int`: due hook = due variabili dello stesso tipo, non
due classi.

### La firma è la mappa dei registri

```cpp
void MegaTC(Kart::Movement& movement, int frames, int unk0, int unk1)
//          ↑ r3               ↑ r4        ↑ r5      ↑ r6
```

- Il `this` di un metodo diventa il **primo parametro esplicito**
- Dichiara **tutti** gli originali, anche quelli che ignori: tengono il posto
- Non inventarne di nuovi: leggerebbero registri che contengono altro

> Un hook che non richiama l'originale quando la sua condizione è falsa non estende il gioco:
> **lo rompe.**

### `kmCall` sull'inizio di una funzione = crash

La `bl` si segna dove tornare. La tua funzione fa `blr` e rientra **a metà dell'originale**, con
`r3` pieno del tuo valore di ritorno invece del puntatore che quel codice si aspetta. DSI immediato,
col tuo numero visibile in `r3` nel dump.

### E la lezione del giorno 3

> **Non puoi rendere condizionale la registrazione. Rendi condizionale il comportamento.**

Gli hook si registrano **al boot, una volta sola**, prima che qualsiasi impostazione voglia dire
qualcosa. E un hook dichiarato dentro una funzione muore alla `}`, lasciando nella lista un
puntatore a memoria che non esiste più.

Effetto collaterale gradito: se l'impostazione la leggi **dentro**, cambiarla fra una gara e l'altra
funziona subito.

---

## 7. Trovare un indirizzo da solo

Gli header elencano gli **inizi** delle funzioni, perché un inizio è unico. Una *chiamata* no: la
stessa funzione può essere chiamata da dieci posti, e nessuno dei dieci ha un nome.

Per `kmCall` ti serve l'indirizzo di una **`bl`**. Quindi:

| | Mossa | Risultato |
|---|---|---|
| 1 | Breakpoint sull'inizio della funzione (dal commento nell'header) | scatta → viene chiamata davvero |
| 2 | Fai succedere la cosa in gara | il breakpoint scatta |
| 3 | Leggi `lr` | es. `0x80798668` |
| 4 | **`lr − 4`** | `0x80798664` ← l'indirizzo per il `kmCall` |

Il motivo è meccanico: eseguendo una `bl` all'indirizzo A, la CPU mette **A + 4** in `lr` — dove
tornare — e salta. Quindi `lr` è l'istruzione *dopo* la chiamata.

**Il vicinato ti dice dove sei atterrato**: se il tuo indirizzo sta fra simboli di `Item::Player`,
sei nel codice dell'item, non della guida. E per trovare dove comincia la funzione che ti contiene,
scorri indietro fino al **prologo** (`mflr r0` / `stwu r1, -0x??(r1)`).

**Attenzione:** il breakpoint ti dice da dove è arrivato *questa volta*. Se ti serve un punto
preciso, fallo scattare nel contesto giusto.

---

## 8. `symbols.txt`

Serve a **chiamare** funzioni del gioco dal tuo codice. Gli hook non ci passano.

```
nome_mangled = 0xINDIRIZZO
```

Una riga viene da due posti: il nome dal linker che fallisce, il numero **dal commento nell'header**
(con lo `0x` davanti). Non c'è niente da calcolare.

> `symbols.txt` è una **rubrica**: nome ↔ numero.

**Non scrivere i nomi a mano.** Dichiara la funzione, chiamala, compila: il linker fallisce e ti
stampa la stringa esatta. Se sbagli la firma il nome cambia, e l'errore sembra assurdo — confronta
**carattere per carattere**.

Le regole, se le vuoi leggere:

```
UseItem__Q24Item9PlayerObjFb
  UseItem  __  Q2  4Item  9PlayerObj  F  b
= Item::PlayerObj::UseItem(bool)
```

Il numero prima di ogni nome è la sua lunghezza. `v` void · `b` bool · `i` int · `Uc` u8 · `Us` u16 ·
`f` float · `P` puntatore · `C` const · `R` riferimento.

### Virtual o no: chi ha bisogno del simbolo

```cpp
void ActivateMushroom();       //8057f3d8   ← metodo normale: SERVE il simbolo
virtual void ActivateMega();   //0x1c       ← virtual: NON serve
```

> Se la chiamata passa dalla **vtable**, il gioco risolve l'indirizzo a runtime. Se è una `bl` a un
> indirizzo fisso, quell'indirizzo glielo devi dare tu.

E occhio: il commento accanto a un metodo virtual **non è un indirizzo**, è l'offset nella vtable.
Due numeri che si somigliano e non sono la stessa cosa.

---

## 9. Le regioni

Quello che scrivi in `symbols.txt` è un indirizzo **PAL (RMCP)**, la versione base. Le altre le
gestisce `versions.txt`, che non è una tabella per simbolo ma **per intervalli**: un indirizzo PAL
cade in un intervallo e si prende quel delta. Quindi la traduzione è gratis.

E non è uno spostamento unico: due funzioni vicine possono avere delta diversi, perché il codice è
stato **ricompilato**, non spostato in blocco.

**Come si rompe:** se un indirizzo PAL cade in un **buco** della tabella, il linker lo lascia com'è.
Nessun errore, nessun avviso — sull'altra regione il gioco chiama un indirizzo che lì è tutt'altro.

> Se funziona su PAL e crasha solo su NTSC, il primo posto da guardare è `versions.txt`.

Nota: questo vale per gli indirizzi passati agli hook. Quelli scritti **dentro** il tuo codice e
castati a puntatore **non** vengono rimappati.

---

## 10. Leggere un crash

Non decodificarlo a mano: il **Pack Creator** ha una finestra crash, ci trascini il `Crash.pul` e ti
stampa tutto — tipo dell'eccezione già col nome, `SRR0` e `LR` **con i simboli risolti**, tutti i
registri, e dieci stack frame.

| | Cosa ha provato a fare | Dove guardare |
|---|---|---|
| **DSI** | leggere/scrivere **dati** a un indirizzo che non esiste | `srr0` è un'istruzione vera; l'indirizzo marcio è **in un registro** |
| **ISI** | eseguire **codice** dove non ce n'è | `srr0` **è** la spazzatura; guarda **`lr`** |

**DSI = puntatore sbagliato. ISI = salto sbagliato.**

E in tutti e due i casi, un registro che contiene un numero riconoscibilmente **tuo** dice che il tuo
codice è appena passato di lì.

---

## 11. Il debugger, in tre mosse

1. **Il breakpoint risponde sì o no.** Scatta → la funzione viene chiamata. Non scatta → stai
   agganciando il posto sbagliato, e nessun ragionamento sul codice te lo avrebbe detto.
2. **Breakpoint sul `blr`, poi leggi `r3`.** Lì il PowerPC tiene i valori di ritorno.
3. **Memory view** con l'emulazione **ferma su un breakpoint**. Senza mappa dei simboli ogni nome
   dice `unk` e navighi per soli indirizzi — quindi **carica prima `RMCP01.map`**: Dolphin mostra i
   nomi veri e demangled su tutto StaticR.rel, e quasi tutto l'orientarsi a mano smette di servire.

> E **prima** di aprire il debugger: **prova un valore assurdo.** Un `1` al posto di un `270` dà un
> effetto impossibile da confondere. Se non cambia niente nemmeno con quello, l'hook non scatta — ed
> è un problema diverso.

Separare *"non scatta"* da *"scatta ma il valore non conta"* è la mossa che sblocca più spesso.

---

## 12. Quando non succede NIENTE

> Quando l'effetto è **nessuno** invece che **sbagliato**, il problema non è quasi mai nella logica.
> È nella catena.

0. **Il build sa che questo file esiste?** (estensione `.cpp`, dentro una cartella che il glob copre)
1. **Salvato?** Il build legge dal disco, non dal tuo editor.
2. **Compilato?** `ls -la build/TuoFile.o` — dev'essere più recente del `.cpp`.
3. **Linkato?** *"No source or header files were modified"* subito dopo che hai modificato qualcosa
   non è un'offerta: è un avvertimento.
4. **Copiato?** La cartella di destinazione è il pack che stai davvero avviando?
5. **Ricaricato?** Riivolution legge i file all'avvio della ISO.

E il difetto che ti ha fregato tre volte su quattro: **un errore di compilazione può essere
silenzioso.** Se niente controlla il codice di uscita del compilatore, l'oggetto vecchio resta, il
link riesce, e ti ritrovi un `Code.pul` con timestamp fresco e dentro il codice di ieri.

I quattro casi, tutti la stessa forma:

| | La modifica non arrivava perché |
|---|---|
| il `pos` del tachimetro | era su una riga morta (`if(count == 1)` con `count` già 2) |
| il `kmBranch` | il `.o` non era stato ricompilato |
| la stella | il quaderno veniva cancellato nello stesso frame |
| l'hook del fungo | il file si chiamava `MUSHROOOOMS`, senza `.cpp` |

---

## 13. `inline` — la roba nuova

Negli header trovi commenti tipo:

```cpp
static ItemId RandomizeRouletteItem(RouletteItems*, ItemId prev); //807baed4 inlined
```

**Inlined NON vuol dire che non viene mai chiamata.** Vuol dire il contrario: viene eseguita eccome,
ma non esiste la *chiamata*.

Il compilatore, quando la funzione è piccola, invece di emettere una `bl` verso di essa **copia il
suo corpo dentro a chi la usa**:

```
Non inlined                      Inlined
─────────────                    ────────
chiamante:                       chiamante:
  bl  Funzione        <-->         [ il corpo, copiato qui ]
```

Lo fa per velocità: una chiamata costa (salvare `lr`, aprire lo stack frame, saltare, tornare,
richiudere), e per una funzione di tre righe quel contorno costa più del lavoro vero.

**Le due conseguenze:**

1. **Ne esistono N copie, non una** — una per ogni punto in cui era chiamata. Non c'è più il punto
   singolo dove metterti in mezzo: per cambiare quel comportamento le devi trovare e modificare
   tutte.
2. **E le copie possono essere diverse fra loro.** Una volta incollato, il compilatore ottimizza il
   corpo *nel contesto di quel chiamante*: se lì un parametro è sempre `0`, i rami che ne dipendono
   spariscono. Due copie della stessa funzione possono avere lunghezze e istruzioni diverse.

Detto in un altro modo: non è "telefonare a un amico e chiedergli di fare una cosa", è "copiarsi le
sue istruzioni sul quaderno e farla da soli". Il lavoro viene fatto lo stesso, ma non c'è nessuna
telefonata da intercettare.

In C++ esiste anche la parola chiave `inline`, ma **è solo un suggerimento**: decide il compilatore,
e inlinea anche roba che non hai marcato — tipicamente le funzioncine definite dentro gli header.

*(Se l'indirizzo che l'header elenca contenga davvero una copia raggiungibile, si scopre solo con un
breakpoint. Se non scatta mai, hai la risposta.)*

---

## Domande di controllo

Rispondi prima di guardare sotto.

1. Ho `Kart::Player** players`. Per arrivare a un campo di `players[2]` uso `.` o `->`?
2. Vedo `lis r3, 0x808C` e sotto `lwz r3, 0x9000(r3)`. Che indirizzo è?
3. Voglio sostituire una funzione intera. Quale macro?
4. Il mio hook compila, il gioco parte, non succede niente. Prima mossa?
5. Perché non posso mettere `if(GetSettingValue(...) == X)` attorno alla registrazione di un
   `RaceFrameHook`?
6. Crash con `error = 3`. Guardo `srr0` o `lr`?
7. Che differenza c'è fra il `//0x1c` accanto a un metodo virtual e il `//805858ac` accanto a uno
   normale?
8. Una funzione è marcata `inlined`. Posso metterci un `kmCall`?
9. Ho 60 errori di `size_assert`. Quanti problemi ho?
10. `UseStar()` ritorna `void`. Posso scrivere `if(UseStar() == true)`?
11. Trovo col debugger una funzione del gioco che nessuno ha documentato. La posso aggiungere a
    `GameSource/`?

<details>
<summary>Risposte</summary>

1. `->`. Due stelle: `players[2]` è ancora un puntatore.
2. `0x9000` comincia con 9 → negativo. `0x9000 − 0x10000 = −0x7000`, quindi
   `0x808C0000 − 0x7000 = 0x808B9000`.
3. `kmBranch`, sulla **prima** istruzione della funzione.
4. Il valore assurdo, non il debugger. E prima ancora la catena: salvato, compilato, linkato,
   copiato, ricaricato.
5. Perché gli hook si registrano al boot, una volta sola, prima che l'impostazione voglia dire
   qualcosa — e perché un hook dichiarato dentro una funzione muore alla `}`. Rendi condizionale il
   **comportamento**, non la registrazione.
6. `3` = ISI = salto sbagliato → `srr0` è spazzatura, guarda **`lr`**.
7. Il primo è l'**offset nella vtable**, il secondo è un **indirizzo**. E solo il secondo ha bisogno
   di una riga in `symbols.txt`.
8. No: non c'è nessuna `bl` da dirottare, il corpo è copiato dentro ai chiamanti. Devi agganciare
   **chi** l'ha inglobata.
9. Uno. Leggi il primo, ignora l'eco.
10. No, e il problema non è solo che non compila: mettendola nell'`if` non stai *chiedendo* se la
    stella è stata usata, **la stai facendo partire**.
11. Sì. È documentazione: dichiari una cosa che nel gioco esiste già, non cambia un byte a runtime.
    Vietato è **allargare** — campi nuovi, array più grandi — perché quello cambia `sizeof`.

</details>

---

## Dove sta il resto

| File | A cosa serve |
|---|---|
| `MANUALE-KAMEK.md` / `KAMEK-MANUAL.md` | il riferimento da consultare mentre lavori |
| `DIARIO.md` / `Four-Days-Inside-Pulsar.md` | come ci sei arrivato, con tutti gli errori |
