# ESP32_DASH_TunerStudio
Display sensors inputs to ESP32 and broadcast them into Tuner Studio. 
========================================
ESP32 DASHBOARD SIMULATOR - VERSION 8.0
========================================

📅 Date: Janvier 2026
🎯 Plateforme: ESP32 Dev Module
⚡ Baud Rate: 115200
📡 WiFi AP: ESP32-Dashboard / password123

========================================
📋 TABLE DES MATIÈRES
========================================

1. DESCRIPTION GÉNÉRALE
2. CARACTÉRISTIQUES PRINCIPALES
3. MATÉRIEL REQUIS
4. INSTALLATION
5. CONFIGURATION
6. INTERFACE WEB
7. CALCUL RPM
8. MODE DEBUG
9. TUNERSTUDIO
10. DÉPANNAGE
11. FICHIERS INCLUS

========================================
1. DESCRIPTION GÉNÉRALE
========================================

ESP32 Dashboard Simulator est un système complet de simulation ECU/Dashboard pour TunerStudio.
Il permet de tester et développer des dashboards automobiles sans avoir besoin d'un moteur réel.

✨ NOUVEAU DANS V8.0:
- 🐛 Mode Debug via interface web (Serial Monitor/Plotter)
- 🔢 RPM sur GPIO2 avec interruption matérielle
- 📐 Support formules personnalisées pour capteurs analogiques
- 🌐 Interface web améliorée avec onglet Debug
- 💾 Sauvegarde persistante configuration debug

========================================
2. CARACTÉRISTIQUES PRINCIPALES
========================================

📊 CAPTEURS ANALOGIQUES (6 canaux):
  • Battery (0-20V)
  • Coolant Temperature (0-250°C)
  • TPS - Throttle Position (0-100%)
  • MAP - Manifold Pressure (0-250 kPa)
  • AFR - Air/Fuel Ratio (10-18)
  • Fuel Level (0-100%)

🔢 RPM (GPIO2 FIXE):
  • Comptage par interruption matérielle
  • Formule: RPM = (fréquence × 60 × 2) / cylindres
  • Support 1-12 cylindres
  • Filtre anti-rebond (< 500µs)
  • Lissage moyenne glissante
  • Limite max: 10,000 RPM

🔘 INDICATEURS DIGITAUX (13 canaux):
  • Turn Left / Turn Right
  • Daylight / High Beam
  • Alternator Error
  • ESP32 Connected (toujours ON)
  • Oil Warning
  • 5 entrées configurables

⚙️ GEAR (Vitesse calculée):
  • Basé sur RPM et TPS
  • 0 = Neutral
  • 1-6 = Gears

🐛 MODE DEBUG:
  • Activation via interface web
  • Serial Monitor (texte détaillé)
  • Serial Plotter (graphique temps réel)
  • Sauvegarde configuration EEPROM

========================================
3. MATÉRIEL REQUIS
========================================

OBLIGATOIRE:
  ✓ ESP32 Dev Module (30 pins)
  ✓ Câble USB (programmation + alimentation)
  ✓ Ordinateur avec Arduino IDE 1.8.19+

OPTIONNEL (pour capteurs réels):
  ○ 6x capteurs analogiques (0-3.3V)
  ○ Capteur RPM (signal digital)
  ○ 13x switches/boutons (digitaux)
  ○ Résistances pull-up/pull-down si besoin

PINS ESP32 DISPONIBLES:
  ADC (analogique): GPIO 32, 33, 34, 35, 36, 39
  RPM (fixe):       GPIO 2
  Digital:          GPIO 12-19, 21-23, 25-27

========================================
4. INSTALLATION
========================================

ÉTAPE 1: PRÉREQUIS ARDUINO IDE
  1. Ouvrir Arduino IDE
  2. File → Preferences → Additional Boards Manager URLs:
     https://dl.espressif.com/dl/package_esp32_index.json
  3. Tools → Board → Boards Manager
  4. Chercher "ESP32" et installer "esp32 by Espressif Systems"

ÉTAPE 2: CONFIGURATION BOARD
  • Tools → Board → ESP32 Arduino → ESP32 Dev Module
  • Upload Speed: 115200
  • Flash Frequency: 80MHz
  • Flash Mode: QIO
  • Partition Scheme: Default 4MB with spiffs

ÉTAPE 3: BIBLIOTHÈQUES
  ✓ WiFi.h (incluse avec ESP32)
  ✓ WebServer.h (incluse avec ESP32)
  ✓ Preferences.h (incluse avec ESP32)

