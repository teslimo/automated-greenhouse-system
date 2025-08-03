#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <EEPROM.h>
#include <DHT.h>

// **Crop Struct & List**
struct Crop {
    const char* name;
    int temp;
    int humidity;
    int moisture;
};
const Crop crops[8] = {
    {"Watermelon", 22, 75, 65},
    {"Watermelon Out", 22, 75, 65},
    {"Beans", 29, 65, 60},
    {"Beans Out", 29, 65, 60},
    {"Onion", 28, 65, 20},
    {"Onion Out", 28, 65, 20},
    {"Okra", 26, 85, 60},
    {"Okra Out", 26, 85, 60},
};

// **LCD & Keypad Configuration**
#define I2C_ADDR_LCD 0x27
#define I2C_ADDR_KEYPAD 0x20
LiquidCrystal_I2C lcd(I2C_ADDR_LCD, 20, 4);

byte tempIcon[8] = {
  B00100,
  B01010,
  B01010,
  B01110,
  B01110,
  B11111,
  B11111,
  B01110
};

byte humidityIcon[8] = {
  B00100,
  B00100,
  B01010,
  B01010,
  B10001,
  B10001,
  B10001,
  B01110
};

byte soilIcon[8] = {
  B00000,
  B00000,
  B00100,
  B01110,
  B11111,
  B01110,
  B11111,
  B01110
};

byte blockIcon[8] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
    {'C', '9', '5', '1'},
    {'D', '0', '6', '2'},
    {'E', 'A', '7', '3'},
    {'F', 'B', '8', '4'}
};
byte rowPins[ROWS] = {0, 1, 2, 3};
byte colPins[COLS] = {4, 5, 6, 7};
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, I2C_ADDR_KEYPAD);

// **Sensor & Relay Pins**
#define DHT_PIN 14
#define DHT_TYPE DHT11
#define SOIL_MOISTURE_1_PIN 32
#define SOIL_MOISTURE_2_PIN 26
#define FAN_PIN 5
#define LIGHT_PIN 35 
#define Grow_pin 18
#define HUMIDIFIER_PIN 23
#define ZC_PIN 25
#define TRIAC_PIN 27
DHT dht(DHT_PIN, DHT_TYPE);

// **Soil Moisture Calibration**
const int DRY_VALUE = 4095;
const int WET_VALUE = 1500;

// **PWM Configuration for Humidifier**
#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

// **System Variables**
int savedCropIndex = -1;
bool selectionMode = false;
volatile bool monitoringActive = false;
int currentPage = 0;
unsigned long lastUpdateTime = 0;
unsigned long lastDHTRead = 0;
volatile bool trigger = false;
volatile int firing = 10000;  
unsigned long lastKeyPressTime = 0;
bool backlightOn = true;
// Shared variables for light control
volatile float sharedTemp = 0.0;
volatile float sharedHumidity = 0.0;
volatile int requiredTemp = 0;
volatile int requiredHumidity = 0;

// Function prototypes
void IRAM_ATTR zeroCrossingISR();
void controlLightIntensityTask(void *pvParameters);
void displaySelectedCrop(int cropIndex);
void displayCrops();
void handleKeyPress(char key);
void displayRealTimeValues();
void controlRelays();
void displayLoadingBar();

void setup() {
    Serial.begin(115200);
    
    Wire.begin();
    Wire.setClock(100000);
    lcd.init();
    lcd.backlight();
    lcd.createChar(0, tempIcon);
    lcd.createChar(1, humidityIcon);
    lcd.createChar(2, soilIcon);
    lcd.createChar(3, blockIcon);

    analogSetAttenuation(ADC_11db);
    EEPROM.begin(512);
    keypad.begin(makeKeymap(keys));
    dht.begin();

    pinMode(FAN_PIN, OUTPUT);
    pinMode(LIGHT_PIN, OUTPUT);
    pinMode(HUMIDIFIER_PIN, OUTPUT);
    pinMode(TRIAC_PIN, OUTPUT);
    pinMode(ZC_PIN, INPUT_PULLUP);
    pinMode(Grow_pin, OUTPUT);
    
    // Setup PWM for HUMIDIFIER_PIN
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(HUMIDIFIER_PIN, PWM_CHANNEL);
    
    attachInterrupt(digitalPinToInterrupt(ZC_PIN), zeroCrossingISR, RISING);

    digitalWrite(FAN_PIN, LOW);
    digitalWrite(LIGHT_PIN, LOW);
    ledcWrite(PWM_CHANNEL, 0); // Initialize humidifier PWM to 0 (off)
    digitalWrite(TRIAC_PIN, LOW);
    digitalWrite(Grow_pin, LOW);

    lcd.clear();
    displayLoadingBar();

    EEPROM.get(0, savedCropIndex);
    if (savedCropIndex >= 0 && savedCropIndex < 8) {
        requiredTemp = crops[savedCropIndex].temp;
        requiredHumidity = crops[savedCropIndex].humidity;
        displaySelectedCrop(savedCropIndex);
    } else {
        selectionMode = true;
        displayCrops();
    }

    xTaskCreatePinnedToCore(
        controlLightIntensityTask,
        "LightControlTask",
        2048,
        NULL,
        1,
        NULL,
        0
    );
}

