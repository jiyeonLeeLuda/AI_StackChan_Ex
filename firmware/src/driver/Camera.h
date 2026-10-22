#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <esp_camera.h>
#include <fb_gfx.h>
#include <vector>
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#define TWO_STAGE 1 /*<! 1: detect by two-stage which is more accurate but slower(with keypoints). */
                    /*<! 0: detect by one-stage which is less accurate but faster(without keypoints). */

#define FACE_COLOR_WHITE  0x00FFFFFF
#define FACE_COLOR_BLACK  0x00000000
#define FACE_COLOR_RED    0x000000FF
#define FACE_COLOR_GREEN  0x0000FF00
#define FACE_COLOR_BLUE   0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)
#define FACE_COLOR_CYAN   (FACE_COLOR_BLUE | FACE_COLOR_GREEN)
#define FACE_COLOR_PURPLE (FACE_COLOR_BLUE | FACE_COLOR_RED)

extern bool isSubWindowON;
extern bool isSilentMode;

extern esp_err_t camera_init(void);
extern void camera_deinit(void);
extern bool camera_capture_and_face_detect(void);
extern void camera_capture_preview(void);   // preview refresh only, no face detect
extern bool camera_capture_base64(String& out);
// camera_capture_base64와 동일한 캡처/JPEG 경로(frame2jpg)를 재사용하되, base64
// String(내부 RAM)을 만들지 않고 원본 JPEG 버퍼와 해상도만 돌려준다. 호출자가
// base64 인코딩 대상 버퍼(PSRAM 등)를 직접 고를 수 있게 하기 위함. 성공 시
// *jpg_buf는 malloc된 버퍼이며 호출자가 free() 해야 한다.
extern bool camera_capture_jpeg(uint8_t** jpg_buf, size_t* jpg_len, int* width, int* height);

#endif //_CAMERA_H_