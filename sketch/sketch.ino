/**
   https://github.com/adafruit/Adafruit-GFX-Library
   Commit: 0752d4bd4f35407684c1123e618368719b95ec22

   https://github.com/adafruit/Adafruit_SSD1306
   Commit: d62796bbe860e046265ba1251a0bf89b15e3481e

   https://github.com/Seeed-Studio/CAN_BUS_Shield
   Commit: edaba2d87efc597770079d6ed63f09aa246eba29
*/
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>
#include <SPI.h>
#include <mcp_can.h>

// Print messages to Serial
#define DEBUG_MODE true

// GPIO the display is attached to
#define pinCanCs        10
#define pinCanInt       11
const int SPI_CS_PIN = pinCanCs;
const int INT_PIN = pinCanInt;

// Change this values if you are using a custom font
#define DEFAULT_SYMBOL_WIDTH  6
#define DEFAULT_SYMBOL_HEIGHT 8

// OLED display width and height, in pixels
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// GPIO the display is attached to
#define pinDisplaySDA 20
#define pinDisplaySCL 21

// GPIO the servo is attached to
#define pinServo        9
#define pinServoAnalog  A2

// Config for servo
#define servoDelay        8
#define servoDiagDelay    30
#define servoMinUs        720  // 0
#define servoMaxUs        2400 // 180
#define servoMaxAttempts  3

// Min and max degree for rotation servo
#define servoDegreeMin  45
#define servoDegreeMax  105

// Min and max analog values
#define servoAnalogMin  330
#define servoAnalogMax  480

// Possible difference between values
#define servoAnalogDegreesDiff  5
#define servoAnalogAnalogDiff   50

// Progress Bar
#define textSize                2
#define progressBarTextSize     2
#define progressBarBorder       1   // px
#define progressBarMarginOuter  13  // px
#define progressBarHeightOuter  13  // px
#define progressBarLengthOuter  (SCREEN_WIDTH - progressBarMarginOuter * 2)
#define progressBarMarginInner  progressBarMarginOuter + progressBarBorder
#define progressBarHeightInner  progressBarHeightOuter - progressBarBorder * 2
#define progressBarLengthInner  progressBarLengthOuter - (progressBarBorder * 2)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
Servo servo;
MCP_CAN CAN(pinCanCs);

bool isDataUpdated = false;

float engineTemp;
float carSpeed;
int shutterDegree;

int newDegree = servoDegreeMin;
int analogDegree;
unsigned long timer;

volatile bool hasNewMessage = false;
unsigned char len = 0;
unsigned char buf[8];

void drawProgressBar(int y, int progress, int margin = 13, int height = 13);
bool writeServo(int degree, bool isDiagnostics = false);

void setup() {
  if (DEBUG_MODE) {
    Serial.begin(115200);
  }
  println("[INIT] Debug mode is enabled");
  println("[INIT] Starting...");

  initDisplay();
  println("--------------");
  initCan();
  println("--------------");
  initServo();
  println("--------------");
  servoDiagnostics();

  println("[INIT] Initialization is complete");

  renderAllData();
}

void loop() {
  while (CAN_MSGAVAIL == CAN.checkReceive()) {
    CAN.readMsgBuf(&len, buf);
    canMessageProcessing(CAN.getCanId(), buf);
  }

  if (isDataUpdated) {
    isDataUpdated = false;
    openCloseShutters();
    renderAllData();
  }
}

// Open or close grille shutters
void openCloseShutters() {
  float temp = getEngineTemp();
  float speed = getSpeed();
  bool openShutters = true;

  if (temp < 70) {
    openShutters = false;
  }

  if (speed > 130) {
    openShutters = (temp > 90);
  }

  print("[INFO] Shutters must be ");
  print(String(openShutters ? servoDegreeMin : servoDegreeMax));
  print(" ");
  print("(" + String(openShutters ? "open" : "close") + ")");
  println("");

  int degreeNeedle = openShutters ? servoDegreeMin : servoDegreeMax;
  if (!isServoRotated(degreeNeedle)) {
    writeServo(degreeNeedle);
  }
}

