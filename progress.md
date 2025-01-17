# Progress Report DarWinWin

Vorab: Ziel Testbarkeit fuer Actor movement, level Generierung und Interaktion, Anpassung der Actorstats

## 30.12.2024: 

**Malin:** Projekt Setup, Webserver ist aufgesetzt

## 31.12.2024: 

**Malin:** 
- Es kann ein Level erstellt werden: Tileflags werden gesetzt mit Bits fuer `Underwater`, `Collidable`, `Protein` etc.
- Die Viewing Cone des Actors kann berechnet werden, je nach Blickrichtung wird eine `viewCone` ausgegeben: 
  ```
    14
   0257
    36
  ```
    mit 0 als aktueller Position des Actors

  Die ViewCone spiegelt wieder, was der Actor sehen kann und soll als Input fuer das NN dienen

## 01.01.2025: 

**Malin:** 
- Refactoring des ViewCone Codes mit LUTs

## 02.01.2025: 

**Malin:**
- Der Actor hat jetzt verschiedene Stats wie Energielevel, verbleibende Luft, Level fuer die verschiedenen Nahrungselemente.
- Der Actor kann sich jetzt um ein Feld in Blickrichtung bewegen und verliert dabei Energie.
- Der Actor kann sich drehen.
- Der Actor kann etwas essen, fuellt damit das Level der Nahrung auf und die Nahrung wird aus dem Level entfernt.

## 03.01.2025 

**Malin:**
- Weitere Arbeit an den ActorStats

## 04.01.2025: 

**Malin:** 
- Der Actor verliert Luft, wenn Unterwasser und Energie, auch ohne Aktion.

## 05.01.2025: 

**Malin:**
- Konsultierung eines Subject Matter Experts zum evolutionären Algorithmus, Evaluierung des Sketchouts fuer den evulutionären Algorithmus und Neuronales Netzwerk Implementierung in SIMD
- Ablauf eines Zuges: Aktuelle Stats anpassen, Viewcone erfassen, Neuronales Netzwerk evaluiert Werte, Ergebnis als Aktion des Actors auswerten und ausfuehren.
- Funktionen fuer Frontend-Anbindung: `getLevel` um das gesamte Level zu erhalten, `setTile` um eine einzelne Tile im Level auf einen Wert zu setzen, `manualAct` um den Actor manuell eine Aktion ausfuehren zu lassen.

## 06.01.2025: Besprechung der Grundlagen und Rahmenbedingungen.

Aktueller Stand:
- Level mit Bitflags für die Umgebung, kann generiert und benutzt werden.
- Actor hat verschieden Stats (z.B. Energie, Luft, Hunger etc.), befindet sich auf dem Spielfeld, kann Aktionen ausfuehren, Stats werden je nach Aktion angepasst.
- Neuronales Netz: Kann mit den Inputs Ergebnisse evaluieren.
- Genetic Algorithm: Prototyp der Infrastruktur besteht.
- Server: kann gestartet werden, handelt `getLevel`, `setTile` und `manualAct`.
- Frontend: Infrakstruktur zur Kommunikation mit dem Server besteht.

Projektorganisation: 
- Daily um 11 Uhr. 
- Taeglicher Progress Report.

Ziele fuer Ende der Woche:
- Level anzeigen koennen und testen.
- Generelle Implementierung evolutioniaerer Algorithmus, testen.

Aktuelle Tasks:
- Jan, Marvin: Level visualisieren, Level manuell veraendern, Player manuell steuern.
- Malin: Genetic Algorithm implementieren, weitere Bereitstellung fuer Frontend Funktionalitaeten im Server, Progress Report Setup

## 07.01.2025:

**Jan:** (Krank)
- Code Familiarization
- Planung

**Marvin:** (Nicht beim Meeting erschienen)
- Code Familiarization
- Planung

**Malin:**
- Das Weblevel wird mit Testwerten initalisiert.
- Der Actor erhaelt Energie zurueck, wenn er Dinge isst.
- Erstellen und Updaten der Readne.md und des Progress Reports

