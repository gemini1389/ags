#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>

// Print messages to Serial
#define DEBUG_MODE true

#define dataSerial SerialUSB
#if DEBUG_ENABLED
#define debugSerial Serial1
#endif

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
#define pinServo        4
#define pinServoAnalog  A0

// Config for servo
#define servoDelay        8
#define servoDiagDelay    30
#define servoMinUs        720  // 0
#define servoMaxUs        2400 // 180
#define servoMaxAttempts  3

// Min and max degree for rotation servo
#define servoDegreeMin  45  // 0
#define servoDegreeMax  105 // 170

// Min and max analog values
#define servoAnalogMin  348 // 221
#define servoAnalogMax  493 // 665

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

float engineTemp  = 66.6;
float carSpeed    = 55.5;
int shutterDegree = 75;

int newDegree = servoDegreeMin;
int analogDegree;
unsigned long timer;

void drawProgressBar(int y, int progress, int margin = 13, int height = 13);

void setup() {
  dataSerial.begin(115200);
  while (!dataSerial);
  dataSerial.println("Initialization");

#if DEBUG_ENABLED
  debugSerial.begin(115200);
  while (!debugSerial);

  dataSerial.println("Debug: ENABLED");
#else
  dataSerial.println("Debug: DISABLED");
#endif

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    dataSerial.println("SSD1306 allocation failed");
    for (;;);
  }
  display.clearDisplay();
  display.setTextColor(HIGH, LOW);
  display.display();
  dataSerial.println("Display: OK");

  renderAllData();

  dataSerial.println("Initialization: OK");
}

void loop() {
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
  String text = String(shutterDegrees) + " o";
  int progress = map(shutterDegrees, servoDegreeMin, servoDegreeMax, 0, progressBarLengthInner);
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
  return constrain(shutterDegree, servoDegreeMin, servoDegreeMax);
}
