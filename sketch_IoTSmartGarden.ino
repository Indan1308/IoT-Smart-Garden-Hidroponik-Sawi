#include <ESP8266WiFi.h>
#include "CTBot.h"

CTBot myBot;
CTBotReplyKeyboard Tbl;
CTBotInlineKeyboard TbPm1on, TbPm1off;

#define Pompa1 D1
#define ppm1 A0
#define ldr1 D0

#define POMPA_1_ON  "POMPA_1_ON"
#define POMPA_1_OFF "POMPA_1_OFF"
#define TIDAK       "Tidak"

int Pompa_1 = HIGH;

// Konfigurasi Wi-Fi & Bot
const char* ssid = "404 NOT Fond";
const char* pass = "Indanbaka4320";
const char* token = "8172354076:AAHKxOcNBZD14uWLWAHTDxkzpHqTSnp7GqM";

// Variabel Waktu untuk Non-Blocking Auto-Reconnect
unsigned long previousMillis = 0;
const long intervalReconnect = 10000; 

float tdsValue = 0;
float lux1 = 0;

// Variabel Fuzzy
float rule1,rule2,rule3,rule4,rule5,rule6,rule7,rule8,rule9;
float buruk1,gelap1,sedang1_ldr,sedang1_tds,sedang2,baik1,terang1;
float z1,z2,z3,z4,z5,z6,z7,z8,z9;

// --- FUNGSI MOVING AVERAGE FILTER UNTUK SENSOR ---
float bacaLDRFiltered() {
  const int numReadings = 5;
  long totalSum = 0;
  for (int i = 0; i < numReadings; i++) {
    totalSum += analogRead(ldr1);
    delay(2);
  }
  float avgRaw = (float)totalSum / numReadings;
  // Konversi ke Lux
  return (0.0023 * avgRaw * avgRaw + 1.575 * avgRaw - 9.1422);
}

float bacaTDSFiltered() {
  const int numReadings = 5;
  long totalSum = 0;
  for (int i = 0; i < numReadings; i++) {
    int sensorValue = analogRead(ppm1);
    float Voltage = sensorValue * 5.0 / 1024.0;
    float currentTDS = (133.42 * Voltage * Voltage * Voltage - 255.86 * Voltage * Voltage + 857.39 * Voltage) * 0.5;
    totalSum += currentTDS;
    delay(2);
  }
  return (float)totalSum / numReadings;
}
// ------------------------------------------------

