// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include the main libnx system header, for Switch development
#include <switch.h>

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

    // Open a service manager session.
    rc = smInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);

    // Retrieve the current version of Horizon OS.
    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }

    // Enable this if you want to use HID.
    rc = hidInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);

    // Enable this if you want to use time.
    rc = timeInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);

    smExit();
}

// Service deinitialization.
void __appExit(void)
{
    timeExit(); // Enable this if you want to use time.
    hidExit(); // Enable this if you want to use HID.
}

#ifdef __cplusplus
}
#endif

// Main program entrypoint
int main(int argc, char* argv[])
{

    // Configure our supported input layout: all players with standard controller styles
    padConfigureInput(8, HidNpadStyleSet_NpadStandard);

    // Initialize the gamepad for reading all controllers
    PadState pad;
    padInitializeAny(&pad);
    
    time_t currentTime = 0;
    time_t keyTime = 0;
    bool canDisconnect = true;
    u32 timerSeconds = 30;

    // Main loop
    while (true)
    {
        svcSleepThread(1e+8L);
        padUpdate(&pad);

        u64 kDown = padGetButtonsDown(&pad);

        if (kDown){
            // Record timestamp of last input
            Result rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&keyTime);
            if (R_FAILED(rc)) {
                fatalThrow(rc);
            }
            canDisconnect = true;
        }
            
        Result rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&currentTime);
        if (R_FAILED(rc)) {
            fatalThrow(rc);
        }

        if (canDisconnect && currentTime - keyTime > timerSeconds){
            // Just disconnect them all
            hidDisconnectNpad(HidNpadIdType_No1);
            hidDisconnectNpad(HidNpadIdType_No2);
            hidDisconnectNpad(HidNpadIdType_No3);
            hidDisconnectNpad(HidNpadIdType_No4);
            hidDisconnectNpad(HidNpadIdType_No5);
            hidDisconnectNpad(HidNpadIdType_No6);
            hidDisconnectNpad(HidNpadIdType_No7);
            hidDisconnectNpad(HidNpadIdType_No8);
            hidDisconnectNpad(HidNpadIdType_Other);
            hidDisconnectNpad(HidNpadIdType_Handheld);
            
            // Don't try again until we get more input
            canDisconnect = false;
        }
    }
    return 0;
}

