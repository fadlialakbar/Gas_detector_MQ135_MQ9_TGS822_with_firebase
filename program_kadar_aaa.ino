//library
#include <MQUnifiedsensor.h>
#include <FirebaseESP32.h>


//Pin Sensor
#define MQ_135_PIN 35
#define MQ_9_PIN 32
#define TGS_822_PIN 34

//Board
#define placa "ESP-32"
#define ADC_Bit_Resolution 12
#define Voltage_Resolution 3.3

//Definitions MQ-135
#define pinMQ135 MQ_135_PIN
#define typeMQ135 "MQ-135"
#define RatioMQ135CleanAir 3.6

//Definitions MQ-9
#define pinMQ9 MQ_9_PIN
#define typeMQ9 "MQ-9" 
#define RatioMQ9CleanAir 9.6

//Definitions TGS822
#define pinTGS822 TGS_822_PIN
#define typeTGS822 "TGS822" 
#define RatioTGS822CleanAir 19

//firabase & Wifi
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#define WIFI_SSID "PEB"
#define WIFI_PASSWORD "pembalap123"
#define API_KEY "AIzaSyBw7Ph__ERZhfIrZku-NocHnVIIheUrhxI"
#define DATABASE_URL "https://arduino-iot-alcohol-sensor-default-rtdb.firebaseio.com" 
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;
static int dataCount = 0;

// Koefisien untuk berbagai gas MQ-135
float a_CO = 605.18;
float b_CO = -3.937;
float a_Alcohol = 77.255;
float b_Alcohol = -3.18;
float a_CO2 = 110.47;
float b_CO2 = -2.862;
float a_Toluene = 44.947;
float b_Toluene = -3.445;
float a_NH4 = 102.2;
float b_NH4 = -2.473;
float a_Acetone = 34.668;
float b_Acetone = -3.369;

// Koefisien untuk berbagai gas MQ-9
float a_LPG_MQ9 = 1000.5;
float b_LPG_MQ9 = -2.186;
float a_CH4_MQ9 = 4269.6;
float b_CH4_MQ9 = -2.648;
float a_CO_MQ9 = 599.65;
float b_CO_MQ9 = -2.244;

// Koefisien untuk berbagai gas TGS-822
float a_Methane_TGS822 = 10.709;
float b_Methane_TGS822 = -0.0002;
float a_CO_TGS822 = 3.4134;
float b_CO_TGS822 = -0.0004;
float a_Isobutane_TGS822 = 2.7155;
float b_Isobutane_TGS822 = -0.0003;
float a_Hexane_TGS822 = 1.8973;
float b_Hexane_TGS822 = -0.0005;
float a_Benzene_TGS822 = 1.7463;
float b_Benzene_TGS822 = -0.0005;
float a_Acetone_TGS822 = 1.5868;
float b_Acetone_TGS822 = -0.0006;

//Declare Sensor
MQUnifiedsensor MQ135(placa, Voltage_Resolution, ADC_Bit_Resolution, pinMQ135, typeMQ135);
MQUnifiedsensor MQ9(placa, Voltage_Resolution, ADC_Bit_Resolution, pinMQ9, typeMQ9);
MQUnifiedsensor TGS822(placa, Voltage_Resolution, ADC_Bit_Resolution, pinTGS822, typeTGS822);

void setup() {
  Serial.begin(115200);
  //Konfigurasi Firebase dan Wifi
   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  //MQ-135 Regresi
  MQ135.setRegressionMethod(1); 
  MQ135.init(); 

  //MQ-9 Regresi
  MQ9.setRegressionMethod(1); 
  MQ9.init();

  //TGS-822 Regresi
  TGS822.setRegressionMethod(1); 
  TGS822.init();

  //MQ-135 Kalibrasi
  Serial.print("Calibrating please wait.");
  float calcR0 = 0;
  for(int i = 1; i<=10; i++) {
    MQ135.update();
    calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
  }
  MQ135.setR0(calcR0/10);
  Serial.println(" MQ-135 done!.");
  MQ135.serialDebug(true);


  //MQ-9 Kalibrasi
  Serial.print("Calibrating please wait.");
  float calcR02 = 0;
  for(int i = 1; i<=10; i++) {
    MQ9.update();
    calcR02 += MQ9.calibrate(RatioMQ9CleanAir);
    Serial.print(".");
  }
  MQ9.setR0(calcR02/10);
  Serial.println(" MQ-9 done!.");
  MQ9.serialDebug(true);


  //TGS-822 Kalibrasi
  Serial.print("Calibrating please wait.");
  float calcR03 = 0;
  for(int i = 1; i<=10; i++) {
    TGS822.update();
    calcR03 += TGS822.calibrate(RatioTGS822CleanAir);
    Serial.print(".");
  }
  TGS822.setR0(calcR03/10);
  Serial.println(" TGS-822 done!.");
  TGS822.serialDebug(true);
}

