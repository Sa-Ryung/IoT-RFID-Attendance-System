#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// ---------------- PIN SETUP ----------------
#define RST_PIN  D1
#define SS_PIN   D2
#define BUZZER   D0

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

// ---------------- WIFI ----------------
#define WIFI_SSID     "12345"
#define WIFI_PASSWORD "12345"

String sheet_url = "https://script.google.com/macros/s/AKfycbwSUf35B9YvV9D3iS0rkW6VIeFVkIgNTltD2JpqDCO9nU8gFpgdpCCa6m2PxYXtyWrx2w/exec";

// ---------------- LCD ----------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------------- MODE ----------------
int currentMode = 0;

// ---------------- Write/Update wizard state ----------------
// writeStep:
// 0  = idle
// 1  = waiting card for READ
// 2  = collecting ID
// 3  = collecting firstname
// 4  = collecting lastname
// 5  = collecting class
// 6  = collecting phone
// 7  = waiting card to WRITE
// 10 = waiting card to scan for UPDATE (shows current data)
// 11 = collecting new ID     (update wizard)
// 12 = collecting new firstname
// 13 = collecting new lastname
// 14 = collecting new class
// 15 = collecting new phone
// 16 = waiting card to WRITE updated data
// 99 = waiting card for DELETE
int writeStep = 0;

String w_id = "", w_firstname = "", w_lastname = "", w_class = "", w_phone = "";

// Stores UID bytes so we can re-auth the same card for update write
byte savedUid[10];
byte savedUidSize = 0;

//--------------------------------------------------
void setup() {
  Serial.begin(9600);

  Wire.begin(D3, D4);
  lcd.init();
  lcd.backlight();
  lcd.print("Initializing...");
  delay(1000);

  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  lcd.clear();
  lcd.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) delay(300);
  lcd.clear();
  lcd.print("WiFi Connected");
  delay(1000);

  pinMode(BUZZER, OUTPUT);

  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  SPI.begin();
  mfrc522.PCD_Init();

  printMenu();
}

//--------------------------------------------------
void printMenu() {
  Serial.println("\n=============================");
  Serial.println("   RFID Attendance System");
  Serial.println("=============================");
  Serial.println("Commands:");
  Serial.println("  ATTEND - Start attendance mode");
  Serial.println("  MGMT   - Enter card management");
  Serial.println("  EXIT   - Exit current mode");
  Serial.println("In MGMT mode:");
  Serial.println("  R - Read card");
  Serial.println("  W - Write new card");
  Serial.println("  U - Update existing card");
  Serial.println("  D - Delete card");
  Serial.println("=============================\n");
  lcd.clear();
  lcd.print("Send: ATTEND");
  lcd.setCursor(0, 1);
  lcd.print("   or MGMT");
}

//--------------------------------------------------
void returnToMgmt() {
  delay(1500);
  Serial.println("   R=Read  W=Write  U=Update  D=Delete  EXIT=back");
  lcd.clear();
  lcd.print("MGMT Mode");
  lcd.setCursor(0, 1);
  lcd.print("R/W/U/D");
}

//--------------------------------------------------
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    handleCommand(cmd);
    return;
  }

  // Attendance mode
  if (currentMode == 1) {
    if (!mfrc522.PICC_IsNewCardPresent()) return;
    if (!mfrc522.PICC_ReadCardSerial()) return;
    attendanceScan();
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }

  // Card management — waiting for card
  if (currentMode == 2) {
    bool waitingForCard = (writeStep == 1  ||
                           writeStep == 7  ||
                           writeStep == 10 ||
                           writeStep == 16 ||
                           writeStep == 99);
    if (!waitingForCard) return;
    if (!mfrc522.PICC_IsNewCardPresent()) return;
    if (!mfrc522.PICC_ReadCardSerial()) return;
    handleCardAction();
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
}

