# MBK GF WP MQTT

Projekt-Spec fuer einen Heltec Wireless Paper Node als MeshCore MQTT Observer.

## Ziel

Das Heltec Wireless Paper laeuft als passiver MeshCore Observer fuer den Bereich
Braunschweig/Gifhorn. Empfangene MeshCore Pakete werden ueber WLAN an den
MQTT-Broker von meshcorenetz.de publiziert. Das Geraet ist im Mesh als
`MBK GF WP MQTT` sichtbar.

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

Status-Screen:

- Node-Name in eigener Kopfzeile
- zweite Kopfzeile mit SNR des letzten empfangenen Pakets, Noise Floor, freiem Heap und Batteriespannung
- Frequenz, SF, Bandbreite und Coding Rate
- MQTT-Status und Broker
- RX/MQTT Pakete der laufenden Minute
- Gesamtsummen seit Start

Pfad-Screen:

- Pfade der in den letzten 60 Sekunden empfangenen Pakete
- Neueste Pakete zuerst
- Pfad-Hashes umgekehrt dargestellt, damit der naehere/lokale Teil links steht
- Anzeige zeigt aus Platzgruenden die letzten 3 Hops, gezaehlt wird intern aber nach vollstaendigem Pfad
- Pfadhistorie speichert bis zu 24 unterschiedliche Pfade
- Rechts ein Aktivitaetsbalken fuer RX-Pakete der letzten 12 Minuten mit einem Balken pro Minute

MQTT-Screen:

- WLAN/MQTT ein/aus
- MQTT-Verbindungsstatus und letzter MQTT-State-Code
- letzter lokaler MQTT-Fehler
- Zaehler fuer WLAN-, MQTT-Verbindungs- und Publish-Fehler

Heards-Screen:

- letzter/naechster Hop der empfangenen Pakete
- Last Heard als Alter in Sekunden
- maximale SNR seit Start fuer diesen Hop
- letzte SNR fuer diesen Hop
- rechts derselbe RX-Aktivitaetsbalken wie auf dem Pfad-Screen

Savepoints-Screen:

- zweite Zeile zeigt den Clock-Sync-Status
- listet gespeicherte Savepoints mit ID, Uhrzeit, RX-Zaehler und Noise Floor
- Doppelklick erzeugt einen neuen Savepoint im Flash
- Long Press setzt Live-Zaehler, Pfade und Heards zurueck
- Histogramme laufen beim Reset weiter und werden beim Savepoint mitgespeichert

## Uhrzeit

Der Node synchronisiert seine Uhr passiv aus validierten Mesh-Adverts. Dafuer
werden nur plausible Sender-Zeitstempel akzeptiert. Nach 5 Samples aus
mindestens 2 verschiedenen Nodes wird der Median als Mesh-Zeit uebernommen,
sofern die lokale Uhr dadurch nur vorwaerts gesetzt wird. Es wird nichts ins
Mesh gesendet.

## Lokale Secrets

WLAN- und MQTT-Passwoerter werden nicht versioniert. Fuer lokale Builds:

```text
cp platformio.local.example.ini platformio.local.ini
```

Danach die Werte in `platformio.local.ini` anpassen. Diese Datei ist in
`.gitignore` eingetragen.

## Tastenbedienung

- Kurzer Klick: Status-Screen, Pfad-Screen, Heards-Screen, Savepoints-Screen und MQTT-Screen durchschalten
- Doppelklick auf Pfad- oder Heards-Screen: direkt zwischen diesen beiden Screens wechseln
- Doppelklick auf Savepoints-Screen: Savepoint speichern
- Doppelklick auf anderen Screens: Zero-Hop Advert senden
- Langer Druck auf Status-Screen: Flood-Advert senden
- Langer Druck auf Pfad-Screen: Hibernate
- Langer Druck auf Heards-Screen: Repeater-Discovery / Find Nearby Nodes senden
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
