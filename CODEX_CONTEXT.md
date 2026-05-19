# Codex Context

Stand: 2026-05-19

## Projekt und Hardware

- Repo: `MeshCore`
- WP: Heltec Wireless Paper, Env `Heltec_Wireless_Paper_mqtt_observer`
- M1: ThinkNode M1, Env `ThinkNode_M1_companion_radio_ble`
- WP-Port zuletzt: `/dev/cu.usbserial-0001`
- M1-Port zuletzt: `/dev/cu.usbmodem11301`

## WP aktueller Stand

WP wurde zuletzt erfolgreich mit `Heltec_Wireless_Paper_mqtt_observer`
geflasht.

Nach letztem Flash:

```text
Boot:PowerOn H:277k/270k Up:12s RX:0 MQTT:0
Prev:none H:277k/270k
web.view status -> View stopped
web.ap status   -> AP stopped
mqtt status     -> MQTT:off running:0
```

Aktuelle Default-Entscheidung:

- AP, WLAN-Station-View und MQTT bleiben beim Boot aus.
- Lokales Display ist die priorisierte Live-Ansicht.
- `web.view on` startet WLAN-Client plus Webserver.
- `web.view off` stoppt Webserver und schaltet WiFi aus.
- `web.ap on/off/status` bleibt fuer Feldzugriff verfuegbar.
- `mqtt on/off/status` bleibt als alternative WiFi-Nutzung verfuegbar.

Wichtige WP-Fixes im Code:

- Observer-Datenzugriffe mit `observer_lock` und Snapshot-Kopien fuer
  Web/Display.
- Web-Request-Guard fuer schwere Routen:
  - `/api/status` -> Handler `Q:2`
  - `/api/monitor` -> Handler `Q:3`
  - `/api/savepoint` -> Handler `Q:4`
  - `/sp.list.csv` -> Handler `Q:5`
  - `/sp.csv` -> Handler `Q:6`
- Breadcrumb enthaelt Screen, Web-State, optional letzten Webhandler und
  Heapwerte.
- `/api/monitor` wird gecached, damit der Web View weniger Druck erzeugt.
- E-Ink Anti-Ghosting:
  - Full refresh nach mehreren Partial-Updates
  - oder nach laengerer Laufzeit
  - Full refresh bei `turnOn()`
- Load-Screen aktualisiert minutenweise und nutzt rechts den letzten
  abgeschlossenen Minuten-Bin (`Last`), nicht den gerade frisch geleerten
  Live-Bin.

Crash-Historie:

- Fruehere Crashes korrelierten mit `W:View+`.
- Ein spaeterer Crash kam mit:

```text
Boot:Panic H:277k/270k Up:1103s RX:63 MQTT:0
Prev:S4 1h W:off H:277k/270k RX:307
```

Interpretation:

- Webserver ist nicht mehr alleiniger Hauptverdaechtiger.
- `S4` zeigt auf den Load-Screen bzw. Display-Renderpfad.
- Heap bleibt stabil, keine klare Heap-Erschoepfung.

Naechster WP-Schritt bei erneutem Crash:

1. Seriell abfragen:

   ```bash
   /Users/dirkehlert/.platformio/penv/bin/python -c "import serial,time; port='/dev/cu.usbserial-0001'; time.sleep(5); s=serial.Serial(port,115200,timeout=2); time.sleep(0.5); s.reset_input_buffer(); s.write(b'\r\ndiag\r\n'); s.flush(); time.sleep(1); print(s.read(4096).decode(errors='replace')); s.close()"
   ```

2. Auf `Prev:` achten:
   - `W:off` schliesst Web View als aktive Crash-Ursache eher aus.
   - `S4` spricht fuer Load/Display.
   - `Q:2` spricht fuer `/api/status`.
   - `Q:3` spricht fuer `/api/monitor`.

Wichtiges CLI-Verhalten:

- `diag` funktioniert, erzeugt aber manchmal nach vorangestelltem CR zunaechst
  einmal `Unknown command`.
- `sp.list` funktioniert und zeigte zuletzt Savepoints `7` bis `16`.
- `web.view status`, `web.ap status`, `mqtt status` sind die Statusbefehle.

## M1 aktueller Stand

M1 wurde zuletzt erfolgreich mit `ThinkNode_M1_companion_radio_ble` geflasht.

Wichtige Korrektur:

- M1 basiert auf `companion_radio_ble`, nicht auf `simple_repeater` oder dem
  WP-Field-Monitor.

Button-Fixes:

- `PIN_BUTTON1 = 42` / P1.10, Page Turn
- `PIN_BUTTON2 = 39` / P1.07, Function
- Beide `MomentaryButton` mit Pullup.
- Companion UI nutzt beide Tasten:
  - Taste 1: `NEXT`
  - Taste 2: `PREV`
  - Longpress: `ENTER`
  - Tripleclick: `SELECT` / Buzzer toggle

M1 Screen-Stand:

- Radio/Status-Screen mit Frequenz, BW, TX, NF, RX, SNR und Last Path.
- Send-Screen direkt nach OriginLane Messages.
- Alte Path-Seite ersetzt durch Heatstrip.
- Heard Repeaters bleibt; Zeilen zeigen keinen `Rep`-Prefix mehr und nutzen
  eine rechtsbuendige 6-Hex-Spalte.
- Histogramm ersetzt durch Load-Screen.
- Load nutzt RX-Airtime-Bins aus `MyMesh`.
- Heatstrip:
  - Maximal 6 Repeater-Zeilen.
  - 10 px Zeilenabstand.
  - PC-Spalte rechts verankert.

M1 Send-Screen:

- Ziele sind Kanaele und zuletzt gehoerte Kontakte.
- Kanalnachrichten erzeugen ein lokales App-Echo.
- Direkte Node-Nachrichten werden nicht als lokales Echo gefaelscht.
- Dynamische GPS-Nachricht:
  `Meine Position ist: <lat>, <lon>`
- Ohne GPS-Fix wird nicht gesendet.

M1 Quick-Message-CLI:

```text
qm.list
qm.set <1-6> <text>
qm.clear <1-6>
qm.reset
```

Die normale serielle Shell laeuft im BLE-Betrieb mit.

## Dirty Worktree Hinweise

- Untracked Savepoint CSVs sind Test-/Exportdaten und sollen nicht automatisch
  committed werden:
  - `sp0010.csv`
  - `sp0013.csv`
  - `sp0015.csv`

## Bewaehrte Commands

WP build:

```bash
/Users/dirkehlert/.platformio/penv/bin/pio run -e Heltec_Wireless_Paper_mqtt_observer
```

WP flash:

```bash
/Users/dirkehlert/.platformio/penv/bin/pio run -e Heltec_Wireless_Paper_mqtt_observer -t upload --upload-port /dev/cu.usbserial-0001
```

M1 build:

```bash
/Users/dirkehlert/.platformio/penv/bin/pio run -e ThinkNode_M1_companion_radio_ble
```

M1 flash:

```bash
/Users/dirkehlert/.platformio/penv/bin/pio run -e ThinkNode_M1_companion_radio_ble -t upload --upload-port /dev/cu.usbmodem11301
```

M1 Upload-Hinweis:

- Wenn Auto-Reset haengt: M1 per Doppel-Reset in Bootloader/Finder bringen.
- Erfolgreicher Upload endet mit `Device programmed.`
