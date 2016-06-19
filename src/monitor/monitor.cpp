/*
 * monitor.cpp
 *
 *  Created on: 2016-6-19
 *      Author: tiankonguse
 */

#include "monitor.h"

namespace SHM_CACHE {
    
    Monitor::Monitor(){
        pstNode = NULL;
        iErrorNo = 0;
        iMonitorShmSize = sizeof(MonitorNode)*MAX_MONITOR_NODE;
        shm.init(MONITOR_SHM_ID, iMonitorShmSize);
    }
    
    int Monitor::find(int iKey, int& iPos){
        iPos = 0;
        while(pstNode[iPos].iUse){
            if(pstNode[iPos].iKey == iKey){
                return 1;
            }
            ++iPos;
            if(iPos >= MAX_MONITOR_NODE){
                iPos = 0;
                return -1;
            }
        }
        return 0;
    }
    
    int Monitor::report(int iKey, int iValue){
        int iPos;
        
        //第一次创建
        if(pstNode == NULL){
            iErrorNo = shm.getShmInit((void**)&pstNode, 0666);
            if(iErrorNo < 0){
                strLastError = shm.getLastError();
                return iErrorNo;
            }
            iErrorNo = 0;
        }
        
        iErrorNo = find(iKey, iPos);
        if(iErrorNo == 0){
            pstNode[iPos].iUse = 1;
            pstNode[iPos].iKey = iKey;
            pstNode[iPos].iValue = iValue;
        }else if(iErrorNo < 0){
            strLastError = "no space";
            return -1;
        }else{
            pstNode[iPos].iValue += iValue;
        }

        return 0;
        
    }
    
    
};
