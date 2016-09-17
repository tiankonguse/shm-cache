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
#include "antisnow.h"

void test(SHM_CACHE::AntiSnow& antiSnow) {
    
    int n = 10000000 * 10;
    int N = 1000000;
    int ret = antiSnow.report(1, 1);
    if (ret < 0) {
        printf("ret=%d errormsg=%s\n", ret, antiSnow.getLastError().c_str());
        return;
    }
    for (int i = 0; i < n; i++) {
        int ret = antiSnow.report(1, 1);
        if (i % N == 0) {
            printf("time=%d getpid=%d ret=%d \n", (int) time(NULL),
                    (int) getpid(), ret);
        }
    }
    
}

int main() {
    
    SHM_CACHE::AntiSnow antiSnow;
    
    int pid;
    pid = fork();
    
    if (pid == 0) {
        test(antiSnow);
        return 0;
        
    } else if (pid > 0) {
        test(antiSnow);
    } else {
        printf("fork error\n");
    }
    
    return 0;
    
}
