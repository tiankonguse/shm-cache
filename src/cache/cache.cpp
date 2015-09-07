/*
 * cache.cpp
 *
 *  Created on: 2015-9-6
 *      Author: tiankonguse
 */

#include "cache.h"

namespace SHM_CACHE {
    
    Cache::Cache() :
            iTimeOut(0), maxKeyLen(MAX_KEY_LEN), maxValueLen(MAX_VALUE_LEN), iNodeSize(
                    sizeof(Node) + MAX_KEY_LEN + MAX_VALUE_LEN) {
        _node = NULL;
    }
    
    Cache::~Cache() {
        if (_node) {
            free(_node);
            _node = NULL;
        }
    }
    
    int Cache::init(int _iKey, int _bucket, int _iTimeOut) {
        uint64_t uShmSize = 0;
        volatile void * pHash = NULL;
        
        //简单的检查超时时间
        iTimeOut = _iTimeOut;
        if (iTimeOut <= 0) {
            return -1;
        }
        
        if (_node) {
            free(_node);
            _node = NULL;
        }
        _node = (Node*) malloc(iNodeSize * sizeof(char));
        if (!_node) {
            return -5;
        }
        
        //计算内存大小
        uShmSize = _hash.EvalHashSize(_bucket, iNodeSize);
        if (uShmSize <= 0) {
            return -2;
        }
        
        _shm.init(_iKey, uShmSize);
        
        if (_shm.getShm2(pHash, (int) (0666 | IPC_CREAT)) < 0) {
            return -3;
        }
        
        if (_hash.init((void *) pHash, fCompare, NULL) < 0) {
            return -4;
        }
        
        return 0;
    }
    
    uint32_t Cache::fCompare(const void * vKey, int ikeyLen,
            const void * vNode) {
        const char * pKey = (const char *) vKey;
        const Node * pNode = (const Node *) vNode;
        
        if (!pKey || !pNode) {
            return 1;
        }
        
        if (pNode->cKeyLen != ikeyLen) {
            return 2;
        }
        
        return memcmp(pNode->cKey, pKey, ikeyLen);
    }
    uint32_t Cache::fElinemate(const void *vKey, int ikeyLen, const void *vNode,
            uint32_t iTimeOut) {
        
        const char * pKey = (const char *) vKey;
        const Node * pNode = (const Node *) vNode;
        
        if (!pKey || !pNode) {
            return 1;
        }
        
        uint32_t dwNow = time(NULL);
        if (pNode->dwAccessTime + iTimeOut > dwNow) {
            return 3;
        }
        
        return 0;
    }
    int Cache::getCache(const std::string &strKey, std::string &strValue) {
        
        if (strKey.length() > maxKeyLen) {
            return -1;
        }
        
        void* vNode = NULL;
        int iRet = _hash.get((void*) strKey.c_str(), strKey.length(), vNode);
        if (iRet < 0) {
            return -2;
        }
        
        Node* pNode = (Node*) vNode;
        
        if (iRet == 0) {
            return 1;
        }
        
        char* pData = (pNode->sData + MAX_KEY_LEN);
        
        strValue.assign(pData, pNode->dwValueLen);
        
        return 0;
    }
    
    int Cache::setCache(const std::string &strKey,
            const std::string &strValue) {
        
        if (strKey.length() <= 0 || strValue.length() <= 0) {
            return -1;
        }
        
        if (strKey.length() > MAX_KEY_LEN) {
            return -2;
        }
        
        if (strValue.length() > MAX_VALUE_LEN) {
            return -3;
        }
        
        memset(_node, 0, iNodeSize);
        _node->cKeyLen = strKey.length();
        _node->dwAccessTime = time(NULL);
        _node->dwValueLen = strValue.length();
        memcpy(_node->sData, strKey.c_str(), strKey.length());
        memcpy(_node->sData + MAX_KEY_LEN, strValue.c_str(), strValue.length());
        
        uint8_t cKeyLen = strKey.length();
        void* vNode = NULL;
        int iRet = _hash.getAll((void*) strKey.c_str(), strKey.length(), vNode,
                iTimeOut);
        if (iRet < 0) {
            return -4;
        }
        
        if (iRet == 0) {
            return 1;
        }
        memcpy(vNode, _node, iNodeSize);
        
        return 0;
    }
    
    int Cache::delCache(const std::string &strKey) {
        
        if (strKey.length() > maxKeyLen) {
            return -1;
        }
        
        void* vNode = NULL;
        int iRet = _hash.get((void*) strKey.c_str(), strKey.length(), vNode);
        if (iRet < 0) {
            return -2;
        }
        
        if (iRet == 0) {
            return 0;
        }
        
        memset(vNode, 0, iNodeSize);
        
        return 0;
    }

}

