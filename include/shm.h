/*************************************************************************
 > File Name: shm.h
 > Author: tiankonguse(skyyuan)
 > Mail: i@tiankonguse.com 
 > Created Time: 2015年09月03日 星期四 17时14分18秒
 ***********************************************************************/

#ifndef _SHM_H_
#define _SHM_H_

#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

namespace SHM_CACHE {
    
    const int LAST_ERROR_SIZE = 128;
    
    class Shm {
        int iShmID;
        int iKey;
        int iSize;
        char lastError[LAST_ERROR_SIZE];
    public:
        
        Shm();

        /*
         * 初始化key和大小
         */
        void init(int iKey, int iSize);

        /*
         * 根据iShmID, 映射到进程内地址
         */
        char* getAdr();

        /*
         * @desc 根据iShmID, 查询或创建iSize大小的共享内存
         * 
         * @param pstShm 返回的共享内存指针, 失败时未定义
         * @param iFlag 标志位,常用标志位是 (0666 | IPC_CREAT)
         * 
         * @return 失败时返回负数. 返回0代表查询成功, 返回1代表创建
         */
        int getShm2(volatile void **pstShm, int iFlag);

        /*
         * @desc 根据shkey删除指定的共享内存
         * 
         * @param shkey 我们分配的共享内存的唯一标识
         * 
         * @return 成功返回0, 失败返回非0
         */
        int delShm();

        /*
         * @desc 根据iKey, 查询或创建iSize大小的共享内存
         * 
         * @param iFlag 标志位,常用标志位是 (0666 | IPC_CREAT)
         * 
         * @return 返回共享内存的指针,失败时返回 NULL
         */
        char* getShm(int iFlag);

        int getShmId(int iFlag);

        char* getLastError() {
            return lastError;
        }
    };

}
#endif

