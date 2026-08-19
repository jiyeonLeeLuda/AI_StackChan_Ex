#ifndef _MIRROR_VISION_H
#define _MIRROR_VISION_H

// Cloud-vision mirroring: while mirror mode is ON, a camera frame is sent to
// an OpenAI vision model every few seconds and the classified
// expression/gesture is played back on the avatar. Never sends anything
// while mirror mode is OFF. Requires -DENABLE_CAMERA.
namespace MirrorVision {
  // Call from the mod's idle(). Rate-limited internally; a no-op when mirror
  // mode is off or the camera is unavailable.
  void poll(void);
}

#endif  //_MIRROR_VISION_H