**Hauptrioritaet Frontend:** Visualisierung Level.

**Hauptrioritaet Backend:** Grundstruktur Genetic Algorithm

## 09.01.2025:

**Jan:** N/A (krank)

**Marvin:**
- Rudimentaere Darstellung Map inkl. api calls wird gehandlet 

**Malin:**
- Grundgeruest des Genetic Algorithm ist implementiert und mit einfachem Beispiel erfolgreich getestet
- Anpassungen der Logik in verschiedenen Actor-Actions.
- Code Review durchgefuehrt

**Aktuelle Tasks:**
- Frontend: angefragte Anpassungen implementieren, handling von `manualAct`
- Backend: Mutator mit Random Chance implementieren, weitere Fixes in verschiedenen Actions

## 10.01.2025:

**Jan:** N/A (krank)

**Marvin:** (Nicht beim Meeting erschienen)
- Implementierung von Feedback und Code-Cleanup
- Map formatierung jetzt durch Flexbox wrapping anstatt von column elementen
- Info Anzeige für Actor

**Malin:**
- ein Mutator mutiert Daten nur bei einer gewissen Wahrscheinlichkeit
- Actors haben Maegen: es passt nur eine bestimmte Kapazitaet in den Magen, sonst wird keine weitere Nahrung aufgenommen und jede Runde wird Nahrung verdaut, damit wird Energie freigesetzt.

**Aktuelle Tasks:**
- Frontend: Info Anzeige Tiles, handling von `manualAct` und `setTile`
- Backend: `Mutator` mit Normalverteilung

## 13.01.2025:

**Marvin:**
- Actor manual actions implementiert
- setTile post request + tile info mit stats

**Malin:**
- mutator und crossbreeder verwenden Pointer
- Tests fuer Evolution und NN
- Neuronales Netz unterstuetzt unterschiedliche grosse Layers in einem Netz

**Aktuelle Tasks:**

Frontend: 
- level & actor daten korrekt anzeigen (bug fix)
- Testen ob manuelle Aktionen richtig funktionieren (Outcome und Stats kontrollieren)
- Darstellung: Collidable & Wasser farbig
- (Refactoring)

Backend:
- Speichersystem NN
- Level Generator

# 14.01.2025

**Marvin:**
- Refactoring, collidables werden korrekt angezeigt

**Jan:**
- Einarbeitung in Code

**Malin:**
- NN in Dateien schreiben vorbereitet

**Aktuelle Tasks:**

Frontend:
- Testen ob manuelle Aktionen richtig funktionieren (Outcome und Stats kontrollieren)
- Viewcone anzeigen
- ~~im Debugger durchsteppen was bei move passiert (im Backend)~~
- Set Tile mit besserer UI
- Info automatisch updaten
- Clean-Up und Refactoring

Backend:
- getLevel viewcone hinzufuegen
- NN in Datei schreiben
- Level Generator

## 16.01.2025

**Jan:**
- Anfang view cone Implementierung

**Malin:**
- viewcone zu getlevel hinzugefuegt
- Load and Save fuer neuestes Brain
- Level Generator

Aktuelle Tasks:

Frontend:
- Branch fertig zum mergen machen!
- Viewcone fertig machen
- Testen!

Backend:
- Vorbereitung fuer Training

## 17.01.2025

Aktuelle Tasks:

**Jan:**
- Bugfix viewcone
- Bugfix Ansicht

**Malin:**
- Code Review

Frontend:
- Testen
- Branch fertig machen!
--
- viewcone seperat anzeigen aus Frontend zusammengebaut (in neuer Branch1, wenn fertig -> neue MR)
- Button: single step fuer ai-actor `/ai_step` (neue Branch2)
- Button: generiertes Level vom Server anfragen `/level_generate` (neue Branch3 & MR)
--
- Buttons: `/start_training`, `/stop_training`, `is_training`  (neue Branch4 & MR)

Backend:
- Frontend Änderungen mergen, wenn bereit
- Training vorbereiten + training testen