#if defined(ENABLE_COCKPIT)

#include "SequencePlayer.h"
#include <M5Unified.h>
#include <Avatar.h>
#include "Robot.h"

using namespace m5avatar;
extern Avatar avatar;

namespace cockpit {

// 프로토콜 표정(11종) → m5stack-avatar Expression(6종) 최선 매핑.
// avatar enum: { Happy, Angry, Sad, Doubt, Sleepy, Neutral }
//   neutral   → Neutral
//   happy     → Happy
//   surprised → Doubt    (놀람: 전용 표정 없음 → 의아/동공확대 표정으로 대용)
//   wink      → Happy    (전용 윙크 표정 없음 → 긍정 표정)
//   shy       → Happy    (수줍음: 긍정 표정으로 대용)
//   love      → Happy    (애정: 긍정 표정으로 대용)
//   scared    → Sad      (빵야 1단계 겁먹음)
//   dead      → Sleepy   (X_X: 눈 감은 표정으로 대용)
//   dizzy     → Doubt    (@_@: 혼란)
//   sad       → Sad
//   sleepy    → Sleepy
static int mapExpression(const String& e) {
  if (e == "happy")     return (int)Expression::Happy;
  if (e == "surprised") return (int)Expression::Doubt;
  if (e == "wink")      return (int)Expression::Happy;
  if (e == "shy")       return (int)Expression::Happy;
  if (e == "love")      return (int)Expression::Happy;
  if (e == "scared")    return (int)Expression::Sad;
  if (e == "dead")      return (int)Expression::Sleepy;
  if (e == "dizzy")     return (int)Expression::Doubt;
  if (e == "sad")       return (int)Expression::Sad;
  if (e == "sleepy")    return (int)Expression::Sleepy;
  return (int)Expression::Neutral;  // "neutral" 및 미지의 값
}

void SequencePlayer::start(const String& id, JsonArrayConst steps) {
  // 같은 id가 재생 중이면 무시(중복 방지). 다른 id면 끊고 교체.
  if (_playing && id == _currentId) {
    Serial.printf("[cockpit] sequence '%s' already playing, ignore\n", id.c_str());
    return;
  }

  _currentId = id;
  _nSteps = 0;
  for (JsonObjectConst st : steps) {
    if (_nSteps >= MAX_STEPS) break;
    Step& d = _steps[_nSteps];
    d.hasExpression = st.containsKey("expression");
    d.expression    = d.hasExpression ? mapExpression(st["expression"].as<String>()) : 0;
    d.hasServo      = st.containsKey("servo");
    if (d.hasServo) {
      JsonObjectConst sv = st["servo"];
      d.pan     = sv.containsKey("pan")  ? sv["pan"].as<int>()  : 90;
      d.tilt    = sv.containsKey("tilt") ? sv["tilt"].as<int>() : 90;
      d.servoMs = sv.containsKey("ms")   ? sv["ms"].as<int>()   : 300;
    } else {
      d.pan = 90; d.tilt = 90; d.servoMs = 300;
    }
    d.hasText     = st.containsKey("text");
    d.text        = d.hasText ? st["text"].as<String>() : String();
    d.durationMs  = st["durationMs"].as<uint32_t>();  // 필수 필드
    _nSteps++;
  }

  if (_nSteps == 0) {
    _playing = false;
    return;
  }

  _idx = 0;
  _playing = true;
  _stepStartMs = millis();
  applyStep(_steps[0]);
  Serial.printf("[cockpit] sequence '%s' start, %d step(s)\n", id.c_str(), _nSteps);
}

void SequencePlayer::tick() {
  if (!_playing) return;

  uint32_t now = millis();
  if (now - _stepStartMs < _steps[_idx].durationMs) return;

  _idx++;
  if (_idx >= _nSteps) {
    _playing = false;
    // 마지막 스텝 표정이 화면에 영구히 남지 않도록 Neutral로 복귀시킨다.
    // 서보는 건드리지 않음(기존 5초 주기 moveToOrigin이 원위치 처리).
    avatar.setExpression(Expression::Neutral);
    avatar.setSpeechText("");   // 말풍선 정리
    return;
  }
  _stepStartMs = now;
  applyStep(_steps[_idx]);
}

void SequencePlayer::applyStep(const Step& s) {
  if (s.hasExpression) {
    avatar.setExpression((Expression)s.expression);
  }
#ifdef USE_SERVO
  if (s.hasServo && robot != nullptr && robot->servo != nullptr) {
    // 프로토콜 절대각(센터 90) → ServoCustom::moveTo의 센터 기준 상대 오프셋으로 변환.
    robot->servo->moveTo(s.pan - 90, s.tilt - 90, (uint32_t)s.servoMs);
  }
#endif
  if (s.hasText) {
    avatar.setSpeechText(s.text.c_str());
  }
  // led/sound는 이번 범위 밖(capabilities에 없음). 필요 시 후속 단계에서 추가.
}

}  // namespace cockpit

#endif  // ENABLE_COCKPIT
