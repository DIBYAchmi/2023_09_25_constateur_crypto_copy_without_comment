#include <HardwareSerial.h>
#include <Eeprom_at24c256.h>
#include "RTClib.h"
#include <EEPROM.h>
#include <AESLib.h>
#undef dump
#include "BluetoothSerial.h"
#define abs(x) ((x)>0?(x):-(x))

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#include <WS2812FX.h>
#include <cstring> 

#define KEY_LENGTH 32
#define BLOCK_SIZE 16
#define RXD 16
#define TXD 5
AESLib aesLib;
byte original_iv[BLOCK_SIZE];
byte key[KEY_LENGTH] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31}; 
byte iv[BLOCK_SIZE] = {0xbb, 0x16, 0xd3, 0xb0, 0x77, 0x5c, 0x4d, 0x3b, 0x6b, 0x02, 0xd8, 0x09, 0x56, 0xbf, 0xeb, 0x47}; 

WS2812FX ws2812fx = WS2812FX(1, 27, NEO_GRB + NEO_KHZ800);

DateTime MyDateAndTime;

BluetoothSerial SerialBT;
Eeprom_at24c256 eeprom(0x50);
RTC_DS3231 Clock;

int Connect = 1;
int Supprime;


int indiceMemory = 0 , endRomMemory = 10, deletMemory = 15, deltaMilliMemory = 20, deltaMilli = 0;


HardwareSerial esp(2);
String Data1, Data2,Data3, code, tag;
bool newcode = false;

const int TONE_OUTPUT_PIN = 26;
const int TONE_PWM_CHANNEL = 0;

void setup() {
  Clock.begin();
  SerialBT.begin("Pigma System01060923"); 
  Serial.begin(9600);
  Serial1.begin(9600, SERIAL_8N1, 4, 26);
  esp.begin(9600, SERIAL_8N1,RXD, TXD);

  aesLib.set_paddingmode((paddingMode)0);
  memcpy(original_iv, iv, BLOCK_SIZE);

  EEPROM.begin(512);
  Supprime = EEPROM.read(deletMemory);
  
  ledcAttachPin(TONE_OUTPUT_PIN, TONE_PWM_CHANNEL);
  ws2812fx.init();
  ws2812fx.setBrightness(255);
  ws2812fx.setSegment(0,  0,  0, FX_MODE_STATIC, RED, 0, NO_OPTIONS);
  ws2812fx.start();
  ws2812fx.setColor(BLUE);
  ws2812fx.service(); 
}


void loop() {
    bool isBTConnected = SerialBT.connected();

    handleBluetoothConnection(isBTConnected);

    processData(esp, Data1);
    processData(Serial, Data3);
    processData(Serial1, Data2);

    if (newcode) {
        newcode = false;
        String date = Date();
        String Da = formatDate(date);
        tag = code.substring(2, 10);

        handleSecretCode(Da, tag, isBTConnected);
    }

    if (isBTConnected && SerialBT.available()) {
        handleReceivedData(SerialBT.readStringUntil('\r'));
    }
}

void handleBluetoothConnection(bool isBTConnected) {
    if (isBTConnected && Connect == 1) {
        // printStoreTag();
        printData(false);
        Connect = 0;
    } else if (!isBTConnected) {
        Connect = 1;
    }
}


void handleSecretCode(const String& Da, const String& tag, bool isBTConnected) {
    if (tag == "12345678") {
        if (isBTConnected) {
            Sound(1);
            sendCrypto("ii", Da, tag);
        } else {
            Sound(0);
        }
    } else if (checkNewTag(tag)) {
        int indice = EEPROM.read(0);
        int endr = EEPROM.read(10);

        if (isBTConnected) {
            Sound(1);
            sendData(tag, Date(), indice);
            writeToEEPROM(0, ++indice);
            writeToEEPROM(10, ++endr);
        } else {
            Storage(tag, false);
            Sound(0);
        }
    }
}



void handleReceivedData(String data) {
    if (check(data).equals("date")) {
        reglage_heur(data);
    } else if (data[0] == 'S') {
        toutSupprimer();
    } else if (data[0] == 'P') {
        // printAllData();
        printData(true);
    } else if (data[0] == 'B') {
        int moyen = 0;
        String text = " [{\"batterie\":\"" + String(moyen) + "\"}]#";
        SerialBT.print(text);
    }
}

String formatDate(String date) {
    return date.substring(4, 8) + "-" + date.substring(2, 4) + "-" + date.substring(0, 2) + " " + 
           date.substring(8, 10) + ":" + date.substring(10, 12) + ":" + 
           date.substring(12, 14) + "." + date.substring(14, 17);
}


