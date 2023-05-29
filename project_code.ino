// soil moist: sensor1 - A0, sensor2 - A1
// dht - 6
// flow sensor - 2
// relay: 1 - 8, 2 - 9, 3 - 5

#include <RTClib.h>
#include <SD.h>
#include <DHT.h>
#include <DHT_U.h>

#define dhtPin 6
#define flowPin 2
#define soilMoisturePin1 A0
#define soilMoisturePin2 A1
#define sprinklerRelayPin 7
#define relayPin1 8
#define relayPin2 9
#define mainPumpPin 5
#define flowConstant 7.5
#define dhtReadDelay 60000     // 60 sec
#define flowReadDelay 10000   // 10 sec
#define logDataDelay 600000  // 10 minutes
#define serialPrintDelay 300000  // 5 minutes
#define UPPER_MOSITURE_CONSTANT 55

bool mainPumpON = 0, valve1_ON = 0, valve2_ON = 0, showOledData = 0, firstTime = 1;

volatile byte pulseCount = 0;
unsigned int flowMilliLitres = 0;
unsigned int totalMilliLitres = 0;
unsigned int oldTimeForFlow = 0, oldTimeForDht = 0, oldTimeForLog = 0, oldTimeForSerial = 0, oldTimeForOled = 0, oldTimeforOled = 0;

String fileName = "", datetime = "";
float humidity = 0, tempCel = 0, flowRate = 0;
int moisturePercent1 = 0, moisturePercent2 = 0;

DHT dht(dhtPin, DHT22);
File dataFile;
RTC_DS1307 RTC;
// Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
  

String getDateAndTime()
{
  DateTime now = RTC.now();
  datetime = String(now.year(), DEC) + "/" + String(now.month(), DEC) + "/" + String(now.day(), DEC) + " --> "
             + String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC);
  return datetime;
}

void setup() 
{
  Serial.begin(9600);
  
  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(mainPumpPin, OUTPUT);
  pinMode(soilMoisturePin1, INPUT);
  pinMode(soilMoisturePin2, INPUT);
  pinMode(flowPin, INPUT);

  RTC.begin();
  dht.begin();

  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  oldTimeForFlow = 0;


  if (!SD.begin()) 
  {
    Serial.println(F("SD card initialization failed!"));
  }

  DateTime now = RTC.now();

  fileName = "data-" + String(now.year(), DEC) + "-" + String(now.month(), DEC) + "-" + String(now.day(), DEC) + ".csv";
  dataFile = SD.open("data.csv", FILE_WRITE);

  if (!dataFile)
  {
    Serial.println("File creation failed!");
    delay(500);
  }

  dataFile.println("Date --> Time, Humidity (%), Temperature (C), Soil Moisture 1 (%), Soil Moisture 2 (%), Water Volume (mL), Valve 1, Valve 2, Main Pump");
  dataFile.close();

  Serial.println("Test Started...");
  delay(500);
  attachInterrupt(digitalPinToInterrupt(flowPin), pulseCounter, FALLING);
}