void renderAllData() {
  display.setTextSize(textSize);
  display.clearDisplay();

  renderEngineTemp();
  renderCarSpeed();
  renderProgressBar();

  display.display();
}

void renderEngineTemp() {
  display.setCursor(0, 0);
  display.println(String(getEngineTemp(), 1) + " oC");
}

void renderCarSpeed() {
  display.setCursor(0, 16);
  display.println(String(getSpeed(), 1) + " km/h");
}

void renderProgressBar() {
  int shutterDegrees = getShutterDegree();
  int progress = map(shutterDegrees, servoDegreeMin, servoDegreeMax, 0, progressBarLengthInner);
  String text = String(shutterDegrees);
  int y = constrain(51, 0, SCREEN_HEIGHT);
  int x = (SCREEN_WIDTH - text.length() * DEFAULT_SYMBOL_WIDTH * progressBarTextSize) / 2;

  display.setTextSize(progressBarTextSize);
  display.setCursor(x, y - DEFAULT_SYMBOL_HEIGHT * progressBarTextSize);
  display.println(text);

  display.drawRect(
    progressBarMarginOuter,
    y,
    progressBarLengthOuter,
    progressBarHeightOuter,
    HIGH
  );
  display.fillRect(
    progressBarMarginInner,
    y + progressBarBorder,
    progress ? : progressBarLengthInner,
    progressBarHeightInner,
    progress ? HIGH : LOW
  );
  display.display();
}

float getEngineTemp() {
  return engineTemp;
}

float getSpeed() {
  return carSpeed;
}

int getShutterDegree() {
  return shutterDegree;
}

// CAN message processing
void canMessageProcessing(unsigned long messageId, byte* messageData) {
  String id = String(messageId, HEX);

  // Engine temp
  if (id == "324") {
    float engineTempOld = getEngineTemp();

    engineTemp = messageData[0] - 40.0;

    if (engineTempOld != engineTemp) {
      isDataUpdated = true;

      println("EngineTemp: " + String(getEngineTemp()));
    }
  }
  // Speeds (general)
  else if (id == "158") {
    float carSpeedOld = getSpeed();

    carSpeed = (messageData[0] * 256 + messageData[1]) / 100.0;

    if (carSpeedOld != carSpeed) {
      isDataUpdated = true;

      println("Speed: " + String(getSpeed()));
    }
  }
}

// Returns analog degree for servo
int getAnalogDegree() {
  int analogVal = analogRead(pinServoAnalog);
  analogVal = constrain(analogVal, servoAnalogMin, servoAnalogMax);
  analogDegree = map(analogVal, servoAnalogMin, servoAnalogMax, servoDegreeMin, servoDegreeMax);

  return analogDegree;
}

// It returns the status of whether the servo turned
bool isServoRotated(int degreeNeedle) {
  bool rotated = false;

  // Check by analog value
  int analogVal = analogRead(pinServoAnalog);
  int checkMinAnalog = servoAnalogMin - servoAnalogAnalogDiff;
  int checkMaxAnalog = servoAnalogMax + servoAnalogAnalogDiff;
  rotated = analogVal > checkMinAnalog && analogVal < checkMaxAnalog;

  print("[INFO] Analog: ");
  print(String(analogVal));
  println(" | Diff min: " + String(checkMinAnalog) + " | Diff max: " + String(checkMaxAnalog));

  // Check by digital value
  if (rotated) {
    int degreeAnalog = getAnalogDegree();
    int checkMin = degreeNeedle - servoAnalogDegreesDiff;
    int checkMax = degreeNeedle + servoAnalogDegreesDiff;
    rotated = degreeAnalog > checkMin && degreeAnalog < checkMax;

    print("[INFO] Digital: ");
    print(String(degreeAnalog) + "/" + String(degreeNeedle) + " (Analog/Digital)");
    println(" | Diff min: " + String(checkMin) + " | Diff max: " + String(checkMax));
  }

  println(rotated ? "[OK] Servo is rotated!" : "[FAIL] Servo is not rotated!");

  return rotated;
}

