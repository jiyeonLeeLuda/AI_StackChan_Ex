#if defined(ENABLE_COCKPIT)

#include "DriveController.h"
#include <math.h>

namespace cockpit {

namespace {

// TB6612 제어핀 (사용자 배선 확정값)
//   왼쪽 모터(A):  AIN1=GPIO2(Port.A G2), AIN2=GPIO1(Port.A G1)
//   오른쪽 모터(B): BIN1=GPIO8(Port.B G8), BIN2=GPIO9(Port.B G9)
// 주의: GPIO2는 CoreS3 카메라 XCLK와 동일 핀 → drive와 미러(카메라) 동시 사용 시 충돌.
// 가드 정책(소프트웨어 상호 배제, 2단계 유선 구동 한정):
//   ① drive=true면 미러 모드 진입 차단(MirrorState::toggle에서 조기 반환, camera_init 미호출).
//   ② 이미 미러 ON이면 drive 메시지 무시(CockpitClient handleMessage, 모터 정지 유지).
//   → 하드웨어를 먼저 점유한 쪽이 이긴다. (3단계에서 drive는 ESP32-C3 보드로 분리 예정)
const int PIN_AIN1 = 2;
const int PIN_AIN2 = 1;
const int PIN_BIN1 = 8;
const int PIN_BIN2 = 9;

// LEDC 채널 배정. 서보(ServoEasing/ESP32PWM = timer0 ch0,1 @50Hz)·카메라 XCLK
// (timer0 ch0 @20MHz)와 겹치지 않도록 timer2/timer3(ch4~7)만 사용.
// arduino-esp32 2.0.x 채널→타이머 매핑: timer = (channel/2) % 4.
const int CH_AIN1 = 4;   // timer2
const int CH_AIN2 = 5;   // timer2
const int CH_BIN1 = 6;   // timer3
const int CH_BIN2 = 7;   // timer3

const uint32_t PWM_FREQ_HZ  = 20000;  // 20kHz: 가청 모터 소음 저감
const uint8_t  PWM_RES_BITS = 10;     // 0~1023. 80MHz/20kHz=4000 ≥ 1024 → 유효
const uint32_t PWM_MAX = (1u << PWM_RES_BITS) - 1;

}  // namespace

void DriveController::begin() {
  ledcSetup(CH_AIN1, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcSetup(CH_AIN2, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcSetup(CH_BIN1, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcSetup(CH_BIN2, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcAttachPin(PIN_AIN1, CH_AIN1);
  ledcAttachPin(PIN_AIN2, CH_AIN2);
  ledcAttachPin(PIN_BIN1, CH_BIN1);
  ledcAttachPin(PIN_BIN2, CH_BIN2);
  _began = true;
  stop();   // 부팅 기본값 = 정지
  Serial.println("[cockpit] drive: TB6612 PWM init (ch4-7 / timer2,3 / 20kHz 10bit)");
}

void DriveController::applyMotor(int chIn1, int chIn2, float v) {
  if (v >  1.0f) v =  1.0f;
  if (v < -1.0f) v = -1.0f;
  uint32_t duty = (uint32_t)(fabsf(v) * PWM_MAX + 0.5f);
  if (v > 0.0f) {          // 전진: IN1 PWM / IN2 LOW
    ledcWrite(chIn1, duty);
    ledcWrite(chIn2, 0);
  } else if (v < 0.0f) {   // 후진: IN1 LOW / IN2 PWM
    ledcWrite(chIn1, 0);
    ledcWrite(chIn2, duty);
  } else {                 // 정지(coast): 둘 다 LOW
    ledcWrite(chIn1, 0);
    ledcWrite(chIn2, 0);
  }
}

void DriveController::setDrive(float left, float right, uint32_t ttlMs) {
  if (!_began) return;
  applyMotor(CH_AIN1, CH_AIN2, left);
  applyMotor(CH_BIN1, CH_BIN2, right);
  _expiryMs = millis() + ttlMs;
  _active = true;
}

void DriveController::tick() {
  if (!_began || !_active) return;
  // 부호 있는 차로 millis() 래핑 안전하게 비교(ttlMs ≤ 2000이라 창 안에서 래핑 없음).
  if ((int32_t)(millis() - _expiryMs) >= 0) {
    stop();   // 데드맨: ttl 만료 → 무조건 정지
  }
}

void DriveController::stop() {
  if (!_began) return;
  ledcWrite(CH_AIN1, 0);
  ledcWrite(CH_AIN2, 0);
  ledcWrite(CH_BIN1, 0);
  ledcWrite(CH_BIN2, 0);
  _active = false;
}

}  // namespace cockpit

#endif  // ENABLE_COCKPIT
