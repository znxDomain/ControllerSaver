// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <cerrno>

#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>


// Include the main libnx system header, for Switch development
#include <switch.h>

#define ENABLE_LOGGING 1
#define IDLE_TIMEOUT_SECONDS 30*60      // min * seconds
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
    // timeExit();
    btmExit();
    hidExit();
}

#ifdef __cplusplus
}
#endif

int mkdirs(char *path, mode_t mode) {
    char tmp_dir[PATH_MAX + 1];
    tmp_dir[0] = '\0';

    char path_dup[PATH_MAX + 1];
    strncpy(path_dup, path, PATH_MAX);
    path_dup[PATH_MAX] = '\0';

    for (char *tmp_str = strtok(path_dup, "/"); tmp_str != NULL; tmp_str = strtok(NULL, "/")) {
        strcat(tmp_dir, tmp_str);
        strcat(tmp_dir, "/");
        
        int res = mkdir(tmp_dir, mode);
        if (res != 0 && errno != EEXIST)
            return res;
    }

    return 0;
}

void LogLine(const char* fmt, ...)
{
#ifdef ENABLE_LOGGING
    mkdirs((char*)"/config/controllersaver/", 777);
    stdout = stderr = fopen("/config/controllersaver/controllersaver.log", "a");
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
    
    u64 keyTick = 0;
    u64 currentTick = 0;
    Result rc;
    
    u64 timerTicks = armNsToTicks(IDLE_TIMEOUT_SECONDS * 1'000'000'000ULL);

    currentTick = armGetSystemTick();
    keyTick = armGetSystemTick();

    static Event g_device_condition_event;
    static s32 g_connected_count = 0;
    static BtdrvAddress g_addresses[8] = {};

    rc = btmAcquireDeviceConditionEvent(&g_device_condition_event);
    if (R_FAILED(rc)) {
        LogLine("Error btmAcquireDeviceConditionEvent:%u - %X\n", rc, rc);
    }
    // Initialize the gamepad for reading all controllers
    padConfigureInput(8, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeAny(&pad);
    
    while(true) {
        // LogLine("Started Loop.\n");
        svcSleepThread(1e+8L);

        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        currentTick = armGetSystemTick();

        if (kDown){
            LogLine("Key Down at: %llu, %llu\n", currentTick, currentTick - keyTick);
            // Record timestamp of last input
            keyTick = armGetSystemTick();
            continue;
        }
        
        if (currentTick - keyTick > timerTicks){
            LogLine("Timer Met: %llu, %llu, %llu\n", currentTick, keyTick, timerTicks);
            keyTick = armGetSystemTick();
            
            LogLine("Disconnecting.\n");

            if (hosversionAtLeast(13,0,0)) {
                BtmConnectedDeviceV13 connected_devices[8];
                rc = btmGetDeviceCondition(BtmProfile_None, connected_devices, 8, &g_connected_count);

                if (R_FAILED(rc)) {
                    LogLine("Error btmGetDeviceCondition:%u - %X\n", rc, rc);
                }
                else {
                    for (s32 i = 0; i != g_connected_count; ++i) {
                        g_addresses[i] = connected_devices[i].address;
                    }
                }
            }
            else {
                BtmDeviceCondition g_device_condition;
                rc = btmLegacyGetDeviceCondition(&g_device_condition);

                if (R_FAILED(rc)) {
                    LogLine("Error btmLegacyGetDeviceCondition:%u - %X\n", rc, rc);
                }
                else {
                    if (hosversionAtLeast(9,0,0)) {
                        g_connected_count = g_device_condition.v900.connected_count;
                        for (s32 i = 0; i != g_connected_count; ++i) {
                            g_addresses[i] = g_device_condition.v900.devices[i].address;
                        }
                    }
                    else if (hosversionAtLeast(8,0,0)) {
                        g_connected_count = g_device_condition.v800.connected_count;
                        for (s32 i = 0; i != g_connected_count; ++i) {
                            g_addresses[i] = g_device_condition.v800.devices[i].address;
                        }
                    }
                    else if (hosversionAtLeast(5,1,0)) {
                        g_connected_count = g_device_condition.v510.connected_count;
                        for (s32 i = 0; i != g_connected_count; ++i) {
                            g_addresses[i] = g_device_condition.v510.devices[i].address;
                        }
                    }
                    else {
                        g_connected_count = g_device_condition.v100.connected_count;
                        for (s32 i = 0; i != g_connected_count; ++i) {
                            g_addresses[i] = g_device_condition.v100.devices[i].address;
                        }
                    }
                }
            }

            if (R_SUCCEEDED(rc)) {
                // Just disconnect them all
                LogLine("Count:%u - %X\n", g_connected_count, g_connected_count);
                for (int i = 0; i != g_connected_count; ++i) {
                    Result rc = btmHidDisconnect(g_addresses[i]);
                    LogLine("Discconnecting btmHidDisconnect Address:%u - %X\n", g_addresses[i], g_addresses[i]);
                    if (R_FAILED(rc)) {
                        LogLine("Error btmHidDisconnect:%u - %X\n", rc, rc);
                    }
                }
                LogLine("All Disconnected.\n");
            }
        }
    }
    return 0;
}

