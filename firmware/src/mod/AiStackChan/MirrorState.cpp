#include <M5Unified.h>
#include <Avatar.h>
#include "MirrorState.h"
#if defined(ENABLE_CAMERA)
#include "driver/Camera.h"
#endif

using namespace m5avatar;
extern Avatar avatar;

namespace MirrorState {

static bool mirrorOn = false;

bool isOn(void){ return mirrorOn; }

void set(bool on){ mirrorOn = on; }

void toggle(void){
  mirrorOn = !mirrorOn;
#if defined(ENABLE_CAMERA)
  if(mirrorOn){
    avatar.setSpeechText("카메라 켜는 중...");
    if(camera_init() != ESP_OK){
      avatar.setSpeechText("카메라 오류");
      delay(1200);
      avatar.setSpeechText("");
      mirrorOn = false;
      return;
    }
  }else{
    camera_deinit();   // stop the DVP/DMA stream entirely outside mirror mode
  }
  isSubWindowON = mirrorOn;
  avatar.set_isSubWindowEnable(mirrorOn);   // camera preview follows mirror mode
#endif
  avatar.setSpeechText(mirrorOn ? "미러 모드 ON" : "미러 모드 OFF");
  M5.Speaker.tone(mirrorOn ? 1200 : 800, 120);
  delay(1200);
  avatar.setSpeechText("");
}

bool isToggleCommand(const String& sttText){
  return sttText.indexOf("미러 모드") >= 0
      || sttText.indexOf("미러모드") >= 0
      || sttText.indexOf("거울 모드") >= 0
      || sttText.indexOf("거울모드") >= 0;
}

}  // namespace MirrorState
