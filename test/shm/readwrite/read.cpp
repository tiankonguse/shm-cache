/*
 * read.cpp
 *
 *  Created on: 2015-9-3
 *      Author: tiankonguse
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

#include "shm.h"

const int TIME_NUM = 3;

typedef struct {
    long long sec;
    long long usec;
} Time;

int generatesKey() {
    int key;
    char pathname[30];
    strcpy(pathname, "/tmp");
    
    key = ftok(pathname, 0x03);
    if (key == -1) {
        perror("ftok error");
        return -1;
    }
    
    printf("key=%d\n", key);
    return key;
}

int main(int argc, char** argv) {
    sleep(1);
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
    
    for (int i = 0; i < TIME_NUM; i++) {
        printf("i=%d sec=%lld usec=%lld\n", i, (p_time + i)->sec,
                (p_time + i)->usec);
    }
    
    return 0;
    
}
