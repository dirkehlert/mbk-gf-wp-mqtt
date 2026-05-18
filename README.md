# MBK GF WP Field Monitor

Projekt-Spec fuer einen Heltec Wireless Paper Node als mobiles MeshCore
Feld-Monitoring-Geraet.

## Ziel

Das Heltec Wireless Paper dient aktuell vor allem als passiver Feldmonitor fuer
MeshCore im Bereich Braunschweig/Gifhorn. Im Vordergrund stehen Empfangs- und
Aktivitaetsanalyse direkt am Geraet: gehoerte Pakete, Pfade, letzte Hops,
Noise Floor, SNR, Histogramme und Savepoints fuer Tests mit Standorten,
Antennen und Filtern.

MQTT ist als vorbereitete Nebenstrecke integriert, steht derzeit aber nicht im
Fokus. Der Node soll auch ohne WLAN/MQTT vollstaendig fuer das lokale
Monitoring im Feld nutzbar bleiben. Das Geraet ist im Mesh als `MBK GF WP MQTT`
sichtbar.

## Hardware

- Board: Heltec Wireless Paper
- MCU: ESP32-S3
- Display: E-Ink, `E213Display`
- Upload-Port: `/dev/cu.usbserial-0001`
- Upload-Speed: `115200`

## PlatformIO Environment

Build- und Upload-Environment:

```sh
/Users/dirkehlert/.platformio/penv/bin/pio run -e Heltec_Wireless_Paper_mqtt_observer
/Users/dirkehlert/.platformio/penv/bin/pio run -e Heltec_Wireless_Paper_mqtt_observer -t upload --upload-port /dev/cu.usbserial-0001
```

Die Konfiguration liegt in:

```text
variants/heltec_wireless_paper/platformio.ini
```

## Mesh-Konfiguration

- Node Name: `MBK GF WP MQTT`
- Position: `52.4770, 10.5419`
- Standort: Deutschland, Gifhorn, Braunschweiger Strasse / Ecke Loenseck
- Default Region/Scope: `bsmesh`
- Node-Typ im Advert: Repeater
- Forwarding: deaktiviert fuer Observer-Betrieb
- Lokale Adverts: aktiv
- Flood-Adverts: manuell per CLI oder Taste moeglich

Wichtige CLI-Kommandos:

```text
set lat 52.4770
set lon 10.5419
region default bsmesh
advert.zerohop
advert
```

## WLAN

- SSID: lokal im Build als `MQTT_OBSERVER_WIFI_SSID` hinterlegt
- Passwort: lokal im Build als `MQTT_OBSERVER_WIFI_PASSWORD` hinterlegt

## Field-Web-AP

Der WP startet im aktuellen Test-Build automatisch einen lokalen Access Point
fuer Feldarbeit. Das ist ueber `OBSERVER_WEB_AP_DEFAULT_ON=1` im
WP-Buildprofil aktiviert und sollte fuer einen spaeteren Feld-/MQTT-Build
wieder bewusst entschieden werden.

CLI-Kommandos:

```text
web.ap on
web.ap status
web.ap off
web.view on
web.view status
web.view off
mqtt on
mqtt status
mqtt off
```

Alternativ kann der Field-Web-AP direkt am Geraet auf dem MQTT-Screen per
Doppelklick ein- und ausgeschaltet werden.

Beim Start des AP wird MQTT beendet, weil der ESP32-WiFi-Mode auf Access Point
wechselt. Beim Stop des AP wird MQTT wieder gestartet, sofern MQTT in den Prefs
aktiviert ist.

Der WLAN-Station-Viewmodus wird auf dem MQTT-Screen per Long Press aktiviert
oder per `web.view on` gestartet. Dabei verbindet sich der Node mit dem
konfigurierten WLAN und stellt dieselbe Webseite im lokalen Netz bereit. MQTT
wird dabei persistent deaktiviert und bleibt aus, bis es explizit per `mqtt on`
oder `set mqtt.enabled on` wieder aktiviert wird.

Im Station-Viewmodus ist die Web-API read-only: Zeit- und Positionsaenderungen
werden abgelehnt. `POST /api/savepoint` bleibt erlaubt, damit waehrend eines
stationaeren Tests direkt ein Savepoint erzeugt werden kann.

- SSID: `MBK-GF-WP`
- Passwort: `observer2026`
- URL: `http://192.168.4.1/`

