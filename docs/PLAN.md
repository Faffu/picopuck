# Ponte Bluetooth Steam Controller 2 → Pico 2 W → USB

Piano approvato dall'utente, salvato il 2026-09-10. Avanzamento operativo: [TODO.md](../TODO.md).

> Nota del 2026-09-14: il piano resta come approvato. Nel frattempo il fork è stato pubblicato come `Faffu/picopuck` (dipendenze vendorizzate) invece di `Faffu/OGX-Mini` su `feat/steam-controller-2-bridge`, e lo Steam Controller 2 si è rivelato un dispositivo HID over GATT (`28de:1303`) invece del servizio GATT Valve del primo Steam Controller. Stato attuale in [ROADMAP.md](ROADMAP.md).

## Obiettivo

Creare un fork di **OGX-Mini**, usando il supporto Pico 2 W, Bluetooth e XInput già presente. Prima release con **Steam Controller 2**, uscita **Switch Pro compatibile con Switch 2**, **giroscopio e vibrazione**, più uscita **Xbox 360/XInput su PC**.

Xbox One come ingresso Bluetooth e le altre modalità OpenPuck seguiranno dopo.

## Repository e file di progetto

- Clonare OGX-Mini con i submodule nella sottocartella `OGX-Mini`, senza modificare i metadati protetti della cartella workspace.
- Creare il fork `Faffu/OGX-Mini`, configurare `upstream` sull'originale e lavorare sul branch `feat/steam-controller-2-bridge`.
- L'autenticazione GitHub locale risulta non valida: procedere con il lavoro locale; creare il fork remoto e pubblicare il branch quando l'accesso sarà ripristinato.
- Aggiungere `docs/PICO_BRIDGE.md` con architettura, build, flash, pairing e scelta modalità; `docs/ROADMAP.md` con requisiti e avanzamento; `docs/TESTING.md` con prove e risultati hardware.
- Fissare revisioni delle dipendenze e preset CMake per `PI_PICO2W`, build Release e `MAX_GAMEPADS=1`.

## Implementazione

1. **Baseline:** compilare il firmware originale per Pico 2 W e produrre un UF2 riproducibile prima delle modifiche.
2. **Bluetooth Steam Controller 2:** integrare riconoscimento, pairing, bond persistenti, riconnessione e decodifica dei report nel percorso Bluepad32 esistente. Verificare servizi e report sul controller reale; il parser Steam originale non è sufficiente.
3. **Dati interni:** estendere lo stato gamepad per accelerometro, giroscopio, trackpad e pulsanti posteriori, conservando precisione e validità dei dati. Riutilizzare mapping e sincronizzazione esistenti.
4. **Switch Pro:** aggiungere una modalità distinta dall'attuale HORI, con handshake USB, report periodici, calibrazione IMU e gestione dei comandi rumble. Destinazione: Switch 2 tramite USB del dock, con comunicazione cablata Pro Controller abilitata.
5. **XInput:** riutilizzare il driver esistente, collegando input e vibrazione al nuovo ingresso Bluetooth.
6. **Configurazione:** mantenere le combinazioni OGX-Mini per cambiare modalità; impostare Switch Pro come predefinita del fork e persistere la scelta. Conservare regolazioni di calibrazione e intensità della vibrazione.
7. **Estensioni successive:** verificare Xbox One Bluetooth e portare le modalità OpenPuck mancanti, documentando per ciascuna funzionalità disponibili e piattaforme provate.

Usare OpenPuck e altri progetti come riferimenti tecnici, conservando attribuzioni e rispettando le licenze dell'eventuale codice riutilizzato.

## Verifiche e completamento

- Test automatici del decoder con report acquisiti: input normali, pacchetti troncati, valori limite e azzeramento degli input alla disconnessione.
- Verifica delle conversioni IMU e dei report USB; build ripetibile del firmware.
- Sullo Steam Controller 2: pairing, riconnessione dopo riavvio, pulsanti, stick, trigger e controlli posteriori.
- Sulla Switch 2: riconoscimento, giroscopio con orientamento corretto, avvio/arresto vibrazione, cambio modalità e almeno 30 minuti di gioco senza blocchi.
- Su PC Windows: riconoscimento XInput, input e rumble; prova aggiuntiva su Linux.
- Registrare firmware della console/controller e risultati. La prima release è completa solo dopo le prove reali di **input, gyro e vibrazione**.

## Assunzioni

Un controller collegato alla volta, Pico alimentato via USB, nessuna radio esterna. L'utente dispone dell'hardware e svolge le prove fisiche guidate. XInput indica compatibilità PC; non include console Xbox non modificate. Le altre modalità restano obiettivi successivi, senza dichiararle già compatibili.
