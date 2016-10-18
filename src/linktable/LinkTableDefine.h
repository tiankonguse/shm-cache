/******************************************************************************

 FILENAME:	LinkTableDefine.h

 DESCRIPTION:	数组索引+链表方式内存结构定义

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

#ifndef _LINK_TABLE_DEFINE_H_
#define _LINK_TABLE_DEFINE_H_

#define MAX_SHM_SIZE (1024UL*1024*1024*120)
#define MAX_BLOCK_COUNT (200)
#define MAX_ELEMENT_SIZE (200)

#define MAX_LT_NUM 128

#define LT_MAX_IDXDATA_SIZE 1024

typedef struct {
	unsigned long ddwPosition :63;
	unsigned long cFlag :1; // 0 - not used, 1 - in use
} IndexNode;

typedef struct {
	uint64_t ddwIndexId;
	int iIndex;
	uint32_t dwIndexDataLen; // 每个主索引允许有用户自定义的数据，最大不超过LT_MAX_IDXDATA_SIZE
	uint8_t abIndexData[LT_MAX_IDXDATA_SIZE];
} IndexHead;

typedef struct {
	uint64_t ddwKey;
	uint64_t ddwNext;
	uint8_t bufData[0];
} Element;

#define _GET_ELE_AT(elist, idx, sz) ((Element *)(((char *)(elist)) + ((idx) * (sz))))

// 返回索引所指向的Element
#define LT_ELE_AT(pLT, idx) _GET_ELE_AT((pLT)->pstElementList, idx, (pLT)->pstTableHead->ddwElementSize)

// 根据句柄返回LinkTable内部结构指针
#define GET_LT(lt) (((lt)>=0 && (lt)<iMaxLtNum)? &g_stTables[lt]:NULL)

// 外部索引转成内部索引ID
#define INDEX_TO_ID(dwIndex)  ((dwIndex)+1)

// 预回收池最多占用256K
#define LT_MAX_PREFREE_POOL_SIZE     (256UL*1024/8)
#define LT_DEF_PREFREE_POOL_SIZE   100
// 预留给数据结构扩展的空间
#define LT_MAX_RESERVE_LEN      (256UL*1024)
#define LT_MAX_USRDATA_LEN      ((512UL*1024)-4)

typedef struct {
	uint64_t ddwIndexCount;
	uint64_t ddwIndexRowCount;
	uint64_t ddwAllElementCount;
	uint64_t ddwFreeElementCount;
	uint64_t ddwElementSize;
	uint64_t ddwLastEmpty;
	uint64_t ddwUsedIndexCount;
	uint8_t bLockedIndexIds;
	uint8_t sReserved1[3];
	uint32_t dwPreFreePoolSize; // 预回收池的大小，可配置，不大于LT_MAX_PREFREE_POOL_SIZE
	uint64_t ddwFirstFreePos; // All free elements are linked together by ddwNext
	uint64_t ddwPreFreeIndex; // Current index to addwPreFreePool
	uint64_t addwPreFreePool[LT_MAX_PREFREE_POOL_SIZE]; // All elements to be deleted are put here for delayed deletion
	uint8_t sReserved2[LT_MAX_RESERVE_LEN];  // Reserved for future extension
	uint32_t dwUserDataLen;
	uint8_t sUserData[LT_MAX_USRDATA_LEN];  // Reserved for application use
} LinkTableHead;

typedef struct {
	LinkTableHead *pstTableHead;
	IndexHead *pstIndexHeadList;
	IndexNode *pstIndexNodeList;
	Element *pstElementList;
} LinkTable;

volatile LinkTable *GetLinkTable(int lt);

#endif
