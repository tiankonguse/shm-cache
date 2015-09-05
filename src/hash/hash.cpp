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
            shmHashInfo(NULL), maxBucket(_maxBucket) {
        localHashInfo.uBucket = 0;
        localHashInfo.uNodeSize = 0;
        localHashInfo.uShmSize = 0;
        _fCompare = NULL;
        _fElinemate = NULL;
        _fHash = NULL;
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
    
    int Hash::init(void* pHash, FCompare fCompare, FElinemate fElinemate,
            FHash fHash) {
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
        _fHash = fHash;
        
        if (shmHashInfo->uShmSize
                != shmHashInfo->uNodeSize * shmHashInfo->uBucket
                        + sizeof(HashInfo)) {
            return -3;
        }
        
        _inited = 1;
        
        return 0;
    }
    
    int Hash::get(const void *pKey, uint32_t uKeySize, void *&pNode) {
        //参数简单检查
        if (pKey == NULL || uKeySize == 0) {
            return -1;
        }
        
        //hash 必须初始化
        if (_inited == 0) {
            return -2;
        }
        
        uint32_t uHashKey = 0;
        char *pRow = NULL;
        
        //hash 找到对应的位置
        uHashKey = _fHash(pKey, uKeySize) % shmHashInfo->uBucket;
        
        //定位到 hash 的位置
        pRow = (char*) (shmHashInfo->pHash) + uHashKey * shmHashInfo->uNodeSize;
        if (_fCompare(pKey, pRow) == 0) {
            pNode = pRow;
            return 1;
        }
        return 0;
    }

}
