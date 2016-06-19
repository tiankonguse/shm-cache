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
        memset(lastError, 0, sizeof(lastError));
        this->iKey = iKey;
        this->iSize = iSize;
    }
    
    int Shm::getShmInit(volatile void * &pstShm, int iFlag) {
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
            
            pstShm = sShm;
            return 1;
        }
        pstShm = sShm;
        return 0;
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
    
    char* Shm::getAdr() {
        char* sShm = NULL;
        sShm = (char*) shmat(iShmID, NULL, 0);
        if ((char *) -1 == sShm) {
            snprintf(lastError, sizeof(lastError), "shmat error. ret=%d",
            errno);
            return NULL;
        }
        return sShm;
    }
    
    int Shm::getShmId(int iFlag) {
        if (iShmID > 0) {
            return iShmID;
        }
        
        iShmID = shmget(iKey, iSize, iFlag);
        if (iShmID < 0) {
            snprintf(lastError, sizeof(lastError), "shmget error. ret=%d",
            errno);
            return 0;
        }
        return iShmID;
    }
    
    char* Shm::getShm(int iFlag) {
        /*
         * 得到一个共享内存标识符或创建一个共享内存对象
         */
        if (getShmId(iFlag) == 0) {
            return NULL;
        }
        return getAdr();
    }

}