ÉTAPE 4: UPLOAD
  1. Brancher ESP32 via USB
  2. Sélectionner port COM: Tools → Port → COM# (ou /dev/ttyUSB#)
  3. Ouvrir ESP32_V8_FIXED_WARNING.ino
  4. Cliquer Upload (→)
  5. Attendre "Done uploading"

========================================
5. CONFIGURATION
========================================

PREMIÈRE CONNEXION:
  1. ESP32 démarre en mode Access Point (AP)
  2. SSID: ESP32-Dashboard
  3. Password: password123
  4. Se connecter au WiFi depuis ordinateur/téléphone
  5. Navigateur: http://192.168.4.1

CONFIGURATION INITIALE:
  Par défaut, TOUT est en SIMULATION
  • Aucun capteur physique requis
  • Valeurs oscillantes pour tests
  • Idéal pour développement dashboard

PASSER EN MODE RÉEL:
  1. Interface web → Onglet approprié (RPM/Analogs/Digitals)
  2. Toggle "SIMULATION" → OFF
  3. Configurer pin GPIO
  4. Cliquer "Sauvegarder Configuration" (en bas)
  5. ESP32 redémarre (~3 secondes)

========================================
6. INTERFACE WEB
========================================

URL: http://192.168.4.1

ONGLETS DISPONIBLES:

📊 RPM:
  • GPIO: 2 (FIXE - interruption)
  • Nombre de cylindres: 1-12
  • Toggle Simulation ON/OFF

📈 ANALOGIQUES (6 canaux):
  • Chaque canal configurable individuellement
  • GPIO Pin: 32, 33, 34, 35, 36, 39
  • Min/Max Voltage: 0-3.3V
  • Min/Max Value: Valeur physique
  • Toggle "Use Formula": Formule personnalisée
    - Variable: x = voltage (0-3.3V)
    - Exemples: x*10, (x-0.5)*100, (x-0.5)/2.8*20
  • Toggle Simulation ON/OFF

🔘 DIGITAUX (13 canaux):
  • GPIO configurable par canal
  • Pull Mode: PULL_UP, PULL_DOWN, NO_PULL
  • Inverted: Normal ou inversé
  • Toggle Simulation ON/OFF

🐛 DEBUG:
  • Toggle Debug ON/OFF
  • Mode:
    - 🚫 Désactivé
    - 📝 Serial Monitor (texte détaillé)
    - 📈 Serial Plotter (graphique temps réel)
  • Instructions complètes incluses

📘 GUIDE:
  • Pins ESP32 disponibles
  • Formules de conversion
  • Calcul RPM détaillé
  • Exemples de formules personnalisées
  • Mode Pull pour digitaux
  • Sauvegarde EEPROM
  • TunerStudio integration

SAUVEGARDE:
  • Bouton "💾 Sauvegarder Configuration" en bas de page
  • Sauvegarde dans EEPROM
  • ESP32 redémarre automatiquement
  • Configuration persistante (survit aux coupures)

========================================
7. CALCUL RPM
========================================

FORMULE (moteur 4-temps):
  RPM = (fréquence × 60 × 2) / cylindres

PARAMÈTRES:
  • fréquence: Hz (impulsions par seconde)
  • 60: Conversion secondes → minutes
  • 2: Facteur 4-temps (1 impulsion = 2 tours vilebrequin)
  • cylindres: Configuration (1-12)

EXEMPLE - 4 cylindres à 3000 RPM:
  1. 3000 RPM → 3000/60 = 50 tours/sec
  2. 50 tours × 2 impulsions/tour = 100 impulsions/sec
  3. Fréquence = 100 Hz
  4. Vérification: (100 × 60 × 2) / 4 = 3000 RPM ✓

ALGORITHME IMPLÉMENTÉ:
  1. Interruption FALLING sur GPIO2
  2. Comptage impulsions pendant 500ms
  3. Calcul fréquence: impulsions / 0.5s
  4. Application formule RPM
  5. Lissage moyenne glissante (facteur 0.3)
  6. Filtre anti-rebond (< 500µs ignoré)
  7. Limite max: 10,000 RPM

CONNEXION CAPTEUR:
  • Signal RPM → GPIO2 (PULL_UP interne)
  • Capteur doit mettre GPIO2 à GND (masse)
  • Front descendant (HIGH → LOW) déclenche interruption
  • Typiquement: sortie collecteur ouvert ou NPN

========================================
8. MODE DEBUG
========================================

