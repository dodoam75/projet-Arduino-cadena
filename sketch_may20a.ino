// === BIBLIOTHÈQUES ===
#include <SPI.h>                   // Pour la communication SPI avec le lecteur RFID
#include <MFRC522.h>               // Bibliothèque pour gérer le module RFID RC522
#include <Wire.h>                  // Pour la communication I2C (utilisée avec l'écran LCD)
#include <LiquidCrystal_I2C.h>     // Pour gérer l'écran LCD via I2C
#include <Keypad.h>                // Pour gérer un clavier matriciel

// === RFID ===
#define SS_PIN 10                  // Pin "Slave Select" du module RFID
#define RST_PIN 9                  // Pin "Reset" du module RFID
MFRC522 rfid(SS_PIN, RST_PIN);     // Création de l’objet RFID

// === BADGES RFID AUTORISÉS ===
#define MAX_BADGES 9                       // Nombre max de badges mémorisables
byte authorizedBadges[MAX_BADGES][10];    // Tableau pour stocker les UID (identifiants uniques) des badges
int badgeCount = 0;                        // Nombre de badges enregistrés

// === LCD I2C ===
LiquidCrystal_I2C lcd(0x27, 16, 2);        // Adresse I2C 0x27, écran 16 colonnes / 2 lignes

// === CLAVIER MATRICIEL ===
const byte ROWS = 4;                       // 4 lignes
const byte COLS = 4;                       // 4 colonnes
char keys[ROWS][COLS] = {                  // Disposition des touches
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {A0, A1, A2, A3};     // Lignes connectées aux broches analogiques
byte colPins[COLS] = {3, 4, 5, 6};         // Colonnes connectées aux broches digitales
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS); // Création de l’objet clavier

// === CODES D’ACCÈS ===
String code = "";                // Code en cours de saisie
String correctCode = "A380";     // Code normal autorisé
String adminCode = "12345";      // Code admin pour ajouter des badges
bool waitingForNewBadge = false; // Indique si on est en mode "ajout de badge"

// === LEDS ET RELAIS ===
const int greenLedPin = 8;       // LED verte (succès)
const int redLedPin = 7;         // LED rouge (erreur)
const int relayPin = 2;          // Relais pour ouvrir la porte

// === SETUP INITIAL ===
void setup() {
  Serial.begin(9600);            // Communication série pour le débogage
  SPI.begin();                   // Initialisation SPI (nécessaire pour RFID)
  rfid.PCD_Init();               // Initialisation du lecteur RFID

  lcd.init();                    // Initialisation de l'écran LCD
  lcd.backlight();               // Active le rétroéclairage
  lcd.setCursor(0, 0);           
  lcd.print("Badge ou code :");  // Message d’accueil

  // Configuration des LEDs et du relais
  pinMode(greenLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(relayPin, OUTPUT);

  // État initial : LED rouge allumée (porte fermée), relais désactivé
  digitalWrite(redLedPin, HIGH);
  digitalWrite(greenLedPin, LOW);
  digitalWrite(relayPin, HIGH);
}

// === BOUCLE PRINCIPALE ===
void loop() {
  if (waitingForNewBadge) {      // Si l’utilisateur a entré le code admin
    addNewBadge();               // On lit les nouveaux badges
    checkExitBadgeMode();        // On vérifie s’il souhaite quitter ce mode
  } else {
    checkRFID();                 // Sinon, on attend un badge RFID
    checkKeypad();               // ... ou une saisie clavier
  }
}

// === FONCTION : lecture badge RFID ===
void checkRFID() {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    if (isAuthorized(rfid.uid.uidByte)) {
      lcd.clear();
      lcd.print("Porte ouverte"); // Badge reconnu
      signalSuccess();            // Allume LED verte, active relais
    } else {
      lcd.clear();
      lcd.print("Badge inconnu"); // Badge non reconnu
      signalError();              // Clignote LED rouge
    }

    delay(2000);
    lcd.clear();
    lcd.print("Badge ou code :");

    rfid.PICC_HaltA();            // Arrêt de la lecture RFID
    rfid.PCD_StopCrypto1();
  }
}