Die Webseite bietet:

- aktuelle Node-Zeit vom verbundenen Client setzen
- aktuelle Position des Clients speichern
- Load-Screen als automatisch aktualisiertes 12-Minuten-Balkendiagramm anzeigen
- Heatstrip-Screen als automatisch aktualisierte Pfad/Repr-Matrix anzeigen
- Savepoints listen
- neuen Savepoint erzeugen
- Savepoint-Liste als CSV exportieren
- einzelne Savepoints als CSV herunterladen

Web-API:

```text
GET  /api/status
GET  /api/monitor
POST /api/time?epoch=<unix_utc>
GET  /api/position?lat=<lat>&lon=<lon>
POST /api/position?lat=<lat>&lon=<lon>
POST /api/savepoint
GET  /sp.list.csv
GET  /sp.csv?id=<id>
```

`/api/status` liefert Node-Name, UTC-Zeit, Position, SNR, Noise Floor, freien
Heap, Batterie und Savepoint-Metadaten. `/api/monitor` liefert RX-Minutenwerte,
Load-Airtime und Heatstrip-Daten fuer die Live-Ansicht.

Hinweis: Browser-Geolocation kann auf iOS ueber unverschluesseltes
`http://192.168.4.1` blockiert werden. Die Webseite bietet deshalb auch
manuelle Lat/Lon-Felder. Fuer automatisches Setzen per iPhone ist ein iOS
Shortcut sinnvoll, der den aktuellen Standort abfragt und `/api/position` mit
`lat` und `lon` aufruft.

iOS Shortcut fuer Position:

1. Aktion `Aktuellen Standort abrufen`
2. Aktion `Inhalt von URL abrufen`
3. URL:

```text
http://192.168.4.1/api/position?lat=<Breitengrad>&lon=<Laengengrad>
```

4. Methode: `POST` oder `GET`

Die Weboberflaeche und die manuellen Lat/Lon-Felder bleiben parallel nutzbar.

## MQTT

- Broker: `mqtt.meshcorenetz.de`
- Port: `1883`
- TLS: nein
- Username: `observer`
- Passwort: im Build als `MQTT_OBSERVER_PASSWORD` hinterlegt
- Topic:

```text
meshcore/BWE/{PUBLIC_KEY}/packets
```

Der Platzhalter `{PUBLIC_KEY}` wird zur Laufzeit durch den Public Key des
Geraets ersetzt.

Aktueller Public Key des Geraets:

```text
A4F610572F1F4C77CC27483B9920C347B7070B9471C8B24C413324BED5146EA8
```

Effektives Topic:

```text
meshcore/BWE/A4F610572F1F4C77CC27483B9920C347B7070B9471C8B24C413324BED5146EA8/packets
```

## MQTT Payload

Die Firmware publiziert derzeit kompakte JSON-Nachrichten:

```json
{
  "type": "rx",
  "pubkey": "...",
  "packet": "...",
  "payload_type": 1,
  "route": 1,
  "path_hash_size": 1,
  "path_hash_count": 2,
  "rssi": -80,
  "snr": 28
}
```

`type` ist `rx` fuer empfangene Pakete und `tx` fuer gesendete Pakete.

Hinweis: Der MQTT-Broker-Eingang wurde erfolgreich getestet. Fuer die
meshcorenetz.de Live-Ansicht kann ggf. noch eine Anpassung an deren
erwartetes Payload-Format sinnvoll sein.

## Display

![Aktueller Display-Snapshot](docs/screen-current.png)

Screen-Galerie:

| Status | Paths | Heards |
| --- | --- | --- |
| ![Status](docs/screens/00-status.png) | ![Paths](docs/screens/01-paths.png) | ![Heards](docs/screens/02-heards.png) |

| Heatstrip | Load | Savepoints |
| --- | --- | --- |
| ![Heatstrip](docs/screens/03-heatstrip.png) | ![Load](docs/screens/04-load.png) | ![Savepoints](docs/screens/05-savepoints.png) |

| MQTT |
| --- |
| ![MQTT](docs/screens/06-mqtt.png) |

Status-Screen:

- Node-Name in eigener Kopfzeile
- zweite Kopfzeile mit SNR des letzten empfangenen Pakets, Noise Floor, freiem Heap und Batteriespannung
- Frequenz, SF, Bandbreite und Coding Rate
- MQTT-Status und Broker
- RX/MQTT Pakete der laufenden Minute
- Gesamtsummen seit Start

