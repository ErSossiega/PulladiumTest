# patches/

Patch da applicare su altri repo, generate da questo repo.

## vk-Pulsar-unnamed-track-crash.patch

Stessa fix del commit che la accompagna, ma scritta sui sorgenti di
[VanzaKartWiiTeam/vk-Pulsar](https://github.com/VanzaKartWiiTeam/vk-Pulsar)
(che ha il sistema delle varianti, quindi i file non combaciano riga per riga con
quelli di questo repo). Generata su `6a1ec89`.

```
git checkout -b fix/unnamed-track-crash
git apply /percorso/vk-Pulsar-unnamed-track-crash.patch
```
