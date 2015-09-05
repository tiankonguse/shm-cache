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
    
    uint32_t Hash::EvalHashSize(uint32_t uSize, const uint32_t uNodeSize) {
        //计算前就大于最大bucket, 直接返回
        if (maxBucket < uSize) {
            return 0;
        }
        
        //先去计算bucket大小
        calcPrime(uSize, localHashInfo.uBucket);
        
        //计算后发现大于最大bucket, 直接返回
        if (maxBucket < localHashInfo.uBucket) {
            return 0;
        }
        
        localHashInfo.uShmSize = localHashInfo.uBucket * uNodeSize
                + sizeof(HashInfo);
        
        return localHashInfo.uShmSize;
    }
    
    uint32_t Hash::init(void* pHash, FCompare compare, FElinemate elinemate) {
        if (!shmHashInfo) {
            return -1;
        }
        
        return 0;
    }
}