//--------------------------------------------------
void handleCommand(String cmd) {
  cmd.trim();

  String cmdUpper = cmd;
  cmdUpper.toUpperCase();

  // ── Mode switching ──
  if (cmdUpper == "ATTEND") {
    currentMode = 1; writeStep = 0;
    Serial.println(">> Attendance Mode ON — Scan a card");
    lcd.clear();
    lcd.print("Attendance Mode");
    lcd.setCursor(0, 1);
    lcd.print("Scan your card");
    return;
  }

  if (cmdUpper == "MGMT") {
    currentMode = 2; writeStep = 0;
    Serial.println(">> Card Management Mode");
    Serial.println("   R=Read  W=Write  U=Update  D=Delete  EXIT=back");
    lcd.clear();
    lcd.print("MGMT Mode");
    lcd.setCursor(0, 1);
    lcd.print("R/W/U/D");
    return;
  }

  if (cmdUpper == "EXIT") {
    currentMode = 0; writeStep = 0;
    w_id = w_firstname = w_lastname = w_class = w_phone = "";
    Serial.println(">> Exited.");
    printMenu();
    return;
  }

  // ── MGMT commands ──
  if (currentMode == 2) {

    if (cmdUpper == "R") {
      writeStep = 1;
      Serial.println(">> Scan a card to READ...");
      lcd.clear();
      lcd.print("Scan to READ");
      return;
    }

    if (cmdUpper == "D") {
      writeStep = 99;
      Serial.println(">> Scan a card to DELETE data...");
      lcd.clear();
      lcd.print("Scan to DELETE");
      return;
    }

    if (cmdUpper == "W") {
      writeStep = 2;
      Serial.println(">> Enter Student ID:");
      lcd.clear();
      lcd.print("Enter ID");
      lcd.setCursor(0, 1);
      lcd.print("via Serial");
      return;
    }

    if (cmdUpper == "U") {
      writeStep = 10;
      Serial.println(">> Scan the card you want to UPDATE...");
      lcd.clear();
      lcd.print("Scan to UPDATE");
      return;
    }

    // ── WRITE wizard: collect fields ──
    if (writeStep == 2) { w_id        = cmd; writeStep = 3; Serial.println(">> Enter First Name:");  return; }
    if (writeStep == 3) { w_firstname = cmd; writeStep = 4; Serial.println(">> Enter Last Name:");   return; }
    if (writeStep == 4) { w_lastname  = cmd; writeStep = 5; Serial.println(">> Enter Class:");       return; }
    if (writeStep == 5) { w_class     = cmd; writeStep = 6; Serial.println(">> Enter Phone:");       return; }
    if (writeStep == 6) {
      w_phone   = cmd;
      writeStep = 7;
      Serial.println(">> Info collected. Scan card to WRITE...");
      Serial.println("   ID: "         + w_id);
      Serial.println("   First Name: " + w_firstname);
      Serial.println("   Last Name: "  + w_lastname);
      Serial.println("   Class: "      + w_class);
      Serial.println("   Phone: "      + w_phone);
      lcd.clear();
      lcd.print("Scan to WRITE");
      return;
    }

    // ── UPDATE wizard: collect new fields ──
    if (writeStep == 11) {
      if (cmdUpper == "KEEP") {
        Serial.println(">> Keeping old ID: " + w_id);
      } else {
        w_id = cmd;
      }
      writeStep = 12;
      Serial.println(">> New First Name (or KEEP):");
      return;
    }
    if (writeStep == 12) {
      if (cmdUpper != "KEEP") w_firstname = cmd;
      else Serial.println(">> Keeping old First Name: " + w_firstname);
      writeStep = 13;
      Serial.println(">> New Last Name (or KEEP):");
      return;
    }
    if (writeStep == 13) {
      if (cmdUpper != "KEEP") w_lastname = cmd;
      else Serial.println(">> Keeping old Last Name: " + w_lastname);
      writeStep = 14;
      Serial.println(">> New Class (or KEEP):");
      return;
    }
    if (writeStep == 14) {
      if (cmdUpper != "KEEP") w_class = cmd;
      else Serial.println(">> Keeping old Class: " + w_class);
      writeStep = 15;
      Serial.println(">> New Phone (or KEEP):");
      return;
    }
    if (writeStep == 15) {
      if (cmdUpper != "KEEP") w_phone = cmd;
      else Serial.println(">> Keeping old Phone: " + w_phone);
      writeStep = 16;
      Serial.println(">> Final data to write:");
      Serial.println("   ID: "         + w_id);
      Serial.println("   First Name: " + w_firstname);
      Serial.println("   Last Name: "  + w_lastname);
      Serial.println("   Class: "      + w_class);
      Serial.println("   Phone: "      + w_phone);
      Serial.println(">> Scan the SAME card again to confirm update...");
      lcd.clear();
      lcd.print("Scan same card");
      lcd.setCursor(0, 1);
      lcd.print("to confirm");
      return;
    }
  }
}

