/*
 * fork.cpp
 *
 *  Created on: 2015-9-3
 *      Author: tiankonguse
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <error.h>

#include "shm.h"

#define SIZE 1024

int main() {
    
    Shm shm;
    
    shm.init(IPC_PRIVATE, SIZE);
    
    int iShmID = shm.getShmId(IPC_CREAT | 0666);
    if (iShmID == 0) {
        printf("getShm error. err=%s\n", shm.getLastError());
        return -1;
    }
    printf("iShmID = %d\n", iShmID);
    
    int pid;
    pid = fork();
    
    if (pid == 0) {
        char *shmaddr = shm.getAdr();
        if (NULL == shmaddr) {
            printf("getAdr error. err=%s\n", shm.getLastError());
            return -1;
        }
        
        snprintf(shmaddr, SIZE, "this is parent, but will print in child !");
        
        printf("father end\n");
        return 0;
        
    } else if (pid > 0) {
        sleep(3);
        char *shmaddr = shm.getAdr();
        if (NULL == shmaddr) {
            printf("getAdr error. err=%s\n", shm.getLastError());
            return -1;
        }
        
        printf("%s\n", shmaddr);
        printf("child end\n");
        shm.delShm();
    } else {
        printf("fork error\n");
        shm.delShm();
    }
    
    return 0;
    
}
