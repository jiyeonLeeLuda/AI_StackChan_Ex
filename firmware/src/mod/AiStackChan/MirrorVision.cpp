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

static constexpr uint32_t POLL_INTERVAL_MS = 2500;
static constexpr const char* VISION_MODEL = "gpt-4o-mini";

static const char* PROMPT =
  "You are the eyes of a small robot. Look at the person in the image and "
  "answer ONLY a JSON object, no prose: "
  "{\"expression\":\"happy|surprised|sad|angry|sleepy|neutral\","
  "\"gesture\":\"wink|blow_kiss|finger_gun|wave|hand_heart|none\"}. "
  "Pick the single most likely value for each.";

// One vision round-trip: capture -> classify -> {expression, gesture}
static bool classifyFrame(String& expression, String& gesture){
  String b64;
  if (!camera_capture_base64(b64) || b64.length() == 0) {
    Serial.println("[MirrorVision] capture failed");
    return false;
  }

  String body;
  body.reserve(b64.length() + 900);
  body += "{\"model\":\"";
  body += VISION_MODEL;
  body += "\",\"max_tokens\":60,\"temperature\":0,"
          "\"messages\":[{\"role\":\"user\",\"content\":["
          "{\"type\":\"text\",\"text\":\"";
  body += PROMPT;
  body += "\"},{\"type\":\"image_url\",\"image_url\":{\"url\":"
          "\"data:image/jpeg;base64,";
  body += b64;
  body += "\",\"detail\":\"low\"}}]}]}";

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

  // choices[0].message.content holds the small JSON answer
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
  expression = ans["expression"].as<String>();
  gesture = ans["gesture"].as<String>();
  Serial.printf("[MirrorVision] expression=%s gesture=%s\n", expression.c_str(), gesture.c_str());
  return true;
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
  static uint32_t lastPollMs = 0;
  uint32_t now = millis();
  if (now - lastPollMs < POLL_INTERVAL_MS) {
    return;
  }
  lastPollMs = now;

  String expression, gesture;
  if (!classifyFrame(expression, gesture)) {
    return;
  }
  if (gesture != "none" && gesture.length() > 0) {
    applyGesture(gesture);      // gestures win over plain mirroring
  } else {
    applyExpression(expression);
  }
}

#endif  //ENABLE_CAMERA

}  // namespace MirrorVision
