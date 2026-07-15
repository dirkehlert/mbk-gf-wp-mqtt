# Bedienungsanleitung – ThinkNode M1 Fieldtest

Empfänger: `peter.kernspecht@web.de`

Firmware: `ThinkNode_M1_fieldtest_pk.uf2`

Basis: MeshCore Companion 1.16.0

Diese Firmware macht den ThinkNode M1 zu einem MeshCore-Companion-Radio mit
zusätzlichen Anzeigen für Feldtests. Empfang, Funkwege, Repeater, Netzlast und
Positionen können direkt auf dem E-Ink-Display beurteilt werden.

## 1. Installation

1. Den ThinkNode M1 per USB mit dem Rechner verbinden.
2. Den Reset-Taster zweimal kurz hintereinander drücken.
3. Warten, bis der M1 als USB-Laufwerk erscheint.
4. `ThinkNode_M1_fieldtest_pk.uf2` auf dieses Laufwerk kopieren.
5. Nach dem Kopieren startet der M1 automatisch neu.

Dass das USB-Laufwerk beim Neustart wieder verschwindet, ist normal.

Der personalisierte Bootscreen zeigt:

```text
Fieldtest fuer
peter.kernspecht@web.de
by Moorbock
```

## 2. Verbindung mit der MeshCore-App

Der M1 verbindet sich über Bluetooth Low Energy (BLE) mit der MeshCore-App.

1. Bluetooth am Smartphone einschalten.
2. In der MeshCore-App ein Companion-Radio hinzufügen.
3. Den angezeigten M1 auswählen.
4. Falls nach einem PIN gefragt wird, `123456` eingeben.

Auf dem Statusscreen steht bei bestehender Verbindung `< Connected >`.
Solange noch keine App verbunden ist, zeigt der M1 stattdessen den BLE-PIN.

Falls keine Verbindung zustande kommt, eine alte gespeicherte M1-Verbindung in
der App entfernen, Bluetooth kurz aus- und wieder einschalten und den M1 neu
starten.

## 3. Tastenbedienung

Der M1 besitzt zwei Tasten:

- **Taste 1 / Page Turn:** vorwärts beziehungsweise nächster Eintrag
- **Taste 2 / Function:** rückwärts beziehungsweise vorheriger Eintrag

Die Grundfunktionen sind:

| Eingabe | Funktion |
|---|---|
| Taste 1 kurz | nächste Hauptseite oder nächster Eintrag |
| Taste 2 kurz | vorherige Hauptseite oder vorheriger Eintrag |
| Eine Taste lange halten | auswählen, bestätigen oder Aktion ausführen |
| Eine Taste doppelklicken | Zusatzansicht oder Kontextfunktion öffnen |
| Eine Taste dreifach klicken | Buzzer ein- oder ausschalten |

Ein Langdruck wird nach ungefähr 1,2 Sekunden erkannt. Ist das Display
ausgeschaltet, weckt der erste Tastendruck zunächst nur das Display; die
eigentliche Aktion wird dabei noch nicht ausgeführt.

Wichtig: Ein Langdruck innerhalb der ersten acht Sekunden nach dem Einschalten
startet den CLI-Rettungsmodus. Für die normale Bedienung daher den Bootscreen
erst vollständig abwarten.

Die Punkte unter dem Gerätenamen zeigen, auf welcher Hauptseite man sich
befindet. Der ausgefüllte Punkt ist die aktuelle Seite. Oben rechts befindet
sich die Akkuanzeige.

## 4. Die Screens

### 4.1 Status

Der Statusscreen ist die Startseite. Er zeigt:

- `MSG`: Anzahl ungelesener Nachrichten
- BLE-PIN oder `< Connected >`
- `Noise floor`: gemessener Rauschpegel des Funkkanals
- `RX`: Anzahl beobachteter Empfangspakete
- `SNR`: Signal-Rausch-Abstand des letzten Empfangs
- `Last path`: zuletzt beobachteter Mesh-Pfad

Ein weniger negativer SNR-Wert bedeutet normalerweise einen besseren Empfang.
Beim Noise Floor sind stark schwankende oder ungewöhnlich hohe Werte ein
Hinweis auf Störungen oder einen belegten Kanal.

**Langdruck:** Öffnet die Nachrichtenansicht, sofern Nachrichten vorhanden
sind.

**Doppelklick:** Öffnet das Aktionsmenü:

- `Send advert`: eigenes Advert senden
- `BLE off/on`: Bluetooth abschalten oder einschalten
- `Pkt fwd off/on`: Paketweiterleitung umschalten
- `Hibernate`: M1 in den Tiefschlaf versetzen

Im Aktionsmenü wählt Taste 1 den nächsten Eintrag. Ein Langdruck führt den
markierten Eintrag aus. Taste 2 verlässt das Menü.

### 4.2 Send

Auf dieser Seite können vorbereitete Schnellnachrichten gesendet werden.

Angezeigt werden:

