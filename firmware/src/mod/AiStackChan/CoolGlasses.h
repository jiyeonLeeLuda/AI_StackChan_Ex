#ifndef _COOL_GLASSES_H
#define _COOL_GLASSES_H

#include <Avatar.h>
#include <Eye.h>

// 주행모드 선글라스(😎). cockpit sequence의 expression "cool"로 켜고,
// 다른 표정이 오면 벗는다. 시퀀스 종료의 Neutral 복귀로는 벗겨지지 않아
// 주행모드 동안 유지된다 (SequencePlayer 참고).
//
// 구성: 왼눈은 CoolEye(이 파일)로 교체, 오른눈은 기존 WinkEye가
// 선글라스 모드일 때 drawLens를 호출한다 (윙크 기능과 공존).
namespace CoolFace {

// 선글라스 렌즈 1짝 + 브리지/템플. WinkEye에서도 호출하므로 공개.
void drawLens(M5Canvas* spi, m5avatar::BoundingRect rect,
              m5avatar::DrawContext* ctx);

class CoolEye : public m5avatar::Drawable {
 private:
  m5avatar::Eye inner;
 public:
  CoolEye() : inner(8, true) {}
  void draw(M5Canvas* spi, m5avatar::BoundingRect rect,
            m5avatar::DrawContext* ctx) override;
};

// 왼눈을 선글라스 가능 눈으로 교체 (WinkFace::install 이후 1회 호출).
void install(m5avatar::Avatar& avatar);

void setCool(bool on);
bool isCool(void);

}  // namespace CoolFace

#endif  // _COOL_GLASSES_H
