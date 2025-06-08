#include <Wire.h>
#include <BH1750.h>
#include <LedControl.h>

// ===== ĐỊNH NGHĨA CHÂN KẾT NỐI =====
// Cảm biến ánh sáng BH1750 (I2C)
#define SDA_PIN 22
#define SCL_PIN 21

// Cảm biến âm thanh MAX9814
#define SOUND_PIN 33

// LED 7 đoạn MAX7219 (SPI)
#define DIN_PIN 23
#define CS_PIN 19
#define CLK_PIN 18

// LED đơn màu (đèn giao thông)
#define LED_RED 5
#define LED_YELLOW 4
#define LED_GREEN 2

// Nút nhấn
#define BTN_ONOFF 34
#define BTN_MODE 35
#define BTN_ADJUST 32
// Nút RESET/EN 

// ===== KHỞI TẠO ĐỐI TƯỢNG =====
BH1750 lightMeter;
LedControl lc = LedControl(DIN_PIN, CLK_PIN, CS_PIN, 1);

// ===== CÁC BIẾN TOÀN CỤC =====
// Ngưỡng cảm biến
const float LIGHT_THRESHOLD = 200.0;  // Lux
const int SOUND_THRESHOLD = 1600;      // ADC value (0-4095)

// Độ sáng LED đơn (PWM: 0-255)
const int LED_BRIGHT = 255;  // Độ sáng cao khi ánh sáng yếu
const int LED_DIM = 50;      // Độ sáng thấp khi ánh sáng mạnh

// Thời gian đèn giao thông (giây)
int redTime = 30;
int yellowTime = 5;
int greenTime = 25;

// Trạng thái hệ thống
enum TrafficState {
  RED_STATE,
  YELLOW_STATE,
  GREEN_STATE
};

enum SystemMode {
  TRAFFIC_TIME_MODE,    // Hiển thị thời gian đèn giao thông
  SENSOR_DISPLAY_MODE   // Hiển thị giá trị cảm biến (BH1750 + MAX9814)
};

TrafficState currentState = RED_STATE;
SystemMode currentMode = TRAFFIC_TIME_MODE;

// Biến thời gian
unsigned long previousMillis = 0;
int countdown = 0;
bool systemOn = true;

// Biến nút nhấn
bool lastBtnOnOff = false;
bool lastBtnMode = false;
bool lastBtnAdjust = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Biến cảm biến
float lightLevel = 0;
int soundLevel = 0;
bool lowLightMode = false;
bool highTrafficMode = false;
int currentLedBrightness = LED_BRIGHT; // Độ sáng hiện tại của LED

// Biến để xử lý đọc cảm biến âm thanh ổn định
unsigned long lastSoundReadTime = 0;
const unsigned long soundReadInterval = 3000; // Đọc cảm biến âm thanh mỗi 3 giây
int soundReadings[5] = {0}; // Mảng lưu 5 lần đọc gần nhất
int soundReadIndex = 0;
int soundAverage = 0;
bool soundReadingComplete = false;

// Biến để xử lý reset system
bool systemInitialized = false;

// ===== HÀM RESET HỆ THỐNG =====
void resetSystemToDefault() {
  Serial.println("=== RESET HỆ THỐNG VỀ TRẠNG THÁI BAN ĐẦU ===");
  
  // Reset tất cả biến về giá trị mặc định
  redTime = 30;
  yellowTime = 5;
  greenTime = 25;
  
  currentState = RED_STATE;
  currentMode = TRAFFIC_TIME_MODE;
  systemOn = true;
  
  countdown = redTime;
  previousMillis = millis();
  
  // Reset trạng thái cảm biến
  lowLightMode = false;
  highTrafficMode = false;
  currentLedBrightness = LED_BRIGHT;
  
  // Reset biến đọc cảm biến âm thanh
  lastSoundReadTime = 0;
  soundReadIndex = 0;
  soundAverage = 0;
  soundReadingComplete = false;
  for (int i = 0; i < 5; i++) {
    soundReadings[i] = 0;
  }
  
  // Reset trạng thái nút nhấn
  lastBtnOnOff = false;
  lastBtnMode = false;
  lastBtnAdjust = false;
  lastDebounceTime = 0;
  
  // Cập nhật hardware
  updateTrafficLights();
  lc.clearDisplay(0);
  
  Serial.println("Hệ thống đã được reset thành công!");
}

