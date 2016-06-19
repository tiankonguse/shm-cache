/*
 * shm.cpp
 *
 *  Created on: 2015-9-3
 *      Author: tiankonguse
 */

#include "shm.h"

namespace SHM_CACHE {
    
    Shm::Shm(){
        iShmID = 0;
        iKey = 0;
        iSize = 0;
    }
    
    void Shm::init(int iKey, int iSize) {
        memset(lastError, 0, sizeof(lastError));
        this->iKey = iKey;
        this->iSize = iSize;
    }
    
    
        
    int Shm::getShmId(int iFlag) {
        if (iShmID > 0) {
            return iShmID;
        }
        
        if(iKey == 0){
            snprintf(lastError, sizeof(lastError), "getShmId error. key = 0");
            return 0;        
        }
        
        iShmID = shmget(iKey, iSize, iFlag);
        if (iShmID < 0) {
            snprintf(lastError, sizeof(lastError), "getShmId error. ret=%d", errno);
            return 0;
        }
        return iShmID;
    }
    
    char* Shm::getAdr() {
        if (iShmID == 0) {
            snprintf(lastError, sizeof(lastError), "getAdr error. iShmID is 0");
            return NULL;
        }
        
        char* sShm = (char*) shmat(iShmID, NULL, 0);
        if ((char *) -1 == sShm) {
            snprintf(lastError, sizeof(lastError), "shmat error. ret=%d", errno);
            return NULL;
        }
        return sShm;
    }
    
    char* Shm::getShm(int iFlag) {
        if (getShmId(iFlag) == 0) {
            return NULL;
        }
        return getAdr();
    }
    
    int Shm::getShmSize(){
        struct shmid_ds stShmStat;
        if(shmctl(iShmID, IPC_STAT, &stShmStat) < 0){
            snprintf(lastError, sizeof(lastError), "shmctl IPC_STAT error. ret=%d", errno);
            return 0;
        }
        return stShmStat.shm_segsz;
    }
    
    char* Shm::getShmNoCreateAndCheck(int iFlag, int *piSize){
        int iRealShmSize = 0;
    
        if (getShmId(iFlag) == 0) {
            return NULL;
        }
        
        iRealShmSize = getShmSize();
        if(iRealShmSize == 0){
            return NULL;
        }
        
        if(*piSize != 0 && *piSize != iRealShmSize){
            snprintf(lastError, sizeof(lastError), "*piSize[%d] != iRealShmSize[%d]", *piSize, iRealShmSize);
            return NULL;
        }
        
        if(*piSize == 0){
            *piSize = iRealShmSize;
        }
        
        return getAdr();
    }
    
    int Shm::getShmInit(void * &pstShm, int iFlag) {
        volatile char *sShm = NULL;
        sShm = getShm(iFlag & (~IPC_CREAT));
        if (NULL == sShm) {
            if (!(iFlag & IPC_CREAT)) {
                return -1;
            }
            sShm = getShm(iFlag);
            if (!sShm) {
                return -2;
            }
            memset((char*) sShm, 0, iSize);
            
            pstShm = (void*)sShm;
            return 1;
        }
        pstShm = (void*)sShm;
        return 0;
    }
    int Shm::getShmInit(void ** ppstShm, int iFlag) {
        return getShmInit(*ppstShm, iFlag);
    }
    
    
    int Shm::delShm() {
        if (iShmID == 0) {
            snprintf(lastError, sizeof(lastError), "delShm error. iShmID is 0");
            return -1;
        }
        
        int iRet = shmctl(iShmID, IPC_RMID, NULL);
        if (iRet < 0) {
            snprintf(lastError, sizeof(lastError), "shmctl error. ret=%d",
            errno);
            return -2;
        }
        
        return 0;
    }
    



}
