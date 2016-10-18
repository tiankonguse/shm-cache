/******************************************************************************

 FILENAME:	LinkTable.h

 DESCRIPTION:	数组索引+链表方式内存结构接口声明

 LinkTable的数据结构：
 IndexHeadList -- 一级索引表，也是主索引表，通过主索引Index(或者叫Unit)可以索引到二级索引表的起始位置
 IndexNodeList -- 二级索引表，也是次索引表，以主索引和次索引Offset共同索引Key(UIN)在ElementList中的下标
 ElementList   -- 元素数组

 注意：
 LinkTable有两类Index
 1, 外部看到的Index，就是UDC的UnitID，从0开始，记为dwIndex
 2, 内部看到的Index=UnitID+1，写作idx，或者IndexID
 所有API的Index参数都是外部Index，也就是UnitID


 ******************************************************************************/

#ifndef _LINK_TABLE_H_
#define _LINK_TABLE_H_

//初始化链式表
//返回LinkTable的句柄
int LtInit(uint32_t dwIndexCount, uint64_t ddwIndexRowCount,
		uint64_t ddwElementCount, uint64_t ddwElementSize, int iKey,
		int iCreateFlag, int nodisplay);

// 如果主索引表为空，则将LinkTable的IndexHeadList初始化为相应的IndexList
// 否则，比较IndexHeadList和IndexList，如果不相对应则返回失败
// dwIndexList必须是已经排好序的
// 成功返回0，否则返回负数
int LtSetIndexes(int lt, uint32_t dwIndexList[], int iIndexNum);

// 判断初始化的时候是不是调用了LtSetIndexes()来设置UnitId
// 如果是，则不允许增加或删除UnitId， 函数返回 1，否则返回0
int LtIsIndexesLocked(int lt);

//取得用户数据
int LtGetData(int lt, uint64_t ddwKey, char *sDataBuf, int *piDataLen);

//打印用户数据的链表指针
int LtPrintData(int lt, uint64_t ddwKey);

//设置数据,如果存在删除已有数据
int LtSetData(int lt, uint64_t ddwKey, char *sDataBuf, int iDataLen);

//删除指定索引下的所有数据包括索引定义
int LtClearIndexData(int lt, uint32_t dwIndex);

//清除某个Key对应的数据
int LtClearKeyData(int lt, uint64_t ddwKey);

// 允许用户改变预回收池的大小，sz必须在1到LT_MAX_PREFREE_POOL_SIZE之间
int LtSetPrefreePoolSize(int lt, int sz);

int LtGetHeaderData(int lt, void *pBuff, int iSize);
int LtSetHeaderData(int lt, const void *pData, int iLen);

int LtGetIndexData(int lt, uint32_t dwIndex, void *pBuff, int iSize);
int LtSetIndexData(int lt, uint32_t dwIndex, const void *pData, int iLen);

//释放头部
int LtRemoveIndex(int lt, uint32_t dwIndex);
int LtPrintInfo(int lt);
int LtPrintElements(int lt);

//关闭链式表共享内存
int LtClose(int lt);

#endif
