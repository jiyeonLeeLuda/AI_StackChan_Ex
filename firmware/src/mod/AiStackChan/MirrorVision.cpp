#include <M5Unified.h>
#include <Avatar.h>
#include <ArduinoJson.h>
#include "MirrorVision.h"
#include "MirrorState.h"
#include "Reaction.h"
#include "Robot.h"
#include "rootCA/rootCACertificate.h"
#if defined(ENABLE_CAMERA)
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "driver/Camera.h"
#endif

using namespace m5avatar;
extern Avatar avatar;

namespace MirrorVision {

#if !defined(ENABLE_CAMERA)

void poll(void) {}

#else

// The slow HTTPS round-trip runs in a background task so the main loop keeps
// updating the camera preview and touch input. The main loop only does the
// fast parts: grabbing/encoding a frame and applying the classified result.
//
//   main loop poll():  IDLE --capture+encode--> REQUESTED
//   worker task:       REQUESTED --HTTPS--> RESULT_READY (or IDLE on error)
//   main loop poll():  RESULT_READY --apply expression/reaction--> IDLE

static constexpr uint32_t POLL_INTERVAL_MS = 2500;
static constexpr const char* VISION_MODEL = "gpt-4o-mini";

enum class State : uint8_t { Idle, Requested, Busy, ResultReady };
static volatile State state = State::Idle;
static String frameB64;        // main -> worker
static String resExpression;   // worker -> main
static String resGesture;      // worker -> main
static TaskHandle_t visionTask = nullptr;

static const char* PROMPT =
  "You are the eyes of a small robot. Look at the person in the image and "
  "answer ONLY a JSON object, no prose: "
  "{\"expression\":\"happy|surprised|sad|angry|sleepy|neutral\","
  "\"gesture\":\"wink|blow_kiss|finger_gun|wave|hand_heart|none\"}. "
  "Pick the single most likely value for each.";

// Runs in the worker task: HTTPS classify of frameB64.
static bool classify(void){
  String body;
  body.reserve(frameB64.length() + 900);
  body += "{\"model\":\"";
  body += VISION_MODEL;
  body += "\",\"max_tokens\":60,\"temperature\":0,"
          "\"messages\":[{\"role\":\"user\",\"content\":["
          "{\"type\":\"text\",\"text\":\"";
  body += PROMPT;
  body += "\"},{\"type\":\"image_url\",\"image_url\":{\"url\":"
          "\"data:image/jpeg;base64,";
  body += frameB64;
  body += "\",\"detail\":\"low\"}}]}]}";
  frameB64 = String();  // free early

  WiFiClientSecure client;
  client.setCACert(root_ca_openai);
  client.setTimeout(15000);
  HTTPClient https;
  https.setTimeout(15000);
  if (!https.begin(client, "https://api.openai.com/v1/chat/completions")) {
    Serial.println("[MirrorVision] https begin failed");
    return false;
  }
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", String("Bearer ") + robot->llm->param.api_key);
  int code = https.POST(body);
  if (code != 200) {
    Serial.printf("[MirrorVision] http %d\n", code);
    https.end();
    return false;
  }
  String payload = https.getString();
  https.end();

  DynamicJsonDocument doc(4096);
  StaticJsonDocument<256> filter;
  filter["choices"][0]["message"]["content"] = true;
  if (deserializeJson(doc, payload, DeserializationOption::Filter(filter)) != DeserializationError::Ok) {
    Serial.println("[MirrorVision] response parse failed");
    return false;
  }
  String content = doc["choices"][0]["message"]["content"].as<String>();
  int b = content.indexOf('{');
  int e = content.lastIndexOf('}');
  if (b < 0 || e <= b) {
    Serial.printf("[MirrorVision] no JSON in: %s\n", content.c_str());
    return false;
  }
  StaticJsonDocument<256> ans;
  if (deserializeJson(ans, content.substring(b, e + 1)) != DeserializationError::Ok) {
    return false;
  }
  resExpression = ans["expression"].as<String>();
  resGesture = ans["gesture"].as<String>();
  Serial.printf("[MirrorVision] expression=%s gesture=%s\n",
                resExpression.c_str(), resGesture.c_str());
  return true;
}

static void visionTaskLoop(void*){
  for(;;){
    if (state == State::Requested) {
      state = State::Busy;
      bool ok = classify();
      state = ok ? State::ResultReady : State::Idle;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

static void applyExpression(const String& expr){
  if      (expr == "happy")     avatar.setExpression(Expression::Happy);
  else if (expr == "surprised") avatar.setExpression(Expression::Doubt);
  else if (expr == "sad")       avatar.setExpression(Expression::Sad);
  else if (expr == "angry")     avatar.setExpression(Expression::Angry);
  else if (expr == "sleepy")    avatar.setExpression(Expression::Sleepy);
  else                          avatar.setExpression(Expression::Neutral);
}

static void applyGesture(const String& gesture){
  const Reaction* r = nullptr;
  if      (gesture == "blow_kiss")  r = findReactionByName("blow_kiss");
  else if (gesture == "finger_gun") r = findReactionByName("bang");
  else if (gesture == "wave")       r = findReactionByName("wave");
  else if (gesture == "hand_heart") r = findReactionByName("heart");
  else if (gesture == "wink")       r = findReactionByName("smile");  // TODO: real wink (custom eye)
  if (r != nullptr) {
    playReaction(*r);
  }
}

void poll(void){
  if (!MirrorState::isOn()) {
    return;
  }

  // apply a finished classification (fast, runs on the main loop)
  if (state == State::ResultReady) {
    if (resGesture != "none" && resGesture.length() > 0) {
      applyGesture(resGesture);   // gestures win over plain mirroring
    } else {
      applyExpression(resExpression);
    }
    state = State::Idle;
    return;
  }

  // hand a fresh frame to the worker (capture+encode is only ~tens of ms)
  static uint32_t lastCaptureMs = 0;
  uint32_t now = millis();
  if (state == State::Idle && now - lastCaptureMs >= POLL_INTERVAL_MS) {
    lastCaptureMs = now;
    String b64;
    if (!camera_capture_base64(b64) || b64.length() == 0) {
      Serial.println("[MirrorVision] capture failed");
      return;
    }
    frameB64 = std::move(b64);
    if (visionTask == nullptr) {
      xTaskCreate(visionTaskLoop, "mirrorVision", 12 * 1024, nullptr, 1, &visionTask);
    }
    state = State::Requested;
  }
}

#endif  //ENABLE_CAMERA

}  // namespace MirrorVision
