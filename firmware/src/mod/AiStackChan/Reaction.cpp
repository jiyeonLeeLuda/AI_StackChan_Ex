#include <M5Unified.h>
#include "Reaction.h"
#include "Robot.h"
#include "share/Mutex.h"

using namespace m5avatar;
extern Avatar avatar;
extern bool servo_home;

// ---------------------------------------------------------------------------
// Reaction data
// ---------------------------------------------------------------------------
// Servo values are OFFSETS from the configured origin (ServoCustom::moveTo
// adds them to start_degree). Pan: +right/-left; tilt: negative = back.

// "빵야" (finger gun): flinch -> tilt back -> play dead with X eyes -> recover
static const char* const kBangKeywords[] = { "빵야", "빵 야", "빵아", "방야", "팡야", "빵냐", "bang", "Bang", nullptr };
static const ReactionStep kBangSteps[] = {
  // expr                balloon   servoX servoY  ms   toneHz toneMs holdMs
  { Expression::Doubt,   nullptr,     0,    0,     0,  1800,   80,   350 },  // flinch
  { Expression::Sad,     nullptr,     0,  -30,   400,     0,    0,   600 },  // knocked back
  { Expression::Sad,     "X_X",       0,    0,     0,   300,  400,  1500 },  // play dead
  { Expression::Neutral, "",          0,    0,   600,     0,    0,   300 },  // recover
};

// "훅" (blow dart): look around -> stagger -> spiral eyes + groan -> recover
static const char* const kDartKeywords[] = { "훅", "후욱", "독화살", "슉", nullptr };
static const ReactionStep kDartSteps[] = {
  { Expression::Doubt,   nullptr,   -30,    0,   300,     0,    0,   450 },  // look left
  { Expression::Doubt,   nullptr,    30,    0,   300,     0,    0,   450 },  // look right
  { Expression::Sad,     nullptr,   -15,  -20,   300,   600,  150,   450 },  // stagger
  { Expression::Sleepy,  "@_@",      15,  -20,   300,   420,  200,   500 },  // wobble
  { Expression::Sleepy,  "으윽...",    0,  -25,   300,   250,  500,  1200 },  // groan
  { Expression::Neutral, "",          0,    0,   600,     0,    0,   300 },  // recover
};

// Blow kiss: surprised -> heart eyes -> shy (3 stages, ~1s each)
static const char* const kNoKeywords[] = { nullptr };
static const ReactionStep kBlowKissSteps[] = {
  { Expression::Doubt,   "!",         0,    0,     0,  1500,  120,  1000 },  // surprised
  { Expression::Happy,   "\u2665 \u2665",  0,  0,  0,  1800,  150,  1000 },  // heart eyes
  { Expression::Happy,   ">///<",     0,    0,     0,     0,    0,  1000 },  // shy
  { Expression::Neutral, "",          0,    0,     0,     0,    0,   200 },
};

// Wave back: swing the pan servo left/right like waving hello
static const ReactionStep kWaveSteps[] = {
  { Expression::Happy,   "안녕!",    -20,    0,   250,  1200,  100,   350 },
  { Expression::Happy,   nullptr,    20,    0,   250,     0,    0,   350 },
  { Expression::Happy,   nullptr,   -20,    0,   250,     0,    0,   350 },
  { Expression::Happy,   nullptr,    20,    0,   250,     0,    0,   350 },
  { Expression::Neutral, "",          0,    0,   400,     0,    0,   200 },
};

// Hand heart: show hearts on screen
static const ReactionStep kHeartSteps[] = {
  { Expression::Happy,   "\u2665",    0,    0,     0,  1600,  120,   800 },
  { Expression::Happy,   "\u2665 \u2665 \u2665", 0, 0, 0, 1900, 120, 1200 },
  { Expression::Neutral, "",          0,    0,     0,     0,    0,   200 },
};

// Mimic smile / surprise (single-step, used by the vision mirror)
static const ReactionStep kSmileSteps[] = {
  { Expression::Happy,   nullptr,     0,    0,     0,     0,    0,   300 },
};
static const ReactionStep kSurpriseSteps[] = {
  { Expression::Doubt,   "!",         0,    0,     0,  1500,  100,   800 },
  { Expression::Doubt,   "",          0,    0,     0,     0,    0,   100 },
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

  // The mic normally owns the I2S port; swap to the speaker for the whole
  // sequence (same pattern as sw_tone(); calling Speaker.tone() while the
  // mic is active crashes/reboots).
  enterMutexAudio();
  M5.Mic.end();
  M5.Speaker.begin();
  delay(50);

  for (size_t i = 0; i < r.nSteps; i++) {
    const ReactionStep& st = r.steps[i];
    avatar.setExpression(st.expr);
    if (st.balloon != nullptr) {
      avatar.setSpeechText(st.balloon);
    }
#ifdef USE_SERVO
    if (st.servoMs > 0 && robot != nullptr && robot->servo != nullptr) {
      robot->servo->moveTo(st.servoX, st.servoY, st.servoMs);
    }
#endif
    if (st.toneHz > 0) {
      M5.Speaker.tone(st.toneHz, st.toneMs);
    }
    delay(st.holdMs);
  }

  // let any queued tone finish before tearing the speaker down
  uint32_t tmo = millis();
  while(M5.Speaker.isPlaying() && millis() - tmo < 1500){ delay(10); }
  M5.Speaker.end();
  M5.Mic.begin();
  exitMutexAudio();

  avatar.setSpeechText("");
  avatar.setExpression(Expression::Neutral);
  servo_home = prev_home;
}
