#include <QMI8658.h>
#include <Adafruit_NeoPixel.h>

// ----------------- IMU CONFIG -----------------
QMI8658 imu;
#define SDA_PIN 11
#define SCL_PIN 12
#define TILT_THRESHOLD 250

enum TiltState { FLAT, FORWARD, BACKWARD, LEFT, RIGHT }; //USB Connestor is the top or FORWARD

TiltState getTiltState(float ax, float ay) {
  if (ay > TILT_THRESHOLD) return LEFT;
  if (ay < -TILT_THRESHOLD) return RIGHT;
  if (ax > TILT_THRESHOLD) return BACKWARD;
  if (ax < -TILT_THRESHOLD) return FORWARD;
  return FLAT;
}

// ----------------- LED MATRIX CONFIG -----------------
#define LED_PIN 14
#define MATRIX_SIZE 8

#define NUM_LEDS (MATRIX_SIZE * MATRIX_SIZE)

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_RGB + NEO_KHZ800);

// Convert (x,y) → LED index (row-major, top-left = (0,0))
int xyToIndex(int x, int y) {
  return y * MATRIX_SIZE + x;
}

// ----------------- SNAKE GAME -----------------
struct Point { int x, y; };
#define MAX_SNAKE_LENGTH 64

Point snake[MAX_SNAKE_LENGTH];
int snakeLength = 3;
int dirX = 1, dirY = 0;  // initial direction: right
Point food;
int score = 0;           // 🍎 food eaten

// Place food at random free spot
void spawnFood() {
  bool valid = false;
  while (!valid) {
    food.x = random(0, MATRIX_SIZE);
    food.y = random(0, MATRIX_SIZE);
    valid = true;
    for (int i = 0; i < snakeLength; i++) {
      if (snake[i].x == food.x && snake[i].y == food.y) {
        valid = false;
        break;
      }
    }
  }
}

// Initialize snake
void initSnake() {
  snakeLength = 3;
  snake[0] = {4, 4};
  snake[1] = {3, 4};
  snake[2] = {2, 4};
  dirX = 1; dirY = 0;
  score = 0;
  spawnFood();
}

// Update snake position
bool updateSnake() {
  // Move body
  for (int i = snakeLength - 1; i > 0; i--) {
    snake[i] = snake[i - 1];
  }
  // Move head
  snake[0].x += dirX;
  snake[0].y += dirY;

  // Wrap around edges
  if (snake[0].x < 0) snake[0].x = MATRIX_SIZE - 1;
  if (snake[0].x >= MATRIX_SIZE) snake[0].x = 0;
  if (snake[0].y < 0) snake[0].y = MATRIX_SIZE - 1;
  if (snake[0].y >= MATRIX_SIZE) snake[0].y = 0;

  // Collision with body?
  for (int i = 1; i < snakeLength; i++) {
    if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
      return false; // game over
    }
  }

  // Eat food?
  if (snake[0].x == food.x && snake[0].y == food.y) {
    if (snakeLength < MAX_SNAKE_LENGTH) {
      snake[snakeLength] = snake[snakeLength - 1];
      snakeLength++;
    }
    score++; // increase score
    spawnFood();
  }

  return true;
}

// Draw snake + food
void drawGame() {
  strip.clear();
  // Draw snake
  for (int i = 0; i < snakeLength; i++) {
    int idx = xyToIndex(snake[i].x, snake[i].y);
    if (i == 0) strip.setPixelColor(idx, strip.Color(100, 0, 0)); // head
    else strip.setPixelColor(idx, strip.Color(50, 0, 0));         // body dim
  }
  // Draw food
  int fidx = xyToIndex(food.x, food.y);
  strip.setPixelColor(fidx, strip.Color(0, 100, 0)); // food
  strip.show();
}

// Game Over screen (flashing red)
void gameOverScreen() {
  Serial.print("💀 Game Over! Final Score: ");
  Serial.println(score);
}

// ----------------- SETUP & LOOP -----------------
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!imu.begin(6, 7)) {
    Serial.println("❌ Failed to init QMI8658!");
    while (1) delay(5000);
  }
  imu.setAccelRange(QMI8658_ACCEL_RANGE_8G);
  imu.setAccelODR(QMI8658_ACCEL_ODR_1000HZ);
  imu.setAccelUnit_mg(true);
  imu.enableSensors(QMI8658_ENABLE_ACCEL);

  strip.begin();
  strip.show();

  initSnake();
  randomSeed(analogRead(A0)); // for food placement
}

unsigned long lastMove = 0;
int moveInterval = 400; // ms per move

void loop() {
  QMI8658_Data sensorData;
  if (imu.readSensorData(sensorData)) {
    TiltState state = getTiltState(sensorData.accelX, sensorData.accelY);

    switch (state) {
      case FORWARD:  if (dirY != 1) { dirX = 0; dirY = -1; } break;
      case BACKWARD: if (dirY != -1) { dirX = 0; dirY = 1; } break;
      case LEFT:     if (dirX != 1) { dirX = -1; dirY = 0; } break;
      case RIGHT:    if (dirX != -1) { dirX = 1; dirY = 0; } break;
      default: break;
    }
  }

  if (millis() - lastMove > moveInterval) {
    lastMove = millis();
    if (!updateSnake()) {
      gameOverScreen();
      initSnake();
    }
    drawGame();
  }
}
