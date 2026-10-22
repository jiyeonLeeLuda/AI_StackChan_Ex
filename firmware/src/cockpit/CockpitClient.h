#if defined(ENABLE_COCKPIT)
#ifndef _COCKPIT_CLIENT_H
#define _COCKPIT_CLIENT_H

#include <Arduino.h>
#include "StackchanExConfig.h"

// 조종실(cockpit) WebSocket 클라이언트.
// 서버(ws://host:port/robot)와 JSON 텍스트 프레임으로 통신한다.
//   송신: hello / state(5초 주기) / touch
//   수신: hello.ack / sequence / tts.chunk / listen / drive
// 프로토콜 정본: packages/protocol/src/index.ts (v0)
namespace cockpit {

// cockpit 클라이언트 시작. cfg.host가 비어있으면 아무 것도 하지 않는다
// (설정이 없으면 cockpit 모드 완전 비활성 → 기존 단독 동작 유지).
// WiFi 연결이 된 상태에서 호출해야 한다.
void begin(const cockpit_s& cfg);

// 활성화 여부(host 설정됨 + begin 성공).
bool isEnabled();

// 디스플레이 터치 좌표를 존(center/forehead/left-edge/right-edge)으로 매핑해
// touch 이벤트를 송신한다. loop() 컨텍스트에서 호출 가능(실제 송신은 내부 태스크에서).
void notifyTouch(int16_t x, int16_t y);

}  // namespace cockpit

#endif  // _COCKPIT_CLIENT_H
#endif  // ENABLE_COCKPIT