ACTIVATION:
  1. Interface web → Onglet 🐛 Debug
  2. Toggle Debug: ON
  3. Choisir mode:
     - 📝 Serial Monitor (texte)
     - 📈 Serial Plotter (graphique)
  4. Sauvegarder Configuration
  5. ESP32 redémarre
  6. Déconnecter TunerStudio (conflit port série!)
  7. Arduino IDE → Tools → Serial Monitor ou Serial Plotter
  8. Baud: 115200

MODE 1: SERIAL MONITOR (TEXTE)
  • Affichage toutes les 2 secondes
  • Format détaillé:
    - Pins analogiques: ADC brut, Voltage, Valeur convertie, Unité
    - RPM: Impulsions, RPM calculé, Cylindres
    - Pins digitaux: État HIGH/LOW
  • Idéal pour: Diagnostiquer connexions, vérifier valeurs exactes

MODE 2: SERIAL PLOTTER (GRAPHIQUE)
  • Affichage continu (100ms)
  • 6 courbes: GPIO32, GPIO33, GPIO34, GPIO35, GPIO36, GPIO39
  • Axe Y: 0-3.3V
  • Défilement automatique
  • Idéal pour: Voir variations, tendances, oscillations

DÉSACTIVATION:
  1. Retourner interface web (reste active!)
  2. Onglet Debug → Toggle OFF
  3. Sauvegarder
  4. ESP32 redémarre sans debug

IMPORTANT:
  ⚠️ Serial Monitor/Plotter ET TunerStudio ne peuvent pas être ouverts simultanément
  ⚠️ Désactiver debug après tests pour éviter ralentissements
  ⚠️ Interface web reste accessible même en mode debug

========================================
9. TUNERSTUDIO
========================================

CONFIGURATION:
  1. Ouvrir TunerStudio
  2. File → New Project
  3. Firmware: "Other / Browse"
  4. Sélectionner: ESP32_V8.ini
  5. Communications:
     - Type: Serial (RS232)
     - Port: Sélectionner port COM de l'ESP32
     - Baud: 115200
     - Test Port → Devrait dire "Signature Match"

DONNÉES DISPONIBLES:
  • battery (V)
  • coolant_temp (°C)
  • tps (%)
  • map (kPa)
  • afr (AFR)
  • fuel_level (%)
  • rpm (RPM)
  • turn_left, turn_right (bits)
  • daylight, highbeam (bits)
  • alt_error, esp_conn, oil_warn (bits)
  • input1-5 (bits)
  • gear (0-6)

PROTOCOLE:
  • Compatible Speeduino/MegaSquirt
  • Query command: "Q"
  • Read command: "r"
  • Block size: 36 bytes
  • Endianness: Little

DÉPANNAGE TUNERSTUDIO:
  ❌ "No Response":
     → Vérifier port COM correct
     → Fermer Serial Monitor si ouvert
     → Vérifier baud 115200
     → Débrancher/rebrancher USB

  ❌ "Signature Mismatch":
     → Vérifier fichier INI correct (ESP32_V8.ini)
     → Signature attendue: "ESP32 Custom v1.0"

  ❌ Valeurs figées:
     → Vérifier mode simulation dans interface web
     → En mode réel, vérifier capteurs branchés

========================================
10. DÉPANNAGE
========================================

PROBLÈME: ESP32 ne démarre pas
  → Vérifier alimentation USB (500mA min)
  → Essayer autre câble USB (pas charge-only)
  → Appuyer sur bouton BOOT pendant upload

PROBLÈME: Pas de WiFi visible
  → Attendre 5 secondes après mise sous tension
  → LED devrait clignoter 3× au démarrage
  → Vérifier Serial Monitor: "WiFi AP started"
  → SSID: ESP32-Dashboard

PROBLÈME: Interface web ne charge pas
  → Vérifier connexion WiFi à ESP32-Dashboard
  → URL exacte: http://192.168.4.1
  → Essayer autre navigateur
  → Désactiver données cellulaires (mobile)

PROBLÈME: Configuration ne sauvegarde pas
  → Cliquer bouton "Sauvegarder Configuration"
  → Attendre redémarrage (LED clignote)
  → Vérifier Serial Monitor pour "Config saved"
  → Si erreur EEPROM, reflasher ESP32

PROBLÈME: RPM toujours à 0 (mode réel)
  → Vérifier signal sur GPIO2
  → Mode Debug → Serial Monitor
  → Vérifier "Impulsions (500ms): X"
  → Si 0 impulsions → problème signal
  → Vérifier capteur met GPIO2 à GND

