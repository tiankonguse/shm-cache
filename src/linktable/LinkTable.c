/******************************************************************************

 FILENAME:	LinkTable.c

 DESCRIPTION:		数组索引+链表方式内存接口定义

 LinkTable的数据结构：
 IndexHeadList -- 一级索引表，也是主索引表，通过主索引Index(或者叫Unit)可以索引到二级索引表的起始位置
 IndexNodeList -- 二级索引表，也是次索引表，以主索引和次索引Offset共同索引Key(UIN)在ElementList中的下标
 ElementList   -- 元素数组

 注意：
 LinkTable有两类Index
 1, 外部看到的Index，就是UDC的UnitID，从0开始，记为dwIndex
 2, 内部看到的Index=UnitID+1，写作idx
 所有API的Index参数都是外部Index，也就是UnitID

 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/mman.h>

#ifdef  __cplusplus
extern "C" {
#endif
#include "common.h"
#include "oi_shm.h"
#include "oi_str2.h"
#include "Attr_API.h"
#include "oi_debug.h"
#ifdef  __cplusplus
}
#endif

#include "LinkTableDefine.h"
#include "LinkTable.h"

#ifdef ELEMENT_DATABUF_LEN
#undef ELEMENT_DATABUF_LEN
#endif
#define ELEMENT_DATABUF_LEN(pTable)	(pTable->pstTableHead->ddwElementSize - (unsigned long)(((Element*)0)->bufData))

int iMaxLtNum = 0;
volatile LinkTable g_stTables[MAX_LT_NUM];

static int compare_indexid(const void *pa, const void *pb) {
	const IndexHead *a, *b;

	a = (const IndexHead *) pa;
	b = (const IndexHead *) pb;

	return a->ddwIndexId - b->ddwIndexId;
}

static int InsertIndexHead(volatile LinkTable *pTable, uint64_t idx,
		IndexHead *base, size_t nmemb, size_t nmaxmemb, IndexHead **ppPtr,
		int createFlag) {
	IndexHead ih;
	IndexHead *pSearch = NULL;

	int iEqual = 0;

	if (idx == 0 || base == NULL || nmaxmemb == 0 || pTable == NULL
			|| pTable->pstTableHead == NULL) {
		return -1;
	}

	ih.ddwIndexId = idx;
	ih.iIndex = 0;

	pSearch = (IndexHead *) my_bsearch(&ih, base, nmemb, sizeof(IndexHead),
			&iEqual, compare_indexid);
	if (pSearch && iEqual) {
		if (ppPtr) {
			*ppPtr = pSearch;
		}
		return 0;
	}

	if (createFlag == 0 ||
	// If IndexIds are loaded at init time, no further modification is allowed
			pTable->pstTableHead->bLockedIndexIds) {
		return -2;
	}

	if ((nmaxmemb - nmemb) < 1) {
		return -2; //特殊的返回码，注意保持
	}

	if (pSearch) {
		int iIndex = base[nmemb].iIndex;

		// move one element forwards
		memmove(pSearch + 1, pSearch,
				((unsigned long) (base + nmemb)) - ((unsigned long) pSearch));

		pSearch->iIndex = iIndex;
	} else {
		pSearch = base + nmemb;
	}

	pSearch->ddwIndexId = idx;

	// Can index go insane?
	if (pSearch->iIndex < 0 || (size_t) pSearch->iIndex >= nmaxmemb) {
		// BUG: insane index
		Attr_API(162204, 1);
		pSearch->iIndex = nmemb;
	}

	if (ppPtr)
		*ppPtr = pSearch;

	return 1;
}

static uint64_t CalShmSize(uint64_t ddwIndexCount, uint64_t ddwIndexRowCount,
		uint64_t ddwElementCount, uint64_t ddwElementSize) {
	uint64_t ddwShmSize = (sizeof(LinkTableHead)
			+ sizeof(IndexHead) * ddwIndexCount
			+ sizeof(IndexNode) * ddwIndexCount * ddwIndexRowCount
			+ ddwElementSize * ddwElementCount);

	// 2M 对齐
	return (ddwShmSize + (2UL << 20) - 1) & (~((2UL << 20) - 1));
}

// 这里重写一下，支持unsigned size，不打印出错，不mlock
static void *LT_GetShm(int iKey, uint64_t ddwSize, int iFlag) {
	int iShmID;
	char *sShm;

	if ((iShmID = shmget(iKey, ddwSize, iFlag)) < 0)
		return 0;

	if ((sShm = (char *) shmat(iShmID, NULL, 0)) == (char *) -1)
		return 0;

	return sShm;
}

static char *CreateLinkTableShm(int iKey, uint64_t ddwShmSize,
		uint64_t ddwIndexCount, uint64_t ddwIndexRowCount,
		uint64_t ddwElementCount, uint64_t ddwElementSize) {
	unsigned long u, hdrsize = sizeof(LinkTableHead)
			+ sizeof(IndexHead) * ddwIndexCount;
	LinkTableHead *pTableHead;

	pTableHead = (LinkTableHead *) GetShm(iKey, ddwShmSize,
			0666 | IPC_CREAT | SHM_HUGETLB);
	if (pTableHead == NULL)
		return 0;

	memset(pTableHead, 0, hdrsize);

	// The first element is reserved
	pTableHead->ddwAllElementCount = ddwElementCount - 1;
	pTableHead->ddwFreeElementCount = ddwElementCount - 1;
	pTableHead->ddwElementSize = ddwElementSize;
	pTableHead->ddwIndexCount = ddwIndexCount;
	pTableHead->ddwIndexRowCount = ddwIndexRowCount;
	pTableHead->dwPreFreePoolSize = LT_DEF_PREFREE_POOL_SIZE;
	pTableHead->ddwLastEmpty = 1;

	// 初始化索引值为自身所在的索引
	for (u = 0; u < ddwIndexCount; u++)
		((IndexHead *) (pTableHead + 1))[u].iIndex = (int) u;

	// memset大共享内存会比较耗时，初始化表头后再执行
	memset(((char*) pTableHead) + hdrsize, 0, ddwShmSize - hdrsize);
	return (char*) pTableHead;
}

static int VerifyLinkTableParam(LinkTableHead *pTableHead,
		uint64_t ddwIndexCount, uint64_t ddwIndexRowCount,
		uint64_t ddwElementCount, uint64_t ddwElementSize, int nodisp) {
	if ((pTableHead->ddwAllElementCount != (ddwElementCount - 1))
			|| (pTableHead->ddwIndexCount != ddwIndexCount)
			|| (pTableHead->ddwIndexRowCount != ddwIndexRowCount)
			|| (pTableHead->ddwElementSize != ddwElementSize)) {
		if (!nodisp) {
			printf("Error: shm parameters mismatched: \n");
			printf("\tdwIndexCount: given %llu, got %llu\n", ddwIndexCount,
					pTableHead->ddwIndexCount);
			printf("\tdwIndexRwCnt: given %llu, got %llu\n", ddwIndexRowCount,
					pTableHead->ddwIndexRowCount);
			printf("\tdwElementCnt: given %llu, got %llu\n", ddwElementCount,
					pTableHead->ddwAllElementCount + 1);
			printf("\tdwElementSz:  given %llu, got %llu\n", ddwElementSize,
					pTableHead->ddwElementSize);
		}
		return -1;
	}

	return 0;
}

// 验证SHM中的参数与配置的参数是否一致，不一致则返回失败
// 返回值： 0 不存在  1 存在且参数合法  -1 参数错误
static int VerifyLinkTableSize(uint64_t ddwIndexCount,
		uint64_t ddwIndexRowCount, uint64_t ddwElementCount,
		uint64_t ddwElementSize, int iKey, int nodisp) {
	LinkTableHead *pTableHead;
	int iRet;

	pTableHead = (LinkTableHead *) LT_GetShm(iKey,
			sizeof(LinkTable) + sizeof(LinkTableHead), 0666);
	if (pTableHead == 0) // shm does not exist
		return 0;

	iRet = VerifyLinkTableParam(pTableHead, ddwIndexCount, ddwIndexRowCount,
			ddwElementCount, ddwElementSize, nodisp);
	shmdt(pTableHead);

	if (iRet < 0)
		return -1;

	return 1;
}

static void RecycleAllPreFreeElements(volatile LinkTable *pTable);

int LtInit(uint32_t dwIndexCount, uint64_t ddwIndexRowCount,
		uint64_t ddwElementCount, uint64_t ddwElementSize, int iKey,
		int iCreateFlag, int nodisp) {
	char * pTable = NULL;
	volatile LinkTable *pLT;
	uint64_t ddwShmSize;
	int iVerifyResult;

	if (iMaxLtNum >= MAX_LT_NUM) {
		fprintf(stderr, "Max linktable num reached.\n");
		return -1;
	}

	pLT = &g_stTables[iMaxLtNum];

	if (dwIndexCount < 10 || ddwIndexRowCount < 100 || ddwElementCount < 1000
			|| ddwElementSize < 10 || ddwElementSize > MAX_ELEMENT_SIZE
			|| (iCreateFlag != 0 && iCreateFlag != 1)) {
		fprintf(stderr,
				"Invalid parameters: "
						"ddwIndexCount=%u, ddwIndexRowCount=%llu, ddwElementCount=%llu, ddwElementSize=%llu(max=%llu), iCreateFlag=%d\n",
				dwIndexCount, ddwIndexRowCount, ddwElementCount, ddwElementSize,
				(uint64_t)MAX_ELEMENT_SIZE, iCreateFlag);
		return -1;
	}

	ddwShmSize = CalShmSize(dwIndexCount, ddwIndexRowCount, ddwElementCount,
			ddwElementSize);
#if 0
	printf("dwIndexCount=%u, ddwIndexRowCount=%llu, ddwElementCount=%llu, ddwElementSize=%llu, shmsize=%lluU=%lldS\n",
			dwIndexCount, ddwIndexRowCount, ddwElementCount, ddwElementSize, ddwShmSize, (int64_t)ddwShmSize);
#endif

	if (ddwShmSize == 0 || ddwShmSize > MAX_SHM_SIZE) {
		fprintf(stderr, "Invalid shm size %llu (max=%lu)\n", ddwShmSize,
		MAX_SHM_SIZE);
		return -2;
	}

	iVerifyResult = VerifyLinkTableSize(dwIndexCount, ddwIndexRowCount,
			ddwElementCount, ddwElementSize, iKey, nodisp);
	if (iVerifyResult < 0) {
		if (!nodisp)
			fprintf(stderr, "VerifyLinkTableSize() returns %d.\n",
					iVerifyResult);
		return -10; // DONOT changed this return value, it is used by the process init() for retry
	}
	if (iVerifyResult == 0)
		printf("LinkTable shm does not exist yet.\n");

	if (iCreateFlag == 1 && iVerifyResult == 0)
		pTable = (char*) CreateLinkTableShm(iKey, ddwShmSize, dwIndexCount,
				ddwIndexRowCount, ddwElementCount, ddwElementSize);
	else
		pTable = GetShm(iKey, ddwShmSize, 0666);

	if (pTable == NULL) {
		if (!nodisp)
			fprintf(stderr, "Attach shm key=%u, size=%llu failed.\n", iKey,
					ddwShmSize);
		return -20; // DONOT changed
	}

	if (mlock(pTable, ddwShmSize) != 0) {
		shmdt(pTable);
		perror("mlock fail");
		return -3;
	}

	pLT->pstTableHead = (LinkTableHead*) pTable;
	pLT->pstIndexHeadList = (IndexHead *) (pTable + sizeof(LinkTableHead));
	pLT->pstIndexNodeList = (IndexNode *) (pTable + sizeof(LinkTableHead)
			+ sizeof(IndexHead) * dwIndexCount);
	pLT->pstElementList = (Element *) (pTable + sizeof(LinkTableHead)
			+ sizeof(IndexHead) * dwIndexCount
			+ sizeof(IndexNode) * dwIndexCount * ddwIndexRowCount);

	// Reverify anyway
	if (VerifyLinkTableParam(pLT->pstTableHead, dwIndexCount, ddwIndexRowCount,
			ddwElementCount, ddwElementSize, nodisp)) {
		shmdt(pTable);
		memset((void*) pLT, 0, sizeof(*pLT));
		return -4;
	}

	// 程序启动时将预回收池中的所有元素回收掉
	if (iCreateFlag == 1)
		RecycleAllPreFreeElements(pLT);

	return iMaxLtNum++;
}

// 如果pstIndexHeadList为空，则将LinkTable的pstIndexHeadList初始化为相应的UnitList
// 否则，比较pstIndexHeadList和dwIndexList，如果不相对应则返回失败
// ddwIndexList必须是已经排好序的
// 成功返回0，否则返回负数
int LtSetIndexes(int lt, uint32_t dwIndexList[], int iIndexNum) {
	int i;
	volatile LinkTable *pTable = GET_LT(lt);

	if (pTable == NULL || pTable->pstTableHead == NULL || dwIndexList == NULL
			|| iIndexNum < 0
			|| iIndexNum > (int) pTable->pstTableHead->ddwIndexCount)
		return -1;

	// 检查 ddwIndexList是否已经排好序
	for (i = 0; i < iIndexNum - 1; i++) {
		if (dwIndexList[i] >= dwIndexList[i + 1])
			return -2;
	}

	if (pTable->pstTableHead->ddwUsedIndexCount > 0) // IndexHeadList不为空
			{
		if (pTable->pstTableHead->ddwUsedIndexCount != (uint64_t) iIndexNum)
			return -3;

		for (i = 0; i < iIndexNum; i++) {
			// IndexId has changed
			if (pTable->pstIndexHeadList[i].ddwIndexId
					!= (uint64_t)(dwIndexList[i] + 1))
				return -4;

			// Index has changed
			if (pTable->pstIndexHeadList[i].iIndex != i)
				return -5;
		}
	} else // IndexHeadList为空
	{
		for (i = 0; i < iIndexNum; i++) {
			pTable->pstIndexHeadList[i].iIndex = i;
			pTable->pstIndexHeadList[i].ddwIndexId = dwIndexList[i] + 1;
		}
		for (; i < (int) pTable->pstTableHead->ddwIndexCount; i++) {
			pTable->pstIndexHeadList[i].iIndex = i;
			pTable->pstIndexHeadList[i].ddwIndexId = 0;
		}
		pTable->pstTableHead->ddwUsedIndexCount = iIndexNum;

		// From now on, pstIndexHeadList should not be changed
		pTable->pstTableHead->bLockedIndexIds = 1;
	}

	return 0;
}

static int GetIndexHead(volatile LinkTable *pTable, uint64_t idx,
		IndexHead **ppih, int createFlag) {
	int ret;
	if (ppih == NULL)
		return -1;

	if (pTable->pstTableHead->ddwUsedIndexCount
			> pTable->pstTableHead->ddwIndexCount) {
		Attr_API(162204, 1);
		return -2; // BUG: index goes insane
	}

	ret = InsertIndexHead(pTable, idx, pTable->pstIndexHeadList,
			pTable->pstTableHead->ddwUsedIndexCount,
			pTable->pstTableHead->ddwIndexCount, ppih, createFlag);

	if (ret < 0 || *ppih == NULL)
	// createFlag=1: 没有空间了，需要马上扩容
	// createFlag=0: 找不到
			{
		return -3;
	} else if (ret == 0) // 找到存在的了
			{
	} else // 找不到，增加新的
	{
		pTable->pstTableHead->ddwUsedIndexCount++;
	}
	return ret;
}

static int RecycleElement(volatile LinkTable *pTable, uint64_t ddwStartPos) {
	uint64_t ddwCurPos = 0, ddwNextPos = 0;
	int iCount = 0;

	Attr_API(241995, 1);

	if (pTable == NULL || pTable->pstTableHead == NULL) {
		return -10;
	}

	if ((ddwStartPos == 0)
			|| (ddwStartPos >= pTable->pstTableHead->ddwAllElementCount)) {
		// Position gone insane
		Attr_API(161639, 1);
		return -2;
	}

	ddwNextPos = ddwStartPos;
	while ((ddwNextPos) && (iCount <= MAX_BLOCK_COUNT)) {
		ddwCurPos = ddwNextPos;
		if (ddwCurPos >= pTable->pstTableHead->ddwAllElementCount) {
			// Position gone insane
			Attr_API(161639, 1);
			return -3;
		}

		ddwNextPos = LT_ELE_AT(pTable,ddwCurPos)->ddwNext;
		memset(LT_ELE_AT(pTable, ddwCurPos), 0,
				pTable->pstTableHead->ddwElementSize);

		// 把节点放入Free列表中
		LT_ELE_AT(pTable,ddwCurPos)->ddwNext =
				pTable->pstTableHead->ddwFirstFreePos;
		pTable->pstTableHead->ddwFirstFreePos = ddwCurPos;

		pTable->pstTableHead->ddwFreeElementCount++;
		iCount++;
	}

	if (ddwNextPos != 0) {
		// Too many blocks
		Attr_API(161640, 1);
		return -4;
	}

	return 0;
}

// 预回收池的主要算法实现：
// 用一个游标dwPreFreeIndex循环遍历预回收池指针数组addwPreFreePool，没次预回收遍历一个指针，
// 将本次希望回收的起始地址挂到addwPreFreePool[ddwPreFreeIndex]上，并回收上次挂到这的地方的元素链表
static int PreRecycleElement(volatile LinkTable *pTable, uint64_t ddwStartPos) {
	int iRet;
	uint64_t ddwOldPos, ddwIdx;

	Attr_API(241972, 1);

	ddwIdx = pTable->pstTableHead->ddwPreFreeIndex;
	if (ddwIdx >= pTable->pstTableHead->dwPreFreePoolSize) {
		Attr_API(241911, 1);
		ddwIdx = 0;
		pTable->pstTableHead->ddwPreFreeIndex = 0;
	}

	// 先放到预回收池中，再回收同个位置中的旧数据
	ddwOldPos = pTable->pstTableHead->addwPreFreePool[ddwIdx];
	pTable->pstTableHead->addwPreFreePool[ddwIdx] = ddwStartPos;
	pTable->pstTableHead->ddwPreFreeIndex = (ddwIdx + 1)
			% pTable->pstTableHead->dwPreFreePoolSize;

	if (ddwOldPos) {
		iRet = RecycleElement(pTable, ddwOldPos);
		if (iRet < 0) {
			// 如果回收失败，将导致单元不能被重新利用，这里只上报，不处理
			Attr_API(241914, 1);
		}
	}

	return 0;
}

// 回收全部预回收池中的元素
static void RecycleAllPreFreeElements(volatile LinkTable *pTable) {
	int i, nr = 0;

	for (i = 0; i < (int) pTable->pstTableHead->dwPreFreePoolSize; i++) {
		if (pTable->pstTableHead->addwPreFreePool[i]) {
			int iRet;

			if (nr++ == 0) // 第一次执行回收操作就上报
				Attr_API(241908, 1);

			iRet = RecycleElement(pTable,
					pTable->pstTableHead->addwPreFreePool[i]);
			if (iRet < 0) {
				// 如果回收失败，将导致单元不能被重新利用，这里只上报，不处理
				Attr_API(241914, 1);
			}

			pTable->pstTableHead->addwPreFreePool[i] = 0;
		}
	}

	pTable->pstTableHead->ddwPreFreeIndex = 0;
}

int LtSetPrefreePoolSize(int lt, int sz) {
	volatile LinkTable *pTable = GET_LT(lt);

	if (pTable == NULL || pTable->pstTableHead == NULL) {
		return -10;
	}
	if (sz <= 0 || (unsigned long) sz > LT_MAX_PREFREE_POOL_SIZE) {
		return -20;
	}
	RecycleAllPreFreeElements(pTable);
	pTable->pstTableHead->dwPreFreePoolSize = (unsigned int) sz;
	return 0;
}

//使用数组方式管理自由空间，分配和使用自由空间采用步进方式
static int GetEmptyElement(volatile LinkTable *pTable, int iCount,
		uint64_t *pddwStartPos) {
	uint64_t ddwStartPos, ddwCurPos;
	int i, j, iTrySecond = 0;
	float fUsedRate;

	if (pTable == NULL || pTable->pstTableHead == NULL) {
		return -1;
	}

	// 只要空闲块数小于预设比例就告警
	fUsedRate = (pTable->pstTableHead->ddwFreeElementCount * 100.0
			/ pTable->pstTableHead->ddwAllElementCount);
	if (fUsedRate < 20.0)
		Attr_API(178992, 1);
	if (fUsedRate < 15.0)
		Attr_API(232399, 1);
	if (fUsedRate < 10.0)
		Attr_API(241997, 1);
	if (fUsedRate < 5.0)
		Attr_API(242000, 1);

	restart: if (iTrySecond) // 清空预回收池后重新分配
		Attr_API(242023, 1);

	if ((iCount < 0) || (pddwStartPos == NULL) || (iCount > MAX_BLOCK_COUNT)
			|| (iCount > (int) pTable->pstTableHead->ddwFreeElementCount)) {
		// Not enough space
		if (iTrySecond == 0) {
			iTrySecond = 1;
			RecycleAllPreFreeElements(pTable);
			goto restart;
		}
		Attr_API(161647, 1);
		return -2;
	}

	// 先从Free列表搜索，搜不到再挨个搜索
	ddwStartPos = 0;
	ddwCurPos = pTable->pstTableHead->ddwFirstFreePos;
	i = 0;
	while (i < iCount && ddwCurPos > 0
			&& ddwCurPos < pTable->pstTableHead->ddwAllElementCount &&
			LT_ELE_AT(pTable,ddwCurPos)->ddwKey == 0) {
		uint64_t ddwNext;
		i++;
		ddwNext = LT_ELE_AT(pTable,ddwCurPos)->ddwNext;
		if (i == iCount) // 把最后一个的Next置零
			LT_ELE_AT(pTable,ddwCurPos)->ddwNext = 0;
		ddwCurPos = ddwNext;
	}

	if (i == iCount) // 找到了
			{
		*pddwStartPos = pTable->pstTableHead->ddwFirstFreePos;
		pTable->pstTableHead->ddwFirstFreePos = ddwCurPos;
		Attr_API(178921, 1);
		if (iTrySecond)
			Attr_API(242024, 1);
		return 0;
	}

	// FIXME：这里存在一个不是很重要的问题：就是当从Free链表中找到一些节点，但无法满足请求的需要，
	// 这时需要通过遍历搜索来分配，而遍历搜索可能会找到free链表中的节点，这将导致free链表中的节点被
	// 遍历分配了，使得Free链表断链，断链后的链表就只能由遍历分配器来分配了

	// 从pstTableHead->ddwLastEmpty开始进行挨个搜索
	ddwStartPos = 0;
	ddwCurPos = pTable->pstTableHead->ddwLastEmpty;
	i = 0;
	for (j = 0;
			(i < iCount) && (j < (int) pTable->pstTableHead->ddwAllElementCount);
			j++) {
		if (LT_ELE_AT(pTable,ddwCurPos)->ddwKey == 0) {
			LT_ELE_AT(pTable,ddwCurPos)->ddwNext = ddwStartPos;
			ddwStartPos = ddwCurPos;
			i++;
		}
		ddwCurPos++;

		// Wrap around
		if (ddwCurPos >= pTable->pstTableHead->ddwAllElementCount) {
			ddwCurPos = 1;
		}
	}

	if (i < iCount) {
		// Not enough space
		if (iTrySecond == 0) {
			iTrySecond = 1;
			RecycleAllPreFreeElements(pTable);
			goto restart;
		}
		Attr_API(161647, 1);
		return -4;
	}

	*pddwStartPos = ddwStartPos;
	pTable->pstTableHead->ddwLastEmpty = ddwCurPos;
	Attr_API(178922, 1);
	if (iTrySecond)
		Attr_API(242024, 1);
	return 0;
}

inline int TranslateIndexId(volatile LinkTable *pTable, uint64_t ddwKey,
		uint64_t *pidx) {
	if (pTable == NULL || pTable->pstTableHead == NULL || pidx == NULL) {
		return -10;
	}

	*pidx = (ddwKey / pTable->pstTableHead->ddwIndexRowCount + 1);
	return 0;
}

inline int TranslateIndexOffset(volatile LinkTable *pTable, uint64_t ddwKey,
		uint64_t *poffset) {
	if (pTable == NULL || pTable->pstTableHead == NULL || poffset == NULL) {
		return -10;
	}

	*poffset = ddwKey % pTable->pstTableHead->ddwIndexRowCount;
	return 0;
}

static int GetIndexNode(volatile LinkTable *pTable, uint64_t ddwKey,
		IndexNode **ppIndexNode, int createFlag) {
	uint64_t idx = 0, offset = 0;
	IndexHead *pih = NULL;

	if (pTable == NULL || pTable->pstTableHead == NULL) {
		return -10;
	}

	TranslateIndexId(pTable, ddwKey, &idx);
	TranslateIndexOffset(pTable, ddwKey, &offset);

	GetIndexHead(pTable, idx, &pih, createFlag);

	if (pih)
		*ppIndexNode = &(pTable->pstIndexNodeList[pih->iIndex
				* pTable->pstTableHead->ddwIndexRowCount + offset]);
	else
		*ppIndexNode = NULL;

	return 0;
}

//取得用户数据
/*
 <0:错误
 =0:找到数据
 >0:不存在
 */