void IRAM_ATTR zeroCrossingISR() {
    if (monitoringActive) {
        trigger = true;
    }
}

void controlLightIntensityTask(void *pvParameters) {
    while (1) {
        if (!monitoringActive) {
            firing = 10000;
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        if (trigger) {
            delayMicroseconds(firing);
            digitalWrite(TRIAC_PIN, HIGH);
            delayMicroseconds(5);
            digitalWrite(TRIAC_PIN, LOW);
            trigger = false;
        }

        if (isnan(sharedTemp) || isnan(sharedHumidity)) {
            firing = 10000;
        } else {
            if (sharedTemp < requiredTemp && sharedHumidity > requiredHumidity) {
                firing = 100;
            } else if (sharedTemp > requiredTemp && sharedHumidity > requiredHumidity) {
                firing = 1000;
            } else if (sharedTemp < requiredTemp && sharedHumidity < requiredHumidity) {
                firing = 1000;
            } else {
                firing = 10000;
            }
        }

        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

void loop() {
    char key = keypad.getKey();
    if (key) {
        handleKeyPress(key);
    }

    if (monitoringActive) {
        unsigned long currentMillis = millis();
        
        if (currentMillis - lastDHTRead >= 1000) {
            sharedTemp = dht.readTemperature();
            sharedHumidity = dht.readHumidity();
            lastDHTRead = currentMillis;
        }
        
        if (currentMillis - lastUpdateTime >= 5000) {
            displayRealTimeValues();
            controlRelays();
            lastUpdateTime = currentMillis;
        }

        if (backlightOn && (currentMillis - lastKeyPressTime >= 60000)) {
            lcd.noBacklight();
            backlightOn = false;
        }
    }
}

void displayLoadingBar() {
    lcd.clear();
    lcd.setCursor(6, 0);
    lcd.print("Loading...");
    
    const int barWidth = 20;
    const int totalSteps = 20;
    const int totalTimeMs = 4000;
    const int delayPerStep = totalTimeMs / totalSteps;

    for (int i = 0; i <= totalSteps; i++) {
        int blocks = (i * barWidth) / totalSteps;
        lcd.setCursor(0, 1);
        for (int j = 0; j < blocks; j++) {
            lcd.write(byte(3));
        }
        for (int j = blocks; j < barWidth; j++) {
            lcd.print(" ");
        }
        lcd.setCursor(9, 2);
        lcd.print((i * 100) / totalSteps);
        lcd.print("%");
        delay(delayPerStep);
    }
}

void displayRealTimeValues() {
    Crop selectedCrop = crops[savedCropIndex];
    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();

    long soil1Sum = 0, soil2Sum = 0;
    const int numReadings = 5;
    for (int i = 0; i < numReadings; i++) {
        soil1Sum += analogRead(SOIL_MOISTURE_1_PIN);
        soil2Sum += analogRead(SOIL_MOISTURE_2_PIN);
        delay(10);
    }
    int soil1 = soil1Sum / numReadings;
    int soil2 = soil2Sum / numReadings;
    int soilAvg = (soil1 + soil2) / 2;
    int soilMoisture;

    if (soil1 < 0 || soil1 > 4095 || soil2 < 0 || soil2 > 4095) {
        soilMoisture = -1;
    } else {
        soilMoisture = map(soilAvg, DRY_VALUE, WET_VALUE, 0, 100);
        soilMoisture = constrain(soilMoisture, 0, 100);
    }

    lcd.setCursor(0, 0);
    lcd.print(selectedCrop.name);
    lcd.print("                ");

    lcd.setCursor(0, 1);
    lcd.print("Temp");
    lcd.setCursor(4, 1);
    lcd.write(byte(0));
    lcd.print(":");
    if (isnan(temp)) {
        lcd.print("Err");
    } else {
        lcd.print(temp, 1);
    }
    lcd.print("/");
    lcd.print(selectedCrop.temp);
    lcd.print((char)223);
    lcd.print("C  ");

    lcd.setCursor(0, 2);
    lcd.print("Humi");
    lcd.setCursor(4, 2);
    lcd.write(byte(1));
    lcd.print(":");
    if (isnan(humidity)) {
        lcd.print("Err");
    } else {
        lcd.print(humidity, 0);
    }
    lcd.print("/");
    lcd.print(selectedCrop.humidity);
    lcd.print("%  ");

    lcd.setCursor(0, 3);
    lcd.print("Soil:");
    if (soilMoisture == -1) {
        lcd.print("Err");
    } else {
        lcd.print(soilMoisture);
    }
    lcd.print("%");
}

void controlRelays() {
    Crop selectedCrop = crops[savedCropIndex];
    float currentTemp = dht.readTemperature();
    float currentHumidity = dht.readHumidity();

    long soil1Sum = 0, soil2Sum = 0;
    const int numReadings = 5;
    for (int i = 0; i < numReadings; i++) {
        soil1Sum += analogRead(SOIL_MOISTURE_1_PIN);
        soil2Sum += analogRead(SOIL_MOISTURE_2_PIN);
        delay(10);
    }
    int soil1 = soil1Sum / numReadings;
    int soil2 = soil2Sum / numReadings;
    int soilAvg = (soil1 + soil2) / 2;
    int soilMapped;

    if (soil1 < 0 || soil1 > 4095 || soil2 < 0 || soil2 > 4095) {
        soilMapped = -1;
    } else {
        soilMapped = map(soilAvg, DRY_VALUE, WET_VALUE, 0, 100);
        soilMapped = constrain(soilMapped, 0, 100);
    }

    if (isnan(currentTemp) || isnan(currentHumidity) || soilMapped == -1) {
        digitalWrite(FAN_PIN, LOW);
        ledcWrite(PWM_CHANNEL, 0); // Turn off humidifier
        digitalWrite(LIGHT_PIN, LOW);
        digitalWrite(Grow_pin, LOW);
        return;
    }

    bool isIndoorCrop = (strstr(selectedCrop.name, "Out") == NULL);
    digitalWrite(Grow_pin, isIndoorCrop ? HIGH : LOW);

    digitalWrite(FAN_PIN, currentTemp > selectedCrop.temp ? HIGH : LOW);
    delay(10);

    // PWM control for humidifier
    if ( currentTemp > (selectedCrop.temp) && currentHumidity > selectedCrop.humidity) {
        ledcWrite(PWM_CHANNEL, 200); // 80% duty cycle for ~4V (204/255)
    } else if ( currentTemp > (selectedCrop.temp) && currentHumidity < selectedCrop.humidity) {
        ledcWrite(PWM_CHANNEL, 255); // 100% duty cycle for 5V
    } else if (currentTemp <= (selectedCrop.temp)) {
        ledcWrite(PWM_CHANNEL, 0); // Turn off humidifier when temp reaches or falls below requiredTemp + 3
    } else {
        ledcWrite(PWM_CHANNEL, 0); // Turn off humidifier for all other cases
    }
    delay(10);
}

void handleKeyPress(char key) {
    static bool fPressed = false;
    
    lastKeyPressTime = millis();
    if (!backlightOn) {
        lcd.backlight();
        backlightOn = true;
    }

    if (key == 'F') {
        fPressed = true;
    } else if (key == 'B' && fPressed) {
        EEPROM.put(0, -1);
        EEPROM.commit();
        ESP.restart();
    } else {
        fPressed = false;
    }

    if (key == 'D') {
        selectionMode = true;
        lcd.clear();
        displayCrops();
    } else if (selectionMode) {
        if (key == 'E') {
            currentPage = 1 - currentPage;
            displayCrops();
        } else if (key >= '1' && key <= '8') {
            savedCropIndex = key - '1';
            requiredTemp = crops[savedCropIndex].temp;
            requiredHumidity = crops[savedCropIndex].humidity;
            EEPROM.put(0, savedCropIndex);
            EEPROM.commit();
            selectionMode = false;
            displaySelectedCrop(savedCropIndex);
        }
    } else if (key == 'A' && !monitoringActive) {
        monitoringActive = true;
        lcd.clear();
        lastKeyPressTime = millis();
        if (!backlightOn) {
            lcd.backlight();
            backlightOn = true;
        }
    }
}

void displayCrops() {
    lcd.clear();
    int startIdx = currentPage * 4;

    lcd.setCursor(0, 0); lcd.print((startIdx + 1)); lcd.print("."); lcd.print(crops[startIdx].name);
    lcd.setCursor(0, 1); lcd.print((startIdx + 2)); lcd.print("."); lcd.print(crops[startIdx + 1].name);
    lcd.setCursor(0, 2); lcd.print((startIdx + 3)); lcd.print("."); lcd.print(crops[startIdx + 2].name);
    lcd.setCursor(0, 3); lcd.print((startIdx + 4)); lcd.print("."); lcd.print(crops[startIdx + 3].name);
}

void displaySelectedCrop(int cropIndex) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(crops[cropIndex].name);
    lcd.setCursor(0, 1);
    lcd.print("Temp");
    lcd.setCursor(4, 1);
    lcd.write(byte(0));
    lcd.print(": ");
    lcd.print(crops[cropIndex].temp);
    lcd.print((char)223);
    lcd.print("C");
    
    lcd.setCursor(0, 2);
    lcd.print("HumI");
    lcd.setCursor(4, 2);
    lcd.write(byte(1));
    lcd.print(": ");
    lcd.print(crops[cropIndex].humidity);
    lcd.print("%");

    lcd.setCursor(0, 3);
    lcd.print("Soil");
    lcd.setCursor(4, 3);
    lcd.write(byte(2));
    lcd.print(": ");
    lcd.print(crops[cropIndex].moisture);
    lcd.print("%");
}