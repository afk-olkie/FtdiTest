#ifndef FTDITEST_VISUALSTUDIO_H
#define FTDITEST_VISUALSTUDIO_H

#include <iostream>
#include <Windows.h> //must be above ftd2xx.h include

LARGE_INTEGER lPreTime, lPostTime, lFrequency;
void startTiming(){
    QueryPerformanceFrequency(&lFrequency);
    QueryPerformanceCounter(&lPreTime);
}
double stopTiming(){
    QueryPerformanceCounter(&lPostTime);
    double lPassTick = lPostTime.QuadPart - lPreTime.QuadPart;
    double lPassTime = lPassTick / (double)lFrequency.QuadPart;
    return lPassTime;
}

void sleepMs(int ms){
    Sleep(ms);
}

#endif //end guard