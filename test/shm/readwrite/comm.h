/*
 * comm.h
 *
 *  Created on: 2015-9-3
 *      Author: tiankonguse
 */

#ifndef COMM_H_
#define COMM_H_

const int TIME_NUM = 3;

typedef struct {
    long long sec;
    long long usec;
    long long val;
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



#endif /* COMM_H_ */
