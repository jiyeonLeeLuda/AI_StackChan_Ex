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

// Blow kiss: surprised -> heart eyes -> shy (3 stages, ~1s each)
static const char* const kNoKeywords[] = { nullptr };
static const ReactionStep kBlowKissSteps[] = {
  { Expression::Doubt,   "!",        -1,   -1,     0,  1500,  120,  1000 },  // surprised
  { Expression::Happy,   "\u2665 \u2665", -1, -1,  0,  1800,  150,  1000 },  // heart eyes
  { Expression::Happy,   ">///<",    -1,   -1,     0,     0,    0,  1000 },  // shy
  { Expression::Neutral, "",         -1,   -1,     0,     0,    0,   200 },
};

// Wave back: swing the pan servo left/right like waving hello
static const ReactionStep kWaveSteps[] = {
  { Expression::Happy,   "안녕!",     70,   90,   250,  1200,  100,   350 },
  { Expression::Happy,   nullptr,   110,   90,   250,     0,    0,   350 },
  { Expression::Happy,   nullptr,    70,   90,   250,     0,    0,   350 },
  { Expression::Happy,   nullptr,   110,   90,   250,     0,    0,   350 },
  { Expression::Neutral, "",         90,   90,   400,     0,    0,   200 },
};

// Hand heart: show hearts on screen
static const ReactionStep kHeartSteps[] = {
  { Expression::Happy,   "\u2665",   -1,   -1,     0,  1600,  120,   800 },
  { Expression::Happy,   "\u2665 \u2665 \u2665", -1, -1, 0, 1900, 120, 1200 },
  { Expression::Neutral, "",         -1,   -1,     0,     0,    0,   200 },
};

// Mimic smile / surprise (single-step, used by the vision mirror)
static const ReactionStep kSmileSteps[] = {
  { Expression::Happy,   nullptr,    -1,   -1,     0,     0,    0,   300 },
};
static const ReactionStep kSurpriseSteps[] = {
  { Expression::Doubt,   "!",        -1,   -1,     0,  1500,  100,   800 },
  { Expression::Doubt,   "",         -1,   -1,     0,     0,    0,   100 },
};

static const Reaction REACTIONS[] = {
  { "bang",      kBangKeywords, kBangSteps,     sizeof(kBangSteps) / sizeof(kBangSteps[0]) },
  { "dart",      kDartKeywords, kDartSteps,     sizeof(kDartSteps) / sizeof(kDartSteps[0]) },
  { "blow_kiss", kNoKeywords,   kBlowKissSteps, sizeof(kBlowKissSteps) / sizeof(kBlowKissSteps[0]) },
  { "wave",      kNoKeywords,   kWaveSteps,     sizeof(kWaveSteps) / sizeof(kWaveSteps[0]) },
  { "heart",     kNoKeywords,   kHeartSteps,    sizeof(kHeartSteps) / sizeof(kHeartSteps[0]) },
  { "smile",     kNoKeywords,   kSmileSteps,    sizeof(kSmileSteps) / sizeof(kSmileSteps[0]) },
  { "surprise",  kNoKeywords,   kSurpriseSteps, sizeof(kSurpriseSteps) / sizeof(kSurpriseSteps[0]) },
};

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------

const Reaction* findReactionByName(const char* name){
  for (auto& r : REACTIONS) {
    if (strcmp(r.name, name) == 0) {
      return &r;
    }
  }
  return nullptr;
}

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
