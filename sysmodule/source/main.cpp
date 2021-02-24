// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Include the main libnx system header, for Switch development
#include <switch.h>

#define ENABLE_LOGGING 1

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
    fsdevUnmountAll(); // Disable this if you don't want to use the SD card filesystem.
    fsExit(); // Disable this if you don't want to use the filesystem.
    timeExit(); // Enable this if you want to use time.
    hidExit(); // Enable this if you want to use HID.
}

#ifdef __cplusplus
}
#endif

void LogLine(const char* fmt, ...)
{

    stdout = stderr = fopen("/controllersaver.log", "a");
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fclose(stdout);
}

// Main program entrypoint
int main(int argc, char* argv[])
{
    LogLine("Starting Sysmodule.\n");

    // Configure our supported input layout: all players with standard controller styles
    padConfigureInput(8, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeAny(&pad);
    padUpdate(&pad);
    u64 kDown = padGetButtonsDown(&pad);
    LogLine("Configured input.\n");

    time_t currentTime = 0;
    time_t keyTime = 0;
    u32 timerSeconds = 30;

    Result rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&keyTime);
    if (R_FAILED(rc)) {
        fatalThrow(rc);
    }

    // Main loop
    while (true)
    {
        // LogLine("Started Loop.\n");
        svcSleepThread(1e+8L);
            
        Result rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&currentTime);
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
            Result rc1 = hidDisconnectNpad(HidNpadIdType_No1);
            if (true || R_FAILED(rc1)) {
                LogLine("Error HidNpadIdType_No1:%u - %X", rc1, rc1);
            }
            Result rc2 = hidDisconnectNpad(HidNpadIdType_No2);
            if (true || R_FAILED(rc2)) {
                LogLine("Error HidNpadIdType_No2:%u - %X", rc2, rc2);
            }
            Result rc3 = hidDisconnectNpad(HidNpadIdType_No3);
            if (true || R_FAILED(rc3)) {
                LogLine("Error HidNpadIdType_No3:%u - %X", rc3, rc3);
            }
            Result rc4 = hidDisconnectNpad(HidNpadIdType_Other);
            if (true || R_FAILED(rc4)) {
                LogLine("Error HidNpadIdType_Other:%u - %X", rc4, rc4);
            }
            Result rc6 = hidDisconnectNpad(HidNpadIdType_Handheld);
            if (true || R_FAILED(rc6)) {
                LogLine("Error HidNpadIdType_Handheld:%u - %X", rc6, rc6);
            }
            LogLine("Disconnected.\n");
        }
    }
    return 0;
}

