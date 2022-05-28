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

  Keine Referenzen gefunden, sollen Vakuumröhrenkollektoren sein.
  Die bisher mit den neuen Temperatursensoren maximal gemessene
  Temperatur am Vorlauf betrug knapp 80°C.

- **Wärmemedium**

  Tyfocor LS

  https://www.resol.de/Produktdokumente/TYFOCOR-LS.datde.pdf

- **Steuerung des Bypassventils**

  Thermomax TMX300 (defekt) mit Thermometern am Vorlauf und am
  Speicher

  Keine Referenzen gefunden.

- **Steuerung der Pumpe**

  Eigenbaulösung des Vorbesitzers mit Fotodiode auf dem Dach und
  OP-Amp als Komparator, der über ein Relais die Pumpe steuert

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

- **Speicher**

  Brötje SSB 300

  https://polo.broetje.de/pdf/7638646=6=pdf_(bdr_a4_manual)=de-de_ma_ssb_300.pdf

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
**R**ücklauf, **B**ypasszweig und -ventil zu sehen, oben im Bild
der erste Prototyp der neuen Steuerung:

![Kern der Anlage](https://github.com/farblos/solarcontrol/blob/main/assets/dsc_0010.jpg?raw=true)