int LtGetData(int lt, uint64_t ddwKey, char *sDataBuf, int *piDataLen) {
	IndexNode *pIndexNode = NULL;
	uint64_t ddwCurPos = 0, ddwNextPos = 0;
	int iRet = 0;
	int iDataLen = 0, iBufLen = 0;
	volatile LinkTable *pTable = GET_LT(lt);

	if (pTable == NULL || pTable->pstTableHead == NULL) {
		return -10;
	}

	if (sDataBuf == NULL || piDataLen == NULL) {
		return -1;
	}

	iBufLen = *piDataLen;
	*piDataLen = 0;
	if (iBufLen < (int) ELEMENT_DATABUF_LEN(pTable)) {
		return -2;
	}

	iRet = GetIndexNode(pTable, ddwKey, &pIndexNode, 0); // do not create
	if (iRet < 0) {
		return -3;
	}

	//index不存在或者未初始化返回不存在
	if ((pIndexNode == NULL) || (pIndexNode->cFlag == 0)) {
		return 1;
	}

	ddwNextPos = pIndexNode->ddwPosition;
	while (ddwNextPos) {
		ddwCurPos = ddwNextPos;
		if (ddwCurPos >= pTable->pstTableHead->ddwAllElementCount) {
			Attr_API(161639, 1);
			return -21;
		}
		if ((iDataLen + (int) ELEMENT_DATABUF_LEN(pTable)) > iBufLen) {
			Attr_API(161640, 1);
			return -4;
		}
		if (LT_ELE_AT(pTable,ddwCurPos)->ddwKey != ddwKey) {
			Attr_API(161641, 1);
			return -5;
		}
		memcpy(sDataBuf + iDataLen, LT_ELE_AT(pTable,ddwCurPos)->bufData,
				ELEMENT_DATABUF_LEN(pTable));
		iDataLen += ELEMENT_DATABUF_LEN(pTable);
		ddwNextPos = LT_ELE_AT(pTable,ddwCurPos)->ddwNext;
	}
	*piDataLen = iDataLen;

	return 0;
}

