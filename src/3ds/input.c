#include "3ds.h"

#include <malloc.h>

#include <3ds/services/hid.h>
#include <3ds/services/irrst.h>

#include <time.h>
//#include <alarm.h>

#define millis() OSTicksToMilliseconds(OSGetTime())

int swap_buttons = 0;
int swap_shoulders = 0;
int absolute_positioning = 0;

static char lastTouched = 0;
static char touched = 0;

static uint16_t last_x = 0;
static uint16_t last_y = 0;

#define TAP_MILLIS 100
#define DRAG_DISTANCE 10
static uint64_t touchDownMillis = 0;

#define TOUCH_WIDTH 320
#define TOUCH_HEIGHT 240

static int thread_running;
static OSThread inputThread;
static OSAlarm inputAlarm;

// ~60 Hz
#define INPUT_UPDATE_RATE OSMillisecondsToTicks(16)

void handleTouch(u32 kHeld, touchPosition touch) {
  if (absolute_positioning) {
    if (kHeld & KEY_TOUCH) {
      LiSendMousePositionEvent(touch.px, touch.py, TOUCH_WIDTH, TOUCH_HEIGHT);

      if (!touched) {
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
        touched = 1;
      }
    }
    else if (touched) {
      LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
      touched = 0;
    }
  }
  else {
    // Just pressed (run this twice to allow touch position to settle)
    if (lastTouched < 2 && kHeld & KEY_TOUCH) {
      touchDownMillis = millis();
      last_x = touch.px;
      last_y = touch.py;

      lastTouched++;
      return; // We can't do much until we wait for a few hundred milliseconds
              // since we don't know if it's a tap, a tap-and-hold, or a drag
    }

    // Just released
    if (lastTouched && !(kHeld & KEY_TOUCH)) {
      if (millis() - touchDownMillis < TAP_MILLIS) {
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
        LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
      }
    }

    if (kHeld & KEY_TOUCH) {
      // Holding & dragging screen, not just tapping
      if (millis() - touchDownMillis > TAP_MILLIS || touchDownMillis == 0) {
        if (touch.px != last_x || touch.py != last_y) // Don't send extra data if we don't need to
          LiSendMouseMoveEvent(touch.px - last_x, touch.py - last_y);
        last_x = touch.px;
        last_y = touch.py;
      } else {
        if (touch.px - last_x < -10 || touch.px - last_x > 10) touchDownMillis=0;
        if (touch.py - last_y < -10 || touch.py - last_y > 10) touchDownMillis=0;
        int16_t diff_x = touch.px - last_x;
        int16_t diff_y = touch.py - last_y;
        if (diff_x < 0) diff_x = -diff_x;
        if (diff_y < 0) diff_y = -diff_y;
        if (diff_x + diff_y > DRAG_DISTANCE) touchDownMillis = 0;
      }
    }

    lastTouched = (kHeld & KEY_TOUCH) ? lastTouched : 0; // Keep value unless released
  }
}

void n3ds_input_init(void)
{
	hidInit();
  irrstInit();
}