void loop() {
  Serial.println("Mulai Deteksi");
  dataCount++;
  // CO
  MQ135.setA(a_CO); MQ135.setB(b_CO);
  MQ135.update();
  MQ135.readSensor();
  Serial.print("MQ-135 CO concentration: "); Serial.println(MQ135.readSensor());
  MQ135.serialDebug(); 
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath1 = "mq135/CO/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath1, MQ135.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}
  
  // Alcohol
  MQ135.setA(a_Alcohol); MQ135.setB(b_Alcohol);
  MQ135.update();
  MQ135.readSensor();
  Serial.print("MQ-135 Alcohol concentration: "); Serial.println(MQ135.readSensor());
  MQ135.serialDebug(); 
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath2 = "mq135/Alcohol/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath2, MQ135.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}

  // CO2
  MQ135.setA(a_CO2); MQ135.setB(b_CO2);
  MQ135.update();
  MQ135.readSensor();
  Serial.print("CO2 concentration: "); Serial.println(MQ135.readSensor());
  MQ135.serialDebug();
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath3 = "mq135/CO2/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath3, MQ135.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  } }


  // Toluene
  MQ135.setA(a_Toluene); MQ135.setB(b_Toluene);
  MQ135.update();
  MQ135.readSensor();
  Serial.print("MQ-135 Toluene concentration: "); Serial.println(MQ135.readSensor());
  MQ135.serialDebug(); 
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath4 = "mq135/Toluene/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath4, MQ135.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}


  // NH4
  MQ135.setA(a_NH4); MQ135.setB(b_NH4);
  MQ135.update();
  MQ135.readSensor();
  Serial.print("MQ-135 NH4 concentration: "); Serial.println(MQ135.readSensor());
  MQ135.serialDebug(); 
  Serial.println("");
    if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath5 = "mq135/NH4/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath5, MQ135.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}


  // Acetone
  MQ135.setA(a_Acetone); MQ135.setB(b_Acetone);
  MQ135.update();
  MQ135.readSensor();
  Serial.print("MQ-135 Acetone concentration: "); Serial.println(MQ135.readSensor());
  MQ135.serialDebug(); 
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath6 = "mq135/Acetone/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath6, MQ135.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}


  // MQ-9 untuk LPG
  MQ9.setA(a_LPG_MQ9); MQ9.setB(b_LPG_MQ9);
  MQ9.update();
  MQ9.readSensor();
  Serial.print("MQ-9 LPG concentration: "); Serial.println(MQ9.readSensor());
  MQ9.serialDebug(); 
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath7 = "mq9/LPG/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath7,  MQ9.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}
 

  // MQ-9 untuk CH4
  MQ9.setA(a_CH4_MQ9); MQ9.setB(b_CH4_MQ9);
  MQ9.update();
  MQ9.readSensor();
  Serial.print("MQ-9 CH4 concentration: "); Serial.println(MQ9.readSensor());
  MQ9.serialDebug(); 
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath8 = "mq9/CH4/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath8,  MQ9.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}

  // MQ-9 untuk CO
  MQ9.setA(a_CO_MQ9); MQ9.setB(b_CO_MQ9);
  MQ9.update();
  MQ9.readSensor();
  Serial.print("MQ-9 CO concentration: "); Serial.println(MQ9.readSensor());
  MQ9.serialDebug(); 
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath9 = "mq9/CO/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath9,  MQ9.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}


  // TGS822 untuk Methane
  TGS822.setA(a_Methane_TGS822); TGS822.setB(b_Methane_TGS822);
  TGS822.update();
  TGS822.readSensor();
  Serial.print("TGS822 Methane concentration: "); Serial.println(TGS822.readSensor());
  TGS822.serialDebug(); 
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath10 = "tgs822/Methane/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath10, TGS822.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}


  // TGS822 untuk CO
  TGS822.setA(a_CO_TGS822); TGS822.setB(b_CO_TGS822);
  TGS822.update();
  TGS822.readSensor();
  Serial.print("TGS822 CO concentration: "); Serial.println(TGS822.readSensor());
  TGS822.serialDebug(); 
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath11 = "tgs822/CO/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath11, TGS822.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}


  // TGS822 untuk Isobutane
  TGS822.setA(a_Isobutane_TGS822); TGS822.setB(b_Isobutane_TGS822);
  TGS822.update();
  TGS822.readSensor();
  Serial.print("TGS822 Isobutane concentration: "); Serial.println(TGS822.readSensor());
  TGS822.serialDebug(); 
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath12 = "tgs822/Isobutane/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath12, TGS822.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}


  // TGS822 untuk Hexane
  TGS822.setA(a_Hexane_TGS822); TGS822.setB(b_Hexane_TGS822);
  TGS822.update();
  TGS822.readSensor();
  Serial.print("TGS822 Hexane concentration: "); Serial.println(TGS822.readSensor());
  TGS822.serialDebug(); 
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath13 = "tgs822/Hexane/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath13, TGS822.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}

  // TGS822 untuk Benzene
  TGS822.setA(a_Benzene_TGS822); TGS822.setB(b_Benzene_TGS822);
  TGS822.update();
  TGS822.readSensor();
  Serial.print("TGS822 Benzene concentration: "); Serial.println(TGS822.readSensor());
  TGS822.serialDebug(); 
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath14 = "tgs822/Benzene/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath14, TGS822.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}
 

  // TGS822 untuk Acetone
  TGS822.setA(a_Acetone_TGS822); TGS822.setB(b_Acetone_TGS822); Serial.println("");
  TGS822.update();
  TGS822.readSensor();
  Serial.print("TGS822 Acetone concentration: "); Serial.println(TGS822.readSensor());
  TGS822.serialDebug(); 
  Serial.println("");
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
  String fbPath15 = "tgs822/Acetone/" + String(dataCount);
  if (Firebase.RTDB.setFloat(&fbdo, fbPath15, TGS822.readSensor())) {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }}

  delay(1000);
}