int LtPrintData(int lt, uint64_t ddwKey) {
	IndexNode *pIndexNode = NULL;
	uint64_t ddwCurPos = 0, ddwNextPos = 0;
	int iRet = 0;
	volatile LinkTable *pTable = GET_LT(lt);

	if (pTable == NULL || pTable->pstTableHead == NULL) {
		return -10;
	}

	iRet = GetIndexNode(pTable, ddwKey, &pIndexNode, 0);
	if (iRet < 0) {
		return -20;
	}

	//index不存在或者未初始化返回不存在
	if ((pIndexNode == NULL) || (pIndexNode->cFlag == 0)) {
		if (pIndexNode == NULL)
			printf("No index node found.\n");
		else
			printf("cFlag is zero, ddwPosition=%lu.\n",
					(unsigned long) pIndexNode->ddwPosition);
		return 11;
	}

	ddwNextPos = pIndexNode->ddwPosition;
	printf("ddwPosition=%llu, ddwKey=%llu\n", ddwNextPos, ddwKey);
	while (ddwNextPos) {
		ddwCurPos = ddwNextPos;
		printf("ddwCurPos=%llu, ddwKey=%llu", ddwCurPos,
		LT_ELE_AT(pTable,ddwCurPos)->ddwKey);
		printf("\n%s",
				DumpMemory(LT_ELE_AT(pTable,ddwCurPos)->bufData, 0,
						ELEMENT_DATABUF_LEN(pTable)));
		ddwNextPos = LT_ELE_AT(pTable,ddwCurPos)->ddwNext;
	}

	return 0;
}

