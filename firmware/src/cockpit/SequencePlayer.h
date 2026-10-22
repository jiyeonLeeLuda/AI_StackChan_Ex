#if defined(ENABLE_COCKPIT)
#ifndef _COCKPIT_SEQUENCE_PLAYER_H
#define _COCKPIT_SEQUENCE_PLAYER_H

#include <Arduino.h>
#include <ArduinoJson.h>

namespace cockpit {

// 서버가 내려준 sequence(표정·서보·텍스트 스텝 배열)를 비블로킹 상태머신으로 재생한다.
// 설계 의도: loop()/태스크를 막지 않기 위해 start()는 스텝을 복사만 하고 즉시 반환하며,
// 실제 스텝 전환은 tick()을 주기적으로 호출할 때 durationMs 경과를 보고 진행한다.
class SequencePlayer {
public:
  // steps: ServerMessage "sequence"의 steps 배열. 같은 id가 재생 중이면 무시(중복 방지).
  void start(const String& id, JsonArrayConst steps);
  // 주기 호출. 현재 스텝의 durationMs가 지나면 다음 스텝으로 넘어간다.
  void tick();
  bool isPlaying() const { return _playing; }

private:
  static const int MAX_STEPS = 16;   // 한 시퀀스가 가질 수 있는 최대 스텝 수

  struct Step {
    bool     hasExpression;
    int      expression;   // m5avatar::Expression으로 캐스팅할 int (매핑은 start에서 수행)
    bool     hasServo;
    int      pan;          // 프로토콜 절대각(0-180), 센터 90
    int      tilt;         // 프로토콜 절대각(50-130), 센터 90
    int      servoMs;      // 목표각까지 이동 시간
    bool     hasText;
    String   text;
    uint32_t durationMs;
  };

  void applyStep(const Step& s);

  bool     _playing = false;
  String   _currentId;
  Step     _steps[MAX_STEPS];
  int      _nSteps = 0;
  int      _idx = 0;
  uint32_t _stepStartMs = 0;
};

}  // namespace cockpit

#endif  // _COCKPIT_SEQUENCE_PLAYER_H
#endif  // ENABLE_COCKPIT
