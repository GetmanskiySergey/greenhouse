// 1) все библиотеки устройств (includы)
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>


// 2) все переменные чисел (значений)

// ФЛАГ РЕЖИМА УПРАВЛЕНИЯ
bool manualMode = false;   // false = авто, true = ручной режим

// ---- ПИНЫ ДАТЧИКА ВЛАЖНОСТИ ?????
const int sensorPin = 0; 
int sensorValue = 0;      

// ---- ПИНЫ ДАТЧИКА УРОВНЯ ВОДЫ
const int sensorWaterLevelPin = 7; 
int sensorWaterLevelValue = 0;     

//------ПИНЫ ДАТЧИКА ТЕМПЕРАТУРЫ И ВЛАЖНОСТИ
const int sensorTEMPPin = 6; 
int sensorTEMPValue = 0;     


// 3) переменные устройств (пины или адрес)
// ---- ПИНЫ ПОМПЫ
const byte pin_M1_speed = 11;
const byte pin_M1_dir1 = 12;
const byte pin_M1_dir2 = 13;

//_ _ _ ПИНЫ ВИНТЕЛЯТРА
const byte pin_M2_speed = 10;
const byte pin_M2_dir1 = 9;
const byte pin_M2_dir2 = 8;

// ---- ПИНЫ ДИСПЛЕЯ LCD
LiquidCrystal_I2C lcd(0x27,20,4); 

// ---- ПИНЫ BLUETOOTH
#define BT Serial2

void setup() { // функция настройки

  // 4) подключаем устроцства к виртуальным адресам или настраиваем их

  Serial.begin(9600); // Initialize serial communication at 9600 baud rate

  // сериал блютуза
  BT.begin(9600);

  // ПОМПА
  pinMode(pin_M1_speed, OUTPUT);
  pinMode(pin_M1_dir1, OUTPUT);
  pinMode(pin_M1_dir2, OUTPUT);

  // СЕНСОР ВЛАЖНОСТИ ПОЧВЫ
  pinMode(sensorPin, INPUT);

  //------СЕНСОР ТЕМПЕРАТУРЫ 
   pinMode(sensorTEMPPin, INPUT);

  // СЕНСОР УРОВНЯ ВОДЫ
  pinMode(sensorWaterLevelPin, INPUT);

  // ИНИЦИАЛИЗАЦИЯ ДИСПЛЕЯ
  lcd.init(); 
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("SMART GREENHOUSE");
  lcd.setCursor(0,1);
  lcd.print("Loading...");
  delay(1000);
  lcd.clear();
}

// 5) простые функции

void vent(){
  lcd.setCursor(0,1);
  lcd.print("START POLIV");

  digitalWrite(pin_M2_dir1, HIGH);
  digitalWrite(pin_M2_dir2, LOW);
  digitalWrite(pin_M2_speed, 100);
}

void poliv(){
  lcd.setCursor(0,1);
  lcd.print("START POLIV");

  digitalWrite(pin_M1_dir1, HIGH);
  digitalWrite(pin_M1_dir2, LOW);
  digitalWrite(pin_M1_speed, 100);
}

void stop_poliv(){
  
  digitalWrite(pin_M1_dir1, HIGH);
  digitalWrite(pin_M1_dir2, LOW);
  digitalWrite(pin_M1_speed, 0);
}

void loop() {
  // читаем датчики
  sensorValue = analogRead(sensorPin); // ДАТЧИК ВЛАЖНОСТИ ПОЧВЫ
  sensorWaterLevelValue = analogRead(sensorWaterLevelPin);  // ДАТЧИК УРОВНЯ ВОДЫ
  sensorTEMPValue = analogRead(sensorTEMPPin); // ДАТЧИК ТЕМПЕРАТУРЫ
   
  

  // выводим значения датчиков
  // влажность
  lcd.setCursor(0,0);
  lcd.print("W:");
  lcd.setCursor(2,0);
  lcd.print(sensorValue);

  // уровень воды
  lcd.setCursor(5,0);
  lcd.print("L:");
  lcd.setCursor(7,0);
  lcd.print(sensorWaterLevelValue);

  lcd.setCursor(10,0);
  lcd.print("T:");
  lcd.setCursor(12,0);
  lcd.print(sensorTEMPValue);





  // ---- ЗОНА РУЧНОГО УПРАВЛЕНИЯ
  if (BT.available()) {
    // Читаем символ
    String command = BT.readString();

    
    
    // Выводим его в монитор порта USB
    Serial.print(command);

    if (command == "ON1"){
      manualMode = true; 
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("ON MANUAL");
      delay(1000);
      stop_poliv();
    }
    if (command == "OFF1"){
      manualMode = false; 
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("OFF MANUAL");
      delay(1000);
    }

    if (command == "ON2"){
      poliv();
    }
    if (command == "OFF2"){
      stop_poliv();
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("STOP POLIV");
    }
  }

  // ----------------------------

  // ---- ЗОНА АВТОМАТИКИ
  if (manualMode == false){


      

    if (sensorWaterLevelValue < 500){
      // НЕТ ВОДЫ
      lcd.setCursor(0,1);
      lcd.print("!NO WATER!!");
      stop_poliv();
    } else {
      //  ЕСТЬ ВОДА
      if (sensorValue > 300){
        poliv(); 
      } else {
        stop_poliv();
      }
    }



  }
  
  // ---------------------
  
  

  

  //Serial.print("Moisture Level: ");
  //Serial.println(sensorValue); // Print the sensor value to the serial monitor
  
}