# solarcontrol - Neues Leben für eine alte Solaranlage

Ziel des Projektes ist es, eine bestehende Solaranlage mit
defekter Steuerung mit möglichst geringem finanziellen Aufwand so
effektiv wie möglich zu betreiben.  Dazu soll die Steuerung durch
einen Arduino Nano ersetzt werden.

Gründe für dieses Vorgehen:

- Lust am Basteln und am Nicht-Wegwerfen

- Eine längerfristig geplante Sanierung des Daches mit dann
  vollständigem Austausch der Solaranlage

Im Folgenden bezeichnet "bestehend" alle Komponenten, die ich mit
dem Haus vom Vorbesitzer übernommen habe, und den Solarspeicher,
der von meinem Installateur installiert wurde.  Dagegen
bezeichnet "neu" die Komponenten der Steuerung durch den Arduino
Nano.

## Beschreibung der bestehenden Anlage

Die Anlage ist größtenteils 20 Jahre alt oder älter, sie wurde
vom Vorbesitzer des Hauses installiert.  Ursprünglich war die
Solaranlage und der Warmwasserspeicher mit dem Heizkessel
integriert, nach einem Tausch sowohl von Speicher als auch
Heizkessel sind Solaranlage und Heizkessel komplett voneinander
getrennt.  Soll heißen: Der Heizkessel ist an die obere
Heizspirale des Speichers angeschlossen, die Solaranlage an die
untere, aber Heizkessel und Solaranlage "wissen" nichts
voneinander.

Das Haus ist ein Doppelhaushälfte mit zwei Geschossen und
Dachgeschoss, Heizungs- und Solaranlage sind im Kellergeschoss
installiert.

- **Kollektoren**

  2 Module Hitachi Solar Collector Type SK-21D in
  Ost-West-Ausrichtung auf der Südseite des Daches

  Keine Referenzen gefunden.  Es waren mal
  Vakuumröhrenkollektoren, jetzt sind es nur noch, nun ja,
  Röhrenkollektoren.

  ![Kollektoren](assets/dsc_0013.jpg?raw=true)

- **Wärmemedium**

  Tyfocor LS

  https://www.resol.de/Produktdokumente/TYFOCOR-LS.datde.pdf

- **Steuerung des Bypassventils**

  Thermomax TMX300 (defekt) mit Thermometern am Vorlauf und am
  Speicher

  Keine Referenzen gefunden.

- **Steuerung der Pumpe**

  Eigenbaulösung des Vorbesitzers mit Fotodiode BPW41N auf dem
  Dach und OP-Amp als Komparator, der über ein Relais die Pumpe
  steuert

  https://www.vishay.com/docs/81522/bpw41n.pdf

- **Bypassventil**

  Honeywell V4044F1034

  Das Drei-Wege-Ventil schaltet zwischen geschlossenem und
  offenem A-Zweig.  Der B-Zweig ist immer offen.  Nach meinem
  Gefühl fließt auch bei offenem A-Zweig eine nicht geringe Menge
  des Wärmemediums über den B-Zweig.

  https://customer.honeywell.com/en-US/Pages/Product.aspx?cat=HonECC+Catalog&pid=V4044F1034/U

- **Pumpe**

  Grundfos Typ 25-40 180 auf Drehzahlstufe 1

  https://product-selection.grundfos.com/de/products/up-ups-series-100/up-ups/ups-25-40-180-96281375?tab=variant-curves&pumpsystemid=1563828155

  Da das Modell in der bestehenden Anlage schon etwas älter ist,
  hier sein Typenschild:

  ![Typenschild der Pumpe](assets/dsc_0012.jpg?raw=true)

- **Speicher**

  Brötje SSB 300

  https://polo.broetje.de/pdf/7638646=6=pdf_%28bdr_a4_manual%29=de-de_ma_ssb_300.pdf

- **Installation und Isolation der Rohre zum Dach**

  Die Rohre sind fast ideal verlegt in der Hinsicht, dass die
  Installation fast ausschließlich senkrecht verläuft.  Auf dem
  Dach haben die Vögel die Isolation komplett entfernt, im Keller
  ist die Isolation zum Teil geschmolzen.  Wie es im senkrechten
  Isolationsschacht aussieht, weiß ich nicht.

