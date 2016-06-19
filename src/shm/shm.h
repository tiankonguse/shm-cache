/*************************************************************************
 > File Name: shm.h
 > Desc: 面向对象共享内存库(非进程安全,由上层业务保证)
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
#include <stdlib.h>
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
         * 初始化key和内存大小
         */
        void init(int iKey, int iSize);
        

        /*
         * @desc 基础操作: 根据iKey, 查询或创建iSize大小的共享内存
         * 
         * @return iShmID
         */
        int getShmId(int iFlag);
        

        /*
         * @desc 基础操作: 根据iShmID, 映射到进程内地址
         * 
         * @return 映射到进程内的地址
         */
        char* getAdr();
        
        
        
        /*
         * @desc 根据iKey, 查询或创建iSize大小的共享内存
         * 
         * @param iFlag 标志位,常用标志位是 (0666 | IPC_CREAT)
         * 
         * @return 返回共享内存的指针,失败时返回 NULL
         */
        char* getShm(int iFlag);

        /*
         * @desc 根据iShmID, 查询或创建iSize大小的共享内存, 第一次创建时初始化内存
         * 
         * @param pstShm 返回的共享内存指针, 失败时未定义
         * @param iFlag 标志位,常用标志位是 (0666 | IPC_CREAT)
         * 
         * @return 失败时返回负数. 
         *    0 已存在, 查询成功
         *    1 创建成功
         *   -1 共享内存不存在, flag没有 IPC_CREAT 标示位
         *   -2 共享内存不一致或者不存在, 但是创建失败
         *    
         */
        int getShmInit(void * &pstShm, int iFlag);
        int getShmInit(void ** ppstShm, int iFlag);
        
        /*
         * 得到共享内存大小
         */
        int getShmSize();
        
        
        /*
         * @desc 根据iKey, 得到共享内存, 并检查Size大小
         *       如果 piSize 是0, 则赋值 
         * 
         * @return 返回共享内存的指针,失败时返回 NULL
         */
        char* getShmNoCreateAndCheck(int iFlag, int *piSize);

        /*
         * @desc 根据shkey删除指定的共享内存
         * 
         * @param shkey 我们分配的共享内存的唯一标识
         * 
         * @return 成功返回0, 失败返回非0
         */
        int delShm();


        char* getLastError() {
            return lastError;
        }
    };

}
#endif