// ===== HÀM KHỞI TẠO =====
void setup() {
  Serial.begin(115200);
  delay(1000); // Đợi serial port ổn định
  
  Serial.println("========================================");
  Serial.println("Khởi động hệ thống đèn giao thông thông minh...");
  Serial.println("========================================");
  
  // Khởi tạo I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  
  // Khởi tạo cảm biến ánh sáng
  if (lightMeter.begin()) {
    Serial.println(" Cảm biến BH1750 đã sẵn sàng");
  } else {
    Serial.println(" Lỗi khởi tạo cảm biến BH1750!");
  }
  
  // Khởi tạo LED 7 đoạn
  lc.shutdown(0, false);
  lc.setIntensity(0, 8);
  lc.clearDisplay(0);
  Serial.println(" LED 7 đoạn MAX7219 đã sẵn sàng");
  
  // Khởi tạo chân GPIO
  pinMode(SOUND_PIN, INPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BTN_ONOFF, INPUT_PULLDOWN);
  pinMode(BTN_MODE, INPUT_PULLDOWN);
  pinMode(BTN_ADJUST, INPUT_PULLDOWN);
  Serial.println(" GPIO đã được cấu hình");
  
  // Reset hệ thống về trạng thái mặc định
  resetSystemToDefault();
  
  // Hiển thị thông tin khởi tạo
  Serial.println("========================================");
  Serial.println("THÔNG TIN HỆ THỐNG:");
  Serial.println("- Đèn đỏ: " + String(redTime) + "s");
  Serial.println("- Đèn vàng: " + String(yellowTime) + "s"); 
  Serial.println("- Đèn xanh: " + String(greenTime) + "s");
  Serial.println("- Ngưỡng ánh sáng: " + String(LIGHT_THRESHOLD) + " lux");
  Serial.println("- Ngưỡng âm thanh: " + String(SOUND_THRESHOLD));
  Serial.println("========================================");
  Serial.println("Hệ thống đã sẵn sàng!");
  Serial.println("========================================");
  
  systemInitialized = true;
}

// ===== HÀM CHÍNH =====
void loop() {
  if (systemOn) {
    readSensors();
    processButtons();
    updateTrafficLogic();
    updateDisplay();
  } else {
    // Khi hệ thống tắt, vẫn xử lý nút để có thể bật lại
    processButtons();
  }
  
  delay(100);
}

// ===== ĐỌC DỮ LIỆU CẢM BIẾN =====
void readSensors() {
  // Đọc cảm biến ánh sáng (đọc liên tục vì ổn định)
  lightLevel = lightMeter.readLightLevel();
  
  // Đọc cảm biến âm thanh (đọc theo chu kỳ để ổn định)
  unsigned long currentTime = millis();
  if (currentTime - lastSoundReadTime >= soundReadInterval) {
    // Đọc giá trị âm thanh mới
    int rawSound = analogRead(SOUND_PIN);
    
    // Lưu vào mảng
    soundReadings[soundReadIndex] = rawSound;
    soundReadIndex = (soundReadIndex + 1) % 5;
    
    // Tính trung bình của 5 lần đọc
    int sum = 0;
    for (int i = 0; i < 5; i++) {
      sum += soundReadings[i];
    }
    soundAverage = sum / 5;
    soundLevel = soundAverage; // Cập nhật giá trị hiển thị
    
    lastSoundReadTime = currentTime;
    soundReadingComplete = true;
    
    Serial.println(" Âm thanh: " + String(rawSound) + " (Trung bình: " + String(soundAverage) + ")");
  }
  
  // Xử lý điều kiện ánh sáng - LOGIC ĐÃ SỬA cho LED đơn
  if (lightLevel < LIGHT_THRESHOLD) {
    // Ánh sáng YẾU (< 200 lux) -> LED sáng MẠNH
    if (!lowLightMode) {
      lowLightMode = true;
      currentLedBrightness = LED_BRIGHT;
      updateTrafficLights(); // Cập nhật lại độ sáng LED
      Serial.println(" Ánh sáng yếu (" + String(lightLevel) + " lux) -> LED sáng mạnh (PWM: " + String(LED_BRIGHT) + "/255)");
    }
  } else {
    // Ánh sáng MẠNH (>= 200 lux) -> LED sáng YẾU
    if (lowLightMode) {
      lowLightMode = false;
      currentLedBrightness = LED_DIM;
      updateTrafficLights(); // Cập nhật lại độ sáng LED
      Serial.println(" Ánh sáng mạnh (" + String(lightLevel) + " lux) -> LED sáng yếu (PWM: " + String(LED_DIM) + "/255)");
    }
  }
  
  // Xử lý điều kiện âm thanh (giao thông đông đúc) - chỉ khi đã có đủ dữ liệu
  if (soundReadingComplete) {
    if (soundAverage > SOUND_THRESHOLD) {
      if (!highTrafficMode) {
        highTrafficMode = true;
        Serial.println(" Phát hiện giao thông đông đúc (Âm thanh TB: " + String(soundAverage) + " > " + String(SOUND_THRESHOLD) + ")");
      }
    } else {
      if (highTrafficMode) {
        highTrafficMode = false;
        Serial.println(" Giao thông bình thường (Âm thanh TB: " + String(soundAverage) + " <= " + String(SOUND_THRESHOLD) + ")");
      }
    }
  }
}