// Fungsi untuk memproses sensor dan logika fuzzy
void bacaDanHitungSensor() {
  lux1 = bacaLDRFiltered();
  tdsValue = bacaTDSFiltered();

  // --- FUZZIFIKASI LDR ---
  if (lux1 <= 50) { terang1 = 1; sedang2 = 0; gelap1 = 0; }
  else if (lux1 > 50 && lux1 <= 127.5) { terang1 = (127.5 - lux1) / 77.5; sedang2 = (lux1 - 50) / 77.5; gelap1 = 0; }
  else if (lux1 > 127.5 && lux1 < 200) { terang1 = 0; sedang2 = (200 - lux1) / 72.5; gelap1 = (lux1 - 127.5) / 72.5; }
  else { terang1 = 0; sedang2 = 0; gelap1 = 1; }

  // --- FUZZIFIKASI TDS ---
  if (tdsValue <= 300) { buruk1 = 1; sedang1_tds = 0; baik1 = 0; }
  else if (tdsValue > 300 && tdsValue <= 650) { buruk1 = (650 - tdsValue) / 350.0; sedang1_tds = (tdsValue - 300) / 350.0; baik1 = 0; }
  else if (tdsValue > 650 && tdsValue <= 950) { buruk1 = 0; sedang1_tds = (1150 - tdsValue) / 450.0; baik1 = 0; }
  else if (tdsValue > 950 && tdsValue < 1200) { buruk1 = 0; sedang1_tds = 0; baik1 = (tdsValue - 950) / 250.0; }
  else { buruk1 = 0; sedang1_tds = 0; baik1 = 1; }

  // --- RULE BASE & INFERENSI ---
  rule1 = min(buruk1, gelap1); z1 = 0;
  rule2 = min(baik1, gelap1);  z2 = 100;
  rule3 = min(sedang1_tds, gelap1); z3 = 100;
  
  rule4 = min(buruk1, sedang2); z4 = 0;
  rule5 = min(baik1, sedang2);  z5 = 100;
  rule6 = min(sedang1_tds, sedang2); z6 = 100;
  
  rule7 = min(buruk1, terang1); z7 = 0;
  rule8 = min(baik1, terang1);  z8 = 100;
  rule9 = min(sedang1_tds, terang1); z9 = 100;
  
  // --- DEFUZIFIKASI ---
  float pembagi = (rule1 + rule2 + rule3 + rule4 + rule5 + rule6 + rule7 + rule8 + rule9);
  float hasilPompa = 0;
  if (pembagi != 0) {
    hasilPompa = ((rule1*z1)+(rule2*z2)+(rule3*z3)+(rule4*z4)+(rule5*z5)+(rule6*z6)+(rule7*z7)+(rule8*z8)+(rule9*z9)) / pembagi;
  }

  // Otomatisasi pompa berdasarkan hasil fuzzy
  if (hasilPompa < 50 && Pompa_1 == LOW) {
    digitalWrite(Pompa1, HIGH);
    Pompa_1 = HIGH;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(Pompa1, OUTPUT);
  pinMode(ldr1, INPUT);
  pinMode(ppm1, INPUT);
  digitalWrite(Pompa1, HIGH);

  WiFi.begin(ssid, pass);
  myBot.setTelegramToken(token);

  Serial.println("Memulai Koneksi...");
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (myBot.testConnection())
    Serial.println("\nKoneksi Ke Telegram BOT Berhasil!");
  else
    Serial.println("\nTidak Terkoneksi Ke Telegram BOT");

  // Inisialisasi Tombol Telegram
  Tbl.addButton("Pompa");
  Tbl.addRow();
  Tbl.addButton("Pompa ON");
  Tbl.addButton("Pompa OFF");
  Tbl.addRow();
  Tbl.addButton("Cek Status");
  Tbl.addButton("Cek Sensor");
  Tbl.enableResize();

  TbPm1on.addButton("✅ Ya", POMPA_1_ON, CTBotKeyboardButtonQuery);
  TbPm1on.addButton("❌ Tidak", TIDAK, CTBotKeyboardButtonQuery);

  TbPm1off.addButton("✅ Ya", POMPA_1_OFF, CTBotKeyboardButtonQuery);
  TbPm1off.addButton("❌ Tidak", TIDAK, CTBotKeyboardButtonQuery);
}

void loop() {
  // --- BAGIAN AUTO-RECONNECT WI-FI ---
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= intervalReconnect) {
    previousMillis = currentMillis;
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\nKoneksi Wi-Fi terputus! Mencoba menyambungkan kembali...");
      WiFi.disconnect();
      WiFi.begin(ssid, pass);
    }
  }
  // --------------------------------------------------

  // Panggil fungsi pembacaan sensor dan perhitungan fuzzy
  bacaDanHitungSensor();

  TBMessage msg;
  if (myBot.getNewMessage(msg)) {
    if (msg.text.equalsIgnoreCase("/start")) {
        myBot.sendMessage(msg.sender.id, "Halo...\nSelamat Datang di Smart Garden Hidroponik.\nGunakan tombol di bawah untuk mengontrol sistem 👇", Tbl);
    } 
    else if (msg.text.equalsIgnoreCase("Cek Status")) {
        //AMAN & Hemat Memori
        char bufferStatus[120];// menyiapkan wadah teks
        const char* stPompa = (Pompa_1 == LOW) ? "Pompa ON" : "Pompa OFF";
        const char* stNutrisi = (tdsValue <= 300) ? "Buruk" : ((tdsValue <= 950) ? "Sedang/Baik" : "Sangat Baik");
        
        snprintf(bufferStatus, sizeof(bufferStatus), "Status saat ini:\n\n👉 %s\n👉 Nutrisi: %s", stPompa, stNutrisi);
        myBot.sendMessage(msg.sender.id, bufferStatus, Tbl);
    } 
    else if (msg.text.equalsIgnoreCase("Cek Sensor")) {
        char bufferSensor[100];
        snprintf(bufferSensor, sizeof(bufferSensor), "Lux: %.2f\nTDS: %.2f ppm\nPompa: %d", lux1, tdsValue, Pompa_1);
        myBot.sendMessage(msg.sender.id, bufferSensor, Tbl);
    } 
    else if (msg.messageType == CTBotMessageQuery) {
        if (msg.callbackQueryData.equals(POMPA_1_ON)) {
            digitalWrite(Pompa1, LOW);
            Pompa_1 = LOW;
            myBot.sendMessage(msg.sender.id, "💡 Pompa Telah Dinyalakan");
        } else if (msg.callbackQueryData.equals(POMPA_1_OFF)) {
            digitalWrite(Pompa1, HIGH);
            Pompa_1 = HIGH;
            myBot.sendMessage(msg.sender.id, "🚫 Pompa Telah Dimatikan");
        }
    }
  }
}
