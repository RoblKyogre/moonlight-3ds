// procui implementation based on <https://github.com/devkitPro/wut/blob/master/libraries/libwhb/src/proc.c>
// we need to use a custom one to handle home button input properly

#include "3ds.h"

#include <malloc.h>

#include <3ds/services/apt.h>
#include <3ds/services/ac.h>
#include <3ds/services/soc.h>
//#include <3ds/services/sslc.h>
//#include <3ds/services/httpc.h>
#include <3ds/services/ndm.h>

#include <3ds/os.h>
#include <3ds/svc.h>

static int running = 0;

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

static u32 *SOC_buffer = NULL;

static acuConfig *acConf;

void n3ds_proc_init(void)
{
    aptInit();
    acInit();
    SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    socInit(SOC_buffer, SOC_BUFFERSIZE);
    ndmuInit();
    //sslcInit(0);
    //httpcInit(0x200000);

    osSetSpeedupEnable(1);

    APT_SetAppCpuTimeLimit(90);

    acWaitInternetConnection();

    NDMU_EnterExclusiveState(NDM_EXCLUSIVE_STATE_INFRASTRUCTURE);
    NDMU_LockState();

    svcSetThreadPriority(CUR_THREAD_HANDLE, 0x20);

    running = 1;

    n3ds_proc_register_home_callback();
}

void n3ds_proc_shutdown(void)
{
    running = 0;
    NDMU_UnlockState();
    NDMU_LeaveExclusiveState();

    //httpcExit();
    //sslcExit();
    ndmuExit();
    socExit();
    acExit();
    aptExit();
}

void n3ds_proc_register_home_callback(void)
{
    //no-op, no need to register home callback function
}

int n3ds_proc_running(void)
{
    if (!(aptMainLoop())) {
        running = false;
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
    aptSetSleepAllowed(enabled);
}
