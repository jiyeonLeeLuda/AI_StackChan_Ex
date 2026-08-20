#include <ArduinoJson.h>
#include <Avatar.h>
#include "Whisper.h"

extern m5avatar::Avatar avatar;
#include "rootCA/rootCACertificate.h"

namespace {
constexpr char* API_HOST = "api.openai.com";
constexpr int API_PORT = 443;
constexpr char* API_PATH = "/v1/audio/transcriptions";
}  // namespace

Whisper::Whisper(stt_param_t param) : client(), STTBase(param) {
  client.setCACert(root_ca_openai);
  client.setTimeout(10000); 
  //if (!client.connect(API_HOST, API_PORT)) {
  //  Serial.println("Whisper: Connection failed!");
  //}
}

Whisper::~Whisper() {
  //client.stop();
}

String Whisper::Transcribe(AudioWhisper* audio) {
  char boundary[64] = "------------------------";
  for (auto i = 0; i < 2; ++i) {
    ltoa(random(0x7fffffff), boundary + strlen(boundary), 16);
  }
  // Language from SC_ExConfig.yaml (stt.language). Whisper expects ISO-639-1,
  // so cut a BCP-47 code like "ko-KR" down to "ko". Default is "ko".
  String language = param.stt_conf.language;
  int sep = language.indexOf('-');
  if(sep > 0){
    language = language.substring(0, sep);
  }
  if(language.length() == 0 || language == "null"){
    language = "ko";
  }
  const String header = "--" + String(boundary) + "\r\n"
    "Content-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-1\r\n"
    "--" + String(boundary) + "\r\n"
    "Content-Disposition: form-data; name=\"language\"\r\n\r\n" + language + "\r\n"
    "--" + String(boundary) + "\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"speak.wav\"\r\n"
    "Content-Type: application/octet-stream\r\n\r\n";
  const String footer = "\r\n--" + String(boundary) + "--\r\n";

  // header
  client.printf("POST %s HTTP/1.1\n", API_PATH);
  client.printf("Host: %s\n", API_HOST);
  client.println("Accept: */*");
  client.printf("Authorization: Bearer %s\n", param.api_key.c_str());
  client.printf("Content-Length: %d\n", header.length() + audio->GetSize() + footer.length());
  client.printf("Content-Type: multipart/form-data; boundary=%s\n", boundary);
  client.println();
  client.print(header.c_str());
  client.flush();

  auto ptr = audio->GetBuffer();
  auto remainings = audio->GetSize();
  // Send in large chunks without per-chunk flush(): flushing every 512 bytes
  // waits for an ACK ~190 times per take and stretched the upload of a 3s
  // recording (~96KB) to tens of seconds.
  while (remainings > 0) {
    auto sz = (remainings > 8192) ? 8192 : remainings;
    client.write(ptr, sz);
    remainings -= sz;
    ptr += sz;
  }
  client.flush();

  // footer
  client.print(footer.c_str());
  client.flush();

  // wait response
  const auto now = ::millis();
  while (client.available() == 0) {
    if (::millis() - now > 10000) {
      Serial.println(">>> Client Timeout !");
      return "";
    }
  }

  bool isBody = false;
  String body = "";
  while(client.available()){
    const auto line = client.readStringUntil('\r');
    if (isBody) {
      body += line;
    } else if (line.equals("\n")) {
      isBody = true;
    }
  }

  // The response may be chunked; keep only the JSON object part.
  int b = body.indexOf('{');
  int e = body.lastIndexOf('}');
  if (b >= 0 && e > b) {
    body = body.substring(b, e + 1);
  }
  DynamicJsonDocument doc(2048);   // 200B was too small for long Korean sentences
  DeserializationError err = ::deserializeJson(doc, body);
  String text = doc["text"].as<String>();
  if (err != DeserializationError::Ok || text.length() == 0 || text == "null") {
    Serial.printf("[Whisper] empty result (err=%s) body: %.300s\n",
                  err.c_str(), body.c_str());
    return "";
  }
  return text;
}



String Whisper::speech_to_text(){
  String ret;
  AudioWhisper* audio = new AudioWhisper();
  Serial.println("\r\nRecord start!\r\n");
  audio->Record();  
  Serial.println("Record end\r\n");
  Serial.println("音声認識開始");
  avatar.setSpeechText("인식 중...");   // recording is over; now waiting for Whisper
  if (!client.connect(API_HOST, API_PORT)) {
    Serial.println("Whisper: Connection failed!");
    ret = String("");
  }
  else{
    ret = Transcribe(audio);
    client.stop();
  }
  delete audio;
  return ret;
}