void printData(bool allData) {
    int Size = EEPROM.read(endRomMemory);
    for (int i = 0; i < Size; i++) {
        char message[16], message1[16];
        String check = "", date = "";

        eeprom.read(i * 32 + 16, (byte *) message1, sizeof(message1));
        delay(15);
        eeprom.read(i * 32, (byte *) message, sizeof(message));
        delay(15);
        for (int z = 0; z < 16; z++) {
            check += message1[z];
            date += message[z];
        }

        if (allData) {
            String Da = formatDate(date);
            sendCrypto(String(i), Da, check.substring(0, 8));
        } else if (message1[11] == '0') {
            message1[11] = '1';
            eeprom.write(i * 32 + 16, (byte *) message1, sizeof(message1));
            sendData(check.substring(0, 8), date, check.substring(8, 11).toInt());
        }
    }
}




bool checkNewTag(String card)
{

  int endrom;
  endrom = EEPROM.read(endRomMemory);
  for (int i = 0; i < endrom ; i++) {
    char message[16], message1[16];

    eeprom.read(i * 32 + 16, (byte *) message1, sizeof(message1));
    delay(15);
    String compare = "";
    for (int z = 0; z < 8; z++) {

      compare += message1[z];
    }

    if (compare.equals(card)) {
      return false;
    }



  }

  return true;

}

String  filtrage_number(int indice) {

  String temp = String(indice);
  if (temp.length() == 2) {
    temp = "0" + temp;

  }
  else if (temp.length() == 1) {
    temp = "00" + temp;

  }

  return temp;

}


String Date() {
  MyDateAndTime = Clock.now();

  String formattedDate = "";

  int values[] = {
    MyDateAndTime.day(),
    MyDateAndTime.month(),
    MyDateAndTime.year(),
    MyDateAndTime.hour(),
    MyDateAndTime.minute(),
    MyDateAndTime.second(),
    abs(millis() % 1000)
  };

  for (int i = 0; i < 7; i++) {
    String current = String(values[i]);
    if (i == 6) { // Special case for milliseconds
      while (current.length() < 3) {
        current = "0" + current;
      }
    } else {
      if (current.length() < 2) {
        current = "0" + current;
      }
    }
    formattedDate += current;
  }

  formattedDate += "0"; // Appending the trailing zero

  return formattedDate;
}





void toutSupprimer()
{
  writeToEEPROM(indiceMemory, 0);

  writeToEEPROM(endRomMemory, 0);

  writeToEEPROM(deletMemory, 0);
  String text = " [{\"message\":\"supprimer\"}]#";
  
  SerialBT.print(text);

}


String check(String data) {

  int i = data.indexOf(":");
  String info;
  info = data.substring(0, i);

  return info;
}


bool Check_secret(String code) {
  String secret = code.substring(0, 2);
  uint16_t code_1_hex = strtoul(code.substring(6, 10).c_str(), NULL, 16);
  uint16_t code_2_hex = strtoul(code.substring(2, 6).c_str(), NULL, 16);
  uint16_t secret_hex = strtoul(secret.c_str(), NULL, 16);

  uint16_t modulo_hex = code_1_hex % code_2_hex;
  if (modulo_hex > 256) modulo_hex %= 256;

  if (modulo_hex == secret_hex) return true;

  bool allInt = true;
  for (char c : code) {
    if (c >= 'A' && c <= 'F') {
      allInt = false;
      break;
    }
  }

  if (allInt) {
    int secret_int = secret.toInt();
    int modulo_int = code.substring(6, 10).toInt() % code.substring(2, 6).toInt();
    if (modulo_int > 100) modulo_int %= 100;
    return modulo_int == secret_int;
  }

  return false;
}




void reglage_heur(String date) {

  int index = date.indexOf(":");

  index = index + 1;
  String da = date.substring(index, date.length());
  if (da)
  {
    int space = da.indexOf(" ");
    String date1 = da.substring(0, space);
    String date2 = da.substring(space + 1, da.length());
    String anne, mois, jour, heur, minut, sec, milli;

    if (date1.substring(0, date1.indexOf("-")).toInt() >= 2021) {
      anne = date1.substring(0, date1.indexOf("-"));
      mois = date1.substring(date1.indexOf("-") + 1, date1.indexOf("-") + 3);
      jour = date1.substring(date1.indexOf("-") + 4, date1.indexOf("-") + 6);
      //00:24:50
      heur = date2.substring(0, date2.indexOf(":"));
      minut = date2.substring(date2.indexOf(":") + 1, date2.indexOf(":") + 3);
      sec = date2.substring(date2.indexOf(":") + 4, date2.indexOf(":") + 6);
      milli = date2.substring(date2.indexOf(":") + 7, date2.indexOf(":") + 10);


      deltaMilli = abs( milli.toInt() - millis() % 1000);
      writeToEEPROM(deltaMilliMemory, deltaMilli);

      Clock.adjust(DateTime(anne.toInt(), mois. toInt(),  jour.toInt(), heur. toInt(), minut. toInt(), sec. toInt()));

    }
  }
}