void n3ds_input_update(void) {
  static uint64_t start_pressed = 0;

  short controllerNumber = 0;
  short gamepad_mask = 0;

  for (int i = 0; i < n3ds_input_num_controllers(); i++)
    gamepad_mask |= 1 << i;

  u32 btns = hidKeysHeld();
  circlePosition cPad, cStick;
  hidCircleRead(&cPad);
	hidCstickRead(&cStick);
  short buttonFlags = 0;
#define CHECKBTN(v, f) if (btns & v) buttonFlags |= f;
  if (swap_buttons) {
    CHECKBTN(KEY_A,       B_FLAG);
    CHECKBTN(KEY_B,       A_FLAG);
    CHECKBTN(KEY_X,       Y_FLAG);
    CHECKBTN(KEY_Y,       X_FLAG);
  }
  else {
    CHECKBTN(KEY_A,       A_FLAG);
    CHECKBTN(KEY_B,       B_FLAG);
    CHECKBTN(KEY_X,       X_FLAG);
    CHECKBTN(KEY_Y,       Y_FLAG);
  }
    CHECKBTN(KEY_DUP,      UP_FLAG);
    CHECKBTN(KEY_DDOWN,    DOWN_FLAG);
    CHECKBTN(KEY_DLEFT,    LEFT_FLAG);
    CHECKBTN(KEY_DRIGHT,   RIGHT_FLAG);
  if (swap_shoulders) {
    CHECKBTN(KEY_ZL,       LB_FLAG);
    CHECKBTN(KEY_ZR,       RB_FLAG);
  } else {
    CHECKBTN(KEY_L,       LB_FLAG);
    CHECKBTN(KEY_R,       RB_FLAG);
  }
    CHECKBTN(KEY_START,    PLAY_FLAG);
    CHECKBTN(KEY_SELECT,   BACK_FLAG);
#undef CHECKBTN

  // If the button was just pressed, reset to current time
  if (hidKeysDown() & KEY_START) start_pressed = millis();

  if (btns & KEY_START  && millis() - start_pressed > 3000) {
    state = STATE_STOP_STREAM;
    return;
  }
  
#define CHECKSWAP(noswap, swap) ((swap_shoulders) ? (swap) : (noswap))
  LiSendMultiControllerEvent(controllerNumber++, gamepad_mask, buttonFlags,
    (btns & CHECKSWAP(KEY_ZL, KEY_L)) ? 0xFF : 0,
    (btns & CHECKSWAP(KEY_ZR, KEY_R)) ? 0xFF : 0,
    cPad.dx * INT16_MAX, cPad.dy * INT16_MAX,
    cStick.dx * INT16_MAX, cStick.dy * INT16_MAX);
#undef CHECKSHOULDER

  touchPosition touch;
  hidTouchRead(&touch);
  handleTouch(btns, touch);

  /*KPADStatus kpad_data = {0};
	int32_t kpad_err = -1;
	for (int i = 0; i < 4; i++) {
		KPADReadEx((KPADChan) i, &kpad_data, 1, &kpad_err);
		if (kpad_err == KPAD_ERROR_OK && controllerNumber < 4) {
      if (kpad_data.extensionType == WPAD_EXT_PRO_CONTROLLER) {
        uint32_t btns = kpad_data.pro.hold;
        short buttonFlags = 0;
#define CHECKBTN(v, f) if (btns & v) buttonFlags |= f;
        if (swap_buttons) {
          CHECKBTN(WPAD_PRO_BUTTON_A,       B_FLAG);
          CHECKBTN(WPAD_PRO_BUTTON_B,       A_FLAG);
          CHECKBTN(WPAD_PRO_BUTTON_X,       Y_FLAG);
          CHECKBTN(WPAD_PRO_BUTTON_Y,       X_FLAG);
        }
        else {
          CHECKBTN(WPAD_PRO_BUTTON_A,       A_FLAG);
          CHECKBTN(WPAD_PRO_BUTTON_B,       B_FLAG);
          CHECKBTN(WPAD_PRO_BUTTON_X,       X_FLAG);
          CHECKBTN(WPAD_PRO_BUTTON_Y,       Y_FLAG);
        }
        CHECKBTN(WPAD_PRO_BUTTON_UP,      UP_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_DOWN,    DOWN_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_LEFT,    LEFT_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_RIGHT,   RIGHT_FLAG);
        CHECKBTN(WPAD_PRO_TRIGGER_L,      LB_FLAG);
        CHECKBTN(WPAD_PRO_TRIGGER_R,      RB_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_STICK_L, LS_CLK_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_STICK_R, RS_CLK_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_PLUS,    PLAY_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_MINUS,   BACK_FLAG);
        CHECKBTN(WPAD_PRO_BUTTON_HOME,    SPECIAL_FLAG);
#undef CHECKBTN

        // If the button was just pressed, reset to current time
        if (kpad_data.pro.trigger & WPAD_PRO_BUTTON_HOME)
          home_pressed[controllerNumber] = millis();

        if (btns & WPAD_PRO_BUTTON_HOME && millis() - home_pressed[controllerNumber] > 3000) {
          state = STATE_STOP_STREAM;
          return;
        }

        LiSendMultiControllerEvent(controllerNumber++, gamepad_mask, buttonFlags,
          (kpad_data.pro.hold & WPAD_PRO_TRIGGER_ZL) ? 0xFF : 0,
          (kpad_data.pro.hold & WPAD_PRO_TRIGGER_ZR) ? 0xFF : 0,
          kpad_data.pro.leftStick.x * INT16_MAX, kpad_data.pro.leftStick.y * INT16_MAX,
          kpad_data.pro.rightStick.x * INT16_MAX, kpad_data.pro.rightStick.y * INT16_MAX);
      }
      else if (kpad_data.extensionType == WPAD_EXT_CLASSIC || kpad_data.extensionType == WPAD_EXT_MPLUS_CLASSIC) {
        uint32_t btns = kpad_data.classic.hold;
        short buttonFlags = 0;
#define CHECKBTN(v, f) if (btns & v) buttonFlags |= f;
        if (swap_buttons) {
          CHECKBTN(WPAD_CLASSIC_BUTTON_A,       B_FLAG);
          CHECKBTN(WPAD_CLASSIC_BUTTON_B,       A_FLAG);
          CHECKBTN(WPAD_CLASSIC_BUTTON_X,       Y_FLAG);
          CHECKBTN(WPAD_CLASSIC_BUTTON_Y,       X_FLAG);
        }
        else {
          CHECKBTN(WPAD_CLASSIC_BUTTON_A,       A_FLAG);
          CHECKBTN(WPAD_CLASSIC_BUTTON_B,       B_FLAG);
          CHECKBTN(WPAD_CLASSIC_BUTTON_X,       X_FLAG);
          CHECKBTN(WPAD_CLASSIC_BUTTON_Y,       Y_FLAG);
        }
        CHECKBTN(WPAD_CLASSIC_BUTTON_UP,      UP_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_DOWN,    DOWN_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_LEFT,    LEFT_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_RIGHT,   RIGHT_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_ZL,      LB_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_ZR,      RB_FLAG);
        // don't have stick buttons on a classic controller
        // CHECKBTN(WPAD_CLASSIC_BUTTON_STICK_L, LS_CLK_FLAG);
        // CHECKBTN(WPAD_CLASSIC_BUTTON_STICK_R, RS_CLK_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_PLUS,    PLAY_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_MINUS,   BACK_FLAG);
        CHECKBTN(WPAD_CLASSIC_BUTTON_HOME,    SPECIAL_FLAG);
#undef CHECKBTN

        // If the button was just pressed, reset to current time
        if (kpad_data.classic.trigger & WPAD_CLASSIC_BUTTON_HOME)
          home_pressed[controllerNumber] = millis();

        if (btns & WPAD_CLASSIC_BUTTON_HOME && millis() - home_pressed[controllerNumber] > 3000) {
          state = STATE_STOP_STREAM;
          return;
        }

        LiSendMultiControllerEvent(controllerNumber++, gamepad_mask, buttonFlags,
          kpad_data.classic.leftTrigger * 0xFF,
          kpad_data.classic.rightTrigger * 0xFF,
          kpad_data.classic.leftStick.x * INT16_MAX, kpad_data.classic.leftStick.y * INT16_MAX,
          kpad_data.classic.rightStick.x * INT16_MAX, kpad_data.classic.rightStick.y * INT16_MAX);
      }
    }
  }*/
}