//设置数据,如果存在删除已有数据
int LtSetData(int lt, uint64_t ddwKey, char *sDataBuf, int iDataLen) {
	IndexNode *pIndexNode = NULL;
	uint64_t ddwCurPos = 0, ddwNextPos = 0, ddwStartPos = 0, ddwOldPos = 0;
	int iCount = 0, iLeftLen = 0, iCopyLen = 0;
	int iRet = 0;
	volatile LinkTable *pTable = GET_LT(lt);

	if (pTable == NULL || pTable->pstTableHead == NULL) {
		return -10;
	}

	if (sDataBuf == NULL || iDataLen < 0) {
		return -1;
	}

	iCount = (iDataLen + ELEMENT_DATABUF_LEN(pTable) - 1)
			/ ELEMENT_DATABUF_LEN(pTable);
	if (iCount > MAX_BLOCK_COUNT) {
		return -2;
	}

	//创建或取得 IndexNode
	iRet = GetIndexNode(pTable, ddwKey, &pIndexNode, 1); // create if not found
	if (iRet < 0) {
		return -3;
	}

	if (pIndexNode == NULL) //IndexHeadList空间不够，需要扩容
			{
		// 出现这种情况，很有可能是传入了非法的 UIN 所致
		return -4;
	}

	//先构造新数据
	iRet = GetEmptyElement(pTable, iCount, &ddwStartPos);
	if (iRet < 0) {
		return -7;
	}

	ddwNextPos = ddwStartPos;
	iLeftLen = iDataLen - iCopyLen;
	while ((ddwNextPos) && (iLeftLen > 0)) {
		ddwCurPos = ddwNextPos;
		if (ddwCurPos >= pTable->pstTableHead->ddwAllElementCount) {
			Attr_API(161639, 1);
			return -8;
		}

		if (iLeftLen > (int) ELEMENT_DATABUF_LEN(pTable)) {
			memcpy(LT_ELE_AT(pTable,ddwCurPos)->bufData, sDataBuf + iCopyLen,
					ELEMENT_DATABUF_LEN(pTable));
			iCopyLen += ELEMENT_DATABUF_LEN(pTable);
		} else {
			memcpy(LT_ELE_AT(pTable,ddwCurPos)->bufData, sDataBuf + iCopyLen,
					(unsigned) iLeftLen);
			iCopyLen += iLeftLen;
		}

		ddwNextPos = LT_ELE_AT(pTable,ddwCurPos)->ddwNext;
		LT_ELE_AT(pTable,ddwCurPos)->ddwKey = ddwKey;
		pTable->pstTableHead->ddwFreeElementCount--;
		iLeftLen = iDataLen - iCopyLen;
	}

	if (iLeftLen != 0) {
		//bug
		return -9;
	}

	LT_ELE_AT(pTable,ddwCurPos)->ddwNext = 0;
	LT_ELE_AT(pTable,0)->ddwNext = ddwNextPos;
	ddwOldPos = pIndexNode->ddwPosition;
	pIndexNode->ddwPosition = ddwStartPos;
	pIndexNode->cFlag = 1;

	//删除旧数据
	if (ddwOldPos != 0) {
		iRet = PreRecycleElement(pTable, ddwOldPos);
		if (iRet < 0) {
			return -6;
		}
	}

	return 0;
}

