/*
 * hash.cpp
 *
 *  Created on: 2015-9-5
 *      Author: tiankonguse
 */

#include "hash.h"
#include <math.h>

namespace SHM_CACHE {
    
    Hash::Hash(const uint32_t _maxBucket) :
            shmHashInfo(NULL) {
        maxBucket = _maxBucket;
        if (maxBucket <= 0) {
            maxBucket = MAX_BUCKET;
        }
        
        localHashInfo.uBucket = 0;
        localHashInfo.uNodeSize = 0;
        localHashInfo.uShmSize = 0;
        _fCompare = NULL;
        _fElinemate = NULL;
        _inited = 0;
    }
    
    void Hash::calcPrime(uint32_t uSize, uint32_t& uBucket) {
        for (uint32_t n = uSize;; n++) {
            uint32_t m = 1;
            uint32_t sqrtn = sqrt(n * 1.0);
            for (uint32_t j = 2; j <= sqrtn; ++j) {
                if (n % j) {
                    m = 0;
                    break;
                }
            }
            if (m) {
                uBucket = n;
                break;
            }
        }
    }
    
    int Hash::EvalHashSize(uint32_t uSize, const uint32_t uNodeSize) {
        //计算前就大于最大bucket, 直接返回
        if (maxBucket < uSize) {
            return -1;
        }
        
        //先去计算bucket大小
        calcPrime(uSize, localHashInfo.uBucket);
        
        //计算后发现大于最大bucket, 直接返回
        if (maxBucket < localHashInfo.uBucket) {
            return -2;
        }
        
        localHashInfo.uNodeSize = uNodeSize;
        localHashInfo.uShmSize = localHashInfo.uBucket * localHashInfo.uNodeSize
                + sizeof(HashInfo);
        
        return localHashInfo.uShmSize;
    }
    
    int Hash::init(void* pHash, FCompare fCompare, FElinemate fElinemate) {
        if (!pHash) {
            return -1;
        }
        
        shmHashInfo = (HashInfo*) pHash;
        
        //第一次申请,会初始化为0, 不为0,说明不是第一次, 需要安全验证
        if (shmHashInfo->uBucket || shmHashInfo->uNodeSize
                || shmHashInfo->uShmSize) {
            if (shmHashInfo->uBucket != localHashInfo.uBucket
                    || shmHashInfo->uNodeSize != localHashInfo.uNodeSize
                    || shmHashInfo->uShmSize != localHashInfo.uShmSize) {
                return -2;
            }
        } else {
            shmHashInfo->uBucket = localHashInfo.uBucket;
            shmHashInfo->uNodeSize = localHashInfo.uNodeSize;
            shmHashInfo->uShmSize = localHashInfo.uShmSize;
        }
        _fCompare = fCompare;
        _fElinemate = fElinemate;
        
        if (shmHashInfo->uShmSize
                != shmHashInfo->uNodeSize * shmHashInfo->uBucket
                        + sizeof(HashInfo)) {
            return -3;
        }
        
        _inited = 1;
        
        return 0;
    }
    
    uint32_t Hash::hash(const char *sKey, int iSize) {
        uint32_t dwKey = 0;
        static const uint64_t seed = 17;
        uint32_t uHashKey = 0;
        for (size_t i = 0; i < iSize; i++) {
            dwKey = dwKey * seed + sKey[i];
        }
        
        uHashKey = hash(dwKey);
        
        return uHashKey;
    }
    
    int Hash::get(const void *pKey, uint32_t uKeyLen, void *&pNode) {
        //参数简单检查
        if (pKey == NULL) {
            return -1;
        }
        
        //hash 必须初始化
        if (_inited == 0) {
            return -2;
        }
        
        char *pRow = NULL;
        uint32_t uHashKey = 0;
        
        //hash 找到对应的位置
        uHashKey = hash((const char *) pKey, uKeyLen);
        
        //定位到 hash 的位置
        pRow = (char*) (shmHashInfo->pHash) + uHashKey * shmHashInfo->uNodeSize;
        if (_fCompare(pKey, uKeyLen, pRow) == 0) {
            pNode = pRow;
            return 1;
        }
        return 0;
    }
    
    int Hash::getAll(const void *pKey, uint32_t uKeyLen, void *&pNode,
            int iTimeOut) {
        //参数简单检查
        if (pKey == NULL) {
            return -1;
        }
        
        //hash 必须初始化
        if (_inited == 0) {
            return -2;
        }
        
        char *pRow = NULL;
        uint32_t uHashKey = 0;
        
        //hash 找到对应的位置
        uHashKey = hash((const char *) pKey, uKeyLen);
        
        //定位到 hash 的位置
        pRow = (char*) (shmHashInfo->pHash) + uHashKey * shmHashInfo->uNodeSize;
        if (_fElinemate(pKey, uKeyLen, pRow, iTimeOut) == 0) {
            pNode = pRow;
            return 1;
        }
        return 0;
    }
    
    uint32_t Hash::hash(uint32_t uHashKey) {
        return uHashKey % shmHashInfo->uBucket;
    }

}