uint32_t n3ds_input_num_controllers(void)
{
  return 1;
}

uint32_t n3ds_input_buttons_triggered(void)
{
  return hidKeysDown();
}

static void alarm_callback(OSAlarm* alarm, OSContext* ctx)
{
  n3ds_input_update();
}

static int input_thread_proc()
{
  OSCreateAlarm(&inputAlarm);
  OSSetPeriodicAlarm(&inputAlarm, 0, INPUT_UPDATE_RATE, alarm_callback);

  while (thread_running) {
    OSWaitAlarm(&inputAlarm);
  }
}

static void thread_deallocator(OSThread *thread, void *stack)
{
  free(stack);
}

void start_input_thread(void)
{
  thread_running = 1;
  int stack_size = 4 * 1024 * 1024;
  void* stack_addr = (uint8_t *)memalign(8, stack_size) + stack_size;

  if (!OSCreateThread(&inputThread,
                      input_thread_proc,
                      0, NULL,
                      stack_addr, stack_size,
                      0x10, OS_THREAD_ATTRIB_AFFINITY_ANY))
  {
    return;
  }

  OSSetThreadDeallocator(&inputThread, thread_deallocator);
  OSResumeThread(&inputThread);
}

void stop_input_thread(void)
{
  thread_running = 0;
  OSCancelAlarm(&inputAlarm);
  OSJoinThread(&inputThread, NULL);
}
