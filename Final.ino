//======================================== Khai báo chân ===========================v=============//
const uint8_t LEFT_LIMIT      = A1;
const uint8_t RIGHT_LIMIT     = A2;
const uint8_t spraySwitch     = A3;   // Công tắc bật/tắt hệ tưới (enable)
const uint8_t startButton     = A4;   // Nút khởi động 

// Motor - kênh A L298N
const uint8_t ENA = 9;               // Chân Enable A của module L298N
const uint8_t IN1 = 7;               // IN1 L298N
const uint8_t IN2 = 8;               // IN2 L298N

// Bơm nước - kênh B L298N
const uint8_t ENB = 10;              // PWM bơm nước
const uint8_t IN3 = 6;               // IN3 L298N
const uint8_t IN4 = 13;              // IN4 L298N

// Van nước
const uint8_t leftValve  = 11;       // Van nước bên trái
const uint8_t rightValve = 12;       // Van nước bên phải

// Biến trở chỉnh công suất bơm
const uint8_t potPin = A5;           // Biến trở chỉnh công suất bơm (PWM)

//======================================== Điều khiển motor ========================================//
// Điều khiển tốc độ quay
const int PWM_MAX = 200;   // PWM tối đa cho motor
const int PWM_MIN = 0;     // PWM tối thiểu
int currentPWM = PWM_MIN;  // PWM hiện tại

// PWM tối thiểu để motor khởi động
const int MOTOR_PWM_START = 80; 

// Xác định trạng thái hoạt động
bool buttonState      = LOW;
bool leftHit          = LOW;
bool rightHit         = HIGH;
bool lastButtonState  = LOW;
bool pressed          = false;

// Biến thời gian cho motor
unsigned long now        = 0;
unsigned long lastStepMs = 0;
const unsigned long ACCE_INTERVAL = 20;  
const unsigned long DECE_INTERVAL = 10;

// Mô tả chuyển động
enum movingPhases {IDLE, ACCE, CSSP, DECE};
movingPhases phase = IDLE;

enum movingStates {FORWARD, RETURN, STOPPED};
movingStates state = STOPPED;

enum sides {LEFT, RIGHT, NONE};
sides side     = NONE;
sides lastSide = NONE;

// Cờ báo đang thực hiện một chu trình
bool cycleRunning = false;

//---------------------------------------- Hàm điều khiển motor ----------------------------------------//
// Bắt đầu chuyển động
void startMoving(movingStates newState) {
  state = newState;
  setDirection(state);

  currentPWM = MOTOR_PWM_START;
  if (currentPWM > PWM_MAX) currentPWM = PWM_MAX;
  analogWrite(ENA, currentPWM);   // cho motor quay ngay lập tức

  phase      = ACCE;
  lastStepMs = millis();
}

