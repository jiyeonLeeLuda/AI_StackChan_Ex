#if defined(ENABLE_COCKPIT)
#ifndef _COCKPIT_DRIVE_CONTROLLER_H
#define _COCKPIT_DRIVE_CONTROLLER_H

#include <Arduino.h>

namespace cockpit {

// TB6612 2채널 모터 드라이버 제어 + 데드맨 안전장치.
// STBY/PWMA/PWMB가 하드웨어로 HIGH 고정이라 속도·방향 모두 AIN/BIN 핀 PWM으로 제어한다.
//   전진 = IN1 PWM / IN2 LOW,  후진 = IN1 LOW / IN2 PWM,  정지 = 둘 다 LOW(coast)
// 모든 메서드는 cockpit WS 태스크 컨텍스트에서만 호출된다(단일 스레드, begin 제외).
class DriveController {
public:
  // LEDC PWM 채널 초기화. drive=true일 때만 호출(이때만 핀을 건드린다).
  void begin();
  // drive 명령 적용. left/right ∈ [-1,1](부호=방향, 절대값=듀티). ttlMs 후 자동 정지.
  void setDrive(float left, float right, uint32_t ttlMs);
  // 주기 호출: ttl 만료 시 정지(데드맨).
  void tick();
  // 즉시 정지(coast). 연결 끊김/에러 경로에서 호출.
  void stop();
  // ttl 유효 구간이면 true (state.mode="driving" 판정).
  bool isActive() const { return _active; }

private:
  void applyMotor(int chIn1, int chIn2, float v);

  bool     _began = false;
  bool     _active = false;
  uint32_t _expiryMs = 0;
};

}  // namespace cockpit

#endif  // _COCKPIT_DRIVE_CONTROLLER_H
#endif  // ENABLE_COCKPIT