// === FONCTION : lecture clavier ===
void checkKeypad() {
  char key = keypad.getKey();    // Lit une touche
  if (key) {
    Serial.println(key);         // Affiche la touche dans le moniteur série

    if (key == '#') {            // Validation du code
      lcd.clear();
      if (code == correctCode) {
        lcd.print("Porte ouverte");
        signalSuccess();
      } else if (code == adminCode) {
        lcd.print("Presenter badge");
        lcd.setCursor(0, 1);
        lcd.print("* pour finir");
        waitingForNewBadge = true;
      } else {
        lcd.print("Code erroné");
        signalError();
      }
      delay(2000);
      if (!waitingForNewBadge) {
        lcd.clear();
        lcd.print("Badge ou code :");
      }
      code = "";                 // Réinitialise la saisie
    }

    else if (key == '*') {       // Suppression du code en cours
      code = "";
      lcd.clear();
      lcd.print("Code efface");
      delay(1000);
      lcd.clear();
      lcd.print("Badge ou code :");
    }

    else {                       // Saisie d'une touche (chiffre ou lettre)
      code += key;
      lcd.setCursor(0, 0);
      lcd.print("# pour valider");
      lcd.setCursor(0, 1);
      lcd.print("* pour supprimer");
      lcd.setCursor(0, 2);       // Affiche des étoiles pour masquer le code
      for (int i = 0; i < code.length(); i++) {
        lcd.print("*");
      }
    }
  }
}

// === FONCTION : quitter le mode d’ajout de badge ===
void checkExitBadgeMode() {
  char key = keypad.getKey();
  if (key == '*') {
    waitingForNewBadge = false;
    lcd.clear();
    lcd.print("Fin ajout badge");
    delay(2000);
    lcd.clear();
    lcd.print("Badge ou code :");
  }
}

// === FONCTION : vérifier si le badge est autorisé ===
bool isAuthorized(byte *uid) {
  for (int i = 0; i < badgeCount; i++) {
    bool match = true;
    for (int j = 0; j < 4; j++) {
      if (authorizedBadges[i][j] != uid[j]) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

// === FONCTION : signal visuel et ouverture relais ===
void signalSuccess() {
  digitalWrite(redLedPin, LOW);      // Éteint la LED rouge
  digitalWrite(greenLedPin, HIGH);   // Allume la LED verte
  digitalWrite(relayPin, LOW);       // Active le relais (porte ouverte)
  delay(10000);                      // Porte ouverte pendant 10 secondes
  digitalWrite(relayPin, HIGH);      // Referme la porte
  digitalWrite(greenLedPin, LOW);    // Éteint LED verte
  digitalWrite(redLedPin, HIGH);     // Rallume LED rouge
}

// === FONCTION : signal visuel d’erreur ===
void signalError() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(redLedPin, LOW);
    delay(300);
    digitalWrite(redLedPin, HIGH);
    delay(300);
  }
}

// === FONCTION : ajouter un badge RFID ===
void addNewBadge() {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    if (isAuthorized(rfid.uid.uidByte)) {
      lcd.clear();
      lcd.print("Badge deja ajoute");
    } else if (badgeCount < MAX_BADGES) {
      for (byte i = 0; i < 4; i++) {
        authorizedBadges[badgeCount][i] = rfid.uid.uidByte[i];
      }
      badgeCount++;
      lcd.clear();
      lcd.print("Badge ajoute");
    } else {
      lcd.clear();
      lcd.print("Limite atteinte");
    }

    delay(2000);
    lcd.setCursor(0, 0);
    lcd.print("Presenter badge");
    lcd.setCursor(0, 1);
    lcd.print("* pour finir");

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}
