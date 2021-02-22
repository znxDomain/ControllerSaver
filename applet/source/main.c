#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

int main(int argc, char* argv[])
{
    consoleInit(NULL);

    // Configure our supported input layout: all players with standard controller styles
    padConfigureInput(8, HidNpadStyleSet_NpadStandard);

    // Initialize the gamepad for reading all controllers
    PadState pad;
    padInitializeAny(&pad);
    
    time_t currentTime = 0;
    time_t previousTime = 0;
    time_t keyTime = 0;
    bool canDisconnect = false;
    u32 timerSeconds = 30;

    printf("ControllerSaver - Disconnect Controllers\n");

    printf("Press A to disconnect controllers.\n");
    printf("Press + to exit.\n");

    // Main loop
    while (appletMainLoop())
    {
        svcSleepThread(1e+8L);
        padUpdate(&pad);

        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus)
            break; // break in order to return to hbmenu

        if (kDown){
            // Record timestamp of last input
            Result rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&keyTime);
            if (R_FAILED(rc)) {
                printf("timeGetCurrentTime failed with %x", rc);
            }
            canDisconnect = true;
        }
            
        Result rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&currentTime);
        if (R_FAILED(rc)) {
            printf("timeGetCurrentTime failed with %x", rc);
        }
        
        if (canDisconnect && (currentTime - keyTime) % 2){
            printf("Time since last input: %ld\n", (currentTime - keyTime));
            if (currentTime != previousTime){
                // printf("Time since last input: %ld\n", (currentTime - keyTime));
            }
            previousTime = currentTime;
        }

        if (canDisconnect && currentTime - keyTime > timerSeconds){
            printf("Disconnecting ALL Controllers:\n");
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
            printf("Done.\n");
        }

        consoleUpdate(NULL);
    }

    consoleExit(NULL);
    return 0;
}
