# 📊 GUIDE - MONITORING VOLTAGES TEMPS RÉEL

## 🎯 DEUX OPTIONS DISPONIBLES

### **OPTION 1: Serial Monitor (Texte)**
Affiche les voltages toutes les 2 secondes en texte clair

### **OPTION 2: Serial Plotter (Graphique)**
Affiche les voltages en graphique en temps réel

---

## 🔧 ACTIVATION

### **Dans le fichier INO, fonction `loop()`:**

```cpp
void loop() {
  server.handleClient();
  updateValues();
  updateHeartbeat();
  processSerial();
  
  // DEBUG: Décommenter UNE des deux lignes ci-dessous
  
  // debugPrintVoltages();      // ← Pour Serial Monitor (texte)
  // debugPlotterVoltages();    // ← Pour Serial Plotter (graphique)
}
```

---

## 📝 OPTION 1: SERIAL MONITOR (TEXTE)

### **Étapes:**

1. **Décommenter** la ligne:
   ```cpp
   debugPrintVoltages();
   ```

2. **Upload** le code sur l'ESP32

3. **Ouvrir Serial Monitor:**
   - `Tools` → `Serial Monitor`
   - OU raccourci: `Ctrl+Shift+M`

4. **Configurer baud rate:**
   - En bas à droite: Sélectionner **115200**

### **Résultat - Exemple:**

```
========================================
📊 VOLTAGES TEMPS RÉEL
========================================

🔌 PINS ANALOGIQUES (ADC):
  GPIO 32 (Battery): ADC=2048  Voltage=1.650V
  GPIO 33 (Coolant): ADC=1024  Voltage=0.825V
  GPIO 34 (TPS):     ADC=3072  Voltage=2.475V
  GPIO 35 (MAP):     ADC=2560  Voltage=2.063V
  GPIO 36 (AFR):     ADC=1536  Voltage=1.238V
  GPIO 39 (Fuel):    ADC=2816  Voltage=2.269V

⚡ RPM:
  GPIO 2: 45 impulsions (dernier 500ms)
  RPM calculé: 2700

🔘 PINS DIGITAUX:
  GPIO 13 (Turn Left):  LOW (0V)
  GPIO 14 (Turn Right): HIGH (3.3V)
  GPIO 15 (Daylight):   HIGH (3.3V)
  GPIO 16 (High Beam):  LOW (0V)
  ...
========================================
```

### **Avantages:**
- ✅ Lecture facile
- ✅ Noms des pins
- ✅ Valeurs ADC + Voltage
- ✅ RPM et digitaux inclus

---

## 📈 OPTION 2: SERIAL PLOTTER (GRAPHIQUE)

### **Étapes:**

1. **Décommenter** la ligne:
   ```cpp
   debugPlotterVoltages();
   ```

2. **Upload** le code sur l'ESP32

3. **Ouvrir Serial Plotter:**
   - `Tools` → `Serial Plotter`
   - OU raccourci: `Ctrl+Shift+L`

4. **Configurer baud rate:**
   - En bas à droite: Sélectionner **115200**

### **Résultat:**

Tu verras un **graphique en temps réel** avec 6 courbes:
- 🔵 GPIO32 (Battery)
- 🟢 GPIO33 (Coolant)
- 🟡 GPIO34 (TPS)
- 🔴 GPIO35 (MAP)
- 🟣 GPIO36 (AFR)
- 🟠 GPIO39 (Fuel)

Axe Y: 0V à 3.3V
Axe X: Temps (défile automatiquement)

### **Avantages:**
- ✅ Visualisation graphique
- ✅ Tendances visibles
- ✅ Plusieurs pins simultanément
- ✅ Rafraîchissement 100ms (fluide)

---

## ⚡ DIFFÉRENCES

| Feature | Serial Monitor | Serial Plotter |
|---------|---------------|----------------|
| **Format** | Texte | Graphique |
| **Rafraîchissement** | 2 secondes | 100 ms |
| **Pins monitorés** | Tous (ADC + Digital + RPM) | Seulement 6 ADC |
| **Lisibilité** | Noms clairs | Couleurs |
| **Détection variations** | Difficile | Facile |

---

## 🎯 RECOMMANDATION

### **Pour TESTER des capteurs:**
→ **Serial Plotter** (voir variations en temps réel)

### **Pour DIAGNOSTIQUER un problème:**
→ **Serial Monitor** (voir valeurs exactes + digitaux + RPM)

---

## ⚠️ IMPORTANT

### **Conflit avec TunerStudio:**

Si TunerStudio est connecté au port série, le Serial Monitor/Plotter ne fonctionnera pas (et vice-versa).

**Solution:**
1. Déconnecter TunerStudio
2. Ouvrir Serial Monitor/Plotter
3. Tester
4. Fermer Serial Monitor/Plotter
5. Reconnecter TunerStudio

### **Désactiver après tests:**

**IMPORTANT:** Après tes tests, **re-commenter** la ligne debug dans `loop()`:

```cpp
void loop() {
  server.handleClient();
  updateValues();
  updateHeartbeat();
  processSerial();
  
  // debugPrintVoltages();      // ← Commenté après tests
  // debugPlotterVoltages();    // ← Commenté après tests
}
```

Sinon, le spam Serial va ralentir l'ESP32 et causer des problèmes avec TunerStudio!

---

## 🔬 EXEMPLE D'UTILISATION

### **Scénario: Tester un capteur TPS**

1. Connecter TPS sur GPIO34
2. Décommenter `debugPlotterVoltages();`
3. Upload code
4. Ouvrir Serial Plotter
5. Bouger le TPS de 0% à 100%
6. Observer la courbe GPIO34 monter de ~0.5V à ~3.3V
7. Si ça ne bouge pas → problème câblage!

### **Scénario: Vérifier toutes les connections**

1. Décommenter `debugPrintVoltages();`
2. Upload code
3. Ouvrir Serial Monitor
4. Vérifier que chaque pin lit une valeur cohérente
5. Pins non-connectés devraient lire ~1.65V (flottant)
6. Pins connectés à GND → 0V
7. Pins connectés à 3.3V → 3.3V

---

## 📊 INTERPRÉTATION VALEURS

### **Pins ADC non-connectés:**
- Valeur flottante (varie, souvent ~1.5-1.8V)
- Normal si capteur pas encore branché

### **Pins ADC à GND:**
- Devrait lire ~0V (ADC=0-50)
- Si > 0.2V → problème connexion

### **Pins ADC à 3.3V:**
- Devrait lire ~3.3V (ADC=4045-4095)
- Si < 3.1V → problème connexion

### **RPM:**
- En simulation: oscille 0-6000
- En réel sans signal: 0
- En réel avec signal: compte impulsions

---

## 🎉 PRÊT!

Active le debug et surveille tes pins en temps réel! 📈
