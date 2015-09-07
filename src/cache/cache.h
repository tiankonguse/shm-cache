/*
 * cache.h
 *
 *  Created on: 2015-9-5
 *      Author: tiankonguse
 */

#ifndef CACHE_H_
#define CACHE_H_

#include <string>

#include "shm.h"
#include "hash.h"

namespace SHM_CACHE {
    
#define MAX_KEY_LEN 128
#define MAX_VALUE_LEN  1024
#pragma pack(1)
    typedef struct {
        char cTimeOutFlag;
        uint8_t cKeyLen;
        uint32_t dwValueLen;
        uint32_t dwAccessTime;
        char cKey[0];
        char sData[0];
    } Node;
#pragma Node()
    
    class Cache {
        Node* _node;
        //hash 函数操作类
        Hash _hash;

        //共享内存操作类
        Shm _shm;

        //超时时间
        int iTimeOut;
        int maxKeyLen;
        int maxValueLen;
        int iNodeSize;
    private:
        uint32_t HashKey(const char *sKey, int iSize);

        uint32_t fCompare(const void *, int, const void *);
        uint32_t fElinemate(const void *pKey, int ikeyLen, const void *pNode,
                uint32_t iTimeOut);
    public:
        Cache();
        Cache::~Cache();
        /*
         * 初始化, 指定共享内存的key, hash的bucket大小,以及cache超时时间
         */
        int init(int _iKey, int _bucket, int _iTimeOut);

        int getCache(const std::string &strKey, std::string &strValue);

        int setCache(const std::string &strKey, const std::string &strValue);

        int delCache(const std::string &strKey);
    };
}

#endif /* CACHE_H_ */
