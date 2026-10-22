#if defined(ENABLE_COCKPIT)
#ifndef _COCKPIT_CLIENT_H
#define _COCKPIT_CLIENT_H

#include <Arduino.h>
#include "StackchanExConfig.h"

// 조종실(cockpit) WebSocket 클라이언트.
// 서버(ws://host:port/robot)와 JSON 텍스트 프레임으로 통신한다.
//   송신: hello / state(5초 주기) / touch / camera.frame(미러 모드)
//   수신: hello.ack / sequence / tts.chunk / listen / drive
// 프로토콜 정본: packages/protocol/src/index.ts (v0)
namespace cockpit {

// cockpit 클라이언트 시작. cfg.host가 비어있으면 아무 것도 하지 않는다
// (설정이 없으면 cockpit 모드 완전 비활성 → 기존 단독 동작 유지).
// WiFi 연결이 된 상태에서 호출해야 한다.
void begin(const cockpit_s& cfg);

// 활성화 여부(host 설정됨 + begin 성공).
bool isEnabled();

// WS 연결 상태. 미러 모드 분류 이관 분기에 사용.
bool isConnected();

// drive(차량 구동) 활성 여부(cockpit.drive=true로 TB6612 초기화됨).
// GPIO2(AIN1)가 카메라 XCLK와 겹쳐 미러 모드와 상호 배제하기 위한 가드에 사용.
bool isDriveEnabled();

// 미러 모드용: 카메라 프레임(JPEG)을 즉시 캡처해 camera.frame 메시지로 큐잉한다.
// 캡처/인코딩은 호출 스레드(미러 poll = 메인 루프)에서, 실제 송신은 WS 태스크에서.
// 미연결이거나 직전 프레임 전송 중이면 드롭한다(전송 중 프레임 1장만 유지).
// ENABLE_CAMERA가 없으면 아무 것도 하지 않는다.
void sendCameraFrame();

// 음성 턴: 마이크를 캡처(기존 AudioWhisper 경로, 16kHz PCM16 mono, 3.0s)해
// audio.chunk로 서버에 전송하고 응답(tts.chunk/sequence) 대기 상태로 전환한다.
// 캡처는 호출 스레드(발화 트리거 = 모드 idle)에서 블로킹으로 수행. 재생 중이거나
// 이미 턴 진행 중이면 즉시 반환(half-duplex, 에코 방지).
void runVoiceTurn();

// 디스플레이 터치 좌표를 존(center/forehead/left-edge/right-edge)으로 매핑해
// touch 이벤트를 송신한다. loop() 컨텍스트에서 호출 가능(실제 송신은 내부 태스크에서).
void notifyTouch(int16_t x, int16_t y);

}  // namespace cockpit

#endif  // _COCKPIT_CLIENT_H
#endif  // ENABLE_COCKPIT
