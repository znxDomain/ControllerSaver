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
    mkdirs("/config/controllersaver/", 777);
    stdout = stderr = fopen("/config/controllersaver/controllersaver.log", "a");
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fclose(stdout);
#endif
}

typedef struct {
    BtdrvAddress address;
    u8 pad[2];
    u32 unk_x8;
    char name[0x20];
    u8 unk_x2c[0x1c];
    u16 vid;
    u16 pid;
    u8 _unk2[0x20];
} BtmConnectedDevice;

typedef struct {
    u32 unk_x0;
    u8 unk_x4;
    u8 unk_x5;
    u8 max_count;
    u8 connected_count;
    BtmConnectedDevice devices[8];
} BtmDeviceConditionV900;

// Main program entrypoint
int main(int argc, char* argv[])
{
    LogLine("Starting Sysmodule.\n");
    
    time_t currentTime = 0;
    time_t keyTime = 0;
    Result rc;

    rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&keyTime);
    if (R_FAILED(rc)) {
        LogLine("timeGetCurrentTime failed with %x\n", rc);
    }

    static Event g_device_condition_event;
    static BtmDeviceCondition g_device_condition = {};
        
    rc = btmAcquireDeviceConditionEvent(&g_device_condition_event);
    if (R_FAILED(rc)) {
        LogLine("Error btmAcquireDeviceConditionEvent:%u - %X\n", rc, rc);
    }
    // Initialize the gamepad for reading all controllers
    padConfigureInput(8, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeAny(&pad);
    
    while(appletMainLoop()) {
        // LogLine("Started Loop.\n");
        svcSleepThread(1e+8L);

        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&currentTime);
        if (R_FAILED(rc)) {
            LogLine("timeGetCurrentTime failed with %x\n", rc);
        }

        if (kDown){
            LogLine("Key Down at: %jd, %jd\n", (intmax_t)currentTime, currentTime - keyTime);
            // Record timestamp of last input
            rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&keyTime);
            if (R_FAILED(rc)) {
                LogLine("timeGetCurrentTime failed with %x\n", rc);
            }
        }

        if (currentTime - keyTime > timerSeconds){
            LogLine("Timer Met: %jd\n", (intmax_t)currentTime);

            Result rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&keyTime);
            if (R_FAILED(rc)) {
                fatalThrow(rc);
            }
            LogLine("Disconnecting.\n");
            
            rc = btmGetDeviceCondition(&g_device_condition);
            if (R_FAILED(rc)) {
                LogLine("Error btmGetDeviceCondition:%u - %X\n", rc, rc);
            }

            BtmDeviceConditionV900* device_condition = reinterpret_cast<BtmDeviceConditionV900*>(&g_device_condition);

            // Just disconnect them all
            LogLine("Count:%u - %X\n", device_condition->connected_count, device_condition->connected_count);
            for (int i = 0; i < device_condition->connected_count; ++i) {
                Result rc = btmHidDisconnect(device_condition->devices[i].address);
                LogLine("Discconnecting btmHidDisconnect Address:%u - %X\n", device_condition->devices[i].address, device_condition->devices[i].address);
                if (R_FAILED(rc)) {
                    LogLine("Error btmHidDisconnect:%u - %X\n", rc, rc);
                }
            }
            LogLine("All Disconnected.\n");
        }
    }
    return 0;
}