//--------------------------------------------------
void handleCardAction() {
  if (writeStep == 1)  readCard();
  if (writeStep == 7)  writeCard();
  if (writeStep == 10) readCardForUpdate();
  if (writeStep == 16) updateCard();
  if (writeStep == 99) deleteCard();
}

//--------------------------------------------------
void writeCard() {
  Serial.println(">> Writing to card...");
  lcd.clear();
  lcd.print("Writing...");

  bool ok = true;
  ok &= writeBlock(4, w_id);
  ok &= writeBlock(5, w_firstname);
  ok &= writeBlock(6, w_lastname);
  ok &= writeBlock(8, w_class);
  ok &= writeBlock(9, w_phone);

  if (ok) {
    Serial.println(">> Card written successfully!");
    lcd.clear();
    lcd.print("Write OK!");
    beep(1, 100);
  } else {
    Serial.println(">> Write FAILED!");
    lcd.clear();
    lcd.print("Write FAILED");
    beep(3, 150);
  }

  writeStep = 0;
  w_id = w_firstname = w_lastname = w_class = w_phone = "";
  returnToMgmt();
}

//--------------------------------------------------
void readCard() {
  Serial.println(">> Reading card...");
  lcd.clear();
  lcd.print("Reading...");

  String s_id        = readBlock(4);
  String s_firstname = readBlock(5);
  String s_lastname  = readBlock(6);
  String s_class     = readBlock(8);
  String s_phone     = readBlock(9);

  Serial.println("========= Card Data =========");
  Serial.println("ID:         " + s_id);
  Serial.println("First Name: " + s_firstname);
  Serial.println("Last Name:  " + s_lastname);
  Serial.println("Class:      " + s_class);
  Serial.println("Phone:      " + s_phone);
  Serial.println("=============================");

  lcd.clear();
  lcd.print(s_firstname + " " + s_lastname);
  lcd.setCursor(0, 1);
  lcd.print("ID: " + s_id);

  beep(1, 100);
  writeStep = 0;
  returnToMgmt();
}

//--------------------------------------------------
// Step 1 of update: read current data, show it, then ask for new values
void readCardForUpdate() {
  Serial.println(">> Reading current card data...");
  lcd.clear();
  lcd.print("Reading...");

  w_id        = readBlock(4);
  w_firstname = readBlock(5);
  w_lastname  = readBlock(6);
  w_class     = readBlock(8);
  w_phone     = readBlock(9);

  w_id.trim(); w_firstname.trim(); w_lastname.trim();
  w_class.trim(); w_phone.trim();

  if (w_id == "" && w_firstname == "") {
    Serial.println(">> Card is empty! Use W to write first.");
    lcd.clear();
    lcd.print("Card is empty!");
    lcd.setCursor(0, 1);
    lcd.print("Use W to write");
    beep(3, 150);
    writeStep = 0;
    returnToMgmt();
    return;
  }

  Serial.println("======= Current Card Data =======");
  Serial.println("ID:         " + w_id);
  Serial.println("First Name: " + w_firstname);
  Serial.println("Last Name:  " + w_lastname);
  Serial.println("Class:      " + w_class);
  Serial.println("Phone:      " + w_phone);
  Serial.println("=================================");
  Serial.println(">> Type new value or type KEEP to keep existing.");
  Serial.println(">> New ID (or KEEP):");

  lcd.clear();
  lcd.print(w_firstname + " " + w_lastname);
  lcd.setCursor(0, 1);
  lcd.print("Check Serial...");

  beep(1, 100);
  writeStep = 11;
}

//--------------------------------------------------
// Step 2 of update: write the final data back
void updateCard() {
  Serial.println(">> Updating card...");
  lcd.clear();
  lcd.print("Updating...");

  bool ok = true;
  ok &= writeBlock(4, w_id);
  ok &= writeBlock(5, w_firstname);
  ok &= writeBlock(6, w_lastname);
  ok &= writeBlock(8, w_class);
  ok &= writeBlock(9, w_phone);

  if (ok) {
    Serial.println(">> Card updated successfully!");
    lcd.clear();
    lcd.print("Update OK!");
    beep(1, 100);
  } else {
    Serial.println(">> Update FAILED!");
    lcd.clear();
    lcd.print("Update FAILED");
    beep(3, 150);
  }

  writeStep = 0;
  w_id = w_firstname = w_lastname = w_class = w_phone = "";
  returnToMgmt();
}

