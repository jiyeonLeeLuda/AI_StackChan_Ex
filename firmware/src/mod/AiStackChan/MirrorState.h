#ifndef _MIRROR_STATE_H
#define _MIRROR_STATE_H

#include <Arduino.h>

// Mirror mode: when ON, the robot mimics the user's facial expression using
// the camera (cloud vision polling) and reacts to gestures. Toggled by
// touching the left edge of the screen or by voice ("미러 모드").
namespace MirrorState {
  bool isOn(void);
  void set(bool on);
  void toggle(void);              // toggles + shows a balloon notification
  bool isToggleCommand(const String& sttText);   // "미러 모드" / "거울 모드" etc.
}

#endif  //_MIRROR_STATE_H