//删除指定索引下的所有节点和节点数据(*删除整个Unit的接口*)
int LtClearIndexData(int lt, uint32_t dwIndex) {
	int i = 0, iRet = 0, iIndex;
	uint64_t ddwIndexStart = 0, sz;
	IndexHead *pstIndexHead = NULL;
	IndexNode *pIndexNode = NULL;
	volatile LinkTable *pTable = GET_LT(lt);

	if (pTable == NULL || pTable->pstTableHead == NULL
			|| dwIndex >= pTable->pstTableHead->ddwIndexCount) {
		return -10;
	}

	// If IndexIds are loaded at init time, no further modification is allowed
	if (pTable->pstTableHead->bLockedIndexIds) {
		return -9;
	}

	iRet = GetIndexHead(pTable, INDEX_TO_ID(dwIndex), &pstIndexHead, 0);
	if (iRet != 0 || pstIndexHead == NULL) // 找不到
			{
		return -1;
	}

	ddwIndexStart = pstIndexHead->iIndex
			* pTable->pstTableHead->ddwIndexRowCount;

	for (i = 0; i < (int) pTable->pstTableHead->ddwIndexRowCount; i++) {
		pIndexNode = &(pTable->pstIndexNodeList[ddwIndexStart + i]);
		if (pIndexNode->ddwPosition != 0) {
			iRet = RecycleElement(pTable, pIndexNode->ddwPosition);
			if (iRet < 0) {
				Attr_API(241914, 1);
				return -3;
			}
			pIndexNode->ddwPosition = 0;
			pIndexNode->cFlag = 0;
		}
	}

	sz = (unsigned long) (pTable->pstIndexHeadList
			+ pTable->pstTableHead->ddwUsedIndexCount)
			- (unsigned long) (pstIndexHead + 1);
	iIndex = pstIndexHead->iIndex;
	if (sz > 0) // move one element backwards
		memmove(pstIndexHead, pstIndexHead + 1, sz);
	pTable->pstIndexHeadList[pTable->pstTableHead->ddwUsedIndexCount - 1].iIndex =
			iIndex;
	pTable->pstIndexHeadList[pTable->pstTableHead->ddwUsedIndexCount - 1].ddwIndexId =
			0;
	pTable->pstTableHead->ddwUsedIndexCount--;

	return 0;
}

