#include "WinkEye.h"

namespace WinkFace {

static bool winking = false;

void setWink(bool on){ winking = on; }
bool isWinking(void){ return winking; }

void WinkEye::draw(M5Canvas* spi, m5avatar::BoundingRect rect,
                   m5avatar::DrawContext* ctx){
  if (!winking) {
    inner.draw(spi, rect, ctx);
    return;
  }
  // closed winking eye: a happy-style arc (upper half hidden)
  uint32_t x = rect.getCenterX();
  uint32_t y = rect.getCenterY();
  uint16_t r = 8;
  uint16_t primaryColor = ctx->getColorDepth() == 1
      ? 1 : ctx->getColorPalette()->get(COLOR_PRIMARY);
  uint16_t backgroundColor = ctx->getColorDepth() == 1
      ? 0 : ctx->getColorPalette()->get(COLOR_BACKGROUND);
  spi->fillCircle(x, y, r, primaryColor);
  spi->fillCircle(x, y + 3, r, backgroundColor);   // carve a smiling arc
  spi->fillRect(x - r - 2, y + 2, r * 2 + 4, r + 4, backgroundColor);
}

void install(m5avatar::Avatar& avatar){
  static WinkEye winkEye;
  avatar.getFace()->setRightEye(&winkEye);
}

}  // namespace WinkFace
