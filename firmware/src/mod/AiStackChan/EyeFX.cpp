#include "EyeFX.h"

namespace EyeFX {

static Mode s_mode = Mode::None;

void set(Mode m) { s_mode = m; }
Mode get(void) { return s_mode; }

bool draw(M5Canvas* spi, m5avatar::BoundingRect rect, m5avatar::DrawContext* ctx) {
  if (s_mode == Mode::None) return false;

  int32_t x = rect.getCenterX();
  int32_t y = rect.getCenterY();
  uint16_t primary = ctx->getColorDepth() == 1
      ? 1 : ctx->getColorPalette()->get(COLOR_PRIMARY);

  switch (s_mode) {
    case Mode::Big:
      spi->fillCircle(x, y, 14, primary);
      return true;
    case Mode::Small:
      spi->fillCircle(x, y, 4, primary);
      return true;
    case Mode::LetterOK:
    case Mode::LetterNO: {
      // 화면 왼쪽 눈이 첫 글자 — 보는 사람 기준 왼→오로 "OK"/"NO"가 읽히게.
      bool viewerLeft = x < spi->width() / 2;
      const char* ch = (s_mode == Mode::LetterOK) ? (viewerLeft ? "O" : "K")
                                                  : (viewerLeft ? "N" : "O");
      spi->setTextSize(4);                 // 기본 폰트 8px × 4 = 32px
      spi->setTextColor(primary);
      spi->setTextDatum(middle_center);
      spi->drawString(ch, x, y);
      spi->setTextSize(1);                 // 말풍선 등 다른 그리기 영향 방지
      return true;
    }
    default:
      return false;
  }
}

}  // namespace EyeFX