// ===== XỬ LÝ NÚT NHẤN =====
void processButtons() {
  unsigned long currentTime = millis();
  
  // Đọc trạng thái nút
  bool btnOnOff = digitalRead(BTN_ONOFF);
  bool btnMode = digitalRead(BTN_MODE);
  bool btnAdjust = digitalRead(BTN_ADJUST);
  
  // Xử lý nút ON/OFF
  if (btnOnOff && !lastBtnOnOff && (currentTime - lastDebounceTime) > debounceDelay) {
    systemOn = !systemOn;
    lastDebounceTime = currentTime;
    
    if (systemOn) {
      Serial.println(" Hệ thống Khởi Động");
      countdown = redTime;
      currentState = RED_STATE;
      previousMillis = millis(); // Reset thời gian
      updateTrafficLights();
    } else {
      Serial.println(" Hệ thống TẮT");
      turnOffAllLights();
      lc.clearDisplay(0);
    }
  }
  
  // Xử lý nút MODE (chỉ khi hệ thống đang bật)
  if (systemOn && btnMode && !lastBtnMode && (currentTime - lastDebounceTime) > debounceDelay) {
    currentMode = (SystemMode)((currentMode + 1) % 2);
    lastDebounceTime = currentTime;
    
    switch (currentMode) {
      case TRAFFIC_TIME_MODE:
        Serial.println(" Chế độ: Hiển thị thời gian đèn giao thông");
        break;
      case SENSOR_DISPLAY_MODE:
        Serial.println(" Chế độ: Hiển thị cảm biến (BH1750 + MAX9814)");
        break;
    }
  }
  
  // Xử lý nút ADJUST (chỉ khi hệ thống đang bật)
  if (systemOn && btnAdjust && !lastBtnAdjust && (currentTime - lastDebounceTime) > debounceDelay) {
    lastDebounceTime = currentTime;
    
    if (currentMode == TRAFFIC_TIME_MODE) {
      // Điều chỉnh thời gian theo trạng thái hiện tại
      switch (currentState) {
        case RED_STATE:
          redTime += 5;
          if (redTime > 60) redTime = 10;
          countdown = redTime;
          Serial.println(" Thời gian đèn đỏ: " + String(redTime) + "s");
          break;
        case YELLOW_STATE:
          yellowTime += 1;
          if (yellowTime > 10) yellowTime = 3;
          countdown = yellowTime;
          Serial.println(" Thời gian đèn vàng: " + String(yellowTime) + "s");
          break;
        case GREEN_STATE:
          greenTime += 5;
          if (greenTime > 60) greenTime = 10;
          countdown = greenTime;
          Serial.println(" Thời gian đèn xanh: " + String(greenTime) + "s");
          break;
      }
    }
  }
  
  // Lưu trạng thái nút
  lastBtnOnOff = btnOnOff;
  lastBtnMode = btnMode;
  lastBtnAdjust = btnAdjust;
}

// ===== LOGIC ĐÈN GIAO THÔNG =====
void updateTrafficLogic() {
  unsigned long currentMillis = millis();
  
  // Cập nhật đếm ngược mỗi giây
  if (currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;
    countdown--;
    
    // Điều chỉnh thời gian dựa trên giao thông
    if (currentState == GREEN_STATE && highTrafficMode && countdown <= 5) {
      countdown += 10; // Tăng thêm 10s cho đèn xanh khi kẹt xe
      Serial.println(" Tăng thời gian đèn xanh do giao thông đông đúc");
    }
    
    // Chuyển đổi trạng thái đèn
    if (countdown <= 0) {
      switch (currentState) {
        case RED_STATE:
          currentState = GREEN_STATE;
          countdown = greenTime;
          Serial.println(" Chuyển sang đèn XANH (" + String(countdown) + "s)");
          break;
        case GREEN_STATE:
          currentState = YELLOW_STATE;
          countdown = yellowTime;
          Serial.println(" Chuyển sang đèn VÀNG (" + String(countdown) + "s)");
          break;
        case YELLOW_STATE:
          currentState = RED_STATE;
          countdown = redTime;
          // Giảm thời gian đèn đỏ khi giao thông thưa
          if (!highTrafficMode && countdown > 15) {
            countdown = 15;
            Serial.println(" Giảm thời gian đèn đỏ do giao thông thưa");
          }
          Serial.println(" Chuyển sang đèn ĐỎ (" + String(countdown) + "s)");
          break;
      }
      
      updateTrafficLights();
    }
  }
}

