/*
INTRO:
    This tiny program is intended to benchmark the read speed of an FTDI FT232H IC in async/sync 245 mode.

    It prints some info about the connected FTDI ICs and then repeatedly fills up a buffer X times as determined
    by the settings defined below.

    I hacked this code around (low quality) FTDI example code and documentation and am not (yet) an expert on this IC.
    There may be better ways to setup and use the IC.

    This tiny program deserves an entire re-write.

    Please report bugs to afk@olkie.com

    TODO:
        - require FT232H EEPROM description match some string before we use it instead of just grabbing the first one.
        - check mode of FT232H for correct settings and exit/warn if differs from expected (245 mode, clock out AC8...)
        - use command line flags for controlling settings
        - provide flags to allow programming FT232H EEPROM appropriately
        - port to Linux & maybe use lib FTDI (buggy?) instead
        - (done) print interim updates during read loop & flag to disable

        
        
BEFORE YOU USE:

    You MUST download and install the FTDI D2XX drivers for your OS before you can use this file.
    http://www.ftdichip.com/Drivers/D2XX.htm

    The driver install file will also provide you with the files necessary for compiling for your OS / architecture:
        All: ftd2xx.h
        Windows: ftd2xx.lib, ftd2xx.dll
        Linux: follow ReadMe.txt in the tar.gz file you downloaded
    
    Download official FTDI program FT_PROG (Windows only) from here: http://www.ftdichip.com/Support/Utilities.htm
    Linux alternative: https://github.com/richardeoin/ftx-prog
    
    Use FT_PROG to setup the FTDI IC to run with the following settings 
        "D2XX" driver
        "245 Mode"
        output a clock signal (7.5 MHz or higher) on pin ACBUS8
        "Schmitt Trigger" - can probably omit, but it is what I used
        "16mA output" - can probably omit, but it is what I used

    physically connect the ACBUS8 clock signal to the WR# pin
 
    Click Program.

    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     DO NOT FORGET TO HIT CYCLE PORTS AFTER YOU HIT PROGRAM!
     Changes to EEPROM settings via FT_Prog do not take until 
     the FTDI chip is power cycled or until you hit "cycle ports".
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    You are now ready to start using this program to capture data and analyze speeds.


Useful Info:
    The "D2XX Programmer's  Guide" has FTDI command explanations available below
    http://www.ftdichip.com/Support/Documents/ProgramGuides.htm

    FTDI 245 Reference Code
    http://www.ftdichip.com/Support/Documents/AppNotes/AN_130_FT2232H_Used_In_FT245%20Synchronous%20FIFO%20Mode.pdf


Notes for building from scratch using Visual Studio Express 2013 for Windows Desktop
    Add ftd2xx.lib under Project Properties->Configuration Properties->Linker->Input and look for Additional dependencies.
    If you put the name of the .lib file (including the extension) here then it will try to link with it.
    You should also put the .lib file somewhere the linker can find it, either in the same directory as the project file
    or add a directory to Additional Library Directories under General.
    more details: http://social.msdn.microsoft.com/Forums/en-US/84deabaa-ae82-47cc-aac0-592f5a8dfa22/linking-an-external-dll


Copyright (C) 2015 Adam Fraser-Kruck <afk@olkie.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/


//INCLUDES specific to Visual Studio (AKA Windows Build)
    #include "stdafx.h" //must be first include for Visual Studio
    #include "FtdiTest_VisualStudio.h" //must be above #include "ftd2xx.h"

//generic includes
    #include "FtdiTest.h"
    #include "ftd2xx.h"


//----------------------------------------------------------------------------------------------------------------
// SETTINGS YOU CAN ADJUST
//----------------------------------------------------------------------------------------------------------------
    //define for AFK_DEVICE_INDEX_TO_USE
    #define AFK_DEVICE_INDEX_PROMPT_USER -1
    
    //this controls which FTDI IC to use if you have multiples
    //if = -1, then the user will be prompted 
    #define AFK_DEVICE_INDEX_TO_USE   AFK_DEVICE_INDEX_PROMPT_USER

    //if true, the FTDI IC will run in ASYNC mode (default).
    #define AFK_RUN_ASYNC_245 true

    //set the USB transfer size. Must be set to a multiple of 64 bytes between 64 bytes and 64k bytes.
    //0x10000 is max, 0x40 is min.
    #define AFK_USB_TRANSFER_SIZE 0x10000

    //latency setting. Valid range = 2 to 255. Lower is better. IC hangs if set to 0. Doesn't make a huge impact.
    #define AFK_USB_LATENCY 2 

    //defines flow control used for USB transfer
    //can also try FT_FLOW_RTS_CTS
    #define AFK_USB_FLOW_CONTROL FT_FLOW_NONE

    //defines size of array to read into
    #define AFK_SINGLE_READ_BYTES 128*1024

    //defines number of times we fill the array
    #define AFK_NUMBER_OF_READS 1000

    //controls how many reads before an update is printed. Set to 0 if no updates printed.
    #define AFK_READ_INTERIM_UPDATES 100




//----------------------------------------------------------------------------------------------------------------
// PROGRAM GUTS BELOW
//----------------------------------------------------------------------------------------------------------------

//this controls which FTDI IC to use if you have multiples 
int deviceIndexToUse = AFK_DEVICE_INDEX_TO_USE; 


/*
* A simple macro that checks for the expected FTDI FT_OK value 
* or sets result to 1, prints an error msg, then jumps to 'exit' label.
* Not in header to improve readability of code.
*/
#define gotoExitIfNotOk(ftstatus, msg) \
if (ftdiStatus != FT_OK) \
{\
    result = 1;\
    printf("%s FAILED!\n", msg); \
    goto exit; /* be eaten by raptors! */ \
}




