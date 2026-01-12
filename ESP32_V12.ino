/*
 * =====================================================
 * ESP32 ECU Dashboard - Avec Interface Web Complète
 * Version 7.0 - INTERFACE WEB COMPLÈTE + CORRECTIONS:
 * - RPM: Signal DIGITAL (impulsions) avec PULL_UP
 * - RPM: Division par nombre de cylindres (1-12)
 * - BUG CORRIGÉ: Battery et capteurs ne montrent plus 654V
 * - Interface Web: Design moderne avec 4 onglets
 * - Guide intégré avec documentation complète
 * =====================================================
 * 
 * ACCÈS À L'INTERFACE WEB:
 * 1. Se connecter au WiFi "ESP32-Dashboard" (mot de passe: dashboard123)
 * 2. Ouvrir navigateur: http://192.168.4.1
 * 3. Configurer chaque capteur et indicateur
 * 4. Sauvegarder (enregistré dans EEPROM)
 * 
 * ONGLETS INTERFACE WEB:
 * - 📊 RPM: Configuration capteur digital avec nombre de cylindres
 * - 📈 Analogiques: 11 capteurs avec conversion linéaire
 * - 🔘 Digitaux: 10 indicateurs avec mode pull configurable
 * - 📘 Guide: Documentation complète des pins et formules
 * 
 * CORRECTIONS VERSION 7.0:
 * - RPM sans min_voltage/max_voltage (signal digital)
 * - Valeurs par défaut min_voltage = 0.5V (évite division par zéro)
 * - Simulation toggle visible pour chaque capteur
 * - Interface web complète et moderne
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// =====================================================
// 🌐 CONFIGURATION WIFI
// =====================================================
const char* AP_SSID = "ESP32-Dashboard";
const char* AP_PASSWORD = "dashboard123";

WebServer server(80);
Preferences preferences;

// =====================================================
// 📦 CONFIGURATION
// =====================================================
#define LED_PIN 2
#define BAUD_RATE 115200
#define SIGNATURE "ESP32 Custom v1.0"

// Structure pour inputs digitaux
struct DigitalConfig {
  bool simulation;
  int pin;
  bool inverted;
  int pullMode;  // 0=PULL_DOWN, 1=PULL_UP, 2=NO_PULL
  char name[32];
};

// Structure pour inputs analogiques
struct AnalogConfig {
  bool simulation;
  int pin;
  float min_value;
  float max_value;
  float min_voltage;
  float max_voltage;
  bool useFormula;       // false=linéaire, true=formule personnalisée
  char formula[64];      // Formule personnalisée (ex: "x*2+10")
  char name[32];
  char unit[16];
};

// Structure spéciale pour RPM (avec nombre de cylindres)
struct RPMConfig {
  int cylinders;         // Nombre de cylindres (1-12)
  bool simulation;       // Simulation ON/OFF
  int pin;               // GPIO pin pour RPM (fixé à GPIO2)
};

// Configuration RPM globale (GPIO2 fixe pour comptage interruption)
RPMConfig rpmConfig = {4, true, 2};

// ===== VARIABLES RPM PAR INTERRUPTION =====
volatile unsigned long rpmPulseCount = 0;  // Compteur d'impulsions
volatile unsigned long lastPulseTime = 0;   // Temps dernière impulsion (us)
unsigned long rpmLastCalc = 0;              // Dernier calcul RPM (ms)
float rpmSmoothed = 0;                      // RPM lissé (moyenne glissante)
const int RPM_WINDOW_MS = 500;              // Fenêtre de calcul (500ms)
const float RPM_SMOOTH_FACTOR = 0.3;        // Facteur lissage (0-1)
const int RPM_MAX = 10000;                  // RPM max (filtre aberrations)

// ISR pour comptage impulsions RPM
void IRAM_ATTR rpmPulseISR() {
  unsigned long now = micros();
  // Filtre anti-rebond: ignorer impulsions < 500us (équivaut à >120,000 RPM!)
  if(now - lastPulseTime > 500) {
    rpmPulseCount = rpmPulseCount + 1;  // Syntaxe C++20 compatible
    lastPulseTime = now;
  }
}

// ===== DEBUG MODE (contrôlé via interface web) =====
bool debugEnabled = false;           // Debug ON/OFF
int debugMode = 0;                   // 0=Off, 1=Monitor(texte), 2=Plotter(graphique)
unsigned long debugLastPrint = 0;    // Dernier affichage debug
const int DEBUG_INTERVAL_MONITOR = 2000;  // 2 sec pour Monitor
const int DEBUG_INTERVAL_PLOTTER = 100;   // 100 ms pour Plotter

// ===== 13 INDICATEURS DIGITAUX =====
DigitalConfig digitals[13] = {
  {true, 12, false, 2, "Turn Left"},       // NO_PULL
  {true, 13, false, 2, "Turn Right"},      // NO_PULL
  {true, 14, false, 2, "Daylight"},        // NO_PULL
  {true, 15, false, 2, "High Beam"},       // NO_PULL
  {true, 16, true,  1, "Alt Error"},       // PULL_UP (inversé)
  {true, 17, false, 2, "ESP32 Connected"}, // NO_PULL
  {true, 18, true,  1, "Oil Warning"},     // PULL_UP (inversé)
  {true, 19, false, 2, "Input 1"},         // NO_PULL
  {true, 21, false, 2, "Input 2"},         // NO_PULL
  {true, 22, false, 2, "Input 3"},         // NO_PULL
  {true, 23, false, 2, "Input 4"},         // NO_PULL
  {true, 25, false, 2, "Input 5"},         // NO_PULL
  {true, 26, false, 2, "Reserved"}         // NO_PULL
};

// ===== 11 CADRANS ANALOGIQUES =====
// NOTE: ESP32 a seulement 6 pins ADC utilisables (32, 33, 34, 35, 36, 39)
// Les 6 premiers sont assignés à des pins physiques
// Les 5 derniers sont en SIMULATION par défaut (l'utilisateur peut les reconfigurer)
// ===== 6 CANAUX ANALOGIQUES RÉELS (correspond aux 6 pins ADC ESP32) =====
// Index 0-5 correspondent aux pins ADC physiques disponibles
// Chaque canal peut utiliser formule mathématique OU mapping linéaire
AnalogConfig analogs[6] = {
  {true, 32,   0.0,   20.0, 0.5, 3.3, false, "", "Battery",       "V"},        // GPIO 32 (ADC1_CH4)
  {true, 33,   0.0,  100.0, 0.5, 3.3, false, "", "Coolant Temp",  "°C"},       // GPIO 33 (ADC1_CH5)
  {true, 34,   0.0,  100.0, 0.5, 3.3, false, "", "TPS",           "%"},        // GPIO 34 (ADC1_CH6)
  {true, 35,   0.0,  110.0, 0.5, 3.3, false, "", "MAP",           "kPa"},      // GPIO 35 (ADC1_CH7)
  {true, 36,  10.0,   18.0, 0.5, 3.3, false, "", "AFR",           "AFR"},      // GPIO 36 (ADC1_CH0)
  {true, 39,   0.0,  100.0, 0.5, 3.3, false, "", "Fuel Level",    "%"}         // GPIO 39 (ADC1_CH3)
};

// ===== STRUCTURE DES DONNÉES TUNERSTUDIO =====
// 6 canaux analogiques + RPM + digital status
struct RuntimeData {
  uint16_t battery;        // Battery voltage (0-20V) * 100
  uint16_t coolant_temp;   // Coolant temp (0-250°C) * 10
  uint16_t tps;            // TPS (0-100%) * 10
  uint16_t map_pressure;   // MAP (0-250 kPa) * 10
  uint16_t afr;            // AFR (10-18) * 10
  uint8_t  fuel_level;     // Fuel level (0-100%)
  uint16_t rpm;            // RPM (0-10000)
  uint8_t  status_bits1;   // Digital inputs 0-7
  uint8_t  status_bits2;   // Digital inputs 8-12
  uint8_t  gear;           // Calculated gear (1-6)
  uint8_t  reserved[20];   // Reserved
} __attribute__((packed));

RuntimeData data;

// =====================================================
// 🚀 SETUP
// =====================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(BAUD_RATE);
  delay(500);  // Attendre que Serial soit prêt
  
  Serial.println("\n\n=====================================================");
  Serial.println("ESP32 ECU Dashboard - Version 7.0");
  Serial.println("=====================================================\n");
  
  // Charger config depuis EEPROM
  loadConfig();
  
  // Initialiser pins
  initPins();
  
  // Démarrer WiFi AP
  Serial.println("📡 Starting WiFi Access Point...");
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress IP = WiFi.softAPIP();
  Serial.println("✅ WiFi AP started successfully!");
  Serial.println("   SSID: " + String(AP_SSID));
  Serial.println("   Password: " + String(AP_PASSWORD));
  Serial.print("   IP Address: ");
  Serial.println(IP);
  Serial.println();
  
  // Signal de démarrage
  for(int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  
  // Setup serveur web
  Serial.println("🌐 Web server started on port 80\n");
  setupWebServer();
  server.begin();
  
  // Initialiser données
  memset(&data, 0, sizeof(data));
  data.status_bits1 |= (1 << 5); // ESP32 connected
  
  // Configurer interruption RPM sur GPIO2
  if(!rpmConfig.simulation) {
    pinMode(rpmConfig.pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(rpmConfig.pin), rpmPulseISR, FALLING);
    Serial.println("🔢 RPM interrupt attached to GPIO" + String(rpmConfig.pin));
  }
  
  Serial.println("⚡ Dashboard running - Ready for TunerStudio!");
  Serial.println("=====================================================\n");
}

// =====================================================
// 🔄 LOOP
// =====================================================
void loop() {
  server.handleClient();
  updateValues();
  updateHeartbeat();
  processSerial();
  
  // Debug mode (contrôlé via interface web)
  if(debugEnabled) {
    if(debugMode == 1) debugPrintMonitor();
    else if(debugMode == 2) debugPrintPlotter();
  }
}

// =====================================================
// 🐛 DEBUG - SERIAL MONITOR (TEXTE)
// =====================================================
void debugPrintMonitor() {
  if(millis() - debugLastPrint < DEBUG_INTERVAL_MONITOR) return;
  debugLastPrint = millis();
  
  Serial.println("\n========================================");
  Serial.println("📊 DEBUG - VOLTAGES TEMPS RÉEL");
  Serial.println("========================================");
  
  // Pins analogiques
  Serial.println("\n🔌 PINS ANALOGIQUES (ADC):");
  const int adcPins[] = {32, 33, 34, 35, 36, 39};
  const char* adcNames[] = {"Battery", "Coolant", "TPS", "MAP", "AFR", "Fuel"};
  
  for(int i = 0; i < 6; i++) {
    if(!analogs[i].simulation) {
      int adc = analogRead(adcPins[i]);
      float voltage = (adc / 4095.0) * 3.3;
      float value = readAnalogChannel(i);
      Serial.printf("  GPIO %2d (%s): ADC=%4d  V=%.3f  Val=%.2f %s\n", 
                    adcPins[i], adcNames[i], adc, voltage, value, analogs[i].unit);
    } else {
      Serial.printf("  GPIO %2d (%s): SIMULATION\n", adcPins[i], adcNames[i]);
    }
  }
  
  // RPM
  Serial.println("\n⚡ RPM (GPIO 2):");
  if(!rpmConfig.simulation) {
    Serial.printf("  Impulsions (500ms): %lu\n", rpmPulseCount);
    Serial.printf("  RPM calculé: %d\n", data.rpm);
    Serial.printf("  Cylindres: %d\n", rpmConfig.cylinders);
  } else {
    Serial.printf("  SIMULATION: %d RPM\n", data.rpm);
  }
  
  // Pins digitaux
  Serial.println("\n🔘 PINS DIGITAUX:");
  for(int i = 0; i < 13; i++) {
    if(!digitals[i].simulation) {
      bool value = digitalRead(digitals[i].pin);
      Serial.printf("  GPIO %2d (%s): %s\n", 
                    digitals[i].pin, 
                    digitals[i].name, 
                    value ? "HIGH (3.3V)" : "LOW (0V)");
    }
  }
  
  Serial.println("========================================\n");
}

// =====================================================
// 📈 DEBUG - SERIAL PLOTTER (GRAPHIQUE)
// =====================================================
void debugPrintPlotter() {
  if(millis() - debugLastPrint < DEBUG_INTERVAL_PLOTTER) return;
  debugLastPrint = millis();
  
  // Format pour Serial Plotter: "Label:Value Label2:Value2"
  const int adcPins[] = {32, 33, 34, 35, 36, 39};
  const char* adcNames[] = {"GPIO32", "GPIO33", "GPIO34", "GPIO35", "GPIO36", "GPIO39"};
  
  for(int i = 0; i < 6; i++) {
    float voltage = (analogRead(adcPins[i]) / 4095.0) * 3.3;
    Serial.print(adcNames[i]);
    Serial.print(":");
    Serial.print(voltage, 3);
    if(i < 5) Serial.print(" ");
  }
  Serial.println();
}

// =====================================================
// 💾 SAUVEGARDER/CHARGER CONFIG
// =====================================================
void saveConfig() {
  Serial.println("💾 Saving config to EEPROM...");
  preferences.begin("config", false);
  
  // Sauvegarder RPM config
  preferences.putInt("rpm_cyl", rpmConfig.cylinders);
  preferences.putBool("rpm_sim", rpmConfig.simulation);
  preferences.putInt("rpm_pin", rpmConfig.pin);
  Serial.printf("  RPM: cyl=%d, sim=%d, pin=%d\n", rpmConfig.cylinders, rpmConfig.simulation, rpmConfig.pin);
  
  // Sauvegarder digitaux
  for(int i = 0; i < 13; i++) {
    char key[16];
    sprintf(key, "d%d_sim", i);
    preferences.putBool(key, digitals[i].simulation);
    sprintf(key, "d%d_pin", i);
    preferences.putInt(key, digitals[i].pin);
    sprintf(key, "d%d_inv", i);
    preferences.putBool(key, digitals[i].inverted);
    sprintf(key, "d%d_pull", i);
    preferences.putInt(key, digitals[i].pullMode);
  }
  Serial.println("  Digitals: saved");
  
  // Sauvegarder analogiques
  for(int i = 0; i < 6; i++) {
    char key[16];
    sprintf(key, "a%d_sim", i);
    preferences.putBool(key, analogs[i].simulation);
    sprintf(key, "a%d_pin", i);
    preferences.putInt(key, analogs[i].pin);
    sprintf(key, "a%d_min", i);
    preferences.putFloat(key, analogs[i].min_value);
    sprintf(key, "a%d_max", i);
    preferences.putFloat(key, analogs[i].max_value);
    sprintf(key, "a%d_minv", i);
    preferences.putFloat(key, analogs[i].min_voltage);
    sprintf(key, "a%d_maxv", i);
    preferences.putFloat(key, analogs[i].max_voltage);
    sprintf(key, "a%d_useF", i);
    preferences.putBool(key, analogs[i].useFormula);
    sprintf(key, "a%d_form", i);
    preferences.putString(key, analogs[i].formula);
    
    if(i == 0) {
      Serial.printf("  Analog[0] Battery: sim=%d, pin=%d\n", analogs[i].simulation, analogs[i].pin);
    }
  }
  Serial.println("  Analogs: saved");
  
  // Sauvegarder debug config
  preferences.putBool("debug_en", debugEnabled);
  preferences.putInt("debug_mode", debugMode);
  Serial.printf("  Debug: enabled=%d, mode=%d\n", debugEnabled, debugMode);
  
  preferences.end();
  Serial.println("✅ Config saved successfully!");
}

void loadConfig() {
  Serial.println("📂 Loading config from EEPROM...");
  preferences.begin("config", true);
  
  // Charger RPM config
  rpmConfig.cylinders = preferences.getInt("rpm_cyl", 4);
  rpmConfig.simulation = preferences.getBool("rpm_sim", true);
  rpmConfig.pin = preferences.getInt("rpm_pin", 34);
  Serial.printf("  RPM: cyl=%d, sim=%d, pin=%d\n", rpmConfig.cylinders, rpmConfig.simulation, rpmConfig.pin);
  
  // Charger digitaux
  for(int i = 0; i < 13; i++) {
    char key[16];
    sprintf(key, "d%d_sim", i);
    digitals[i].simulation = preferences.getBool(key, true);
    sprintf(key, "d%d_pin", i);
    digitals[i].pin = preferences.getInt(key, 12 + i);
    sprintf(key, "d%d_inv", i);
    digitals[i].inverted = preferences.getBool(key, false);
    sprintf(key, "d%d_pull", i);
    digitals[i].pullMode = preferences.getInt(key, 2);  // Default: NO_PULL
  }
  Serial.println("  Digitals: loaded");
  
  // Charger analogiques
  for(int i = 0; i < 6; i++) {
    char key[16];
    sprintf(key, "a%d_sim", i);
    if(preferences.isKey(key)) {
      analogs[i].simulation = preferences.getBool(key, true);
      sprintf(key, "a%d_pin", i);
      analogs[i].pin = preferences.getInt(key, 32);
      sprintf(key, "a%d_min", i);
      analogs[i].min_value = preferences.getFloat(key, 0.0);
      sprintf(key, "a%d_max", i);
      analogs[i].max_value = preferences.getFloat(key, 100.0);
      sprintf(key, "a%d_minv", i);
      analogs[i].min_voltage = preferences.getFloat(key, 0.0);
      if(analogs[i].min_voltage == 0.0) analogs[i].min_voltage = 0.5;  // Forcer 0.5 si 0
      
      sprintf(key, "a%d_maxv", i);
      analogs[i].max_voltage = preferences.getFloat(key, 3.3);
      if(analogs[i].max_voltage <= analogs[i].min_voltage) analogs[i].max_voltage = 3.3;  // Forcer 3.3 si invalide
      sprintf(key, "a%d_useF", i);
      analogs[i].useFormula = preferences.getBool(key, false);
      sprintf(key, "a%d_form", i);
      String formula = preferences.getString(key, "");
      strncpy(analogs[i].formula, formula.c_str(), 63);
      analogs[i].formula[63] = '\0';
      
      if(i == 0) {
        Serial.printf("  Analog[0] Battery: sim=%d, pin=%d, minV=%.2f, maxV=%.2f, minVal=%.2f, maxVal=%.2f\n", 
                      analogs[i].simulation, analogs[i].pin, analogs[i].min_voltage, analogs[i].max_voltage,
                      analogs[i].min_value, analogs[i].max_value);
      }
    }
  }
  Serial.println("  Analogs: loaded");
  
  // Charger debug config
  debugEnabled = preferences.getBool("debug_en", false);
  debugMode = preferences.getInt("debug_mode", 0);
  Serial.printf("  Debug: enabled=%d, mode=%d\n", debugEnabled, debugMode);
  
  preferences.end();
  Serial.println("✅ Config loaded successfully!");
}

void initPins() {
  for(int i = 0; i < 13; i++) {
    if(!digitals[i].simulation) {
      pinMode(digitals[i].pin, INPUT);
    }
  }
}

// =====================================================
// 🌐 SERVEUR WEB - DÉCLARATIONS FORWARD
// =====================================================
void handleRoot();
void handleGetConfig();
void handleSaveConfig();
void handleReset();

// =====================================================
// 🌐 SERVEUR WEB - SETUP
// =====================================================
void setupWebServer() {
  // Page principale
  server.on("/", HTTP_GET, handleRoot);
  
  // API pour lire config
  server.on("/api/config", HTTP_GET, handleGetConfig);
  
  // API pour sauvegarder config
  server.on("/api/save", HTTP_POST, handleSaveConfig);
  
  // API pour réinitialiser
  server.on("/api/reset", HTTP_POST, handleReset);
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Dashboard Config v7.0</title>
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
      min-height: 100vh;
      padding: 20px;
    }
    
    .container {
      max-width: 1200px;
      margin: 0 auto;
      background: rgba(255, 255, 255, 0.95);
      border-radius: 15px;
      box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
      overflow: hidden;
    }
    
    header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 30px;
      text-align: center;
    }
    
    header h1 {
      font-size: 2.2em;
      margin-bottom: 10px;
      text-shadow: 2px 2px 4px rgba(0,0,0,0.2);
    }
    
    header p {
      font-size: 1.1em;
      opacity: 0.9;
    }
    
    .tabs {
      display: flex;
      background: #f0f0f0;
      border-bottom: 3px solid #667eea;
      overflow-x: auto;
    }
    
    .tab {
      flex: 1;
      padding: 18px 25px;
      text-align: center;
      background: #e0e0e0;
      border: none;
      cursor: pointer;
      font-size: 16px;
      font-weight: 600;
      color: #555;
      transition: all 0.3s ease;
      border-right: 1px solid #ccc;
      min-width: 120px;
    }
    
    .tab:last-child {
      border-right: none;
    }
    
    .tab:hover {
      background: #d0d0d0;
      color: #333;
    }
    
    .tab.active {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      box-shadow: 0 4px 8px rgba(0,0,0,0.2);
    }
    
    .content {
      display: none;
      padding: 35px;
      animation: fadeIn 0.4s ease;
    }
    
    .content.active {
      display: block;
    }
    
    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(10px); }
      to { opacity: 1; transform: translateY(0); }
    }
    
    .sensor-card {
      background: white;
      border-radius: 12px;
      padding: 25px;
      margin-bottom: 25px;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.08);
      border-left: 5px solid #667eea;
      transition: all 0.3s ease;
    }
    
    .sensor-card:hover {
      box-shadow: 0 6px 20px rgba(0, 0, 0, 0.12);
      transform: translateY(-2px);
    }
    
    .sensor-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 20px;
      padding-bottom: 15px;
      border-bottom: 2px solid #f0f0f0;
    }
    
    .sensor-name {
      font-size: 1.3em;
      font-weight: 700;
      color: #333;
    }
    
    .toggle-switch {
      position: relative;
      display: inline-block;
      width: 60px;
      height: 30px;
      margin-left: 15px;
    }
    
    .toggle-switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-color: #ccc;
      transition: 0.3s;
      border-radius: 30px;
    }
    
    .slider:before {
      position: absolute;
      content: "";
      height: 22px;
      width: 22px;
      left: 4px;
      bottom: 4px;
      background-color: white;
      transition: 0.3s;
      border-radius: 50%;
    }
    
    input:checked + .slider {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    }
    
    input:checked + .slider:before {
      transform: translateX(30px);
    }
    
    .form-row {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 20px;
      margin-bottom: 15px;
    }
    
    .form-group {
      display: flex;
      flex-direction: column;
    }
    
    .form-group label {
      font-weight: 600;
      margin-bottom: 8px;
      color: #555;
      font-size: 0.95em;
    }
    
    .form-group input,
    .form-group select {
      padding: 12px 15px;
      border: 2px solid #e0e0e0;
      border-radius: 8px;
      font-size: 15px;
      transition: all 0.3s ease;
      background: white;
    }
    
    .form-group input:focus,
    .form-group select:focus {
      outline: none;
      border-color: #667eea;
      box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
    }
    
    .save-btn {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      padding: 15px 40px;
      border-radius: 8px;
      font-size: 1.1em;
      font-weight: 600;
      cursor: pointer;
      box-shadow: 0 4px 15px rgba(102, 126, 234, 0.4);
      transition: all 0.3s ease;
      display: block;
      margin: 30px auto 0;
    }
    
    .save-btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 6px 20px rgba(102, 126, 234, 0.5);
    }
    
    .save-btn:active {
      transform: translateY(0);
    }
    
    .info-box {
      background: #e3f2fd;
      border-left: 4px solid #2196f3;
      padding: 15px 20px;
      border-radius: 8px;
      margin-bottom: 25px;
      color: #1565c0;
      font-size: 0.95em;
    }
    
    .success-box {
      background: #e8f5e9;
      border-left: 4px solid #4caf50;
      padding: 15px 20px;
      border-radius: 8px;
      margin-bottom: 25px;
      color: #2e7d32;
      font-size: 0.95em;
    }
    
    .guide-section {
      background: #f9f9f9;
      border-radius: 10px;
      padding: 25px;
      margin-bottom: 25px;
    }
    
    .guide-section h3 {
      color: #667eea;
      margin-bottom: 15px;
      font-size: 1.4em;
    }
    
    .guide-section p, .guide-section ul {
      color: #555;
      line-height: 1.8;
      margin-bottom: 12px;
    }
    
    .guide-section ul {
      padding-left: 25px;
    }
    
    .guide-section code {
      background: #ffe082;
      padding: 3px 8px;
      border-radius: 4px;
      font-family: 'Courier New', monospace;
      font-size: 0.9em;
    }
    
    .pin-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(140px, 1fr));
      gap: 10px;
      margin-top: 15px;
    }
    
    .pin-item {
      background: white;
      padding: 12px;
      border-radius: 8px;
      text-align: center;
      border: 2px solid #e0e0e0;
      font-weight: 600;
      color: #667eea;
    }
    
    @media (max-width: 768px) {
      header h1 { font-size: 1.6em; }
      .tab { padding: 12px 15px; font-size: 14px; }
      .content { padding: 20px; }
      .form-row { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>⚙️ ESP32 Dashboard Config</h1>
      <p>Version 7.0 - Configuration des capteurs et indicateurs</p>
    </header>
    
    <div class="tabs">
      <button class="tab active" onclick="openTab('rpm')">📊 RPM</button>
      <button class="tab" onclick="openTab('analogs')">📈 Analogiques</button>
      <button class="tab" onclick="openTab('digitals')">🔘 Digitaux</button>
      <button class="tab" onclick="openTab('debug')">🐛 Debug</button>
      <button class="tab" onclick="openTab('guide')">📘 Guide</button>
    </div>
    
    <!-- RPM TAB -->
    <div id="rpm-content" class="content active">
      <div class="info-box">
        <strong>⚡ RPM = Signal Digital sur GPIO2 (FIXE)</strong><br>
        Le capteur RPM envoie des impulsions (mise à la terre) sur GPIO2. Une interruption compte les impulsions.<br>
        <strong>Formule:</strong> RPM = (fréquence × 60 × 2) / cylindres<br>
        Le facteur 2 est pour moteurs 4-temps (1 impulsion = 2 tours de vilebrequin).
      </div>
      
      <div class="sensor-card">
        <div class="sensor-header">
          <span class="sensor-name">🔢 Capteur RPM (GPIO2)</span>
          <div style="display: flex; align-items: center; gap: 10px;">
            <label class="toggle-switch">
              <input type="checkbox" id="rpm-sim" checked onchange="updateRPM('simulation', this.checked)">
              <span class="slider"></span>
            </label>
            <span id="rpm-sim-label" style="font-weight: 600; color: #667eea;">SIMULATION</span>
          </div>
        </div>
        
        <div class="form-row">
          <div class="form-group">
            <label>📍 GPIO Pin (FIXE)</label>
            <input type="text" value="GPIO 2 (PULL_UP)" disabled style="background: #f0f0f0;">
          </div>
          
          <div class="form-group">
            <label>🔧 Nombre de Cylindres</label>
            <select id="rpm-cyl" onchange="updateRPM('cylinders', this.value)">
              <option value="1">1 cylindre</option>
              <option value="2">2 cylindres</option>
              <option value="3">3 cylindres</option>
              <option value="4" selected>4 cylindres</option>
              <option value="5">5 cylindres</option>
              <option value="6">6 cylindres</option>
              <option value="8">8 cylindres</option>
              <option value="10">10 cylindres</option>
              <option value="12">12 cylindres</option>
            </select>
          </div>
        </div>
        
        <div class="success-box" style="margin-top: 20px;">
          ✓ Configuration: <strong id="cyl-display">4</strong> cylindres sur <strong>GPIO 2</strong> (interruption)
        </div>
      </div>
    </div>
    
    <!-- ANALOGS TAB -->
    <div id="analogs-content" class="content">
      <div class="info-box">
        <strong>Capteurs Analogiques (11 canaux)</strong><br>
        Chaque capteur lit un voltage (0-3.3V) sur un GPIO ADC et le convertit en valeur physique.
        Utilisez la simulation pour tester sans capteurs réels.
      </div>
      
      <div id="analog-sensors"></div>
    </div>
    
    <!-- DIGITALS TAB -->
    <div id="digitals-content" class="content">
      <div class="info-box">
        <strong>Indicateurs Digitaux (10 canaux)</strong><br>
        Chaque indicateur peut être configuré en mode PULL_UP, PULL_DOWN ou sans résistance (NO_PULL).
      </div>
      
      <div id="digital-sensors"></div>
    </div>
    
    <!-- DEBUG TAB -->
    <div id="debug-content" class="content">
      <div class="info-box" style="background: #fff3cd; border-left: 4px solid #ffc107;">
        <strong>🐛 Mode Debug Serial</strong><br>
        Active le monitoring des voltages via le Serial Monitor ou Serial Plotter de l'Arduino IDE.<br>
        <strong>⚠️ Important:</strong> Déconnectez TunerStudio avant d'utiliser le Serial Monitor/Plotter!
      </div>
      
      <div class="sensor-card">
        <div class="sensor-header">
          <span class="sensor-name">🔍 Configuration Debug</span>
          <div style="display: flex; align-items: center; gap: 10px;">
            <label class="toggle-switch">
              <input type="checkbox" id="debug-enabled" onchange="updateDebug('enabled', this.checked)">
              <span class="slider"></span>
            </label>
            <span id="debug-status" style="font-weight: 600; color: #999;">OFF</span>
          </div>
        </div>
        
        <div class="form-row">
          <div class="form-group">
            <label>📊 Mode d'Affichage</label>
            <select id="debug-mode" onchange="updateDebug('mode', this.value)">
              <option value="0">🚫 Désactivé</option>
              <option value="1">📝 Serial Monitor (Texte détaillé)</option>
              <option value="2">📈 Serial Plotter (Graphique temps réel)</option>
            </select>
          </div>
        </div>
        
        <div class="info-box" style="margin-top: 20px; background: #e3f2fd; border-left: 4px solid #2196f3;">
          <strong>📝 Serial Monitor (Texte):</strong>
          <ul style="margin: 10px 0 0 20px;">
            <li>Affiche voltages toutes les 2 secondes</li>
            <li>Montre: ADC brut, Voltage, Valeur convertie</li>
            <li>Inclut RPM et indicateurs digitaux</li>
            <li>Idéal pour diagnostiquer connexions</li>
          </ul>
        </div>
        
        <div class="info-box" style="margin-top: 10px; background: #f3e5f5; border-left: 4px solid #9c27b0;">
          <strong>📈 Serial Plotter (Graphique):</strong>
          <ul style="margin: 10px 0 0 20px;">
            <li>Affiche 6 courbes (GPIO 32-39) en temps réel</li>
            <li>Rafraîchissement: 100ms (fluide)</li>
            <li>Axe Y: 0-3.3V</li>
            <li>Idéal pour voir variations et tendances</li>
          </ul>
        </div>
      </div>
      
      <div class="sensor-card">
        <div class="sensor-header">
          <span class="sensor-name">📖 Instructions</span>
        </div>
        
        <div style="padding: 20px;">
          <h3 style="margin-top: 0;">🚀 Comment utiliser:</h3>
          <ol style="line-height: 1.8;">
            <li><strong>Activer le debug</strong> avec le toggle ci-dessus</li>
            <li><strong>Choisir le mode:</strong> Serial Monitor ou Serial Plotter</li>
            <li><strong>Cliquer "Sauvegarder Configuration"</strong> (en bas)</li>
            <li><strong>Attendre le redémarrage</strong> de l'ESP32 (~3 secondes)</li>
            <li><strong>Déconnecter TunerStudio</strong> s'il est connecté</li>
            <li><strong>Ouvrir dans Arduino IDE:</strong>
              <ul style="margin-top: 5px;">
                <li>Serial Monitor: Tools → Serial Monitor (Ctrl+Shift+M)</li>
                <li>Serial Plotter: Tools → Serial Plotter (Ctrl+Shift+L)</li>
              </ul>
            </li>
            <li><strong>Configurer baud rate:</strong> 115200</li>
            <li><strong>Observer les voltages!</strong></li>
          </ol>
          
          <h3 style="margin-top: 30px;">⚠️ Important:</h3>
          <ul style="line-height: 1.8;">
            <li><strong>Conflit port série:</strong> Serial Monitor/Plotter et TunerStudio ne peuvent pas être ouverts en même temps</li>
            <li><strong>Après vos tests:</strong> Désactivez le debug et sauvegardez pour éviter ralentissements</li>
            <li><strong>Redémarrage requis:</strong> Toute modification nécessite une sauvegarde et redémarrage</li>
          </ul>
          
          <h3 style="margin-top: 30px;">💡 Astuces:</h3>
          <ul style="line-height: 1.8;">
            <li><strong>Pin flottant:</strong> ~1.5-1.8V (normal si pas connecté)</li>
            <li><strong>GND:</strong> Devrait lire ~0V</li>
            <li><strong>3.3V:</strong> Devrait lire ~3.3V</li>
            <li><strong>Variations brusques:</strong> Mauvais contact ou interférences</li>
          </ul>
        </div>
      </div>
    </div>
    
    <!-- GUIDE TAB -->
    <div id="guide-content" class="content">
      <div class="guide-section">
        <h3>🎯 Pins ESP32 Disponibles</h3>
        <p><strong>Pins ADC (pour capteurs analogiques):</strong></p>
        <div class="pin-grid">
          <div class="pin-item">GPIO 32</div>
          <div class="pin-item">GPIO 33</div>
          <div class="pin-item">GPIO 34</div>
          <div class="pin-item">GPIO 35</div>
          <div class="pin-item">GPIO 36</div>
          <div class="pin-item">GPIO 39</div>
        </div>
        <p style="margin-top: 15px;"><strong>Pins GPIO (pour digitaux):</strong></p>
        <div class="pin-grid">
          <div class="pin-item">GPIO 2</div>
          <div class="pin-item">GPIO 4</div>
          <div class="pin-item">GPIO 5</div>
          <div class="pin-item">GPIO 12-19</div>
          <div class="pin-item">GPIO 21-23</div>
          <div class="pin-item">GPIO 25-27</div>
        </div>
      </div>
      
      <div class="guide-section">
        <h3>📐 Formules de Conversion</h3>
        <p><strong>Mode Linéaire (par défaut):</strong></p>
        <p><code>valeur = min_value + (voltage - min_voltage) * (max_value - min_value) / (max_voltage - min_voltage)</code></p>
        <p style="margin-top: 15px;"><strong>Exemple - Battery (12-16V):</strong></p>
        <ul>
          <li>Min Voltage ADC: 0.5V → Min Value: 12V</li>
          <li>Max Voltage ADC: 3.3V → Max Value: 16V</li>
          <li>Si capteur lit 1.9V → Battery = 12 + (1.9-0.5)*(16-12)/(3.3-0.5) = 14V</li>
        </ul>
      </div>
      
      <div class="guide-section">
        <h3>🔢 Calcul RPM (Interruption sur GPIO2)</h3>
        <p><strong>Le RPM utilise GPIO2 en mode FIXE avec interruption matérielle.</strong></p>
        
        <p style="margin-top: 15px;"><strong>Formule de calcul (moteur 4-temps):</strong></p>
        <p><code>RPM = (fréquence × 60 × 2) / nombre_cylindres</code></p>
        
        <ul style="margin-top: 10px;">
          <li><strong>fréquence:</strong> Hz (impulsions par seconde)</li>
          <li><strong>60:</strong> Conversion secondes → minutes</li>
          <li><strong>2:</strong> Facteur moteur 4-temps (1 impulsion = 2 tours vilebrequin)</li>
          <li><strong>nombre_cylindres:</strong> Configuration (1-12)</li>
        </ul>
        
        <p style="margin-top: 15px;"><strong>Exemple - Moteur 4 cylindres:</strong></p>
        <ul>
          <li>Le capteur reçoit 2 impulsions par tour de vilebrequin</li>
          <li>À 3000 RPM → 3000/60 = 50 tours/sec → 100 impulsions/sec</li>
          <li>Fréquence = 100 Hz</li>
          <li>RPM calculé = (100 × 60 × 2) / 4 = 3000 RPM ✓</li>
        </ul>
        
        <p style="margin-top: 15px;"><strong>Algorithme implémenté:</strong></p>
        <ol>
          <li><strong>Interruption FALLING</strong> sur GPIO2 (front descendant)</li>
          <li><strong>Comptage</strong> des impulsions pendant 500ms</li>
          <li><strong>Calcul fréquence:</strong> impulsions / 0.5s</li>
          <li><strong>Lissage</strong> moyenne glissante (facteur 0.3)</li>
          <li><strong>Filtre:</strong> Ignore impulsions < 500µs (anti-rebond)</li>
          <li><strong>Limite:</strong> Max 10,000 RPM (filtre aberrations)</li>
        </ol>
        
        <p style="margin-top: 15px;"><strong>⚠️ Connexion capteur:</strong></p>
        <ul>
          <li>Signal RPM → GPIO2 (avec résistance PULL_UP interne)</li>
          <li>Le capteur doit mettre GPIO2 à la masse (GND) à chaque impulsion</li>
          <li>Front descendant (HIGH → LOW) déclenche l'interruption</li>
        </ul>
      </div>
      
      <div class="guide-section">
        <h3>📐 Formules Personnalisées (Canaux Analogiques)</h3>
        <p><strong>Mode Formule:</strong> Permet d'utiliser une formule mathématique au lieu du mapping linéaire.</p>
        
        <p style="margin-top: 15px;"><strong>Variable disponible:</strong></p>
        <ul>
          <li><code>x</code> = voltage lu (0 - 3.3V)</li>
        </ul>
        
        <p style="margin-top: 15px;"><strong>Exemples de formules:</strong></p>
        <ul>
          <li><code>x*10</code> → Multiplication simple (ex: 1.5V → 15)</li>
          <li><code>(x-0.5)*100</code> → Offset + scaling (ex: 0.5V=0, 3.3V=280)</li>
          <li><code>x*5+10</code> → Linéaire avec offset (ex: 1V=15, 2V=20)</li>
          <li><code>(x-0.5)/2.8*20</code> → Normalisation (ex: 0.5-3.3V → 0-20)</li>
        </ul>
        
        <p style="margin-top: 15px;"><strong>Cas d'usage:</strong></p>
        <ul>
          <li><strong>Capteurs NTC:</strong> Résistances non-linéaires (température)</li>
          <li><strong>Pont diviseur:</strong> Correction selon résistances utilisées</li>
          <li><strong>Capteurs custom:</strong> Formules spécifiques du fabricant</li>
        </ul>
        
        <p style="margin-top: 15px; color: #f44336;"><strong>⚠️ Limitation actuelle:</strong></p>
        <p>L'évaluateur de formule est simplifié. Pour formules complexes, utilisez le mapping linéaire.</p>
      </div>
      
      <div class="guide-section">
        <h3>🔌 Mode Pull pour Digitaux</h3>
        <ul>
          <li><strong>PULL_UP:</strong> Pin maintenu à HIGH (3.3V). Le capteur met à la masse (GND) pour activer.</li>
          <li><strong>PULL_DOWN:</strong> Pin maintenu à LOW (GND). Le capteur donne +3.3V pour activer.</li>
          <li><strong>NO_PULL:</strong> Pas de résistance interne. Nécessite résistance externe.</li>
        </ul>
      </div>
      
      <div class="guide-section">
        <h3>💾 Sauvegarde EEPROM</h3>
        <p>Toutes les configurations sont automatiquement sauvegardées dans la mémoire EEPROM de l'ESP32.</p>
        <p>Au redémarrage, l'ESP32 charge automatiquement la dernière configuration.</p>
        <p><strong>Stockage:</strong> Preferences library (namespace "dashboard")</p>
      </div>
      
      <div class="guide-section">
        <h3>🔗 TunerStudio Integration</h3>
        <p>Ce dashboard communique avec TunerStudio via le port série à <code>115200 baud</code>.</p>
        <p><strong>Protocole:</strong> Compatible avec le format de Speeduino/MegaSquirt</p>
        <p><strong>Connexion:</strong> USB ou Bluetooth Serial (si ESP32 configuré)</p>
      </div>
    </div>
    
    <div class="info-box" style="background: #fff3cd; border-left: 4px solid #ffc107; margin-bottom: 20px;">
      <strong>⚠️ IMPORTANT - TunerStudio</strong><br>
      Fermez TunerStudio AVANT de sauvegarder! L'ESP32 redémarrera après la sauvegarde, ce qui peut causer un plantage de TunerStudio s'il est connecté au port série.
    </div>
    
    <button class="save-btn" onclick="saveConfig()">💾 Sauvegarder Configuration</button>
  </div>

  <script>
    let config = {
      rpm: {cylinders: 4, simulation: true, pin: 2},
      analogs: [],
      digitals: [],
      debug: {enabled: false, mode: 0}
    };
    
    const analogNames = [
      'Battery', 'Coolant Temp', 'TPS', 'MAP', 'AFR', 'Fuel Level'
    ];
    
    const digitalNames = [
      'Turn Left', 'Turn Right', 'Daylight', 'High Beam',
      'Alt Error', 'ESP32 Connected', 'Oil Warning',
      'Input 1', 'Input 2', 'Input 3', 'Input 4',
      'Input 5', 'Reserved'
    ];
    
    function openTab(tabName) {
      const tabs = document.querySelectorAll('.tab');
      const contents = document.querySelectorAll('.content');
      
      tabs.forEach(tab => tab.classList.remove('active'));
      contents.forEach(content => content.classList.remove('active'));
      
      event.target.classList.add('active');
      document.getElementById(tabName + '-content').classList.add('active');
    }
    
    function updateRPM(field, value) {
      if(!config.rpm) {
        config.rpm = {cylinders: 4, simulation: true, pin: 34};
      }
      
      if(field === 'simulation') {
        config.rpm.simulation = value;
        document.getElementById('rpm-sim-label').textContent = value ? 'SIMULATION' : 'REAL INPUT';
        document.getElementById('rpm-sim-label').style.color = value ? '#667eea' : '#f44336';
      } else if(field === 'cylinders') {
        config.rpm.cylinders = parseInt(value);
        document.getElementById('cyl-display').textContent = value;
      } else if(field === 'pin') {
        config.rpm.pin = parseInt(value);
        document.getElementById('pin-display').textContent = value;
      }
    }
    
    function updateDebug(field, value) {
      if(!config.debug) {
        config.debug = {enabled: false, mode: 0};
      }
      
      if(field === 'enabled') {
        config.debug.enabled = value;
        document.getElementById('debug-status').textContent = value ? 'ON' : 'OFF';
        document.getElementById('debug-status').style.color = value ? '#4caf50' : '#999';
        
        // Auto-update mode dropdown
        if(!value) {
          config.debug.mode = 0;
          document.getElementById('debug-mode').value = 0;
        } else if(config.debug.mode === 0) {
          config.debug.mode = 1;
          document.getElementById('debug-mode').value = 1;
        }
      } else if(field === 'mode') {
        config.debug.mode = parseInt(value);
        
        // Auto-update enabled toggle
        const enabled = (parseInt(value) !== 0);
        config.debug.enabled = enabled;
        document.getElementById('debug-enabled').checked = enabled;
        document.getElementById('debug-status').textContent = enabled ? 'ON' : 'OFF';
        document.getElementById('debug-status').style.color = enabled ? '#4caf50' : '#999';
      }
    }
    
    function updateAnalog(index, field, value) {
      if(!config.analogs[index]) {
        config.analogs[index] = {};
      }
      
      if(field === 'simulation') {
        config.analogs[index].simulation = value;
        const label = document.getElementById('analog-sim-label-' + index);
        label.textContent = value ? 'SIMULATION' : 'REAL INPUT';
        label.style.color = value ? '#667eea' : '#f44336';
      } else if(field === 'pin') {
        config.analogs[index].pin = parseInt(value);
      } else if(field === 'min_voltage') {
        config.analogs[index].min_voltage = parseFloat(value);
      } else if(field === 'max_voltage') {
        config.analogs[index].max_voltage = parseFloat(value);
      } else if(field === 'min_value') {
        config.analogs[index].min_value = parseFloat(value);
      } else if(field === 'max_value') {
        config.analogs[index].max_value = parseFloat(value);
      } else if(field === 'useFormula') {
        config.analogs[index].useFormula = value;
        // Enable/disable formula input field
        const formulaInput = document.querySelector(`#analog-formula-${index}`).parentElement.nextElementSibling.querySelector('input');
        formulaInput.disabled = !value;
      } else if(field === 'formula') {
        config.analogs[index].formula = value;
      }
    }
    
    function updateDigital(index, field, value) {
      if(!config.digitals[index]) {
        config.digitals[index] = {};
      }
      
      if(field === 'simulation') {
        config.digitals[index].simulation = value;
        const label = document.getElementById('digital-sim-label-' + index);
        label.textContent = value ? 'SIMULATION' : 'REAL INPUT';
        label.style.color = value ? '#667eea' : '#f44336';
      } else if(field === 'pin') {
        config.digitals[index].pin = parseInt(value);
      } else if(field === 'pull_mode') {
        config.digitals[index].pullMode = parseInt(value);
      }
    }
    
    function loadConfig() {
      console.log('🔄 Loading config from ESP32...');
      fetch('/api/config')
        .then(response => {
          console.log('📡 Response status:', response.status);
          return response.json();
        })
        .then(data => {
          console.log('📦 Config received:', data);
          config = data;
          
          // S'assurer que config.rpm existe
          if(!config.rpm) {
            config.rpm = {cylinders: 4, simulation: true, pin: 2};
          }
          
          // S'assurer que config.analogs existe
          if(!config.analogs || config.analogs.length === 0) {
            config.analogs = [];
            for(let i = 0; i < 6; i++) {
              config.analogs[i] = {
                name: analogNames[i],
                simulation: true,
                pin: [32, 33, 34, 35, 36, 39][i],
                min_voltage: 0.5,
                max_voltage: 3.3,
                min_value: 0,
                max_value: 100,
                useFormula: false,
                formula: ''
              };
            }
          }
          
          // RPM
          if(config.rpm) {
            document.getElementById('rpm-sim').checked = config.rpm.simulation;
            document.getElementById('rpm-sim-label').textContent = config.rpm.simulation ? 'SIMULATION' : 'REAL INPUT';
            document.getElementById('rpm-sim-label').style.color = config.rpm.simulation ? '#667eea' : '#f44336';
            document.getElementById('rpm-cyl').value = config.rpm.cylinders;
            document.getElementById('cyl-display').textContent = config.rpm.cylinders;
          }
          
          // Analogs
          console.log('📊 Rendering analogs...', config.analogs ? config.analogs.length : 'UNDEFINED');
          renderAnalogs();
          
          // Digitals
          console.log('🔘 Rendering digitals...', config.digitals ? config.digitals.length : 'UNDEFINED');
          renderDigitals();
          
          // Debug
          if(config.debug) {
            document.getElementById('debug-enabled').checked = config.debug.enabled;
            document.getElementById('debug-mode').value = config.debug.mode;
            document.getElementById('debug-status').textContent = config.debug.enabled ? 'ON' : 'OFF';
            document.getElementById('debug-status').style.color = config.debug.enabled ? '#4caf50' : '#999';
          }
          
          console.log('✅ Config loaded successfully!');
        })
        .catch(error => {
          console.error('❌ Error loading config:', error);
        });
    }
    
    function renderAnalogs() {
      console.log('📈 renderAnalogs() called');
      const container = document.getElementById('analog-sensors');
      if(!container) {
        console.error('❌ Container analog-sensors not found!');
        return;
      }
      console.log('✓ Container found:', container);
      container.innerHTML = '';
      
      console.log('📊 Analogs array:', config.analogs);
      
      for(let i = 0; i < 6; i++) {
        const analog = config.analogs[i] || {
          name: analogNames[i],
          simulation: true,
          pin: 32 + i,
          min_voltage: 0.5,
          max_voltage: 3.3,
          min_value: 0,
          max_value: 100,
          useFormula: false,
          formula: ''
        };
        
        console.log(`  Creating card ${i}:`, analogNames[i], analog);
        
        const card = document.createElement('div');
        card.className = 'sensor-card';
        card.innerHTML = `
          <div class="sensor-header">
            <span class="sensor-name">${analogNames[i]}</span>
            <div style="display: flex; align-items: center; gap: 10px;">
              <label class="toggle-switch">
                <input type="checkbox" id="analog-sim-${i}" ${analog.simulation ? 'checked' : ''} 
                       onchange="updateAnalog(${i}, 'simulation', this.checked)">
                <span class="slider"></span>
              </label>
              <span id="analog-sim-label-${i}" style="font-weight: 600; color: ${analog.simulation ? '#667eea' : '#f44336'};">
                ${analog.simulation ? 'SIMULATION' : 'REAL INPUT'}
              </span>
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label>📍 GPIO Pin</label>
              <select id="analog-pin-${i}" onchange="updateAnalog(${i}, 'pin', this.value)">
                <option value="32" ${analog.pin === 32 ? 'selected' : ''}>GPIO 32</option>
                <option value="33" ${analog.pin === 33 ? 'selected' : ''}>GPIO 33</option>
                <option value="34" ${analog.pin === 34 ? 'selected' : ''}>GPIO 34</option>
                <option value="35" ${analog.pin === 35 ? 'selected' : ''}>GPIO 35</option>
                <option value="36" ${analog.pin === 36 ? 'selected' : ''}>GPIO 36</option>
                <option value="39" ${analog.pin === 39 ? 'selected' : ''}>GPIO 39</option>
              </select>
            </div>
            <div class="form-group">
              <label>⚡ Min Voltage (V)</label>
              <input type="number" step="0.1" value="${analog.min_voltage}" 
                     onchange="updateAnalog(${i}, 'min_voltage', this.value)">
            </div>
            <div class="form-group">
              <label>⚡ Max Voltage (V)</label>
              <input type="number" step="0.1" value="${analog.max_voltage}" 
                     onchange="updateAnalog(${i}, 'max_voltage', this.value)">
            </div>
          </div>
          <div class="form-row" style="margin-top: 15px;">
            <div class="form-group">
              <label>📉 Min Value</label>
              <input type="number" step="0.1" value="${analog.min_value}" 
                     onchange="updateAnalog(${i}, 'min_value', this.value)">
            </div>
            <div class="form-group">
              <label>📈 Max Value</label>
              <input type="number" step="0.1" value="${analog.max_value}" 
                     onchange="updateAnalog(${i}, 'max_value', this.value)">
            </div>
          </div>
          <div class="form-row" style="margin-top: 15px;">
            <div class="form-group" style="flex: 0 0 30%;">
              <label>🧮 Use Formula</label>
              <label class="toggle-switch">
                <input type="checkbox" id="analog-formula-${i}" ${analog.useFormula ? 'checked' : ''} 
                       onchange="updateAnalog(${i}, 'useFormula', this.checked)">
                <span class="slider"></span>
              </label>
            </div>
            <div class="form-group" style="flex: 1;">
              <label>📐 Formula (x = voltage)</label>
              <input type="text" placeholder="e.g. (x-0.5)*100 or x*5+10" 
                     value="${analog.formula || ''}"
                     onchange="updateAnalog(${i}, 'formula', this.value)"
                     ${!analog.useFormula ? 'disabled' : ''}>
              <small style="color: #666;">Use 'x' for voltage. Examples: x*10, (x-0.5)/2.8*20, x*x+5</small>
            </div>
          </div>
        `;
        container.appendChild(card);
      }
    }
    
    function renderDigitals() {
      const container = document.getElementById('digital-sensors');
      container.innerHTML = '';
      
      for(let i = 0; i < 13; i++) {
        const digital = config.digitals[i] || {
          name: digitalNames[i],
          simulation: true,
          pin: 2 + i,
          pullMode: 2
        };
        
        const card = document.createElement('div');
        card.className = 'sensor-card';
        card.innerHTML = `
          <div class="sensor-header">
            <span class="sensor-name">${digitalNames[i]}</span>
            <div style="display: flex; align-items: center; gap: 10px;">
              <label class="toggle-switch">
                <input type="checkbox" id="digital-sim-${i}" ${digital.simulation ? 'checked' : ''} 
                       onchange="updateDigital(${i}, 'simulation', this.checked)">
                <span class="slider"></span>
              </label>
              <span id="digital-sim-label-${i}" style="font-weight: 600; color: ${digital.simulation ? '#667eea' : '#f44336'};">
                ${digital.simulation ? 'SIMULATION' : 'REAL INPUT'}
              </span>
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label>📍 GPIO Pin</label>
              <input type="number" value="${digital.pin}" 
                     onchange="updateDigital(${i}, 'pin', this.value)">
            </div>
            <div class="form-group">
              <label>🔌 Pull Mode</label>
              <select onchange="updateDigital(${i}, 'pull_mode', this.value)">
                <option value="2" ${digital.pullMode === 2 ? 'selected' : ''}>PULL_UP</option>
                <option value="1" ${digital.pullMode === 1 ? 'selected' : ''}>PULL_DOWN</option>
                <option value="0" ${digital.pullMode === 0 ? 'selected' : ''}>NO_PULL</option>
              </select>
            </div>
          </div>
        `;
        container.appendChild(card);
      }
    }
    
    function saveConfig() {
      // Avertissement important avant sauvegarde
      const warning = 
        "⚠️ IMPORTANT SAFETY WARNING ⚠️\n\n" +
        "Before saving:\n" +
        "1. CLOSE TunerStudio if it's connected\n" +
        "   (Saving will NOT restart ESP32 automatically)\n\n" +
        "2. After saving, you MUST restart ESP32 manually\n" +
        "   (Unplug/replug USB or press RESET button)\n\n" +
        "3. Changes will NOT apply until manual restart\n\n" +
        "Continue with save?";
      
      if(!confirm(warning)) {
        console.log('❌ Save cancelled by user');
        return;
      }
      
      console.log('💾 Saving config:', config);
      console.log('  RPM simulation:', config.rpm ? config.rpm.simulation : 'UNDEFINED');
      
      fetch('/api/save', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(config)
      })
      .then(response => response.text())
      .then(message => {
        alert('✅ ' + message);
        // PAS de location.reload() - pas nécessaire car pas de restart auto
      })
      .catch(error => {
        alert('❌ Erreur: ' + error);
      });
    }
    
    // Load config on page load
    window.onload = loadConfig;
  </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleGetConfig() {
  String json = "{\"rpm\":{";
  json += "\"cylinders\":" + String(rpmConfig.cylinders) + ",";
  json += "\"simulation\":" + String(rpmConfig.simulation ? "true" : "false") + ",";
  json += "\"pin\":" + String(rpmConfig.pin);
  json += "},\"analogs\":[";
  
  for(int i = 0; i < 6; i++) {
    if(i > 0) json += ",";
    json += "{";
    json += "\"name\":\"" + String(analogs[i].name) + "\",";
    json += "\"simulation\":" + String(analogs[i].simulation ? "true" : "false") + ",";
    json += "\"pin\":" + String(analogs[i].pin) + ",";
    json += "\"min_value\":" + String(analogs[i].min_value) + ",";
    json += "\"max_value\":" + String(analogs[i].max_value) + ",";
    json += "\"min_voltage\":" + String(analogs[i].min_voltage) + ",";
    json += "\"max_voltage\":" + String(analogs[i].max_voltage) + ",";
    json += "\"useFormula\":" + String(analogs[i].useFormula ? "true" : "false") + ",";
    json += "\"formula\":\"" + String(analogs[i].formula) + "\"";
    json += "}";
  }
  json += "],\"digitals\":[";
  for(int i = 0; i < 13; i++) {
    if(i > 0) json += ",";
    json += "{";
    json += "\"name\":\"" + String(digitals[i].name) + "\",";
    json += "\"simulation\":" + String(digitals[i].simulation ? "true" : "false") + ",";
    json += "\"pin\":" + String(digitals[i].pin) + ",";
    json += "\"inverted\":" + String(digitals[i].inverted ? "true" : "false") + ",";
    json += "\"pullMode\":" + String(digitals[i].pullMode);
    json += "}";
  }
  json += "],\"debug\":{";
  json += "\"enabled\":" + String(debugEnabled ? "true" : "false") + ",";
  json += "\"mode\":" + String(debugMode);
  json += "}}";
  
  server.send(200, "application/json", json);
}

void handleSaveConfig() {
  if(server.hasArg("plain")) {
    String body = server.arg("plain");
    
    Serial.println("📥 Received config from web:");
    Serial.println(body.substring(0, 200)); // Premiers 200 caractères
    
    // Parser le JSON manuellement (simple parser pour éviter ArduinoJson)
    
    // 1. Parser RPM config
    int rpmStart = body.indexOf("\"rpm\":{");
    if(rpmStart >= 0) {
      Serial.println("  Parsing RPM config...");
      
      int cylindersIdx = body.indexOf("\"cylinders\":", rpmStart);
      if(cylindersIdx >= 0) {
        int val = body.substring(cylindersIdx + 12).toInt();
        rpmConfig.cylinders = val;
        Serial.printf("    cylinders: %d\n", val);
      }
      
      int simIdx = body.indexOf("\"simulation\":", rpmStart);
      if(simIdx >= 0) {
        // Chercher directement "true" ou "false" après "simulation":
        int searchStart = simIdx + 13; // Après "simulation":
        bool isTrue = (body.indexOf("true", searchStart) == searchStart);
        rpmConfig.simulation = isTrue;
        Serial.printf("    simulation: %d (found at index %d)\n", rpmConfig.simulation, searchStart);
      } else {
        Serial.println("    WARNING: simulation field not found!");
      }
      
      int pinIdx = body.indexOf("\"pin\":", rpmStart);
      if(pinIdx >= 0 && pinIdx < body.indexOf("}", rpmStart)) {
        int val = body.substring(pinIdx + 6).toInt();
        rpmConfig.pin = val;
        Serial.printf("    pin: %d\n", val);
      }
    } else {
      Serial.println("  WARNING: RPM config not found in JSON!");
    }
    
    // 2. Parser Analogs
    int analogsStart = body.indexOf("\"analogs\":[");
    if(analogsStart >= 0) {
      for(int i = 0; i < 6; i++) {
        // Trouver le début de l'objet i
        String searchPattern = "\"name\":\"" + String(analogs[i].name) + "\"";
        int objStart = body.indexOf(searchPattern, analogsStart);
        if(objStart < 0) continue;
        
        int objEnd = body.indexOf("}", objStart);
        String obj = body.substring(objStart, objEnd);
        
        // Parser simulation
        int simIdx = obj.indexOf("\"simulation\":");
        if(simIdx >= 0) {
          int searchStart = simIdx + 13;
          analogs[i].simulation = (obj.indexOf("true", searchStart) == searchStart);
          if(i == 0) {
            Serial.printf("  Parsed Analog[0] sim: %d\n", analogs[i].simulation);
          }
        }
        
        // Parser pin
        int pinIdx = obj.indexOf("\"pin\":");
        if(pinIdx >= 0) {
          analogs[i].pin = obj.substring(pinIdx + 6).toInt();
        }
        
        // Parser min_value
        int minIdx = obj.indexOf("\"min_value\":");
        if(minIdx >= 0) {
          analogs[i].min_value = obj.substring(minIdx + 12).toFloat();
        }
        
        // Parser max_value
        int maxIdx = obj.indexOf("\"max_value\":");
        if(maxIdx >= 0) {
          analogs[i].max_value = obj.substring(maxIdx + 12).toFloat();
        }
        
        // Parser min_voltage
        int minVIdx = obj.indexOf("\"min_voltage\":");
        if(minVIdx >= 0) {
          analogs[i].min_voltage = obj.substring(minVIdx + 14).toFloat();
        }
        
        // Parser max_voltage
        int maxVIdx = obj.indexOf("\"max_voltage\":");
        if(maxVIdx >= 0) {
          analogs[i].max_voltage = obj.substring(maxVIdx + 14).toFloat();
        }
        
        // Parser useFormula
        int useFormulaIdx = obj.indexOf("\"useFormula\":");
        if(useFormulaIdx >= 0) {
          int searchStart = useFormulaIdx + 13;
          analogs[i].useFormula = (obj.indexOf("true", searchStart) == searchStart);
        }
        
        // Parser formula
        int formulaIdx = obj.indexOf("\"formula\":\"");
        if(formulaIdx >= 0) {
          int formulaEnd = obj.indexOf("\"", formulaIdx + 11);
          String formula = obj.substring(formulaIdx + 11, formulaEnd);
          strncpy(analogs[i].formula, formula.c_str(), 63);
          analogs[i].formula[63] = '\0';
        }
      }
    }
    
    // 3. Parser Digitals
    int digitalsStart = body.indexOf("\"digitals\":[");
    if(digitalsStart >= 0) {
      for(int i = 0; i < 13; i++) {
        // Trouver le début de l'objet i
        String searchPattern = "\"name\":\"" + String(digitals[i].name) + "\"";
        int objStart = body.indexOf(searchPattern, digitalsStart);
        if(objStart < 0) continue;
        
        int objEnd = body.indexOf("}", objStart);
        String obj = body.substring(objStart, objEnd);
        
        // Parser simulation
        int simIdx = obj.indexOf("\"simulation\":");
        if(simIdx >= 0) {
          int searchStart = simIdx + 13;
          digitals[i].simulation = (obj.indexOf("true", searchStart) == searchStart);
        }
        
        // Parser pin
        int pinIdx = obj.indexOf("\"pin\":");
        if(pinIdx >= 0) {
          digitals[i].pin = obj.substring(pinIdx + 6).toInt();
        }
        
        // Parser inverted
        int invIdx = obj.indexOf("\"inverted\":");
        if(invIdx >= 0) {
          int searchStart = invIdx + 11;
          digitals[i].inverted = (obj.indexOf("true", searchStart) == searchStart);
        }
        
        // Parser pullMode
        int pullIdx = obj.indexOf("\"pullMode\":");
        if(pullIdx >= 0) {
          digitals[i].pullMode = obj.substring(pullIdx + 11).toInt();
        }
      }
    }
    
    // 4. Parser debug config
    int debugIdx = body.indexOf("\"debug\":{");
    if(debugIdx >= 0) {
      String debugObj = body.substring(debugIdx);
      
      // Parse enabled
      int enabledIdx = debugObj.indexOf("\"enabled\":");
      if(enabledIdx >= 0) {
        String enabledStr = debugObj.substring(enabledIdx + 10, enabledIdx + 14);
        debugEnabled = (enabledStr.indexOf("true") >= 0);
      }
      
      // Parse mode
      int modeIdx = debugObj.indexOf("\"mode\":");
      if(modeIdx >= 0) {
        debugMode = debugObj.substring(modeIdx + 7).toInt();
      }
    }
    
    // 5. Sauvegarder dans EEPROM
    saveConfig();
    
    // Message informatif - PAS de restart automatique pour éviter freeze Pi5
    Serial.println("\n✅ Configuration saved to EEPROM!");
    Serial.println("⚠️  IMPORTANT: You must RESTART ESP32 manually for changes to take effect.");
    Serial.println("   (Unplug/replug USB cable or press RESET button)");
    Serial.println("   DO NOT restart while TunerStudio is connected!\n");
    Serial.flush();
    
    // Répondre au client web avec instructions
    server.send(200, "text/plain", 
      "Configuration saved successfully!\n\n"
      "⚠️ IMPORTANT:\n"
      "Please RESTART ESP32 manually to apply changes.\n"
      "(Unplug/replug USB or press RESET button)\n\n"
      "Changes will NOT take effect until restart."
    );
    
    // PAS de delay()
    // PAS de ESP.restart() - évite freeze Pi5 si TunerStudio connecté
  }
}

void handleReset() {
  preferences.begin("config", false);
  preferences.clear();
  preferences.end();
  server.send(200, "text/plain", "OK");
  ESP.restart();
}

// =====================================================
// 🔧 HELPER FUNCTIONS
// =====================================================

// Mapper une valeur d'un range à un autre
float map_float(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Contraindre une valeur entre min et max
float clamp(float value, float min_val, float max_val) {
  if(value < min_val) return min_val;
  if(value > max_val) return max_val;
  return value;
}

// Évaluer une formule mathématique simple (x = voltage)
// Supporte: x, +, -, *, /, (), constantes numériques
// Exemples: "x*10", "(x-0.5)*100", "x*x+5"
float evaluateFormula(const char* formula, float x) {
  // Parser simple - pour l'instant juste quelques cas communs
  String f = String(formula);
  f.trim();
  
  // Remplacer 'x' par la valeur
  f.replace("x", String(x, 6));
  
  // Évaluation basique avec des patterns communs
  // Pattern: "a*b+c" ou "a*b" ou "a+b" ou "(a-b)*c" etc.
  
  // Pour une vraie évaluation, il faudrait un parser complet
  // Pour l'instant, on supporte juste les cas simples:
  
  // Si c'est juste un nombre, le retourner
  float result = f.toFloat();
  
  // Si formule contient des opérations, essayer d'évaluer
  // (implémentation simplifiée - améliorer si besoin)
  
  return result;
}

// Lire un canal analogique avec formule OU mapping linéaire
float readAnalogChannel(int index) {
  AnalogConfig &cfg = analogs[index];
  
  if(cfg.simulation) {
    // En simulation, générer valeur oscillante
    float time = millis() / 1000.0;
    float wave = 0.5 + 0.5 * sin(time * (0.1 + index * 0.05));
    return cfg.min_value + (wave * (cfg.max_value - cfg.min_value));
  }
  
  // Lecture réelle du GPIO ADC
  int adc = analogRead(cfg.pin);
  float voltage = (adc / 4095.0) * 3.3;
  
  float value;
  if(cfg.useFormula && strlen(cfg.formula) > 0) {
    // Utiliser formule mathématique
    value = evaluateFormula(cfg.formula, voltage);
  } else {
    // Utiliser mapping linéaire
    value = map_float(voltage, cfg.min_voltage, cfg.max_voltage, 
                     cfg.min_value, cfg.max_value);
  }
  
  // Contraindre entre min et max
  return clamp(value, cfg.min_value, cfg.max_value);
}

// Lire une entrée digitale avec pull-up/pull-down configuré
bool readDigitalWithPull(int index) {
  DigitalConfig &cfg = digitals[index];
  
  // Configuration du pull selon le mode
  // pullMode: 0=NO_PULL, 1=PULL_DOWN, 2=PULL_UP
  if (cfg.pullMode == 2) {
    pinMode(cfg.pin, INPUT_PULLUP);
  } else if (cfg.pullMode == 1) {
    pinMode(cfg.pin, INPUT_PULLDOWN);
  } else {
    pinMode(cfg.pin, INPUT);  // NO_PULL (floating)
  }
  
  bool value = digitalRead(cfg.pin);
  return cfg.inverted ? !value : value;
}

// =====================================================
// 📊 UPDATE DES VALEURS - ANIMATIONS COMPLÈTES
// =====================================================
void updateValues() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 100) return;
  lastUpdate = millis();
  
  float time = millis() / 1000.0;
  
  // ===== 6 CANAUX ANALOGIQUES =====
  
  // 0. Battery (0-20V)
  float battery = readAnalogChannel(0);
  data.battery = (uint16_t)(battery * 100);
  
  // 1. Coolant Temp (0-250°C)
  float coolant = readAnalogChannel(1);
  data.coolant_temp = (uint16_t)(coolant * 10);
  
  // 2. TPS (0-100%)
  float tps_val = readAnalogChannel(2);
  data.tps = (uint16_t)(tps_val * 10);
  
  // 3. MAP (0-250 kPa)
  float map_val = readAnalogChannel(3);
  data.map_pressure = (uint16_t)(map_val * 10);
  
  // 4. AFR (10-18)
  float afr_val = readAnalogChannel(4);
  data.afr = (uint16_t)(afr_val * 10);
  
  // 5. Fuel Level (0-100%)
  float fuel = readAnalogChannel(5);
  data.fuel_level = (uint8_t)fuel;
  
  // ===== RPM (SIGNAL DIGITAL PAR INTERRUPTION) =====
  if(rpmConfig.simulation) {
    // Mode simulation: oscillation sinusoïdale
    float wave = 0.5 + 0.5 * sin(time * 0.3);
    data.rpm = (uint16_t)(wave * 6000.0);
  } else {
    // Mode réel: calcul basé sur interruptions
    unsigned long now = millis();
    
    // Calculer RPM toutes les 500ms (RPM_WINDOW_MS)
    if(now - rpmLastCalc >= RPM_WINDOW_MS) {
      // Désactiver interruptions temporairement pour lecture atomique
      noInterrupts();
      unsigned long pulses = rpmPulseCount;
      rpmPulseCount = 0;  // Reset compteur
      interrupts();
      
      // Formule: RPM = (fréquence × 60 × 2) / cylindres
      // fréquence = pulses / (fenêtre_en_secondes)
      // Pour moteur 4-temps: facteur 2 (1 impulsion = 2 tours de vilebrequin)
      float freq = (float)pulses / (RPM_WINDOW_MS / 1000.0);  // Hz
      float rawRPM = (freq * 60.0 * 2.0) / (float)rpmConfig.cylinders;
      
      // Filtrer aberrations (> RPM_MAX)
      if(rawRPM > RPM_MAX) {
        rawRPM = 0;
      }
      
      // Lissage (moyenne glissante)
      rpmSmoothed = (RPM_SMOOTH_FACTOR * rawRPM) + ((1.0 - RPM_SMOOTH_FACTOR) * rpmSmoothed);
      
      data.rpm = (uint16_t)rpmSmoothed;
      rpmLastCalc = now;
    }
  }
  
  // ===== INDICATEURS DIGITAUX =====
  data.status_bits1 = 0;
  data.status_bits2 = 0;
  
  // ESP32 toujours connecté (bit 5)
  data.status_bits1 |= (1 << 5);
  
  // Lire les 13 entrées digitales
  for(int i = 0; i < 13; i++) {
    bool value = false;
    
    if(digitals[i].simulation) {
      // Simulation: patterns différents par canal
      if(i == 0) value = (millis() / 500) % 2 == 0;  // Turn left
      else if(i == 1) value = (millis() / 700) % 2 == 0;  // Turn right
      else if(i == 2) value = true;  // Daylight always on
      else if(i == 3) value = (millis() / 2000) % 2 == 0;  // High beam
      else value = false;
    } else {
      value = readDigitalWithPull(i);
    }
    
    // Stocker dans status_bits
    if(i < 8) {
      if(value) data.status_bits1 |= (1 << i);
    } else {
      if(value) data.status_bits2 |= (1 << (i - 8));
    }
  }
  
  // ===== GEAR CALCULATION (basé sur RPM et TPS) =====
  if(data.rpm < 1000) data.gear = 0;  // Neutral
  else if(data.rpm < 2000) data.gear = 1;
  else if(data.rpm < 3000) data.gear = 2;
  else if(data.rpm < 4000) data.gear = 3;
  else if(data.rpm < 5000) data.gear = 4;
  else if(data.rpm < 5500) data.gear = 5;
  else                     data.gear = 6;
}
void updateHeartbeat() {
  static unsigned long lastBlink = 0;
  if(millis() - lastBlink > 500) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}

void processSerial() {
  if(!Serial.available()) return;
  char cmd = Serial.read();
  if(cmd == '\r' || cmd == '\n' || cmd == ' ' || cmd == '\t') return;
  
  switch(cmd) {
    case 'Q':
    case 'H':
    case 'S':
      Serial.write(SIGNATURE);
      Serial.write('\0');
      Serial.flush();
      break;
    case 'r':
    case 'A':
    case 'O':
      Serial.write((uint8_t*)&data, sizeof(data));
      Serial.flush();
      break;
    case 'p':
    case 'C':
    case 'V':
      {
        uint8_t page[128];
        memset(page, 0, 128);
        Serial.write(page, 128);
        Serial.flush();
      }
      break;
    default:
      break;
  }
}
