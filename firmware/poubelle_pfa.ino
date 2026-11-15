#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// --- Configuration des Broches ---
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int LED_ROUGE = 26;   
const int LED_VERTE = 27;   

// --- Paramètres Wi-Fi et Telegram ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// !!! METS TES INFOS TELEGRAM ICI !!!
#include "secrets.h"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Icônes de Batterie ---
byte p0[8] = {B01110, B10001, B10001, B10001, B10001, B10001, B10001, B11111}; 
byte p1[8] = {B01110, B10001, B10001, B10001, B10001, B10001, B11111, B11111}; 
byte p2[8] = {B01110, B10001, B10001, B10001, B11111, B11111, B11111, B11111}; 
byte p3[8] = {B01110, B10001, B11111, B11111, B11111, B11111, B11111, B11111}; 
byte p4[8] = {B01110, B11111, B11111, B11111, B11111, B11111, B11111, B11111}; 

int etatPoubelle = 0; 
unsigned long chrono = 0; 

const unsigned long TEMPS_COMPRESSION = 4000; 
const unsigned long TEMPS_REMONTEE = 4000;    
const unsigned long TEMPS_STOP = 10000;       

// --- Fonction d'envoi Telegram ---
void envoyerAlerte() {
  Serial.println("Envoi du message Telegram en cours...");
  String message = "⚠️ ALERTE PFA : La poubelle intelligente est PLEINE ! (Niveau critique atteint). Intervention requise.";
  if (bot.sendMessage(CHAT_ID, message, "")) {
    Serial.println("Message Telegram envoyé avec succès !");
  } else {
    Serial.println("Erreur lors de l'envoi du message.");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_ROUGE, OUTPUT);
  pinMode(LED_VERTE, OUTPUT);

  lcd.init();
  lcd.backlight();

  lcd.createChar(0, p0); lcd.createChar(1, p1); lcd.createChar(2, p2); lcd.createChar(3, p3); lcd.createChar(4, p4);

  // Sécurité requise pour l'API Telegram
  client.setInsecure();

  lcd.setCursor(0, 0); lcd.print("WiFi Connexion..");
  WiFi.begin(ssid, password);
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500); Serial.print("."); timeout++;
  }

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print("WiFi Connecte!");
    lcd.setCursor(0, 1); lcd.print(WiFi.localIP()); 
  } else {
    lcd.print("WiFi Error");
  }
  delay(3000); lcd.clear();
}

void loop() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  float distance_cm = (duration == 0) ? 400.0 : duration * 0.034 / 2;
  unsigned long maintenant = millis();

  // Affichage Batterie
  lcd.setCursor(15, 0); 
  if (distance_cm > 30) lcd.write(0);      
  else if (distance_cm > 25) lcd.write(1); 
  else if (distance_cm > 21) lcd.write(2); 
  else if (distance_cm > 18) lcd.write(3); 
  else lcd.write(4);                       

  // Logique Machine à États
  switch(etatPoubelle) {
    
    case 0: // EN ATTENTE
      digitalWrite(LED_ROUGE, LOW); digitalWrite(LED_VERTE, LOW);
      lcd.setCursor(0, 0); lcd.print("Niveau: "); lcd.print(distance_cm, 1); lcd.print(" cm ");
      lcd.setCursor(0, 1); lcd.print("En attente.     ");
      if (distance_cm <= 5.0) { etatPoubelle = 1; chrono = maintenant; }
      break;

    case 1: // COMPRESSION
      digitalWrite(LED_ROUGE, HIGH); digitalWrite(LED_VERTE, LOW);
      lcd.setCursor(0, 0); lcd.print("Action: Compress");
      lcd.setCursor(0, 1); lcd.print("Verrin en bas.. ");
      if (maintenant - chrono >= TEMPS_COMPRESSION) { etatPoubelle = 2; chrono = maintenant; }
      break;

    case 2: // REMONTÉE
      digitalWrite(LED_ROUGE, LOW); digitalWrite(LED_VERTE, HIGH);
      lcd.setCursor(0, 0); lcd.print("Action: Remontee");
      lcd.setCursor(0, 1); lcd.print("Retour piston.. ");
      if (maintenant - chrono >= TEMPS_REMONTEE) { etatPoubelle = 3; chrono = maintenant; }
      break;

    case 3: // ANALYSE (10 SECONDES)
      digitalWrite(LED_VERTE, LOW);
      lcd.setCursor(0, 0); lcd.print("Analyse niveau  ");
      lcd.setCursor(0, 1); lcd.print("STOP - Attente  ");

      if (distance_cm <= 17.0) {
        etatPoubelle = 4;
        envoyerAlerte(); // C'EST ICI QUE LE MESSAGE PART SUR TON TELEPHONE !
      } 
      else if (maintenant - chrono >= TEMPS_STOP) { etatPoubelle = 0; }
      break;

    case 4: // POUBELLE PLEINE
      digitalWrite(LED_ROUGE, LOW); digitalWrite(LED_VERTE, LOW);
      lcd.setCursor(0, 0); lcd.print("POUBELLE PLEINE!");
      lcd.setCursor(0, 1); lcd.print("Veuillez vider. ");
      if (distance_cm > 30.0) { etatPoubelle = 0; }
      break;
  }
  delay(100); 
}