// 判断初始化的时候是不是调用了LtSetIndexes()来设置UnitId
// 如果是，则不允许增加或删除UnitId， 函数返回 1，否则返回0
int LtIsIndexesLocked(int lt) {
	volatile LinkTable *pTable = GET_LT(lt);
	if (pTable == NULL || pTable->pstTableHead == NULL) {
		return 0;
	}
	return pTable->pstTableHead->bLockedIndexIds ? 1 : 0;
}

//删除指定索引下的所有数据包括索引定义
int LtRemoveIndex(int lt, uint32_t dwIndex) {
	int i = 0, iRet = 0;
	uint64_t ddwIndexStart = 0, sz;
	int iIndex;
	volatile LinkTable *pTable = GET_LT(lt);
	IndexHead *pstIndexHead = NULL;
	IndexNode *pIndexNode = NULL;

	if (pTable == NULL || pTable->pstTableHead == NULL
			|| ((uint64_t) dwIndex) >= pTable->pstTableHead->ddwIndexCount) {
		return -10;
	}

	// If IndexIds are loaded at init time, no further modification is allowed
	if (pTable->pstTableHead->bLockedIndexIds) {
		return -9;
	}

	iRet = GetIndexHead(pTable, INDEX_TO_ID(dwIndex), &pstIndexHead, 0);
	if (iRet != 0 || pstIndexHead == NULL) // 找不到
			{
		return -1;
	}

	ddwIndexStart = pstIndexHead->iIndex
			* pTable->pstTableHead->ddwIndexRowCount;

	for (i = 0; i < (int) pTable->pstTableHead->ddwIndexRowCount; i++) {
		pIndexNode = &(pTable->pstIndexNodeList[ddwIndexStart + i]);
		if (pIndexNode->ddwPosition != 0)	// 还有没有释放完毕的，返回错误
				{
			return 10;
		}
	}

	sz = (unsigned long) (pTable->pstIndexHeadList
			+ pTable->pstTableHead->ddwUsedIndexCount)
			- (unsigned long) (pstIndexHead + 1);
	iIndex = pstIndexHead->iIndex;
	if (sz > 0)
		memmove(pstIndexHead, pstIndexHead + 1, sz);
	pTable->pstIndexHeadList[pTable->pstTableHead->ddwUsedIndexCount - 1].iIndex =
			iIndex;
	pTable->pstIndexHeadList[pTable->pstTableHead->ddwUsedIndexCount - 1].ddwIndexId =
			0;
	pTable->pstTableHead->ddwUsedIndexCount--;

	return 0;
}

