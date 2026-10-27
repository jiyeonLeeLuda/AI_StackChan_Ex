#include "CoolGlasses.h"

namespace CoolFace {

static bool cool = false;

void setCool(bool on) { cool = on; }
bool isCool(void) { return cool; }

// 렌즈 1짝 + 중앙 브리지(반쪽) + 바깥쪽 템플 암.
// 좌/우 판별은 눈 중심이 캔버스 중앙보다 왼쪽인지로 — 양쪽이 각자
// 중앙까지 반쪽 브리지를 그리면 자연히 이어진다.
void drawLens(M5Canvas* spi, m5avatar::BoundingRect rect,
              m5avatar::DrawContext* ctx) {
  int32_t x = rect.getCenterX();
  int32_t y = rect.getCenterY();
  uint16_t primary = ctx->getColorDepth() == 1
      ? 1 : ctx->getColorPalette()->get(COLOR_PRIMARY);
  uint16_t background = ctx->getColorDepth() == 1
      ? 0 : ctx->getColorPalette()->get(COLOR_BACKGROUND);

  const int32_t LW = 46, LH = 32, R = 9;   // 렌즈 폭/높이/모서리
  const int32_t half = LW / 2;
  const int32_t cx = spi->width() / 2;

  // 렌즈
  spi->fillRoundRect(x - half, y - LH / 2, LW, LH, R, primary);
  // 광택 하이라이트 (왼쪽 위 사선 2줄)
  spi->drawLine(x - half + 8,  y - 4, x - half + 16, y - LH / 2 + 4, background);
  spi->drawLine(x - half + 13, y - 2, x - half + 21, y - LH / 2 + 6, background);

  const int32_t bar = 5;  // 브리지/템플 두께
  if (x < cx) {
    spi->fillRect(x + half, y - LH / 2 + 3, cx - (x + half) + 1, bar, primary);  // 브리지 →
    spi->fillRect(x - half - 12, y - LH / 2 + 3, 12, bar, primary);              // ← 템플
  } else {
    spi->fillRect(cx, y - LH / 2 + 3, (x - half) - cx + 1, bar, primary);        // ← 브리지
    spi->fillRect(x + half, y - LH / 2 + 3, 12, bar, primary);                   // 템플 →
  }
}

void CoolEye::draw(M5Canvas* spi, m5avatar::BoundingRect rect,
                   m5avatar::DrawContext* ctx) {
  if (!cool) {
    inner.draw(spi, rect, ctx);
    return;
  }
  drawLens(spi, rect, ctx);
}

void install(m5avatar::Avatar& avatar) {
  static CoolEye coolLeft;
  avatar.getFace()->setLeftEye(&coolLeft);
}

}  // namespace CoolFace
