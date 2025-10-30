// Khai báo chân
const uint8_t buttonPin = 4;
const uint8_t LEFT_LIMIT = 3;
const uint8_t RIGHT_LIMIT = 2;

const uint8_t IN1 = 10;
const uint8_t IN2 = 9;
const uint8_t ENA = 6;

// Điều khiển tốc độ quay
const int PWM_MAX = 200;
const int PWM_MIN = 0;
int currentPWM = PWM_MIN;

// Xác định trạng thái hoạt động
bool buttonState = LOW;
bool leftHit = LOW;
bool rightHit = HIGH;
bool lastButtonState = LOW;
bool pressed = false;

// Biến thời gian
unsigned long now = 0;
unsigned long lastStepMs = 0;
const unsigned long ACCE_INTERVAL = 3;
const unsigned long DECE_INTERVAL = 3;

// Mô tả chuyển động
enum movingPhases {IDLE, ACCE, CSSP, DECE};
movingPhases phase = IDLE;

enum movingStates {FORWARD, RETURN, STOPPED};
movingStates state = STOPPED;

enum sides {LEFT, RIGHT, NONE};
sides side = NONE;
sides lastSide = NONE;

// Xử lý pha chuyển động
void movingPhase () {
  now = millis();

  switch (phase){
    case IDLE:

    break;

    case ACCE:
    if (now - lastStepMs >= ACCE_INTERVAL) {
      if (currentPWM < PWM_MAX) {
        currentPWM++;
      }
      else {
        phase = CSSP;
      }
      lastStepMs = now;
    }
    break;

    case CSSP:
    break;

    case DECE:
    if (now - lastStepMs >= DECE_INTERVAL) {
      if (currentPWM > PWM_MIN) {
        currentPWM--;
      }
      else {
        phase = IDLE;
        state = STOPPED;
        stopMoving();
      }
      lastStepMs = now;
    }
  }

  if (state != STOPPED) {
    analogWrite(ENA, currentPWM);
  }
  else {
    analogWrite(ENA, PWM_MIN);
  }
}

// Đổi hướng chuyển động
void setDirection (movingStates dir) {
  switch (dir) {
    case FORWARD:
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    break;

    case RETURN:
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    break;

    case STOPPED:
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    break;
  }
}

// Bắt đầu chuyển động
void startMoving(movingStates newState) {
  state = newState;
  setDirection(state);
  currentPWM = PWM_MIN;
  phase = ACCE;
  lastStepMs = millis();
}

// Dừng chuyển động
void stopMoving () {
  state = STOPPED;
  phase = IDLE;
  currentPWM = PWM_MIN;
  analogWrite(ENA, PWM_MIN);
}

// Xác định đã chạm cạnh bên
void sideCheck () {
  if (leftHit && !rightHit) side = LEFT;
  else if (rightHit && !leftHit) side = RIGHT;
  else side = NONE;
}

// Xác nhận đã nhấn nút
void buttonCheck() {
  if (buttonState != lastButtonState) {
    if (buttonState && state == STOPPED && phase == IDLE && lastSide == LEFT) {
      pressed = true;
    }
    lastButtonState = buttonState;
  }
}

void setup(){
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(LEFT_LIMIT, INPUT_PULLUP);
  pinMode(RIGHT_LIMIT, INPUT_PULLUP);

  side = LEFT;
  lastSide = LEFT;
  lastButtonState = !digitalRead(buttonPin);
}

void loop () {
  leftHit = !digitalRead(LEFT_LIMIT);
  rightHit = !digitalRead(RIGHT_LIMIT);
  buttonState = !digitalRead(buttonPin);

  buttonCheck();

  sideCheck();

  if (pressed == true && lastSide == LEFT && state == STOPPED && phase == IDLE) {
    startMoving(FORWARD);
  }

  if (side == RIGHT && lastSide == LEFT && state == FORWARD && phase == CSSP) {
    phase = DECE;
    lastSide = side;
  }

  if (lastSide == RIGHT && state == STOPPED && phase == IDLE) {
    startMoving(RETURN);
  }

  if (side == LEFT && lastSide == RIGHT && state == RETURN && phase == CSSP) {
    phase = DECE;
    lastSide = side;
  }

  if (lastSide == LEFT && state == STOPPED && phase == IDLE) {
    stopMoving();
    pressed = false;
  }

  if (leftHit && rightHit) {
    stopMoving();
    pressed = false;
  }

  movingPhase();
}