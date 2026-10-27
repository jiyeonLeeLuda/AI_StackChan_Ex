#ifndef _EYE_FX_H
#define _EYE_FX_H

#include <Avatar.h>

// 눈 연출 이펙트 — cockpit sequence의 sound 마커로 제어한다.
//   "eyes-ok" → 눈이 O K  (끄덕/승인)
//   "eyes-no" → 눈이 N O  (도리도리/거절)
//   "eyes-big" / "eyes-small" → 동공 확대/축소 (두리번)
//   시퀀스가 끝나면 SequencePlayer가 None으로 자동 복귀시킨다.
// 선글라스(CoolFace)와는 독립 — 선글라스가 켜져 있으면 렌즈가 우선.
namespace EyeFX {

enum class Mode { None, LetterOK, LetterNO, Big, Small };

void set(Mode m);
Mode get(void);

// 모드가 활성이면 해당 눈을 그리고 true, None이면 false(기본 눈 사용).
bool draw(M5Canvas* spi, m5avatar::BoundingRect rect, m5avatar::DrawContext* ctx);

}  // namespace EyeFX

#endif  // _EYE_FX_H