// Rotate servo
bool writeServo(int degree, bool isDiagnostics) {
  int start = getAnalogDegree();
  int end = degree;
  int position;

  if (start > end) {
    for (position = start; position >= end; position--)
    {
      servo.write(position);
      delay(isDiagnostics ? servoDiagDelay : servoDelay);
    }
  } else {
    for (position = start; position <= end; position++)
    {
      servo.write(position);
      delay(isDiagnostics ? servoDiagDelay : servoDelay);
    }
  }

  delay(300);

  shutterDegree = constrain(getAnalogDegree(), servoDegreeMin, servoDegreeMax);

  return isServoRotated(degree);
}

// Display initialization
void initDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    println("[FAIL] SSD1306 allocation failed");
    for (;;);
  }
  println("[OK] SSD1306 allocation successful");

  display.clearDisplay();
  display.setTextColor(HIGH, LOW);
  display.display();

  println("[INIT] Display initialized");
}

// CAN bus initialization
void initCan() {
  while (CAN_OK != CAN.begin(CAN_500KBPS, MCP_8MHz)) {
    println("[FAIL] CAN BUS module init fail");
    println("[FAIL] Init CAN BUS module again");
    delay(100);
  }

  println("[INIT] CAN initialized");

  // Hardware filters
  CAN.init_Mask(0, 0, 0x158);
  CAN.init_Mask(1, 0, 0x324);
  CAN.init_Filt(0, 0, 0x158);
  CAN.init_Filt(1, 0, 0x324);

  println("[INIT] CAN filters added");
}

// Servo initialization
void initServo() {
  pinMode(pinServoAnalog, INPUT);

  servo.attach(
    pinServo,
    servoMinUs,
    servoMaxUs
  );

  if (servo.attached()) {
    println("[INIT] Servo attached");
  }

  writeServo(newDegree);
}

// Diagnostics for servo
void servoDiagnostics() {
  display.setCursor(0, 0);
  display.println("Servo diag: start");
  display.display();

  println("[INIT] Servo diagnostics: start");

  bool diagStatus = true;
  diagStatus = writeServo(servoDegreeMax, true);

  display.println("Servo diag #1: " + String(diagStatus ? "OK" : "FAIL"));
  display.display();

  print(diagStatus ? "[OK]" : "[FAIL]");
  println(" Servo diagnostics #1 (to " + String(servoDegreeMax) + "°)");

  if (diagStatus) {
    diagStatus = writeServo(servoDegreeMin, true);
  }

  display.println("Servo diag #2: " + String(diagStatus ? "OK" : "FAIL"));
  display.display();

  print(diagStatus ? "[OK]" : "[FAIL]");
  println(" Servo diagnostics #2 (to " + String(servoDegreeMin) + "°)");

  if (diagStatus) {
    diagStatus = writeServo(servoDegreeMax, true);
  }

  display.println("Servo diag #3: " + String(diagStatus ? "OK" : "FAIL"));
  display.display();

  print(diagStatus ? "[OK]" : "[FAIL]");
  println(" Servo diagnostics #3 (to " + String(servoDegreeMax) + "°)");

  if (diagStatus) {
    diagStatus = writeServo(servoDegreeMin, true);
  }

  display.println("Servo diag #4: " + String(diagStatus ? "OK" : "FAIL"));
  display.display();

  print(diagStatus ? "[OK]" : "[FAIL]");
  println(" Servo diagnostics #4 (to " + String(servoDegreeMin) + "°)");

  if (!diagStatus) {
    while (true) {
      if (millis() - timer >= 3000) {
        timer = millis();

        display.println("");
        display.println("Life Is Pain");
        display.println("Shit happens...");
        display.display();

        println("[FAIL] Servo diagnostics: Shit happens...");
      }
    }
  }

  display.println("");
  display.println("Success!");
  display.display();

  println("[INIT] Servo diagnostics: success");
}

// Print message to Serial
void print(String message) {
  if (DEBUG_MODE) {
    Serial.print(message);
  }
}

// Print message to Serial
void println(String message) {
  if (DEBUG_MODE) {
    Serial.println(message);
  }
}