- `To`: Zielkanal (`#`) oder direkter Kontakt (`@`)
- `Msg`: ausgewählte Schnellnachricht
- Nachrichtentext
- `Scope`: verwendeter Mesh-Scope

Bedienung:

1. Langdruck öffnet die Zielauswahl.
2. Mit Taste 1 vorwärts durch die Ziele blättern.
3. Langdruck übernimmt das Ziel und öffnet die Nachrichtenauswahl.
4. Nachricht mit Taste 1 auswählen.
5. Langdruck sendet die Nachricht.

Taste 2 bricht eine geöffnete Ziel-, Nachrichten- oder Scope-Auswahl ab.

Die letzte Nachricht in der Liste ist die dynamische GPS-Nachricht
`Meine Position ist: <Breite>, <Länge>`. Sie kann nur mit gültigem GPS-Fix
gesendet werden. Ohne Fix erscheint `No GPS fix`.

**Doppelklick:** Öffnet die Scope-Auswahl. Mit Taste 1 den Scope wählen und mit
Langdruck bestätigen. Taste 2 bricht ab. `Default` verwendet den
voreingestellten Scope; in diesem Build ist `bsmesh` als Standard konfiguriert.

### 4.3 Recent Adverts

Diese Seite zeigt die zuletzt gehörten Nodes beziehungsweise Adverts. Rechts
neben jedem Namen steht das Alter des Empfangs:

- `s`: Sekunden
- `m`: Minuten
- `h`: Stunden

Damit lässt sich schnell erkennen, welche Nodes am aktuellen Standort noch
regelmäßig empfangen werden.

### 4.4 Heatstrip

Der Heatstrip stellt die in den letzten Empfangspfaden beobachteten Repeater
als Matrix dar.

- `Rep`: sechsstellige Repeater-Kennung
- jede Matrixspalte: ein beobachteter Pfad
- Markierung in einer Zelle: Position des Repeaters im jeweiligen Pfad
- `PC`: Path Count, also in wie vielen Pfaden der Repeater vorkam

Die Repeater werden nach `PC` sortiert. Ein hoher Wert deutet darauf hin, dass
dieser Repeater häufig an den beobachteten Verbindungen beteiligt war.
`No RX paths` bedeutet, dass noch keine auswertbaren Empfangspfade vorliegen.

### 4.5 Positions

Die Positionsseite zeigt bevorzugte Kontakte mit bekannter Position als
Radaransicht:

- Mittelpunkt: eigener Standort
- `N`: Norden
- Abstand vom Mittelpunkt: relative Entfernung
- `Overview`: Übersicht aller bekannten Lieblingspositionen
- Maßstabsangabe: Entfernung bis zum Rand der Darstellung

Bei Auswahl eines einzelnen Nodes werden Name, Entfernung und Himmelsrichtung
angezeigt. `T` kennzeichnet eine Position aus aktivem Tracking, `A` eine aus
einem empfangenen Advert.

**Doppelklick:** Wechselt zwischen Übersicht und den einzelnen Positionen.

**Langdruck:** Öffnet das Position-Tracking. Für den Start müssen in der App
Kontakte als Favoriten markiert sein.

Tracking starten:

1. Langdruck auf der Positionsseite.
2. Noch einmal lang drücken, um die Kontaktauswahl zu öffnen.
3. Kontakt mit Taste 1 auswählen.
4. Langdruck öffnet die Zeitwahl.
5. Mit Taste 1 `twice`, 5, 15, 30 oder 60 Minuten auswählen.
6. Mit Langdruck starten.

Taste 2 bricht die Kontakt- oder Zeitwahl ab.

Bei `twice` werden zwei Positionsabfragen ausgeführt. Während eines aktiven
Trackings beendet ein Langdruck das Tracking. Nach Abschluss löscht ein
Langdruck das angezeigte Trackergebnis.

### 4.6 Load

Die Load-Seite visualisiert die beobachtete Funkkanal-Auslastung als
Balkendiagramm.

- `Now`: Auslastung des aktuellen Zeitfensters
- `Max`: höchster gemessener Wert
- `Avg`: Durchschnitt aller dargestellten Zeitfenster
- Balken: zeitlicher Verlauf der empfangenen Airtime

Ein Langdruck fragt zunächst `Reset stats?`. Ein zweiter Langdruck setzt die
Live-Statistik zurück.

Mit **Doppelklick** wird zwischen drei Unterseiten gewechselt:

1. `Load` – Kanalauslastung
2. `Heard repeaters` – zuletzt als Hop gehörte Repeater
3. `Scopes` – erkannte Mesh-Scopes

Auf der Scopes-Unterseite startet ein Langdruck eine Abfrage der benachbarten
Repeater. Ein Stern vor einem Scope bedeutet, dass er durch diese Abfrage
entdeckt wurde. Die Zahl in Klammern ist die Anzahl der zugehörigen Empfänge.

### 4.7 GPS

Die GPS-Seite zeigt:

