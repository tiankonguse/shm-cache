/*
 * antisnow.h
 *
 *  Created on: 2016-9-17
 *      Author: tiankonguse
 */

#ifndef ANTISNOW_H_
#define ANTISNOW_H_

#include "shm.h"
#include <string>

namespace SHM_CACHE {
    
    const int ANTISNOW_SHM_ID = 456719;
    const int MAX_ANTISNOW_NODE = 100;
    
    class AntiSnow {
        typedef struct AntiSnowNode {
            unsigned int iNum;
            unsigned int iMax;
            unsigned int iTime;
            AntiSnowNode() {
                iNum = 0;
                iMax = 0;
                iTime = 0;
            }
        } AntiSnowNode;

        int iErrorNo;
        int iAntiSnowShmSize;
        AntiSnowNode *pstNode;
        Shm shm;
        std::string strLastError;
        char lastErrorBuf[LAST_ERROR_SIZE];

    public:
        
        AntiSnow();

        /*
         * 上报数值, 为 iKey 加上 iValue.  
         */
        int report(int iKey, int iValue);

        int getErrorNo() {
            return iErrorNo;
        }
        
        std::string getLastError() {
            return strLastError;
        }
    };

}

#endif /* ANTISNOW_H_ */
