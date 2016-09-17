/*************************************************************************
 > File Name: monitor.h
 > Desc: 统计监听库
 > Author: tiankonguse(skyyuan)
 > Mail: i@tiankonguse.com 
 > Created Time: 2016年06月19日 星期日 21时54分18秒
 ***********************************************************************/

#ifndef _MONITOR_H_
#define _MONITOR_H_

#include "shm.h"
#include <string>

namespace SHM_CACHE {
    
    const int MONITOR_SHM_ID = 45678;
    const int MAX_MONITOR_NODE = 21000;
    
    class Monitor {
        typedef struct {
            int iUse;
            int iKey;
            int iValue;
        } MonitorNode;

        int iErrorNo;
        int iMonitorShmSize;
        MonitorNode *pstNode;
        Shm shm;
        std::string strLastError;

    private:
        
        /*
         * 查找位置.
         * 0 未找到, iPos 为第一个未使用的位置
         * 1 找到, iPos 为找到的位置
         * -1 空间不足
         */
        int find(int iKey, int& iPos);

    public:
        
        Monitor();

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
;

#endif