- **Elektroinstallation zum Dach**

  Zum Dach führt eine zwei-adrige (Lautsprecher?)-Leitung hoch,
  an die die Fotodiode angeschlossen ist.  Nachträglich weitere
  Leitungen zu verlegen ist wahrscheinlich eher schwierig.

Hier ist der Kern der Anlage mit Pumpe, **V**orlauf,
**R**ücklauf, **B**ypasszweig zu sehen, oben im Bild der erste
Prototyp der neuen Steuerung:

![Kern der Anlage](assets/dsc_0010.jpg?raw=true)

Links vom Buchstaben "B" befindet sich das elektrische
Bypassventil von Honeywell, rechts oberhalb vom "B" ein
Absperrventil, das im Folgenden "Bypasshandventil" genannt wird.

## Beschreibung des Pumpenalgorithmus

Der größte Teil des [Sketches](src/solarcontrol.ino) besteht aus
langweiligem Haushalten mit Fehlerbehandlung, SD Card Logging und
der Anzeige des aktuellen Zustands im LCD.  Der eigentliche
Pumpenalgorithmus ist in den Zweigen nach dem Zweig `case
STATE_FORCE_OFF` der State Machine in der Funktion `loop`
implementiert.

Vereinfacht gesagt "sammelt" die State Machine im Status
`STATE_WAITING` Licht vom Lichtsensor geteilt durch die aktuelle
Speichertemperatur.  Hat sie davon genug gesammelt, wechselt sie
in den Status `STATE_PUMPING`, pumpt `TEST_PUMP_CYCLES` Zyklen
(aktuell: ca. 120 Sekunden) und dann noch so lange, wie die
Differenz zwichen Vorlauftemperatur und Speichertemperatur
mindestens `TEMP_DELTA` (aktuell: 2.5&deg;C) beträgt.  Fällt die
Differenz unter `TEMP_DELTA`, so wechselt die State Machine
wieder in den Status `STATE_WAITING`, wobei sie dabei die
gesammelte Lichtmenge auf Null zurücksetzt.

## Entwicklungen und Erkenntnisse

### Organisatorisches

- Von Anfang an alle Änderungen gut dokumentieren, sowohl in der
  Hardware als auch in der Software.

- Bei der Versionierung der Software sich nicht nur auf git (oder
  was auch immer) verlassen, sondern besser jeden einzelnen
  Upload auf den Arduino als eigenständige Version wegsichern.
  Damit hat man auch für die "eben mal schnell" Änderungen eine
  lückenlose Dokumentation.  Mit einer geeigneten Erweiterung des
  [Arduino Makefiles](https://github.com/sudar/Arduino-Makefile)
  lässt sich das elegant automatisieren.

- Die aktuelle Versionsnummer oder Uploadnummer des Sketches auch
  in den Logfiles hinterlegen, falls man denn welche schreibt.
  Damit kann man später besser nachvollziehen, wie die Werte in
  den Logfiles genau zu Stande gekommen sind.

### Positionierung der Fotodiode

Am Anfang hing die Fotodiode nur irgendwie herum, und die Kurve
der Lichtintensität über den Tag sah entsprechend aus:

![Fotodiode suboptimal 1](assets/photodiode-1.png?raw=true)

Dann habe ich sie mal irgendwie anders hingedreht:

![Fotodiode suboptimal 2](assets/photodiode-2.png?raw=true)

Und mich schließlich dazu entschlossen, den bestehenden
Schrumpfschlauch doch mal abzumachen und zu schauen, was das für
eine Fotodiode ist und welche Position sie idealerweise haben
sollte.  Nach der vorläufig finalen Positionierung sah die Kurve
der Lichtintensität dann so aus:

![Fotodiode fast optimal](assets/photodiode-3.png?raw=true)

An diesem Tag stand die Sonne gegen 13:30 im Zenit, insofern
passt das Maximum der Lichtintensität schon wesentlich besser zur
Realität.

Da die Siliziumfotodiode ihre Empfindlichkeit doch eher im
sichtbaren Licht und weniger im Infrarotbereich hat, habe ich den
neuen Schrumpfschlauch an der Sensorstelle gelocht, um möglichst
viel vom Infrarot mitzunehmen:

![Fotodiode entblättert](assets/dsc_0014.jpg?raw=true)
