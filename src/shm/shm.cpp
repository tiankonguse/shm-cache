/*
 * shm.cpp
 *
 *  Created on: 2015-9-3
 *      Author: tiankonguse
 */

#include "shm.h"

namespace SHM_CACHE {
    
    Shm::Shm() {
        iShmID = 0;
        iKey = 0;
        iSize = 0;
    }
    
    void Shm::init(int iKey, int iSize) {
        memset(lastErrorBuf, 0, sizeof(lastErrorBuf));
        this->iKey = iKey;
        this->iSize = iSize;
    }
    
    int Shm::getShmId(int iFlag) {
        if (iShmID > 0) {
            return iShmID;
        }
        
        if (iKey == 0) {
            snprintf(lastErrorBuf, sizeof(lastErrorBuf),
                    "getShmId error. key = 0");
            return 0;
        }
        
        iShmID = shmget(iKey, iSize, iFlag);
        if (iShmID < 0) {
            snprintf(lastErrorBuf, sizeof(lastErrorBuf),
                    "getShmId error. ret=%d",
                    errno);
            return 0;
        }
        return iShmID;
    }
    
    char* Shm::getAdr() {
        if (iShmID == 0) {
            snprintf(lastErrorBuf, sizeof(lastErrorBuf),
                    "getAdr error. iShmID is 0");
            return NULL;
        }
        
        char* sShm = (char*) shmat(iShmID, NULL, 0);
        if ((char *) -1 == sShm) {
            snprintf(lastErrorBuf, sizeof(lastErrorBuf), "shmat error. ret=%d",
            errno);
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
    
    int Shm::getShmSize() {
        struct shmid_ds stShmStat;
        if (shmctl(iShmID, IPC_STAT, &stShmStat) < 0) {
            snprintf(lastErrorBuf, sizeof(lastErrorBuf),
                    "shmctl IPC_STAT error. ret=%d", errno);
            return -1;
        }
        return stShmStat.shm_segsz;
    }
    
    char* Shm::getShmNoCreateAndCheck(int iFlag, int *piSize) {
        int iRealShmSize = 0;
        
        if (getShmId(iFlag) == 0) {
            return NULL;
        }
        
        iRealShmSize = getShmSize();
        if (iRealShmSize == 0) {
            return NULL;
        }
        
        if (*piSize != 0 && *piSize != iRealShmSize) {
            snprintf(lastErrorBuf, sizeof(lastErrorBuf),
                    "*piSize[%d] != iRealShmSize[%d]", *piSize, iRealShmSize);
            return NULL;
        }
        
        if (*piSize == 0) {
            *piSize = iRealShmSize;
        }
        
        return getAdr();
    }
    
    int Shm::getShmInit(void * &pstShm, int iFlag) {
        volatile char *sShm = NULL;
        sShm = getShm(iFlag & (~IPC_CREAT));
        if (NULL == sShm) {
            if (!(iFlag & IPC_CREAT)) {
                return -__LINE__;
            }
            sShm = getShm(iFlag);
            if (!sShm) {
                return -__LINE__;
            }
            memset((char*) sShm, 0, iSize);
            
            pstShm = (void*) sShm;
            return -__LINE__;
        }
        pstShm = (void*) sShm;
        return 0;
    }
    int Shm::getShmInit(void ** ppstShm, int iFlag) {
        return getShmInit(*ppstShm, iFlag);
    }
    
    int Shm::delShm() {
        if (iShmID == 0) {
            snprintf(lastErrorBuf, sizeof(lastErrorBuf),
                    "delShm error. iShmID is 0");
            return -__LINE__;
        }
        
        int iRet = shmctl(iShmID, IPC_RMID, NULL);
        if (iRet < 0) {
            snprintf(lastErrorBuf, sizeof(lastErrorBuf), "shmctl error. ret=%d",
            errno);
            return -__LINE__;
        }
        
        return 0;
    }

}
