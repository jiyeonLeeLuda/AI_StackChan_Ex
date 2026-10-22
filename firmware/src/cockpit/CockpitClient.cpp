#if defined(ENABLE_COCKPIT)

#include "CockpitClient.h"
#include "SequencePlayer.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <Avatar.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include "libb64/cdecode.h"

#include "SpiRamJsonDocument.h"
#include "Robot.h"
#include "share/Version.h"

using namespace m5avatar;
extern Avatar avatar;

namespace cockpit {

// WebSocketsClient의 콜백은 일반 함수만 받으므로(클래스 메서드 불가), 이 모듈은
// 파일 스코프 상태로 단일 인스턴스를 관리한다. RealtimeLLMBase의 p_this 패턴과 동일.
namespace {

WebSocketsClient    g_ws;
SequencePlayer      g_seq;
SpiRamJsonDocument* g_rxDoc = nullptr;   // 수신 파싱용(tts.chunk base64 수용, PSRAM)
TaskHandle_t        g_taskH = nullptr;

String   g_host;
uint16_t g_port = 0;
String   g_robotId;
bool     g_enabled = false;
bool     g_connected = false;
bool     g_listening = false;            // listen 제어 상태(모드 보고용)

uint32_t       g_lastStateMs = 0;
const uint32_t STATE_INTERVAL_MS = 5000; // state 송신 주기

// 터치: loop() 컨텍스트에서 존을 큐잉하고 WS 태스크에서 송신한다(스레드 경계 분리).
volatile int g_pendingTouch = -1;        // 0=center,1=forehead,2=left-edge,3=right-edge

// tts.chunk 재생용 더블버퍼(PSRAM). base64 디코드한 PCM16을 담는다.
// 재생 중 다음 청크를 다른 면에 디코드해 음 끊김을 줄인다.
const size_t TTS_BUF_SIZE = 64 * 1024;
uint8_t* g_ttsBuf[2] = { nullptr, nullptr };
int      g_ttsBufIdx = 0;

// libb64 base64 디코드. 반환값은 디코드된 바이트 수.
int b64decode(const char* input, uint8_t* output) {
  base64_decodestate s;
  base64_init_decodestate(&s);
  return base64_decode_block(input, strlen(input), (char*)output, &s);
}

const char* modeStr() {
  if (M5.Speaker.isPlaying()) return "speaking";
  if (g_listening)            return "listening";
  return "idle";
}

void sendHello() {
  DynamicJsonDocument doc(512);
  doc["type"] = "hello";
  doc["protocolVersion"] = 0;
  doc["robotId"] = g_robotId;
  doc["firmware"] = String("AiStackChanEx ") + FW_VERSION;
  // 이 개체의 능력. 차량 미연결 CoreS3이므로 drive/led 없음.
  JsonArray caps = doc.createNestedArray("capabilities");
  caps.add("face");
  caps.add("servo");
  caps.add("mic");
  caps.add("speaker");
  caps.add("camera");
  String out;
  serializeJson(doc, out);
  g_ws.sendTXT(out);
  Serial.printf("[cockpit] -> hello (robotId=%s)\n", g_robotId.c_str());
}

void sendState() {
  DynamicJsonDocument doc(192);
  doc["type"] = "state";
  doc["mode"] = modeStr();
  int bat = M5.Power.getBatteryLevel();   // 0-100, 불명 시 -1
  if (bat >= 0) {
    doc["batteryPct"] = bat;
  }
  String out;
  serializeJson(doc, out);
  g_ws.sendTXT(out);
}

void sendTouch(int zone) {
  static const char* zones[] = { "center", "forehead", "left-edge", "right-edge" };
  if (zone < 0 || zone > 3) return;
  DynamicJsonDocument doc(96);
  doc["type"] = "touch";
  doc["zone"] = zones[zone];
  String out;
  serializeJson(doc, out);
  g_ws.sendTXT(out);
  Serial.printf("[cockpit] -> touch %s\n", zones[zone]);
}

// tts.chunk 재생: base64 PCM16(24kHz mono)을 디코드해 스피커로 순차 재생.
void playTtsChunk(const char* b64) {
  uint8_t* buf = g_ttsBuf[g_ttsBufIdx];
  if (buf == nullptr) return;
  // 디코드 결과는 입력의 3/4 크기. 버퍼(64KB) 초과분은 메모리를 덮으므로 거부한다.
  size_t inLen = strlen(b64);
  if (inLen / 4 * 3 > TTS_BUF_SIZE) {
    Serial.printf("[cockpit] tts.chunk too large (%u b64 bytes), dropped\n", (unsigned)inLen);
    return;
  }
  int len = b64decode(b64, buf);   // 디코드된 바이트 수
  if (len <= 0) return;
  // 직전 청크 재생이 끝날 때까지 대기(더블버퍼라 디코드는 이미 끝난 상태).
  while (M5.Speaker.isPlaying()) { vTaskDelay(1); }
  M5.Speaker.playRaw((int16_t*)buf, len / 2, 24000, false);
  g_ttsBufIdx ^= 1;
}

void handleMessage(uint8_t* payload, size_t length) {
  g_rxDoc->clear();
  DeserializationError err = deserializeJson(*g_rxDoc, payload, length);
  if (err) {
    Serial.printf("[cockpit] rx JSON error: %s\n", err.c_str());
    return;
  }

  String type = (*g_rxDoc)["type"].as<String>();

  if (type == "hello.ack") {
    String sid = (*g_rxDoc)["sessionId"].as<String>();
    Serial.printf("[cockpit] <- hello.ack session=%s\n", sid.c_str());
  } else if (type == "sequence") {
    String id = (*g_rxDoc)["id"].as<String>();
    g_seq.start(id, (*g_rxDoc)["steps"].as<JsonArrayConst>());
  } else if (type == "tts.chunk") {
    const char* data = (*g_rxDoc)["data"].as<const char*>();
    if (data) playTtsChunk(data);
  } else if (type == "listen") {
    String action = (*g_rxDoc)["action"].as<String>();
    g_listening = (action == "start");
    Serial.printf("[cockpit] <- listen %s\n", action.c_str());
  } else if (type == "drive") {
    // 이 개체는 drive 능력이 없다. 무시하고 로그만.
    Serial.println("[cockpit] <- drive (ignored: no drive capability)");
  } else {
    Serial.printf("[cockpit] <- unknown type: %s\n", type.c_str());
  }
}

void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      g_connected = true;
      Serial.printf("[cockpit] connected: %s\n", (char*)payload);
      sendHello();
      g_lastStateMs = 0;   // 접속 직후 state 즉시 송신 유도
      break;
    case WStype_DISCONNECTED:
      g_connected = false;
      Serial.println("[cockpit] disconnected");
      break;
    case WStype_TEXT:
      handleMessage(payload, length);
      break;
    case WStype_ERROR:
      Serial.println("[cockpit] ws error");
      break;
    default:
      break;
  }
}