static int ValifyIndex(volatile LinkTable *pTable) {
	int i, j, dup = 0;

	for (i = 0; i < (int) pTable->pstTableHead->ddwIndexCount; i++) {
		for (j = 0; j < i; j++) {
			if (pTable->pstIndexHeadList[j].iIndex
					== pTable->pstIndexHeadList[i].iIndex)
				break;
		}

		if (j < i) // found duplicate
				{
			printf(
					"Warning: duplicate index found between %d and %d, value=%d.\n",
					i, j, pTable->pstIndexHeadList[i].iIndex);
			dup++;
		}
	}

	return dup;
}

#define PRINT_ALL_ELEMENTS 0

int LtPrintInfo(int lt) {
	long i = 0;
	unsigned long total = 0, unit_total;
	volatile LinkTable *pTable = GET_LT(lt);

	if (pTable == NULL || pTable->pstTableHead == NULL) {
		return -10;
	}

	printf("ddwAllElementCount:  %llu\n",
			pTable->pstTableHead->ddwAllElementCount);
	printf("ddwFreeElementCount: %llu\n",
			pTable->pstTableHead->ddwFreeElementCount);
	printf("ddwElementSize:      %llu\n", pTable->pstTableHead->ddwElementSize);
	printf("ddwIndexCount:       %llu\n", pTable->pstTableHead->ddwIndexCount);
	printf("ddwIndexRowCount:    %llu\n",
			pTable->pstTableHead->ddwIndexRowCount);
	printf("ddwUsedIndexCount:   %llu\n",
			pTable->pstTableHead->ddwUsedIndexCount);
	printf("ddwFirstFreePos:     %llu\n",
			pTable->pstTableHead->ddwFirstFreePos);
	printf("ddwPreFreeIndex:     %llu\n",
			pTable->pstTableHead->ddwPreFreeIndex);

	for (total = 0, i = 0; i < pTable->pstTableHead->dwPreFreePoolSize; i++) {
		if (pTable->pstTableHead->addwPreFreePool[i])
			total++;
	}
	printf("ddwPreFreeCount:     %lu\n", total);

	total = 0;

#if 0
	for(i=0; i<(int)pTable->pstTableHead->ddwIndexCount; i++)
	{
		printf("%d: iIndex:%d, ddwIndexId:%llu\n", i,
				pTable->pstIndexHeadList[i].iIndex,
				pTable->pstIndexHeadList[i].ddwIndexId);
	}
#else
	ValifyIndex(pTable);
#endif

	for (i = 0; i < (long) pTable->pstTableHead->ddwIndexCount; i++) {
		if (pTable->pstIndexHeadList[i].ddwIndexId != 0) {
			uint64_t j;
			uint64_t start_idx = pTable->pstIndexHeadList[i].iIndex
					* pTable->pstTableHead->ddwIndexRowCount;

#if PRINT_ALL_ELEMENTS
			uint64_t start_uin=(pTable->pstIndexHeadList[i].ddwIndexId-1)*pTable->pstTableHead->ddwIndexRowCount;

			printf("ddwIndexId:%llu (UnitID:%llu) iIndex: %d\n",
					pTable->pstIndexHeadList[i].ddwIndexId, pTable->pstIndexHeadList[i].ddwIndexId-1,
					pTable->pstIndexHeadList[i].iIndex);
#endif
			unit_total = 0;
			for (j = 0; j < pTable->pstTableHead->ddwIndexRowCount; j++) {
				if (pTable->pstIndexNodeList[start_idx + j].cFlag != 0) {
					uint64_t pos =
							pTable->pstIndexNodeList[start_idx + j].ddwPosition;
					if (pos > pTable->pstTableHead->ddwAllElementCount)
						printf(
								"\tpstIndexNodeList[%llu].ddwPosition is invalid!\n",
								start_idx + j);
#if PRINT_ALL_ELEMENTS
					else
					printf("\tpstElementList[%llu]: key=%llu, next=%llu, uin=%llu\n",
							pos, LT_ELE_AT(pTable,pos)->ddwKey,
							LT_ELE_AT(pTable,pos)->ddwNext, start_uin+(uint64_t)j);
#endif
					total++;
					unit_total++;
				}
			}
#if PRINT_ALL_ELEMENTS
			printf("UINs in unit: %lu\n", unit_total);
#else
			printf("ddwIndexId:%llu (UnitID:%llu), iIndex: %d, uins:%lu\n",
					pTable->pstIndexHeadList[i].ddwIndexId,
					pTable->pstIndexHeadList[i].ddwIndexId - 1,
					pTable->pstIndexHeadList[i].iIndex, unit_total);
#endif
		}
	}

	printf("\nTotal UINs: %lu\n", total);
	return 0;
}

