// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Include the main libnx system header, for Switch development
#include <switch.h>

#define ENABLE_LOGGING 0
static u32 timerSeconds = 1800;

// Size of the inner heap (adjust as necessary).
#define INNER_HEAP_SIZE 0x80000

#ifdef __cplusplus
extern "C" {
#endif

// Sysmodules should not use applet*.
u32 __nx_applet_type = AppletType_None;

// Sysmodules will normally only want to use one FS session.
u32 __nx_fs_num_sessions = 1;

// We don't need to reserve memory for fsdev, so don't use it.
u32 __nx_fsdev_direntry_cache_size = 1;

// Newlib heap configuration function (makes malloc/free work).
void __libnx_initheap(void)
{
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void* fake_heap_start;
    extern void* fake_heap_end;

    // Configure the newlib heap.
    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

// Service initialization.
void __appInit(void)
{
    Result rc;

    rc = smInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }

    rc = hidInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);

    rc = btmInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);

    rc = timeInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);

    rc = fsInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));

    fsdevMountSdmc();

    smExit();
}

// Service deinitialization.
void __appExit(void)
{
    fsdevUnmountAll();
    fsExit();
    timeExit();
    btmExit();
    hidExit();
}

#ifdef __cplusplus
}
#endif

void LogLine(const char* fmt, ...)
{
#ifdef ENABLE_LOGGING
    stdout = stderr = fopen("/controllersaver.log", "a");
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fclose(stdout);
#endif
}

// Main program entrypoint
int main(int argc, char* argv[])
{
    LogLine("Starting Sysmodule.\n");
    
    time_t currentTime = 0;
    time_t keyTime = 0;
    Result rc;

    rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&keyTime);
    if (R_FAILED(rc)) {
        fatalThrow(rc);
    }

    static Event g_device_condition_event;
    static BtmDeviceCondition g_device_condition = {};

    rc = btmAcquireDeviceConditionEvent(&g_device_condition_event);
    if (R_FAILED(rc)) {
        LogLine("Error btmAcquireDeviceConditionEvent:%u - %X\n", rc, rc);
    }

    while(appletMainLoop()) {
        
        if (R_SUCCEEDED(eventWait(&g_device_condition_event, 1e9))) {
            LogLine("btmGetDeviceCondition event triggered.\n");
            rc = btmGetDeviceCondition(&g_device_condition);
            if (R_FAILED(rc)) {
                LogLine("Error btmGetDeviceCondition:%u - %X\n", rc, rc);
            }
        }

        // LogLine("Started Loop.\n");
        svcSleepThread(1e+8L);
            
        rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&currentTime);
        if (R_FAILED(rc)) {
            fatalThrow(rc);
        }

        if (currentTime - keyTime > timerSeconds){
            LogLine("Timer Met: %jd\n", (intmax_t)currentTime);

            Result rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&keyTime);
            if (R_FAILED(rc)) {
                fatalThrow(rc);
            }
            LogLine("Disconnecting.\n");
            
            // Just disconnect them all
            LogLine("Count:%u - %X", g_device_condition.v900.connected_count, g_device_condition.v900.connected_count);
            for (int i = 0; i < g_device_condition.v900.connected_count; ++i) {
                Result rc = btmHidDisconnect(g_device_condition.v900.devices[i].address);
                LogLine("Discconnecting btmHidDisconnect Address:%u - %X\n", g_device_condition.v900.devices[i].address, g_device_condition.v900.devices[i].address);
                if (R_FAILED(rc)) {
                    LogLine("Error btmHidDisconnect:%u - %X\n", rc, rc);
                }
            }
            LogLine("All Disconnected.\n");
        }
    }
    return 0;
}