// WebSocket 이벤트 처리 + 주기적 송신을 담당하는 전용 태스크.
// loop()를 막지 않기 위해 분리했다(RealtimeLLMBase의 webSocketLoopTask 패턴).
void taskLoop(void*) {
  Serial.println("[cockpit] task started");
  for (;;) {
    g_ws.loop();
    g_seq.tick();   // 시퀀스 스텝 전환(비블로킹)

    if (g_connected) {
      // loop()에서 큐잉된 터치 송신
      int tz = g_pendingTouch;
      if (tz >= 0) {
        g_pendingTouch = -1;
        sendTouch(tz);
      }
      // 주기적 state
      uint32_t now = millis();
      if (now - g_lastStateMs >= STATE_INTERVAL_MS) {
        g_lastStateMs = now;
        sendState();
      }
    }
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

}  // namespace

void begin(const cockpit_s& cfg) {
  if (cfg.host.length() == 0) {
    Serial.println("[cockpit] host not configured; cockpit disabled");
    return;
  }
  g_host    = cfg.host;
  g_port    = cfg.port;
  g_robotId = cfg.robotId;

  // Wi-Fi modem sleep 비활성화. cockpit 모드에서만 적용(비활성 모드는 기존 동작 보존).
  // 실측: modem sleep이 켜진 상태에서 ping RTT 924ms + 패킷 유실 발생. 실시간 WS
  // 제어·오디오 스트리밍에 수백 ms 절전 지연은 치명적이므로 끈다.
  WiFi.setSleep(false);

  // 수신 파싱 버퍼(tts.chunk의 base64가 커질 수 있어 PSRAM에 넉넉히).
  g_rxDoc = new SpiRamJsonDocument(1024 * 200);

  for (int i = 0; i < 2; i++) {
    g_ttsBuf[i] = (uint8_t*)heap_caps_malloc(TTS_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (g_ttsBuf[i] == nullptr) {
      Serial.println("[cockpit] tts buffer alloc failed; cockpit disabled");
      return;
    }
  }

  g_ws.begin(g_host.c_str(), g_port, "/robot");
  g_ws.onEvent(onWsEvent);
  g_ws.setReconnectInterval(5000);   // 끊기면 5초 백오프 후 재접속

  xTaskCreate(taskLoop, "cockpitTask", 8 * 1024, nullptr, 3, &g_taskH);
  g_enabled = true;
  Serial.printf("[cockpit] begin ws://%s:%u/robot\n", g_host.c_str(), g_port);
}

bool isEnabled() { return g_enabled; }

void notifyTouch(int16_t x, int16_t y) {
  if (!g_enabled) return;
  // 디스플레이 좌표를 존으로 매핑:
  //   상단 1/3    → forehead
  //   좌측 1/4    → left-edge
  //   우측 1/4    → right-edge
  //   그 외(중앙) → center
  int w = M5.Display.width();
  int h = M5.Display.height();
  int zone;
  if (y < h / 3)           zone = 1;   // forehead
  else if (x < w / 4)      zone = 2;   // left-edge
  else if (x > w * 3 / 4)  zone = 3;   // right-edge
  else                     zone = 0;   // center
  g_pendingTouch = zone;
}

}  // namespace cockpit

#endif  // ENABLE_COCKPIT
