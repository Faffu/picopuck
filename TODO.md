# TODO — Steam Controller 2 → Pico 2 W → USB

Piano: [docs/PLAN.md](docs/PLAN.md). Architettura e build: [docs/PICO_BRIDGE.md](docs/PICO_BRIDGE.md). Aggiornato: 2026-09-16.
Le caselle vengono completate solo con evidenza; build e test automatici non certificano la compatibilità hardware.

## Repository e baseline

- [x] Salvare il piano e la checklist.
- [x] Completare il clone OGX-Mini con tutti i submodule.
- [x] Configurare `upstream` e branch `feat/steam-controller-2-bridge`.
- [x] Pubblicare il fork: `Faffu/picopuck`, sorgenti e dipendenze vendorizzati con le patch applicate.
- [x] Leggere il flusso Bluepad32 → stato gamepad → USB e le regole del repository.
- [x] Preparare toolchain e compilare la baseline originale Pico 2 W.
- [x] Conservare UF2 baseline, revisioni, comandi e checksum.
- [x] Fissare dipendenze e preset CMake: `PI_PICO2W`, Release, un gamepad.

## Ingresso Steam Controller 2 Bluetooth

- [x] Verificare protocollo, servizi BLE, identificativi e licenze dei riferimenti (HID over GATT `28de:1303`; SDL zlib, 2wiCC MIT, Bluepad32 Apache-2.0).
- [x] Acquisire servizi e report dal controller reale con versione firmware (28de:1303, fw 6a628345; tasto Steam e accesso rapido presi da SDL, non catturati).
- [x] Integrare riconoscimento e inizializzazione nel percorso Bluepad32 (hook sul report HID generico, VID/PID 28de:1303).
- [x] Implementare decoder Steam Controller 2 con controlli di lunghezza (report 0x45 mappato da cattura reale e confrontato con SDL).
- [x] Integrare pairing, bond persistenti e riconnessione dopo riavvio (verificati il 2026-09-16).
- [x] Conservare precisione e validità di accelerometro, giroscopio, trackpad e pulsanti posteriori.
- [x] Riutilizzare mapping e sincronizzazione; azzerare input e validità alla disconnessione.
- [x] Collegare il feedback rumble al controller Bluetooth, incluso arresto (output report 0x80 come SDL, ripetuto ogni 40 ms; verificato da XInput su Windows).

## Uscite e configurazione

- [x] Aggiungere Switch Pro come modalità distinta da HORI.
- [x] Implementare descrittori, handshake USB e report periodici Switch Pro (allineati a 2wiCC; riconosciuto da Windows).
- [x] Implementare conversione IMU, orientamento e calibrazione regolabile (assi rispetto alla Switch da verificare).
- [x] Gestire comandi rumble Switch Pro con intensità regolabile.
- [x] Collegare input e vibrazione al driver XInput esistente.
- [x] Mantenere combinazioni OGX-Mini e persistenza della modalità (combo verificate sull'hardware).
- [x] Impostare Switch Pro come modalità predefinita del fork.

## Verifiche software

- [x] Test decoder: report reali catturati, report di stato e mouse rifiutati, report troncati.
- [ ] Test azzeramento stato alla disconnessione.
- [x] Test conversioni IMU, codifica rumble e buffer di cattura.
- [x] Compilare il firmware modificato; build ripetibile via script, checksum diverso a ogni build per il timestamp compilato.
- [x] Registrare risultati, revisioni e checksum degli artefatti.

## Strumenti di diagnosi

- [x] Modalità cattura via seriale USB, build di cattura che si avvia sempre in cattura, log Bluepad32 e descrittore HID.
- [x] Script PowerShell `Tools/pico-bridge/capture.ps1` con marcatori da tastiera.

## Documentazione

- [x] `docs/PICO_BRIDGE.md`: architettura, mappatura tasti, build, flash, pairing, modalità e cattura.
- [x] `docs/ROADMAP.md`: requisiti, avanzamento e funzionalità future.
- [x] `docs/TESTING.md`: procedura hardware, versioni firmware, risultati e limiti.
- [x] Conservare attribuzioni e licenze di eventuale codice riutilizzato.

## Verifiche hardware — da eseguire con l'utente

- [x] Steam Controller 2: pairing e riconnessione dopo riavvio e cambio modalità (2026-09-16).
- [ ] Steam Controller 2: pulsanti, stick, trigger (verificati in XInput), Start/Back da ricontrollare, tasti Steam e accesso rapido, trackpad e controlli posteriori.
- [x] Switch 2 via USB dock, comunicazione cablata Pro abilitata: riconoscimento e input (2026-09-16).
- [ ] Switch 2: gyro orientato correttamente e calibrazione.
- [ ] Switch 2: avvio/arresto vibrazione e regolazione intensità.
- [ ] Switch 2: cambio modalità e almeno 30 minuti di gioco senza blocchi.
- [x] Windows: riconoscimento XInput, input e rumble.
- [x] Windows: riconoscimento modalità Switch Pro, HORI e DInput.
- [ ] Windows: modalità Switch Pro in Steam (input, gyro, rumble).
- [ ] Linux: riconoscimento, input e rumble.
- [ ] Registrare versioni firmware console/controller e risultati effettivi.
- [ ] Dichiarare completa la prima release solo dopo prove reali di input, gyro e vibrazione.

## Successivi alla prima release

- [ ] Verificare Xbox One Bluetooth come ingresso.
- [ ] Portare le modalità OpenPuck mancanti, con matrice funzionalità/piattaforme provate.