Pfad-Screen:

- haeufigste Pfad-Triples seit Start
- `cnt` zaehlt Treffer fuer das sichtbare Triple
- `age` zeigt, wann dieses Triple zuletzt gehoert wurde
- Anzeige zeigt aus Platzgruenden maximal die letzten 3 Hops
- Pfad-Hashes sind umgekehrt dargestellt, damit der naehere/lokale Teil links steht
- Pfadhistorie speichert bis zu 24 unterschiedliche Triple-Eintraege
- Rechts ein Aktivitaetsbalken fuer RX-Pakete der letzten 12 Minuten mit einem Balken pro Minute

MQTT-Screen:

- WLAN/MQTT ein/aus
- MQTT-Verbindungsstatus und letzter MQTT-State-Code
- letzter lokaler MQTT-Fehler
- Zaehler fuer WLAN-, MQTT-Verbindungs- und Publish-Fehler
- Field-Web-AP-Status mit Clientanzahl und IP-Adresse

Heards-Screen:

- letzter/naechster Hop der empfangenen Pakete
- Last Heard als Alter in Sekunden
- maximale SNR seit Start fuer diesen Hop
- letzte SNR fuer diesen Hop
- `PC` zeigt, in wie vielen aktuellen Pfad-Triples dieser Repeater vorkommt
- rechts derselbe RX-Aktivitaetsbalken wie auf dem Pfad-Screen

Heatstrip-Screen:

- grafische Repeater/Pfad-Matrix
- Spalten sind die Top-Pfad-Triples, Zeilen sind Repeater
- markierte Zellen bedeuten, dass der Repeater am jeweiligen Triple beteiligt ist
- Graustufen kodieren die Position im Triple: dunkel = letzter/naechster Rep, mittel = vorletzter Rep, hell = vorvorletzter Rep
- Punkt bedeutet, dass der Repeater in diesem Triple nicht enthalten ist
- `PC` rechts zeigt, in wie vielen Pfad-Triples dieser Repeater vorkommt
- Snapshot wird beim Betreten aufgebaut und danach nur per Doppelklick auf diesem Screen aktualisiert

Load-Screen:

- prozentuale RX-Netzauslastung auf Basis der geschaetzten Airtime
- 12-Minuten-Verlauf als Balken von links nach rechts
- rechte Legende mit aktueller Minute, Maximum und 12-Minuten-Durchschnitt

Savepoints-Screen:

- zweite Zeile zeigt die aktuelle UTC-Zeit und ob die Uhr gesetzt ist
- listet gespeicherte Savepoints mit ID, Uhrzeit, RX-Zaehler und Noise Floor
- Doppelklick erzeugt einen neuen Savepoint im Flash
- Long Press setzt Live-Zaehler, Pfade und Heards zurueck
- RX-Histogramm und Load-Airtime laufen beim Reset weiter und werden beim Savepoint mitgespeichert

Display-Snapshot aktualisieren:

```sh
tools/capture_screen.py --port /dev/cu.usbserial-0001 --output docs/screen-current.png
tools/capture_screen.py --port /dev/cu.usbserial-0001 --all
```

## Uhrzeit

Der Node synchronisiert seine Uhr nicht mehr aus dem Mesh. Die Uhr wird bewusst
lokal gesetzt:

- per Field-Web-AP ueber die Webseite
- per iOS Shortcut gegen `/api/time`
- per serieller CLI

Savepoints speichern die aktuelle UTC-Zeit, sofern sie zuvor gesetzt wurde.

## Lokale Secrets

WLAN- und MQTT-Passwoerter werden nicht versioniert. Fuer lokale Builds:

```text
cp platformio.local.example.ini platformio.local.ini
```

Danach die Werte in `platformio.local.ini` anpassen. Diese Datei ist in
`.gitignore` eingetragen.

## Tastenbedienung