void Sound(bool Green) {
  if(Green)
  {
    ws2812fx.setColor(GREEN);
    ws2812fx.service(); 
  }
  else{
    ws2812fx.setColor(RED);
    ws2812fx.service(); 
  }
  ledcWriteNote(TONE_PWM_CHANNEL, NOTE_C, 4);
  delay(50);
  ledcWrite(TONE_PWM_CHANNEL, 0);
  ws2812fx.setColor(BLUE);
  ws2812fx.service(); 
}


void sendCrypto(String Indice, String Date, String RFID){
  memcpy(iv, original_iv, BLOCK_SIZE);
  char plaintext[200]; 
  byte ciphertext[200];
  String data="";
  String text = "[{\"code\":\"" + RFID + "\",\"time\":\"" + Date + "\"}]";
  text.toCharArray(plaintext, sizeof(plaintext));
  int enc_len = aesLib.encrypt((byte*)plaintext, strlen(plaintext), ciphertext, key, 256, iv); 
  for (int i = 0; i < enc_len; i++) {
    if(ciphertext[i] < 0x10)  data += "0";
      data += String(ciphertext[i], HEX);
  }
  text = " [{\"indice\":\"" + Indice + "\",\"time\":\"" + Date + "\",\"data\":\"" + data +"\"}]#";
  SerialBT.print(text);
  memset(plaintext, 0, sizeof(plaintext));
  memset(ciphertext, 0, sizeof(ciphertext));
  memcpy(iv, original_iv, BLOCK_SIZE);

}



void processData(Stream &serialInput, String &data) {
  if (serialInput.available()) {
    char charRead = serialInput.read();

    if ((charRead != byte(3)) and (charRead != byte(0))) {
      data += charRead;
    } 
    else if (charRead != byte(0)) {
      data = data.substring(1, 11);

      if (data.length() == 10) {
          bool isError = true;
          
          for (int i = 0; i < data.length(); i++) {
              if (!(data[i] >= '0' && data[i] <= '9') && !(data[i] >= 'A' && data[i] <= 'F')) {
                  isError = false;
              }
          }

          if (isError) {
              code = data;
              newcode = true;
          }
      }

      data = "";
    } 
    else {
      data = "";
    }
  }
}

// Nouvelle fonction pour stocker des données dans l'EEPROM
void storeInEEPROM(String code, String date, int indice, boolean envoi) {
  String writeinfo = code + filtrage_number(indice) + String(envoi) + "0000";
  char writebuffer[16], writebuffer1[16];
  
  for (int x = 0; x < 16; x++) {
    writebuffer[x] = date[x]; 
    writebuffer1[x] = writeinfo[x];
  }

  eeprom.write(indice * 32, (byte*) writebuffer, sizeof(writebuffer));
  delay(15);
  eeprom.write(indice * 32 + 16, (byte*) writebuffer1, sizeof(writebuffer1));
  delay(15);
}

// Fonction mise à jour pour envoyer des données
void sendData(String code, String date, int indice) {
  
  String Da = formatDate(date);
  sendCrypto(String(indice), Da, code);

  // Utilisez la nouvelle fonction pour stocker dans l'EEPROM
  storeInEEPROM(code, date, indice, true);  // 'true' car les données sont toujours envoyées dans cette fonction
}

// Fonction mise à jour pour le stockage
void Storage(String code, boolean envoi) {
  int indice;
  indice = EEPROM.read(indiceMemory);
  
  // Utilisez la nouvelle fonction pour stocker dans l'EEPROM
  storeInEEPROM(code, Date(), indice, envoi);

  indice++;
  int Endrom;
  Endrom = EEPROM.read(endRomMemory);
  if (Endrom < 1000) Endrom++;

  writeToEEPROM(endRomMemory, Endrom);

  writeToEEPROM(indiceMemory, indice);
}

void writeToEEPROM(int address, int value) {
    EEPROM.write(address, value);
    EEPROM.commit();
}