// Dừng chuyển động
void stopMoving() {
  state      = STOPPED;
  phase      = IDLE;
  currentPWM = PWM_MIN;
  analogWrite(ENA, PWM_MIN);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

// Xử lý pha chuyển động
void movingPhase () {
  now = millis();

  switch (phase) {
    case IDLE:
      break;

    case ACCE:
      if (now - lastStepMs >= ACCE_INTERVAL) {
        if (currentPWM < PWM_MAX) {
          currentPWM++;
          if (currentPWM < MOTOR_PWM_START) currentPWM = MOTOR_PWM_START;
        } else {
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
        } else {
          phase = IDLE;
          state = STOPPED;
          stopMoving();
        }
        lastStepMs = now;
      }
      break;
  }

  if (state != STOPPED) {
    analogWrite(ENA, currentPWM);
  } else {
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

// Xác định đã chạm cạnh bên
void sideCheck () {
  if (leftHit && !rightHit)      side = LEFT;
  else if (rightHit && !leftHit) side = RIGHT;
  else                           side = NONE;
}

// Xác nhận đã nhấn nút start
void buttonCheck() {
  if (buttonState != lastButtonState) {
    if (buttonState && state == STOPPED && phase == IDLE && lastSide == LEFT) {
      pressed = true;
    }
    lastButtonState = buttonState;
  }
}

//======================================== Điều khiển bơm nước ========================================//
const uint32_t POT_READ_INTERVAL = 1000; // Chu kỳ cập nhật bơm
const uint8_t PWM_DELTA_MIN = 1;         // Ngưỡng thay đổi PWM tối thiểu
const uint8_t PWM_MIN_ACTIVE = 0;

// ================== Biến trạng thái tưới ==================
unsigned long lastPotReadTime = 0;
bool pumpOn  = false;
int  lastPWM = -1;                 

// ================== Hàm tiện ích tưới ==================
// Ghi PWM nếu thay đổi đáng kể 
void writePumpPWM(int pwm) {
  if (pwm < 0)   pwm = 0;
  if (pwm > 255) pwm = 255;

  if (lastPWM < 0) {
    analogWrite(ENB, pwm);
    lastPWM = pwm;
    return;
  }
  if (abs(pwm - lastPWM) >= PWM_DELTA_MIN) {
    analogWrite(ENB, pwm);
    lastPWM = pwm;
  }
}

// ================== Điều khiển van theo hướng xe ==================
void updateValvesByMotion() {
  if (!pumpOn) {
    digitalWrite(leftValve,  LOW);
    digitalWrite(rightValve, LOW);
    return;
  }

  if (state == FORWARD) {
    digitalWrite(leftValve,  LOW);
    digitalWrite(rightValve, HIGH);
  } else if (state == RETURN) {
    digitalWrite(leftValve,  HIGH);
    digitalWrite(rightValve, LOW);
  } else {
    digitalWrite(leftValve,  LOW);
    digitalWrite(rightValve, LOW);
  }
}

void sprayOn() {
  unsigned long t = millis();
  if (t - lastPotReadTime >= POT_READ_INTERVAL) {
    lastPotReadTime = t; 

    int pot = analogRead(potPin);
    int pwm = map(pot, 0, 1023, 0, 255);

    if (PWM_MIN_ACTIVE > 0 && pwm > 0 && pwm < PWM_MIN_ACTIVE) {
      pwm = PWM_MIN_ACTIVE;
    }

    pwm    = constrain(pwm, 0, 255);
    pumpOn = (pwm > 0); 
    writePumpPWM(pwm);

    updateValvesByMotion();
  }
}

//======================================== Thiết lập ========================================//
void setup(){
  // Motor L298N - kênh A
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);

  // Nút & công tắc hành trình
  pinMode(startButton, INPUT); 
  pinMode(LEFT_LIMIT,  INPUT);
  pinMode(RIGHT_LIMIT, INPUT);

  // Van & bơm
  pinMode(leftValve,  OUTPUT);
  pinMode(rightValve, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  pinMode(spraySwitch, INPUT);

  // Trạng thái ban đầu xe trượt
  side            = LEFT;
  lastSide        = LEFT;
  lastButtonState = digitalRead(startButton);
  stopMoving();

  cycleRunning    = false;

  writePumpPWM(0);

  lastPotReadTime = millis();

  Serial.begin(9600);
}

//======================================== Vòng lặp chính ========================================//
void loop () {
  // ---------- Đọc input ----------
  leftHit     = digitalRead(LEFT_LIMIT);
  rightHit    = digitalRead(RIGHT_LIMIT);
  buttonState = digitalRead(startButton);
  bool sprayEnabled = (digitalRead(spraySwitch) == HIGH);

  buttonCheck();
  sideCheck();

  if (leftHit && rightHit) {
    stopMoving();
    pressed       = false;
    cycleRunning  = false;
    pumpOn        = false;
    writePumpPWM(0);
    digitalWrite(leftValve,  LOW);
    digitalWrite(rightValve, LOW);
  }

  if (lastSide == LEFT && state == STOPPED && phase == IDLE) {
    stopMoving();
  }

  if (pressed == true && lastSide == LEFT && state == STOPPED && phase == IDLE) {
    startMoving(FORWARD);
    cycleRunning = true;            // bắt đầu 1 chu trình mới
    pressed      = false;
  }

  if (side == RIGHT && lastSide == LEFT && state == FORWARD &&
      (phase == ACCE || phase == CSSP)) {
    phase    = DECE;
    lastSide = side;
  }

  if (lastSide == RIGHT && state == STOPPED && phase == IDLE && cycleRunning) {
    startMoving(RETURN);
  }

  if (side == LEFT && lastSide == RIGHT && state == RETURN &&
      (phase == ACCE || phase == CSSP)) {
    phase    = DECE;
    lastSide = side;
  }

  if (cycleRunning && lastSide == LEFT && state == STOPPED && phase == IDLE) {
    cycleRunning  = false;    // kết thúc chu trình
    pumpOn        = false;
    writePumpPWM(0);
    digitalWrite(leftValve,  LOW);
    digitalWrite(rightValve, LOW);
  }

  movingPhase();

  if (!sprayEnabled || !cycleRunning) {
    pumpOn = false;
    writePumpPWM(0);
    digitalWrite(leftValve,  LOW);
    digitalWrite(rightValve, LOW);
    lastPotReadTime = millis(); 
  }
  else {
    sprayOn(); 
  }

  // ---------- Debug ----------
  static unsigned long lastDbg = 0;
  if (millis() - lastDbg >= 500) {
    lastDbg = millis();
    Serial.print("leftHit=");      Serial.print(leftHit);
    Serial.print(" rightHit=");    Serial.print(rightHit);
    Serial.print(" side=");        Serial.print(side);
    Serial.print(" lastSide=");    Serial.print(lastSide);
    Serial.print(" state=");       Serial.print(state);
    Serial.print(" phase=");       Serial.print(phase);
    Serial.print(" cycleRunning=");Serial.print(cycleRunning);
    Serial.print(" sprayEn=");     Serial.print(sprayEnabled);
    Serial.print(" pumpOn=");      Serial.print(pumpOn);
    Serial.print(" LV=");          Serial.print(digitalRead(leftValve));
    Serial.print(" RV=");          Serial.print(digitalRead(rightValve));
    int pot = analogRead(potPin);
    Serial.print(" potRaw=");      Serial.println(pot);
  }
}