/*
 The main function. Gets called first.
 Prints some info about the connected FTDI IC and runs some sampling speed tests
*/
int main(int argc, char* argv[]) {
    int result;
    printf("FTDI Speed Test Program\n");
    printf("-----------------------\n");

    result = printFtdiInfo();
    if (result != 0){
        printf("Cannot run tests. Exiting app.");
    } else {
    
        while(deviceIndexToUse == AFK_DEVICE_INDEX_PROMPT_USER){
            printf("\n\nPlease enter Device Handle # to use: ");
            std::cin >> deviceIndexToUse;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        
        printf("Ready to run tests using device %d... ", deviceIndexToUse);     
        system("pause");
        
        //geting info worked. Now run tests.
        result = runReadSpeedTest();
    }

    printf("\n\n");
    printf("Program finished without segfault or exception :)\n");
    system("pause");

    return result;
}


/*
 Sets up FT232H IC to run in async or synchronous mode and then times how long it takes to 
 repeatedly fill a large array.
 Bytes read, elapsed time, and MB/s are printed once complete.
 Returns 0 on success, 1 on failure.
*/
int runReadSpeedTest(){
    int result = 0;
    
    /* reference to FTDI IC for commands */
    FT_HANDLE ftdiHandle;

    /* stores success of commands */
    FT_STATUS ftdiStatus;

    /* Sets which pins are inputs (0) and outputs (1). Appears irrelevant to 245 mode. */
    UCHAR inputOutputMask = 0x00;

    /* total number of bytes read during test */
    DWORD totalBytesRead;

    /* receive buffer to repeatedly fill */
    char rxBuffer[AFK_SINGLE_READ_BYTES];

    /* bytes read in a single read command */
    DWORD bytesRead = 0;

    /* yep. a temp var. */
    UCHAR temp;

    printf("Entered Read Speed Test\n");

    //open device for use
    printf("opening device %d...", deviceIndexToUse);
    ftdiStatus = FT_Open(deviceIndexToUse, &ftdiHandle);
    gotoExitIfNotOk(ftdiStatus, "FT_Open");
    printf(" done\n");

    //Reset mode of IC. If was in sync 245 mode, it will be put back to async 245 mode
    printf("resetting mode of IC...");
    ftdiStatus = FT_SetBitMode(ftdiHandle, inputOutputMask, FT_BITMODE_RESET);
    gotoExitIfNotOk(ftdiStatus, "FT_SetBitMode reset");
    printf(" done\n");

    sleepMs(100);

    //set mode to async or sync
    if (AFK_RUN_ASYNC_245){
        printf("Leaving mode in async 245 mode (assuming EEPROM in 245 mode)\n");
    } else {
        printf("setting mode to sync 245 mode (assuming EEPROM in 245 mode)...");
        ftdiStatus = FT_SetBitMode(ftdiHandle, inputOutputMask, FT_BITMODE_SYNC_FIFO);
        gotoExitIfNotOk(ftdiStatus, "FT_SetBitMode Mode 0x40\n");
        printf(" done\n");
    }

    //Set USB latency
    printf("setting latency to %d ...", AFK_USB_LATENCY);
    ftdiStatus = FT_SetLatencyTimer(ftdiHandle, AFK_USB_LATENCY);
    gotoExitIfNotOk(ftdiStatus, "FT_SetLatencyTimer");
    printf(" done\n");

    //check latency timer
    ftdiStatus = FT_GetLatencyTimer(ftdiHandle, &temp);
    gotoExitIfNotOk(ftdiStatus, "FT_GetLatencyTimer");
    printf("Get Latency Timer:%d\n", temp);

    //set usb transfer size
    printf("setting USB transfer size to %d ...", AFK_USB_TRANSFER_SIZE);
    ftdiStatus = FT_SetUSBParameters(ftdiHandle, AFK_USB_TRANSFER_SIZE, AFK_USB_TRANSFER_SIZE);
    gotoExitIfNotOk(ftdiStatus, "FT_SetUSBParameters");
    printf(" done\n");

    //set flow control
    printf("setting flow control %d ...", AFK_USB_FLOW_CONTROL);
    FT_SetFlowControl(ftdiHandle, AFK_USB_FLOW_CONTROL, 0, 0);
    gotoExitIfNotOk(ftdiStatus, "FT_SetFlowControl");
    printf(" done\n");
    
    printf("starting reading in 1 second!\n");
    sleepMs(1000); //gives things a chance to settle

    //start timing the reading operation so we can calc MB/s
    startTiming();

    //read loop
    for (int readCount = 0; readCount < AFK_NUMBER_OF_READS; readCount++) {

        //read bytes from USB into array
        ftdiStatus = FT_Read(ftdiHandle, rxBuffer, AFK_SINGLE_READ_BYTES, &totalBytesRead);
        gotoExitIfNotOk(ftdiStatus, "FT_Read");

        //keep tally of bytes received
        bytesRead += totalBytesRead;

        //print update if desired
        if (AFK_READ_INTERIM_UPDATES > 0 && readCount % AFK_READ_INTERIM_UPDATES == 0){
            printf("Read status update %d/%d\n", readCount, AFK_NUMBER_OF_READS);
        }

    }

    printf("finished reading. Calculating results...\n");
    
    //get timestats
    double elapsedSeconds = stopTiming();

    //calc and print results
    double calcMBps = bytesRead / 1e6 / elapsedSeconds;
    printf("Data bytes read:%d\n", bytesRead);
    printf("Elapsed seconds:%f\n", elapsedSeconds);
    printf("MB/s: %f\n", calcMBps);

exit:
    //close the handle to the IC
    FT_Close(ftdiHandle);
    printf("Exiting Read Speed Test\n");
    return result;
}



/*
Prints info regarding attached FTDI ICs.
Returns 0 if everything is OK to run tests, else 1.
*/
int printFtdiInfo(){
    int result = 0;
    FT_STATUS ftdiStatus;
    DWORD libraryVersion;
    DWORD deviceCount;
    FT_DEVICE_LIST_INFO_NODE *deviceInfo;

    // Get DLL library version
    ftdiStatus = FT_GetLibraryVersion(&libraryVersion);
    gotoExitIfNotOk(ftdiStatus, "Reading library version");
    printf("Library version = 0x%x\n", libraryVersion);

    // get number of connected devices
    ftdiStatus = FT_CreateDeviceInfoList(&deviceCount);
    gotoExitIfNotOk(ftdiStatus, "Reading device count");
    printf("Number of devices is %d\n\n\n", deviceCount);

    if (deviceCount <= 0) {
        printf("No devices found. Please connect a device and try again.\n");
        result = 1;
        goto exit;
    } else{
        //allocate room to read information on each connected device
        deviceInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*deviceCount);
        
        //read the device information
        ftdiStatus = FT_GetDeviceInfoList(deviceInfo, &deviceCount);
        gotoExitIfNotOk(ftdiStatus, "FT_GetDeviceInfoList");
        for (unsigned int i = 0; i < deviceCount; i++) {
            printf("Device Handle # %d:\n", i);
            printf(" Flags=0x%x\n", deviceInfo[i].Flags);
            printf(" Type=0x%x\n", deviceInfo[i].Type);
            printf(" ID=0x%x\n", deviceInfo[i].ID);
            printf(" LocId=0x%x\n", deviceInfo[i].LocId);
            printf(" SerialNumber=%s\n", deviceInfo[i].SerialNumber);
            printf(" Description=%s\n", deviceInfo[i].Description);
            printf(" ftHandle=0x%x\n", deviceInfo[i].ftHandle);
        }
        free(deviceInfo);
    }
    
exit: 
    return result;
}