int LtPrintElements(int lt) {
	long i = 0;
	unsigned long total = 0;
	volatile LinkTable *pTable = GET_LT(lt);

	if (pTable == NULL || pTable->pstTableHead == NULL) {
		return -10;
	}

	ValifyIndex(pTable);

	printf("ddwAllElementCount:  %llu\n",
			pTable->pstTableHead->ddwAllElementCount);
	printf("ddwFreeElementCount: %llu\n",
			pTable->pstTableHead->ddwFreeElementCount);
	printf("ddwElementSize:      %llu\n", pTable->pstTableHead->ddwElementSize);
	printf("ddwIndexCount:       %llu\n", pTable->pstTableHead->ddwIndexCount);
	printf("ddwIndexRowCount:    %llu\n",
			pTable->pstTableHead->ddwIndexRowCount);
	printf("ddwUsedIndexCount:   %llu\n",
			pTable->pstTableHead->ddwUsedIndexCount);
	printf("ddwFirstFreePos:     %llu\n",
			pTable->pstTableHead->ddwFirstFreePos);
	printf("ddwPreFreeIndex:     %llu\n",
			pTable->pstTableHead->ddwPreFreeIndex);

	for (total = 0, i = 0; i < pTable->pstTableHead->dwPreFreePoolSize; i++) {
		if (pTable->pstTableHead->addwPreFreePool[i])
			total++;
	}
	printf("ddwPreFreeCount:     %lu\n", total);

	total = 0;

	printf("\nElements:\n");
	for (i = 0; i <= (long) pTable->pstTableHead->ddwAllElementCount; i++) {
		if (LT_ELE_AT(pTable,i)->ddwNext || LT_ELE_AT(pTable,i)->ddwKey) {
			printf("\tpstElementList[%ld]: next=%llu, key=%llu\n", i,
			LT_ELE_AT(pTable,i)->ddwNext, LT_ELE_AT(pTable,i)->ddwKey);

			if (LT_ELE_AT(pTable,i)->ddwKey)
				total++;
		}
	}

	printf("\nTotal elements: %lu\n", total);
	return 0;
}

//删除指定索引下的所有数据包括索引定义
int LtClearKeyData(int lt, uint64_t ddwKey) {
	volatile LinkTable *pTable = GET_LT(lt);
	IndexNode *pIndexNode = NULL;
	int iRet = 0;

	if (pTable == NULL || pTable->pstTableHead == NULL) {
		return -11;
	}

	iRet = GetIndexNode(pTable, ddwKey, &pIndexNode, 0);
	if (iRet < 0) {
		return -20;
	}

	//index不存在或者未初始化返回不存在
	if (pIndexNode == NULL || pIndexNode->cFlag == 0) {
		return 10;
	}

	if (pIndexNode->ddwPosition != 0) {
		uint64_t ddwOldPos = pIndexNode->ddwPosition;

		// 注意这个顺序，程序有可能随时被killed，优先保证指针有效性
		pIndexNode->cFlag = 0;
		pIndexNode->ddwPosition = 0;

		iRet = PreRecycleElement(pTable, ddwOldPos);
		if (iRet < 0) {
			return -30;
		}
	}

	return 0;
}

int LtGetHeaderData(int lt, void *pBuff, int iSize) {
	volatile LinkTable *pTable = GET_LT(lt);

	if (iSize <= 0 || pBuff == NULL || pTable == NULL
			|| pTable->pstTableHead == NULL)
		return -1;

	if (iSize > (int) pTable->pstTableHead->dwUserDataLen)
		iSize = (int) pTable->pstTableHead->dwUserDataLen;

	memcpy(pBuff, pTable->pstTableHead->sUserData, iSize);
	return iSize;
}

int LtSetHeaderData(int lt, const void *pData, int iLen) {
	volatile LinkTable *pTable = GET_LT(lt);

	if (iLen < 0 || (iLen && pData == NULL) || pTable == NULL
			|| pTable->pstTableHead == NULL)
		return -1;

	if (iLen > (int) sizeof(pTable->pstTableHead->sUserData))
		iLen = (int) sizeof(pTable->pstTableHead->sUserData);

	if (iLen > 0)
		memcpy(pTable->pstTableHead->sUserData, pData, iLen);

	pTable->pstTableHead->dwUserDataLen = iLen;
	return iLen;

}

int LtGetIndexData(int lt, uint32_t dwIndex, void *pBuff, int iSize) {
	int iRet;
	volatile LinkTable *pTable = GET_LT(lt);
	IndexHead *pstIndexHead;

	if (iSize <= 0 || pBuff == NULL || pTable == NULL
			|| pTable->pstTableHead == NULL)
		return -1;

	iRet = GetIndexHead(pTable, INDEX_TO_ID(dwIndex), &pstIndexHead, 0);
	if (iRet != 0 || pstIndexHead == NULL) // 找不到
		return -2;

	if (iSize > (int) pstIndexHead->dwIndexDataLen)
		iSize = pstIndexHead->dwIndexDataLen;

	memcpy(pBuff, pstIndexHead->abIndexData, iSize);
	return iSize;
}

int LtSetIndexData(int lt, uint32_t dwIndex, const void *pData, int iLen) {
	int iRet;
	volatile LinkTable *pTable = GET_LT(lt);
	IndexHead *pstIndexHead;

	if (iLen < 0 || (iLen > 0 && pData == NULL) || pTable == NULL
			|| pTable->pstTableHead == NULL)
		return -1;

	iRet = GetIndexHead(pTable, INDEX_TO_ID(dwIndex), &pstIndexHead, 0);
	if (iRet != 0 || pstIndexHead == NULL) // 找不到
		return -2;

	if (iLen > (int) sizeof(pstIndexHead->abIndexData))
		iLen = (int) sizeof(pstIndexHead->abIndexData);

	if (iLen > 0)
		memcpy(pstIndexHead->abIndexData, pData, iLen);

	pstIndexHead->dwIndexDataLen = iLen;
	return iLen;
}

int LtClose(int lt) {
	volatile LinkTable *pTable = GET_LT(lt);
	if (pTable == NULL)
		return -1;

	if (pTable->pstTableHead != NULL)
		shmdt(pTable->pstTableHead);

	memset((void*) pTable, 0, sizeof(g_stTables[0]));

	if (lt == iMaxLtNum - 1)
		iMaxLtNum--;

	return 0;
}

volatile LinkTable *GetLinkTable(int lt) {
	volatile LinkTable *pTable = GET_LT(lt);
	if (pTable == NULL || pTable->pstTableHead == NULL)
		return NULL;
	return pTable;
}

