/*
 * write.cpp
 *
 *  Created on: 2015-9-3
 *      Author: tiankonguse
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "shm.h"
#include "comm.h"

int main(int argc, char** argv) {
    
    int key = generatesKey();
    int size = sizeof(Time) * TIME_NUM;
    
    Shm shm;
    shm.init(key, size);
    
    int iShmID = shm.getShmId(IPC_CREAT | 0666);
    if (iShmID == 0) {
        printf("getShm error. err=%s\n", shm.getLastError());
        return -1;
    }
    printf("iShmID = %d\n", iShmID);
    
    Time *p_time;
    p_time = (Time *) shm.getAdr();
    if (NULL == p_time) {
        printf("getAdr error. err=%s\n", shm.getLastError());
        return -1;
    }
    srand(time(NULL));
    for (int i = 0; i < TIME_NUM; i++) {
        struct timeval start;
        gettimeofday(&start, NULL);
        (p_time + i)->sec = start.tv_sec;
        (p_time + i)->usec = start.tv_usec;
        (p_time + i)->val = rand() % 100;
    }
    
    return 0;
    
}