void loop() 
{
  if(millis() - oldTimeForDht > dhtReadDelay || firstTime)
  {
    humidity = dht.readHumidity();
    tempCel = dht.readTemperature();
    moisturePercent1 = map(analogRead(soilMoisturePin1), 250, 1023, 100, 0);
    moisturePercent2 = map(analogRead(soilMoisturePin2), 250, 1023, 100, 0);
  

    if(isnan(humidity) || isnan(tempCel))
    {
      Serial.println(F("Failed to Read..."));
      // return;
    }

    if(moisturePercent1 < 30)
    {
      digitalWrite(relayPin1, LOW);
      valve1_ON = true;
      digitalWrite(mainPumpPin, LOW);
      mainPumpON = true;
    }

    if(moisturePercent1 > UPPER_MOSITURE_CONSTANT)
    {
      digitalWrite(relayPin1, HIGH);
      valve1_ON = false;
    }

    if(moisturePercent2 < 30)
    {
      digitalWrite(relayPin2, LOW);
      valve2_ON = true;
      digitalWrite(mainPumpPin, LOW);
      mainPumpON = true;
    }

    if(moisturePercent2 > UPPER_MOSITURE_CONSTANT )
    {
      digitalWrite(relayPin2, HIGH);
      valve2_ON = false;
      
    }

    if(moisturePercent1 > UPPER_MOSITURE_CONSTANT && moisturePercent2 > UPPER_MOSITURE_CONSTANT)
    {
      digitalWrite(mainPumpPin, HIGH);
      mainPumpON = false;
    }
    oldTimeForDht = millis();
    firstTime = false;
  }
  
  calculateWaterFlow();

  // printing the data on serial monitor and then logging it to the SD card after the specified delay
  if(millis() - oldTimeForSerial > serialPrintDelay)
  {
    printSerial(humidity, tempCel, moisturePercent1, moisturePercent2, totalMilliLitres);
    oldTimeForSerial = millis();
  }
  logData(humidity, tempCel, moisturePercent1, moisturePercent2, totalMilliLitres);
  // displayData(humidity, tempCel, moisturePercent1, moisturePercent2, totalMilliLitres);
}

void calculateWaterFlow()
{
    // Calculate water flow rate
  if((millis() - oldTimeForFlow) > flowReadDelay)
  {
    detachInterrupt(digitalPinToInterrupt(flowPin));
    flowRate = (((float)flowReadDelay / (millis() - oldTimeForFlow)) * pulseCount) / flowConstant;  // Convert frequency to flow rate
    oldTimeForFlow = millis();
    pulseCount = 0;
    flowMilliLitres = ((flowRate / 60) * 1000);  // Convert flow rate to flow volume
    totalMilliLitres += flowMilliLitres;
    attachInterrupt(digitalPinToInterrupt(flowPin), pulseCounter, FALLING);
  }
}

void printSerial(float hum, float temp, int moist1, int moist2, int waterVolume)
{
  Serial.print(F("Date --> Time: "));
  Serial.print(getDateAndTime());

  Serial.print(F(" Humidity: "));
  Serial.print(hum);
  Serial.print(F("%\tTemperature: "));
  Serial.print(temp);
  Serial.print(F("°C\tSoil Moisture 1: "));
  Serial.print(moist1);
  Serial.print(F("%\tSoil Moisture 2: "));
  Serial.print(moist2);
  Serial.print(F("%\tWater Volume(since last pump ON): "));
  Serial.print(waterVolume);
  Serial.print(F("mL"));

  Serial.print(F("\tValve1: "));
  Serial.print(valve1_ON ? String("ON") : String("OFF"));
  Serial.print(F("\tValve2: "));
  Serial.println(valve2_ON ? String("ON") : String("OFF"));
  Serial.print(F("\tPump: "));
  Serial.println(mainPumpON ? String("ON") : String("OFF"));
}

void logData(float hum, float temp, int moist1, int moist2, int waterVolume)
{
  if(millis() - oldTimeForLog > logDataDelay)
  {

    dataFile = SD.open("data.csv", FILE_WRITE);

    if (dataFile) 
    {
      // write data to file in CSV format
      dataFile.print(getDateAndTime());
      dataFile.print(",");
      dataFile.print(hum);
      dataFile.print(",");
      dataFile.print(temp);
      dataFile.print(",");
      dataFile.print(moist1);
      dataFile.print(",");
      dataFile.print(moist2);
      dataFile.print(",");
      dataFile.print(waterVolume);
      dataFile.print(",");
      dataFile.print(valve1_ON ? "ON" : "OFF");
      dataFile.print(",");
      dataFile.println(valve2_ON ? "ON" : "OFF");
      dataFile.print(",");
      dataFile.println(mainPumpON ? "ON" : "OFF");

      // close file
      dataFile.close();
      
      Serial.println("Data logged successfully.");
    } 
    else 
    {
      Serial.println("Error opening file!");
    }
    oldTimeForLog = millis();
  }
}

void pulseCounter()
{
  pulseCount++;
}