- `gps on/off`: Softwarezustand des GPS
- `fix/no fix`: gültige Positionsbestimmung vorhanden oder nicht
- `sats used`: verwendete Satelliten
- `hdop`: geschätzte horizontale Positionsgenauigkeit
- `utc`: GPS-Zeit in UTC
- `lat` und `lon`: geografische Koordinaten
- `alt m`: Höhe in Metern

**Langdruck:** GPS ein- oder ausschalten.

**Doppelklick:** Aktuelle GPS-Position als Node-Position speichern. Ohne
gültigen Fix erscheint `No GPS fix`.

## 5. Nachrichtenansicht

Geht eine Nachricht ein, erhöht sich `MSG` auf dem Statusscreen. Ein Langdruck
öffnet die Nachrichtenansicht. Dort kann mit den beiden Tasten durch vorhandene
Nachrichten geblättert werden. Abhängig vom Nachrichtentyp ist auch eine
Antwort möglich.

## 6. Fernabfrage mit `SHOWPATHS`

Die Fieldtest-Firmware besitzt eine zusätzliche Fernabfrage für die zuletzt
beobachteten Mesh-Pfade. Damit können die Daten des M1 über das Mesh abgefragt
werden, ohne das Gerät vor Ort bedienen zu müssen.

### Abfrage senden

Dem M1 als **direkte private Textnachricht** exakt folgenden Text senden:

```text
SHOWPATHS
```

Der Befehl muss vollständig großgeschrieben sein. Am sichersten wird er ohne
weitere Wörter, Satzzeichen oder nachfolgende Leerzeichen gesendet.

Wichtig:

- Die Nachricht muss direkt an den M1 gesendet werden.
- Eine Nachricht in einem öffentlichen oder privaten Kanal löst den Befehl
  nicht aus.
- Der Absender muss für den M1 als Kontakt erreichbar sein, damit die Antwort
  zurückgesendet werden kann.
- Die Befehlsnachricht selbst erscheint nicht als normale eingegangene
  Nachricht auf dem M1-Display.

### Antwort verstehen

Der M1 antwortet automatisch mit maximal acht zuletzt beziehungsweise am
häufigsten beobachteten Pfaden. Das Format sieht beispielsweise so aus:

```text
Paths:
1 (12): A1B2 C3D4
2 (7): E5F6
3 (3): A1B2 E5F6 C3D4
```

Dabei bedeutet:

- `1`, `2`, `3`: laufende Nummer des Pfads
- Zahl in Klammern: wie oft dieser Pfad beobachtet wurde
- Kennungen dahinter: Repeater beziehungsweise Hops in Pfadreihenfolge

Wenn noch keine Pfade empfangen wurden, lautet die Antwort:

```text
Paths:
No RX paths
```

Die Daten stammen aus demselben internen Pfadspeicher wie der Heatstrip. Die
Fernantwort ist eine kompakte Textdarstellung, während der Heatstrip die
Repeater über mehrere Pfade als Matrix zusammenfasst.

Da die Antwort selbst über das Mesh zurückgesendet wird, kann sie bei fehlender
Route, schlechtem Empfang oder hoher Kanalauslastung ausbleiben. In diesem Fall
den Befehl nicht schnell wiederholt senden, sondern etwas warten und erneut
versuchen.

## 7. Hinweise für den Feldtest

Für vergleichbare Messungen sollte der M1 an jedem Standort möglichst gleich
ausgerichtet und für einige Minuten nicht bewegt werden. Sinnvoll ist folgende
Reihenfolge:

1. Auf dem Statusscreen Noise Floor, RX, SNR und Last Path notieren.
2. Unter Recent prüfen, welche Nodes aktuell gehört werden.
3. Heatstrip und Heard Repeaters fotografieren.
4. Load einige Minuten beobachten und Max/Avg notieren.
5. Bei GPS-Fix den Standort beziehungsweise die Koordinaten festhalten.

Bei Fehlern bitte möglichst notieren:

- Uhrzeit und Standort
- aktuell sichtbarer Screen
- letzte ausgeführte Tastenfolge
- BLE verbunden oder getrennt
- letzter sichtbarer Pfad, SNR und Noise Floor
- ob der M1 selbstständig neu gestartet hat

Ein Foto des Displays ist bei Darstellungsfehlern besonders hilfreich.

## 8. Ausschalten und Wiederherstellen

Zum stromsparenden Abschalten auf dem Statusscreen per Doppelklick das
Aktionsmenü öffnen, `Hibernate` auswählen und mit Langdruck bestätigen.

Falls der M1 nach einem Update nicht normal startet, erneut per Doppel-Reset in
den Bootloader wechseln und eine zuvor funktionierende UF2-Datei kopieren.

Kontakte und Einstellungen bleiben bei einem normalen UF2-Update üblicherweise
erhalten. Für Testdaten gibt es jedoch keine Garantie; wichtige Daten deshalb
vorher nach Möglichkeit in der MeshCore-App sichern.

## 9. Prüfsumme der Testfirmware

SHA-256 von `ThinkNode_M1_fieldtest_pk.uf2`:

```text
34d2f71113f80964f7b30770e17dd9fcbcb64fedb9e8a167ebae205e9e9403d8
```