PROBLÈME: Valeurs analogiques instables
  → Pins flottants (non-connectés) → normal
  → Ajouter condensateur 100nF si bruit
  → Vérifier GND commun ESP32 ↔ capteur
  → Mode Debug → Serial Plotter pour voir

PROBLÈME: Serial Monitor affiche caractères bizarres
  → Vérifier baud: 115200
  → Vérifier bon port COM sélectionné
  → Fermer TunerStudio si ouvert

PROBLÈME: TunerStudio et Serial Monitor conflit
  → Normal! Un seul à la fois
  → Fermer l'un avant ouvrir l'autre
  → Interface web reste accessible

========================================
11. FICHIERS INCLUS
========================================

📁 FICHIERS PRINCIPAUX:

ESP32_V8_FIXED_WARNING.ino
  • Code source complet ESP32
  • Version 8.0 avec debug web
  • Compilation sans warnings
  • Prêt à uploader

ESP32_V8.ini
  • Fichier configuration TunerStudio
  • 36 bytes ochBlockSize
  • 6 canaux analogiques + RPM + digitaux
  • Compatible Speeduino/MegaSquirt

README.txt
  • Ce fichier
  • Documentation complète
  • Guide installation et utilisation

📁 FICHIERS OPTIONNELS:

GUIDE_MONITORING_VOLTAGES.md
  • Guide détaillé Serial Monitor/Plotter
  • Exemples d'utilisation
  • Interprétation valeurs

========================================
📊 SPÉCIFICATIONS TECHNIQUES
========================================

PERFORMANCE:
  • Fréquence refresh: 10 Hz (100ms)
  • RPM update: 2 Hz (500ms)
  • Debug Monitor: 0.5 Hz (2000ms)
  • Debug Plotter: 10 Hz (100ms)

MÉMOIRE:
  • Flash utilisé: ~900 KB
  • RAM utilisé: ~50 KB
  • EEPROM utilisé: ~2 KB

WIFI:
  • Mode: Access Point (AP)
  • Canal: Auto
  • IP ESP32: 192.168.4.1
  • Subnet: 255.255.255.0
  • Clients max: 4

PINS UTILISÉS:
  • GPIO 2:  RPM (interruption)
  • GPIO 32: Battery (ADC)
  • GPIO 33: Coolant (ADC)
  • GPIO 34: TPS (ADC)
  • GPIO 35: MAP (ADC)
  • GPIO 36: AFR (ADC)
  • GPIO 39: Fuel (ADC)
  • GPIO 12-27: Digitaux (configurables)
  • LED interne: Heartbeat

========================================
🔧 SUPPORT ET MISES À JOUR
========================================

VERSION ACTUELLE: 8.0
DATE: Janvier 2026

HISTORIQUE VERSIONS:
  v8.0 - Mode Debug web + RPM interruption + Formules
  v7.0 - Interface web améliorée
  v6.0 - Support 6 canaux analogiques
  v5.0 - TunerStudio integration
  v4.0 - Configuration EEPROM
  v3.0 - Interface web initiale
  v2.0 - Capteurs digitaux
  v1.0 - Première version

CONTACT:
  Développé pour simulation/test TunerStudio
  Compatible: Arduino IDE 1.8.19+
  Testé sur: ESP32 Dev Module 30 pins

========================================
📝 NOTES IMPORTANTES
========================================

⚠️ SÉCURITÉ:
  • WiFi password par défaut: password123
  • Changez-le si exposition publique
  • Pas de SSL (HTTP seulement)

⚠️ LIMITATIONS:
  • 6 pins ADC maximum (matériel ESP32)
  • RPM limité à 10,000 (filtre logiciel)
  • Debug et TunerStudio incompatibles simultanément
  • Formules personnalisées simplifiées

⚠️ ALIMENTATION:
  • USB: 500mA minimum
  • Éviter pics courant sur pins
  • Max 12mA par pin GPIO
  • Utiliser résistances protection

✅ BONNES PRATIQUES:
  • Tester en simulation avant capteurs réels
  • Sauvegarder config après modifications
  • Désactiver debug quand pas utilisé
  • Vérifier voltages via debug avant connexion
  • GND commun pour tous capteurs

========================================
🎉 PRÊT À UTILISER!
========================================

1. Upload ESP32_V8_FIXED_WARNING.ino
2. Connecter au WiFi: ESP32-Dashboard
3. Ouvrir: http://192.168.4.1
4. Tester en mode simulation
5. Configurer TunerStudio avec ESP32_V8.ini
6. Profiter! 🚀

Questions? Consultez section DÉPANNAGE ci-dessus!

========================================
FIN DU README
========================================