//--------------------------------------------------
void deleteCard() {
  Serial.println(">> Deleting card data...");
  lcd.clear();
  lcd.print("Deleting...");

  bool ok = true;
  ok &= writeBlock(4, "");
  ok &= writeBlock(5, "");
  ok &= writeBlock(6, "");
  ok &= writeBlock(8, "");
  ok &= writeBlock(9, "");

  if (ok) {
    Serial.println(">> Card deleted!");
    lcd.clear();
    lcd.print("Card Deleted!");
    beep(1, 100);
  } else {
    Serial.println(">> Delete FAILED!");
    lcd.clear();
    lcd.print("Delete FAILED");
    beep(3, 150);
  }

  writeStep = 0;
  returnToMgmt();
}

//--------------------------------------------------
void attendanceScan() {
  String s_id        = readBlock(4);
  String s_firstname = readBlock(5);
  String s_lastname  = readBlock(6);
  String s_class     = readBlock(8);
  String s_phone     = readBlock(9);

  s_id.trim(); s_firstname.trim(); s_lastname.trim();
  s_class.trim(); s_phone.trim();

  if (s_id == "" || s_firstname == "") {
    Serial.println(">> Empty or unregistered card!");
    lcd.clear();
    lcd.print("Unknown Card");
    lcd.setCursor(0, 1);
    lcd.print("Access Denied");
    beep(3, 150);
    delay(1500);
    lcd.clear();
    lcd.print("Attendance Mode");
    lcd.setCursor(0, 1);
    lcd.print("Scan your card");
    return;
  }

  Serial.println(">> Card: " + s_id + " " + s_firstname + " " + s_lastname);
  beep(1, 100);

  lcd.clear();
  lcd.print(s_firstname + " " + s_lastname);
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");

  sendToGoogleSheets(s_id, s_firstname, s_lastname, s_class, s_phone);
}

//--------------------------------------------------
void sendToGoogleSheets(String id, String fn, String ln, String cls, String ph) {
  if (WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    lcd.print("No WiFi!");
    return;
  }

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();

  String url = sheet_url
    + "?id="        + id
    + "&firstname=" + fn
    + "&lastname="  + ln
    + "&class="     + cls
    + "&phone="     + ph;

  HTTPClient https;
  if (https.begin(*client, url)) {
    int httpCode = https.GET();
    if (httpCode > 0) {
      String payload = https.getString();
      payload.trim();
      payload.toUpperCase();
      Serial.println(">> Response: " + payload);

      lcd.clear();
      lcd.print(fn + " " + ln);
      lcd.setCursor(0, 1);
      if      (payload.indexOf("IN")  >= 0) lcd.print("Checked IN");
      else if (payload.indexOf("OUT") >= 0) lcd.print("Checked OUT");
      else                                  lcd.print("Done!");
    } else {
      lcd.clear();
      lcd.print("HTTP Error");
      Serial.println(">> HTTP Error: " + String(httpCode));
    }
    https.end();
  }

  delay(2000);
  lcd.clear();
  lcd.print("Attendance Mode");
  lcd.setCursor(0, 1);
  lcd.print("Scan your card");
}

//--------------------------------------------------
bool writeBlock(byte blockNum, String data) {
  byte sector  = blockNum / 4;
  byte trailer = sector * 4 + 3;

  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailer, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) return false;

  byte buffer[16];
  memset(buffer, 0, 16);
  for (int i = 0; i < 16 && i < (int)data.length(); i++)
    buffer[i] = (byte)data[i];

  status = mfrc522.MIFARE_Write(blockNum, buffer, 16);
  return (status == MFRC522::STATUS_OK);
}

//--------------------------------------------------
String readBlock(byte blockNum) {
  byte sector  = blockNum / 4;
  byte trailer = sector * 4 + 3;

  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailer, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) return "";

  byte buffer[18];
  byte size = sizeof(buffer);
  status = mfrc522.MIFARE_Read(blockNum, buffer, &size);
  if (status != MFRC522::STATUS_OK) return "";

  String result = "";
  for (byte i = 0; i < 16; i++) {
    if (buffer[i] == 0) break;
    result += (char)buffer[i];
  }
  return result;
}

//--------------------------------------------------
void beep(int times, int duration) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(duration);
    digitalWrite(BUZZER, LOW);
    if (i < times - 1) delay(duration);
  }
}