- Kurzer Klick: Status-Screen, Pfad-Screen, Heards-Screen, Heatstrip-Screen, Load-Screen, Savepoints-Screen und MQTT-Screen durchschalten
- Doppelklick auf Pfad- oder Heards-Screen: direkt zwischen diesen beiden Screens wechseln
- Doppelklick auf Heatstrip-Screen: Heatstrip-Snapshot aktualisieren
- Doppelklick auf Savepoints-Screen: Savepoint speichern
- Doppelklick auf MQTT-Screen: Field-Web-AP toggeln
- Doppelklick auf anderen Screens: Zero-Hop Advert senden
- Langer Druck auf Status-Screen: Flood-Advert senden
- Langer Druck auf Pfad-Screen: Hibernate
- Langer Druck auf Heards-Screen: Repeater-Discovery / Find Nearby Nodes senden
- Langer Druck auf MQTT-Screen: WLAN-Station-Viewmodus toggeln, MQTT bleibt aus
- Langer Druck auf Savepoints-Screen: Live-Zaehler zuruecksetzen
- Langer Druck auf MQTT-Screen: WLAN/MQTT toggeln

## Savepoints

Savepoints werden als CSV-Dateien im SPIFFS-Flash abgelegt. Maximal 10
Savepoints werden gehalten; beim 11. Savepoint wird der aelteste geloescht.

CLI-Kommandos:

```text
sp.list
sp.show <id> [page]
sp.delete <id>
sp.clear
```

`sp.show <id>` gibt eine kompakte Zusammenfassung mit RX/MQTT, NF, SNR,
Batterie sowie erstem Pfad und erstem Heard-Eintrag aus. `sp.show <id> <page>`
gibt den Savepoint als rohe CSV-Seite aus. Die CSV-Ausgabe ist wegen der
CLI-Antwortlaenge paginiert; jede Seite enthaelt bis zu 4 CSV-Zeilen.

Savepoints enthalten zusaetzlich:

- `hist,<minute>,<rx_packets>`: RX-Pakete pro Minute, neueste Minute zuerst
- `airtime_ms,<minute>,<rx_airtime_ms>`: geschaetzte RX-Airtime pro Minute, neueste Minute zuerst

## MQTT Fehlerverhalten

MQTT ist eine reine Observer-Nebenstrecke. MQTT-Fehler duerfen keine MeshCore
Pakete, ACKs, Adverts oder sonstige Antworten ins Mesh ausloesen. Fehler werden
nur lokal gezaehlt und auf dem MQTT-Screen angezeigt.

Wenn kein WLAN verfuegbar ist, laeuft der MeshCore/LoRa-Empfang weiter. Pakete
werden lokal gezaehlt, Pfade und Aktivitaet werden weiterhin angezeigt, nur der
MQTT-Publish schlaegt lokal fehl und erhoeht die Fehlerzaehler.

## Verifikation

MQTT Subscribe-Test:

```text
Topic: meshcore/BWE/+/packets
Broker: mqtt.meshcorenetz.de:1883
```

Dabei wurden Nachrichten vom Geraet unter dem Public-Key-Topic empfangen.

Letzter bekannter erfolgreicher Upload:

```text
Environment: Heltec_Wireless_Paper_mqtt_observer
Status: SUCCESS
```

## TODO Security Hardening

- MQTT-Credentials fuer den Feldbetrieb rotieren und moeglichst pro Node eigene
  User verwenden.
- MQTT von Port 1883 auf TLS/8883 umstellen; Server-Zertifikat bzw. CA pruefen.
- MQTT-ACL serverseitig eng setzen: Publish nur auf das eigene Topic, keine
  Wildcards, kein Subscribe.
- `ENABLE_PRIVATE_KEY_IMPORT` und `ENABLE_PRIVATE_KEY_EXPORT` fuer
  Produktions-Builds deaktivieren.
- `ADMIN_PASSWORD` nicht auf dem Default `password` lassen; lokales starkes
  Passwort in `platformio.local.ini` oder unbenoetigte Admin-Funktionen im
  Observer-Build deaktivieren.
- Serial CLI haerten: gefaehrliche Befehle nur nach Unlock oder Button-Gate,
  keine Secret-Ausgabe, optional Read-only-Modus nach normalem Boot.
- Observer-Only-Regel hart absichern: MQTT/WLAN/Savepoint/UI-Fehler duerfen
  niemals Mesh-TX ausloesen; Mesh-TX nur ueber explizite Whitelist erlauben.
- OTA aus dem Observer-Build entfernen, falls nicht aktiv benoetigt.
- Savepoint-Dateien weiter begrenzen und robust validieren; optional CRC pro
  Savepoint speichern.
- Build-Secrets nur in der ignorierten `platformio.local.ini` halten und nie
  committen.
- Dependencies moeglichst pinnen und Firmware-Releases mit Build-Hash
  dokumentieren.
