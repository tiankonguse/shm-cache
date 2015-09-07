/*
 * hash.h
 *
 *  Created on: 2015-9-5
 *      Author: tiankonguse
 */

#ifndef HASH_H_
#define HASH_H_

#include <stdlib.h>
#include <stdint.h>

namespace SHM_CACHE {
    
    const uint32_t MAX_BUCKET = 2048;
    typedef uint32_t (*FCompare)(const void *, int ikeyLen, const void *);
    typedef uint32_t (*FElinemate)(const void *pKey, int ikeyLen,
            const void *pNode, uint32_t iTimeOut);
    
    typedef struct HashInfo {
        uint32_t uBucket; //实际的bucket大小
        uint32_t uShmSize; //申请内存的总大小
        uint32_t uNodeSize; //一个节点的大小
        char pHash[0];  //代表节点内存的起始位置
    } HashInfo;
    
    class Hash {
        HashInfo* shmHashInfo;
        HashInfo localHashInfo;
        const uint32_t maxBucket;
        FCompare _fCompare;
        FElinemate _fElinemate;
        uint32_t _inited;
    public:
        
        Hash(const uint32_t maxBucket = MAX_BUCKET);

        /*
         * 使用者传入预计bucket大小, 一个节点的大小, 
         * 返回一个需要申请的内存大小
         * 返回0 代表参数非法,或者节点过大
         */
        int EvalHashSize(uint32_t uSize, const uint32_t uNodeSize);

        /*
         * 传入内存起始地址, 比较函数, 淘汰函数来初始化hash
         * 初始化前会比较内存是第一次使用, 还是复用
         * 第一次使用需要初始化, 复用需要检查配置是否一致
         */
        int init(void* pHash, FCompare fCompare, FElinemate fElinemate);

        /*
         * 传入key的字符串,key的长度
         * 返回值会付给node指针
         */
        int Hash::get(const void *pKey, uint32_t uKeyLen, void *&pNode);

        /*
         * 传入key的字符串,key的长度, 以及超时时间
         * 搜索到的空节点和淘汰都会放在pNode节点中
         */
        int getAll(const void *pKey, uint32_t uKeyLen, void *&pNode,
                int iTimeOut);

    private:
        
        /*
         * 根据传入的bucket, 计算出一个合适的bucket
         * 目前算法: 找到大于等于uSize的第一个素数
         */
        void calcPrime(uint32_t uSize, uint32_t& uBucket);

        uint32_t hash(uint32_t uHashKey);

        uint32_t hash(const char *sKey, int iSize);
        
    };

}

#endif /* HASH_H_ */