// ===== CẬP NHẬT ĐÈN GIAO THÔNG =====
void updateTrafficLights() {
  // Tắt tất cả đèn
  analogWrite(LED_RED, 0);
  analogWrite(LED_YELLOW, 0);
  analogWrite(LED_GREEN, 0);
  
  // Bật đèn theo trạng thái với độ sáng được điều chỉnh
  switch (currentState) {
    case RED_STATE:
      analogWrite(LED_RED, currentLedBrightness);
      break;
    case YELLOW_STATE:
      analogWrite(LED_YELLOW, currentLedBrightness);
      break;
    case GREEN_STATE:
      analogWrite(LED_GREEN, currentLedBrightness);
      break;
  }
}

// ===== CẬP NHẬT HIỂN THỊ =====
void updateDisplay() {
  switch (currentMode) {
    case TRAFFIC_TIME_MODE:
      displayTrafficTime();
      break;
    case SENSOR_DISPLAY_MODE:
      displaySensorValues();
      break;
  }
}

// ===== HIỂN THỊ THỜI GIAN ĐÈN GIAO THÔNG =====
void displayTrafficTime() {
  // Hiển thị đếm ngược (bên phải) và ký tự trạng thái đèn (bên trái)
  char stateChar = ' ';
  
  switch (currentState) {
    case RED_STATE:
      stateChar = 'R';
      break;
    case YELLOW_STATE:
      stateChar = 'Y';
      break;
    case GREEN_STATE:
      stateChar = 'G';
      break;
  }
  
  // Hiển thị ký tự trạng thái ở digit 7 (bên trái nhất)
  lc.setChar(0, 7, stateChar, false);
  
  // Xóa các digit ở giữa
  for (int i = 4; i < 7; i++) {
    lc.setChar(0, i, ' ', false);
  }
  
  // Hiển thị đếm ngược ở 3 digit bên phải
  if (countdown < 100) {
    lc.setChar(0, 3, ' ', false);
    lc.setDigit(0, 2, countdown / 10, false);
    lc.setDigit(0, 1, countdown % 10, false);
  } else {
    lc.setDigit(0, 3, countdown / 100, false);
    lc.setDigit(0, 2, (countdown / 10) % 10, false);
    lc.setDigit(0, 1, countdown % 10, false);
  }
  
  // Digit 0 trống
  lc.setChar(0, 0, ' ', false);
}

// ===== HIỂN THỊ GIÁ TRỊ CẢM BIẾN =====
void displaySensorValues() {
  // Bên trái (digit 7,6,5,4): Giá trị ánh sáng BH1750 (lux)
  int lightValue = (int)lightLevel;
  if (lightValue > 9999) lightValue = 9999;
  
  lc.setDigit(0, 7, lightValue / 1000, false);
  lc.setDigit(0, 6, (lightValue / 100) % 10, false);
  lc.setDigit(0, 5, (lightValue / 10) % 10, false);
  lc.setDigit(0, 4, lightValue % 10, false);
  
  // Bên phải (digit 3,2,1,0): Giá trị âm thanh MAX9814 (ADC)
  int soundValue = soundLevel;
  if (soundValue > 9999) soundValue = 9999;
  
  lc.setDigit(0, 3, soundValue / 1000, false);
  lc.setDigit(0, 2, (soundValue / 100) % 10, false);
  lc.setDigit(0, 1, (soundValue / 10) % 10, false);
  lc.setDigit(0, 0, soundValue % 10, false);
}

// ===== TẮT TẤT CẢ ĐÈN =====
void turnOffAllLights() {
  analogWrite(LED_RED, 0);
  analogWrite(LED_YELLOW, 0);
  analogWrite(LED_GREEN, 0);
}