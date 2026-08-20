#ifndef _WINK_EYE_H
#define _WINK_EYE_H

#include <Avatar.h>
#include <Eye.h>

// A drop-in replacement for the avatar's right eye that can be forced shut
// (wink). Composition instead of inheritance because m5avatar's Eye is final.
namespace WinkFace {

class WinkEye : public m5avatar::Drawable {
 private:
  m5avatar::Eye inner;
 public:
  WinkEye() : inner(8, false) {}
  void draw(M5Canvas* spi, m5avatar::BoundingRect rect,
            m5avatar::DrawContext* ctx) override;
};

// Swap the avatar's right eye for the winkable one (call once after avatar.init()).
void install(m5avatar::Avatar& avatar);

// While true, the right eye renders closed regardless of expression.
void setWink(bool on);
bool isWinking(void);

}  // namespace WinkFace

#endif  //_WINK_EYE_H
