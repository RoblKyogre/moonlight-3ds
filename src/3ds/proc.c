// procui implementation based on <https://github.com/devkitPro/wut/blob/master/libraries/libwhb/src/proc.c>
// we need to use a custom one to handle home button input properly

#include "3ds.h"

#include <3ds/services/apt.h>

static int running = 0;

void n3ds_proc_init(void)
{
    running = 1;
    aptInit();

    n3ds_proc_register_home_callback();
}

void n3ds_proc_shutdown(void)
{
    running = 0;
}

void n3ds_proc_register_home_callback(void)
{
    //no-op, no need to register home callback
}

int n3ds_proc_running(void)
{
    if (aptShouldClose()) {
        running = false;
    }

    if (!running) {
        aptExit();
    }

    return running;
}

void n3ds_proc_stop_running(void)
{
    running = false;
}

void n3ds_proc_set_home_enabled(int enabled)
{
    aptSetHomeAllowed(enabled);
    aptSetSleepAllowed (enabled);
}
