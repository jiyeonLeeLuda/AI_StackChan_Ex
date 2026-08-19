#include <M5Unified.h>
#include "Reaction.h"
#include "Robot.h"

using namespace m5avatar;
extern Avatar avatar;
extern bool servo_home;

// ---------------------------------------------------------------------------
// Reaction data
// ---------------------------------------------------------------------------
// Servo frame of reference (SG90/MG90S, SC_BasicConfig defaults):
//   X (pan)  0..180, center 90 / Y (tilt) 60..90, center 90 (60 = tilted back)

// "빵야" (finger gun): flinch -> tilt back -> play dead with X eyes -> recover
static const char* const kBangKeywords[] = { "빵야", "빵 야", "bang", nullptr };
static const ReactionStep kBangSteps[] = {
  // expr                balloon   servoX servoY  ms   toneHz toneMs holdMs
  { Expression::Doubt,   nullptr,    -1,   -1,     0,  1800,   80,   350 },  // flinch
  { Expression::Sad,     nullptr,    90,   60,   400,     0,    0,   600 },  // knocked back
  { Expression::Sad,     "X_X",      -1,   -1,     0,   300,  400,  1500 },  // play dead
  { Expression::Neutral, "",         90,   90,   600,     0,    0,   300 },  // recover
};

// "훅" (blow dart): look around -> stagger -> spiral eyes + groan -> recover
static const char* const kDartKeywords[] = { "훅", "독화살", nullptr };
static const ReactionStep kDartSteps[] = {
  { Expression::Doubt,   nullptr,    60,   90,   300,     0,    0,   450 },  // look left
  { Expression::Doubt,   nullptr,   120,   90,   300,     0,    0,   450 },  // look right
  { Expression::Sad,     nullptr,    75,   70,   300,   600,  150,   450 },  // stagger
  { Expression::Sleepy,  "@_@",      105,  70,   300,   420,  200,   500 },  // wobble
  { Expression::Sleepy,  "으윽...",   90,   65,   300,   250,  500,  1200 },  // groan
  { Expression::Neutral, "",         90,   90,   600,     0,    0,   300 },  // recover
};

static const Reaction REACTIONS[] = {
  { "bang", kBangKeywords, kBangSteps, sizeof(kBangSteps) / sizeof(kBangSteps[0]) },
  { "dart", kDartKeywords, kDartSteps, sizeof(kDartSteps) / sizeof(kDartSteps[0]) },
};

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------

const Reaction* matchReaction(const String& sttText){
  for (auto& r : REACTIONS) {
    for (const char* const* kw = r.keywords; *kw != nullptr; kw++) {
      if (sttText.indexOf(*kw) >= 0) {
        return &r;
      }
    }
  }
  return nullptr;
}

void playReaction(const Reaction& r){
  Serial.printf("[Reaction] play: %s\n", r.name);
  bool prev_home = servo_home;
  servo_home = false;   // stop idle wiggling while the choreography runs

  for (size_t i = 0; i < r.nSteps; i++) {
    const ReactionStep& st = r.steps[i];
    avatar.setExpression(st.expr);
    if (st.balloon != nullptr) {
      avatar.setSpeechText(st.balloon);
    }
#ifdef USE_SERVO
    if (st.servoX >= 0 && st.servoY >= 0 && robot != nullptr && robot->servo != nullptr) {
      robot->servo->moveTo(st.servoX, st.servoY, st.servoMs);
    }
#endif
    if (st.toneHz > 0) {
      M5.Speaker.tone(st.toneHz, st.toneMs);
    }
    delay(st.holdMs);
  }

  avatar.setSpeechText("");
  avatar.setExpression(Expression::Neutral);
  servo_home = prev_home;
}
