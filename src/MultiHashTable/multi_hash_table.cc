/******************************************************************************

 FILENAME:	multi_hash_table.cc

 DESCRIPTION:	多阶Hash表的一种实现：索引和数据分离，数据层使用LinkTable存储
 ******************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <fcntl.h>
#include <iostream>
#include "multi_hash_table.h"
#include "shm_adapter.h"

// if you have Attr_API, please define HAS_ATTR_API in your makefile
// if you don't have one, we won't blame it, you can download it at http://www.server.com/oss
#ifdef HAS_ATTR_API
#include "Attr_API.h"
#else
#define Attr_API(a, b)
#endif

static const char *errnotostr(int errnum) {
	static __thread char s[64];
	return strerror_r(errno, s, sizeof(s));
}

extern "C" unsigned int DefaultHashFunc(const void *key, int len) {
	int32_t i, h = 0;
#define magic	131
	const unsigned char *p = (const unsigned char *) key;

	if (!key)
		return 0;

	for (i = 0; i < len; i++, p++)
		h = h * magic + *p;

	return h;
}

/*
 * 返回n以下（不包括n）的最大质数
 */
static ull64_t MaxPrime(ull64_t n) {
	while (n--) {
		int k;
		int sqr = ceil(sqrt(n));
		for (k = 2; k <= sqr; ++k)
			if (n % k == 0)
				break;
		if (k == sqr + 1)
			return n;
	}
	return 0;
}

__thread char MultiHashTable::errmsg[1024];

MultiHashTable::~MultiHashTable() {
	if (pShmAdpt)
		delete pShmAdpt;
}

// calculate each row's node num, return the real total node num
ull64_t MultiHashTable::CalcNodeNumForEachRow(ull64_t ddwRowNum,
		ull64_t ddwMaxNodeNum, ull64_t auRowsNodeNum[],
		ull64_t auRowsStartNode[]) {
	ull64_t uNodeNum = 0, uLeftNum = ddwMaxNodeNum, i, j;
#define PD(i) printf("row %llu, num %llu, start %llu\n", (ull64_t)i, auRowsNodeNum[i], auRowsStartNode[i])
	auRowsNodeNum[0] = MaxPrime(
			ddwRowNum == 1 ? uLeftNum : (uLeftNum * 70 / 100));
	auRowsStartNode[0] = uNodeNum;
	uNodeNum += auRowsNodeNum[0];
	uLeftNum -= auRowsNodeNum[0];
	PD(0);
	if (ddwRowNum == 1)
		return uNodeNum;

	auRowsNodeNum[1] = MaxPrime(
			ddwRowNum == 2 ? uLeftNum : (uLeftNum * 10 / 100));
	auRowsStartNode[1] = uNodeNum;
	uNodeNum += auRowsNodeNum[1];
	uLeftNum -= auRowsNodeNum[1];
	PD(1);
	if (ddwRowNum == 2)
		return uNodeNum;

	auRowsNodeNum[2] = MaxPrime(
			ddwRowNum == 3 ? uLeftNum : (uLeftNum * 5 / 100));
	auRowsStartNode[2] = uNodeNum;
	uNodeNum += auRowsNodeNum[2];
	uLeftNum -= auRowsNodeNum[2];
	PD(2);
	if (ddwRowNum == 3)
		return uNodeNum;

	j = uLeftNum / (ddwRowNum - 3);
	for (i = 3; i < ddwRowNum; ++i) {
		auRowsNodeNum[i] = j = MaxPrime(j);
		auRowsStartNode[i] = uNodeNum;
		uNodeNum += j;
		PD(i);
	}
	printf("total %llu\n", uNodeNum);
#undef PD
	return uNodeNum;
}

int MultiHashTable::InitFromBuffer(void *pBuffer, ull64_t ddwBufferSize,
		uint8_t cMaxKeyLen, uint16_t wRowNum, uint32_t dwIndexRatio,
		ull64_t ddwBlockSize, CalcHashKeyFunc HashFunc, bool bUseAccessSeq) {
	if (pBuffer == NULL || ddwBufferSize < 1024 * 1024 * 2 || cMaxKeyLen < 1
			|| cMaxKeyLen > HASH_MAX_KEY_LENGTH || wRowNum < 1
			|| wRowNum > HASH_MAX_ROW || dwIndexRatio < 1 || dwIndexRatio > 1000
			|| ddwBlockSize < 32 || ddwBlockSize > 8192) {
		strncpy(errmsg, "Bad argument", sizeof(errmsg));
		return -1;
	}
	ull64_t ddwNodeSize = EvaluateHashNodeSize(cMaxKeyLen);
	ull64_t ddwMemRatio = NodeNumRatio2MemSizeRatio(ddwNodeSize, ddwBlockSize,
			dwIndexRatio);
	ull64_t ddwHashNodeNum = EvaluateHashNodeNum(ddwBufferSize, ddwNodeSize,
			ddwMemRatio);
	header = (MhtHeader*) pBuffer;
	memset(header, 0, sizeof(*header));
	header->ddwTotalShmSize = ddwBufferSize;
	header->ddwHashNodeSize = ddwNodeSize;
	header->ddwRowNum = wRowNum;
	header->ddwHashNodeNum = CalcNodeNumForEachRow(wRowNum, ddwHashNodeNum,
			header->auRowsNodeNum, header->auRowsStartNode);
	ht = header + 1;

	ull64_t ddwDataSize = ddwBufferSize - sizeof(*header)
			- (header->ddwHashNodeNum * ddwNodeSize);
	ull64_t ddwBlockCount = lt.EvalBlockCount(ddwDataSize, ddwBlockSize);
	header->ddwDataBlockSize = ddwBlockSize;
	header->ddwDataBlockNum = ddwBlockCount;
	header->cUseAccessSeq = bUseAccessSeq;

	int ret = lt.Init(((char *) ht) + (header->ddwHashNodeNum * ddwNodeSize),
			ddwDataSize, ddwBlockCount, ddwBlockSize);
	if (ret) {
		snprintf(errmsg, sizeof(errmsg), "LinkTable Init failed (ret=%d): %s",
				ret, lt.GetErrorMsg());
		return -2;
	}
	if (HashFunc)
		hashfunc = HashFunc;
	else
		hashfunc = DefaultHashFunc;
	return 0;
}

int MultiHashTable::InitFromShm(ull64_t ddwShmKey, CalcHashKeyFunc HashFunc) {
	if (pShmAdpt)
		delete pShmAdpt;

	pShmAdpt = new shm_adapter;
	if (pShmAdpt == NULL) {
		snprintf(errmsg, sizeof(errmsg),
				"Failed to create shm adaptor: Out of memory");
		return -1;
	}
	if (pShmAdpt->open(ddwShmKey) == false) {
		snprintf(errmsg, sizeof(errmsg), "Failed to open shm: %s",
				pShmAdpt->get_err_msg().c_str());
		delete pShmAdpt;
		pShmAdpt = NULL;
		return -100;
	}

	header = (MhtHeader*) pShmAdpt->get_shm();
	if (header == NULL) {
		snprintf(errmsg, sizeof(errmsg),
				"Bug: shm open successfully but shm pointer is NULL");
		delete pShmAdpt;
		pShmAdpt = NULL;
		return -3;
	}

	if (header->ddwTotalShmSize != pShmAdpt->get_size()) {
		snprintf(errmsg, sizeof(errmsg),
				"Error: shm size %llu differs from size in header %llu",
				(ull64_t) pShmAdpt->get_size(), header->ddwTotalShmSize);
		delete pShmAdpt;
		pShmAdpt = NULL;
		return -4;
	}

	ht = header + 1;

	ull64_t ddwDataSize = lt.EvalBufferSize(header->ddwDataBlockNum,
			header->ddwDataBlockSize);

	int ret = lt.InitExisting(
			((char *) ht) + (header->ddwHashNodeNum * header->ddwHashNodeSize),
			ddwDataSize, header->ddwDataBlockNum, header->ddwDataBlockSize);
	if (ret) {
		snprintf(errmsg, sizeof(errmsg), "LinkTable Init failed (ret=%d): %s",
				ret, lt.GetErrorMsg());
		delete pShmAdpt;
		pShmAdpt = NULL;
		return -5;
	}

	if (HashFunc)
		hashfunc = HashFunc;
	else
		hashfunc = DefaultHashFunc;
	return 0;
}

int MultiHashTable::CreateFromShm(const MhtInitParam &user_param) {
	int iret;
	MhtInitParam param = user_param;

	if (param.ddwShmKey == 0
			|| param.ddwBufferSize
					< sizeof(MhtHeader) + sizeof(LinkTableHead) + 1024) {
		snprintf(errmsg, sizeof(errmsg),
				"Bad argument, ShmKey and BufferSize must be given");
		return -1;
	}
	if (param.cMaxKeyLen == 0)
		param.cMaxKeyLen = DEFAULT_MAX_KEY_LENGTH;
	if (param.wRowNum == 0)
		param.wRowNum = DEFAULT_ROW_NUM;
	if (param.dwIndexRatio == 0)
		param.dwIndexRatio = DEFAULT_INDEX_RATIO;
	if (param.ddwBlockSize == 0)
		param.ddwBlockSize = DEFAULT_BLOCK_SIZE;

	if ((iret = InitFromShm(param.ddwShmKey, param.HashFunc)) == 0) // shm exists, check against given parameters
			{
		if (param.ddwBufferSize != header->ddwTotalShmSize) {
			snprintf(errmsg, sizeof(errmsg),
					"Shm size mismatched: shm %llu, given %llu",
					header->ddwTotalShmSize, param.ddwBufferSize);
			return -2;
		}
		if ((int) param.cMaxKeyLen != GetMaxKeyLen()) {
			snprintf(errmsg, sizeof(errmsg),
					"Max key length mismatched: shm %d, given %d",
					GetMaxKeyLen(), (int) param.cMaxKeyLen);
			return -3;
		}
		if (param.wRowNum != header->ddwRowNum) {
			snprintf(errmsg, sizeof(errmsg),
					"Hash row num mismatched: shm %llu, given %hu",
					header->ddwRowNum, param.wRowNum);
			return -4;
		}
		if (param.ddwBlockSize != header->ddwDataBlockSize) {
			snprintf(errmsg, sizeof(errmsg),
					"Data block size mismatched: shm %llu, given %llu",
					header->ddwDataBlockSize, param.ddwBlockSize);
			return -5;
		}
		return 0;
	}

	if (iret && iret != -100) // open existing shm failed
			{
		return -6;
	}

	pShmAdpt = new shm_adapter;
	if (pShmAdpt == NULL) {
		snprintf(errmsg, sizeof(errmsg),
				"Failed to create shm adaptor: Out of memory");
		return -1;
	}

	bool ret = pShmAdpt->create(param.ddwShmKey, param.ddwBufferSize,
			param.bUseHugePage);
	if (ret == false) {
		snprintf(errmsg, sizeof(errmsg), "Failed to create shm: %s",
				pShmAdpt->get_err_msg().c_str());
		delete pShmAdpt;
		pShmAdpt = NULL;
		return -2;
	}

	void *pBuffer = pShmAdpt->get_shm();
	if (pBuffer == NULL) {
		snprintf(errmsg, sizeof(errmsg),
				"Bug: shm created successfully but shm pointer is NULL");
		delete pShmAdpt;
		pShmAdpt = NULL;
		return -3;
	}

	return InitFromBuffer(pBuffer, param.ddwBufferSize, param.cMaxKeyLen,
			param.wRowNum, param.dwIndexRatio, param.ddwBlockSize,
			param.HashFunc, param.bUseAccessSeq);
}

static bool is_hnode_empty(MhtNode *n) {
	return (n->ddwLtPos == 0);
}

static inline int hnode_cmp(const void *pKey, uint8_t cKeyLen,
		const MhtNode *pNode) {
	int k;
	if ((k = (cKeyLen - pNode->cKeyLen)))
		return k;
	return memcmp(pKey, pNode->acKey, cKeyLen);
}

#define HT_GET_AT(ht, idx, ns) (MhtNode *)(((char*)(ht))+(ns)*(idx))

// Search and return a node for pKey, if no such key found, returns NULL, optionally
// 	ppEmptyNode returns the first empty node encountered
//  ppOldestNode returns the oldest node in the search path
MhtNode *MultiHashTable::Search(const void *pKey, int iKeyLen,
		uint32_t dwHashKey, MhtNode **ppEmptyNode, MhtNode **ppOldestNode,
		int *piEmptyRow) {
	ull64_t i;
	void *pRow;
	MhtNode *pNode;
	ull64_t ddwMaxRow = header->ddwMaxUsedRow;
	ull64_t ddwNodeSize = header->ddwHashNodeSize;

	if (ppEmptyNode)
		*ppEmptyNode = NULL;
	if (ppOldestNode)
		*ppOldestNode = NULL;

	if (ddwMaxRow >= header->ddwRowNum) {
		// BUG
		Attr_API(358690, 1);
		header->ddwMaxUsedRow = header->ddwRowNum - 1;
		ddwMaxRow = header->ddwMaxUsedRow;
	}

	for (i = 0, pRow = ht; i <= ddwMaxRow;
			pRow = HT_GET_AT(pRow, header->auRowsNodeNum[i], ddwNodeSize), i++) {
		pNode = HT_GET_AT(pRow, dwHashKey % header->auRowsNodeNum[i],
				ddwNodeSize);
		if (is_hnode_empty(pNode)) {
			if (ppEmptyNode && *ppEmptyNode == NULL) {
				if (piEmptyRow)
					*piEmptyRow = i;
				*ppEmptyNode = pNode;
			}
			continue;
		}
		if (dwHashKey == pNode->dwHashKey
				&& hnode_cmp(pKey, iKeyLen, pNode) == 0) {
			return pNode;
		}
		// keep oldest node
		if (ppOldestNode
				&& (*ppOldestNode == NULL
						|| (*ppOldestNode)->dwAccessTime > pNode->dwAccessTime)) {
			*ppOldestNode = pNode;
		}
	}
	// if no empty is found, return an empty after max row
	if (ppEmptyNode && *ppEmptyNode == NULL && i < header->ddwRowNum) {
		if (piEmptyRow)
			*piEmptyRow = i;
		*ppEmptyNode = HT_GET_AT(pRow, dwHashKey % header->auRowsNodeNum[i],
				ddwNodeSize);
	}
	return NULL;
}

int MultiHashTable::GetData(const void *pKey, int iKeyLen, char *sDataBuf,
		int &iDataLen, bool bUpdateAccessTime, time_t *pdwLastAccessTime) {
	MhtNode *pDataNode;

	if (pKey == NULL || iKeyLen <= 0 || sDataBuf == NULL || iDataLen < 1) {
		snprintf(errmsg, sizeof(errmsg), "Bad argument");
		return -1;
	}
	if (header == NULL || ht == NULL) {
		snprintf(errmsg, sizeof(errmsg), "Hash table is not initialized");
		return -2;
	}

	uint32_t dwHashKey = hashfunc(pKey, iKeyLen);

	pDataNode = Search(pKey, iKeyLen, dwHashKey);
	if (pDataNode == NULL) {
		iDataLen = 0;
		return 0;
	}

	// read/update access time before reading
	// in order to make sure that if read is successful, access time is also valid and updated
	if (pdwLastAccessTime)
		*pdwLastAccessTime = pDataNode->dwAccessTime;
	if (bUpdateAccessTime) {
		if (header->cUseAccessSeq == 0)
			pDataNode->dwAccessTime = time(NULL);
		else
			__sync_fetch_and_add(&pDataNode->dwAccessSeq, 1);
	}

	ull64_t ddwLtPos = pDataNode->ddwLtPos; // read the position before conflict detection

	// conflict detection: if key is changed, it must have been replaced by the write process
	// if so, returns an error, the caller(or the client) is expected to try again
	if (hnode_cmp(pKey, iKeyLen, pDataNode)) {
		snprintf(errmsg, sizeof(errmsg),
				"Key changed while trying to read, maybe r/w conflicted");
		return -3;
	}

	int ret = lt.GetData(ddwLtPos, sDataBuf, iDataLen);
	if (ret < 0) {
		snprintf(errmsg, sizeof(errmsg), "LinkTable GetData failed: %s",
				lt.GetErrorMsg());
		return -4;
	}

	return 0;
}

/*
 * erase bad node in mht, used to fix read/write error
 * for specific key caused by bug
 * FIXME: use with EXTREME CARE!
 */
int MultiHashTable::EraseBadNode(const void *pKey, int iKeyLen) {
	MhtNode *pDataNode;

	if (pKey == NULL || iKeyLen <= 0) {
		snprintf(errmsg, sizeof(errmsg), "Bad argument");
		return -1;
	}
	if (header == NULL || ht == NULL) {
		snprintf(errmsg, sizeof(errmsg), "Hash table is not initialized");
		return -2;
	}

	uint32_t dwHashKey = hashfunc(pKey, iKeyLen);

	pDataNode = Search(pKey, iKeyLen, dwHashKey);
	if (pDataNode == NULL) {
		return 0;
	}

	memset(pDataNode, 0, sizeof(*pDataNode));
	return 1;
}

int MultiHashTable::SearchAndEraseOldestOne(MhtNode *pDataNode) {
	ull64_t ddwNodeSize = header->ddwHashNodeSize;
	MhtNode *oldest = NULL;
	uint32_t oldesttime = (uint32_t) - 1;

	ull64_t it = header->ddwEraseIterator;
	if (it >= header->ddwHashNodeNum)
		it = 0;

	for (int cnt = 0; cnt < 100;) {
		MhtNode *n = HT_GET_AT(ht, it, ddwNodeSize);
		if (n != pDataNode && !is_hnode_empty(n)) {
			if (n->dwAccessTime < oldesttime) {
				oldesttime = n->dwAccessTime;
				oldest = n;
			}
			cnt++;
		}

		it++;
		if (it >= header->ddwHashNodeNum)
			it = 0;

		if (it == header->ddwEraseIterator) // we have searched through the whole ht
				{
			break;
		}
	}
	header->ddwEraseIterator = it;

	if (oldest) // found an oldest
	{
		if (lt.EraseData(oldest->ddwLtPos) < 0) {
			snprintf(errmsg, sizeof(errmsg), "Error: Clear old data failed: %s",
					lt.GetErrorMsg());
			return -6;
		}
		memset(oldest, 0, sizeof(*oldest));
		return 0;
	}
	snprintf(errmsg, sizeof(errmsg),
			"Error: no more index node to be replaced");
	return -7;
}

int MultiHashTable::SetData(const void *pKey, int iKeyLen, const char *sDataBuf,
		int iDataLen, bool bRemoveOldIfNoSpace) {
	if (pKey == NULL || iKeyLen <= 0 || sDataBuf == NULL || iDataLen < 0) {
		snprintf(errmsg, sizeof(errmsg), "Bad argument");
		return -1;
	}
	if (header == NULL || ht == NULL) {
		snprintf(errmsg, sizeof(errmsg), "Hash table is not initialized");
		return -2;
	}

	int ht_usage = header->ddwMaxUsedRow * 100 / header->ddwRowNum;
	if (ht_usage > 50)
		Attr_API(358676, 1);
	if (ht_usage > 80)
		Attr_API(358677, 1);
	if (ht_usage > 90)
		Attr_API(358678, 1);
	if (ht_usage > 95)
		Attr_API(358679, 1);

	int ret, emptyrow = 0;
	MhtNode *pDataNode, *pEmptyNode = NULL, *pOldestNode = NULL;
	uint32_t dwHashKey = hashfunc(pKey, iKeyLen);
	ull64_t ddwLtPos;

	ret = pthread_mutex_lock(&mutex);
	if (ret < 0) {
		Attr_API(391679, 1);
		snprintf(errmsg, sizeof(errmsg), "Mutex lock failed: %s",
				errnotostr(errno));
		return -3;
	}

	pDataNode = Search(pKey, iKeyLen, dwHashKey, &pEmptyNode, &pOldestNode,
			&emptyrow);
	// erase condition 1: if data used space exceeds 95%, erase the oldest node in the search path
	if (bRemoveOldIfNoSpace && pOldestNode && lt.GetUsage() > 95) {
		Attr_API(358680, 1);
		if ((ret = lt.EraseData(pOldestNode->ddwLtPos)) < 0) {
			snprintf(errmsg, sizeof(errmsg), "Error: Clear old data failed: %s",
					lt.GetErrorMsg());
			ret = -4;
			goto unlock_out;
		}
		memset(pOldestNode, 0, sizeof(*pOldestNode));
		if (pEmptyNode == NULL) {
			pEmptyNode = pOldestNode;
			emptyrow = 0;
		}
		pOldestNode = NULL; // mark being used
	}
	if (pDataNode == NULL) {
		if (pEmptyNode == NULL) {
			if (!bRemoveOldIfNoSpace) {
				Attr_API(358681, 1);
				snprintf(errmsg, sizeof(errmsg),
						"Error: Hash table out of space");
				ret = -5;
				goto unlock_out;
			}

			// erase condition 2: if index node is out of space, erase oldest in the search path
			if (pOldestNode == NULL) {
				// It is impossible to come here, if it does, there must be a bug in the code
				Attr_API(358684, 1);
				snprintf(errmsg, sizeof(errmsg),
						"Bug: could not find an oldest hash node for erasing.");
				ret = -6;
				goto unlock_out;
			} else // take position of pOldestNode
			{
				Attr_API(358689, 1); // Removing oldest node
				if ((ret = lt.EraseData(pOldestNode->ddwLtPos)) < 0) {
					snprintf(errmsg, sizeof(errmsg),
							"Error: Clear old data failed: %s",
							lt.GetErrorMsg());
					ret = -7;
					goto unlock_out;
				}
				memset(pOldestNode, 0, sizeof(*pOldestNode));
				pEmptyNode = pOldestNode;
				emptyrow = 0;
				pOldestNode = NULL; // mark being used
			}
		}
		pDataNode = pEmptyNode;
	} else {
		pEmptyNode = NULL;
	}

	do {
		ddwLtPos = pDataNode->ddwLtPos;
		ret = lt.SetData(ddwLtPos, sDataBuf, iDataLen);
		if (ret < 0) {
			if (ret != -100) {
				snprintf(errmsg, sizeof(errmsg),
						"Link table set data failed: %s", lt.GetErrorMsg());
				ret = -8;
				goto unlock_out;
			}
			// Link table has not enough space
			if (!bRemoveOldIfNoSpace) {
				Attr_API(358691, 1);
				snprintf(errmsg, sizeof(errmsg),
						"Error: Link table out of space");
				ret = -9;
				goto unlock_out;
			}Attr_API(358692, 1); // removing old data
			if (pOldestNode) {
				// ERROR: when free space < 5%, the oldest node should have been erased previously!
				Attr_API(358706, 1);
				if ((ret = lt.EraseData(pOldestNode->ddwLtPos)) < 0) {
					snprintf(errmsg, sizeof(errmsg),
							"Error: Clear old data failed: %s",
							lt.GetErrorMsg());
					ret = -10;
					goto unlock_out;
				}
				memset(pOldestNode, 0, sizeof(*pOldestNode));
				pOldestNode = NULL;
				continue;
			}
			// erase condition 3: when not enough data space, remove the oldest in a thousand nodes
			Attr_API(358707, 1); // removing one oldest in a thousand nodes
			if (SearchAndEraseOldestOne(pDataNode)) {
				ret = -11;
				goto unlock_out;
			}
			continue;
		}
	} while (ret < 0);

	pDataNode->ddwLtPos = ddwLtPos;

	// if pEmptyNode is not NULL, it must be same as pDataNode
	if (pEmptyNode) // we are storing new node to emptynode
	{
		pEmptyNode->dwHashKey = dwHashKey;
		if (header->cUseAccessSeq == 0)
			pDataNode->dwAccessTime = time(NULL);
		else
			pDataNode->dwAccessSeq = 0;
		pEmptyNode->cKeyLen = iKeyLen;
		memcpy(pEmptyNode->acKey, pKey, iKeyLen);
		if ((ull64_t) emptyrow > header->ddwMaxUsedRow) // inserting data at higher row
				{
			Attr_API(358708, 1);
			header->ddwMaxUsedRow = (ull64_t) emptyrow;
		}
	}
	ret = 0;
	unlock_out: if (pthread_mutex_unlock(&mutex) < 0) {
		Attr_API(391680, 1);
		snprintf(errmsg, sizeof(errmsg), "Mutex unlock failed: %s",
				errnotostr(errno));
	}
	return ret;
}

//
// Erase a given key, returns:
// 1  -- erased successfully
// 0  -- no key found
// <0 -- error
//
int MultiHashTable::EraseData(const void *pKey, int iKeyLen) {
	if (pKey == NULL || iKeyLen <= 0) {
		snprintf(errmsg, sizeof(errmsg), "Bad argument");
		return -1;
	}
	if (header == NULL || ht == NULL) {
		snprintf(errmsg, sizeof(errmsg), "Hash table is not initialized");
		return -2;
	}

	int ret;
	MhtNode *pDataNode;
	uint32_t dwHashKey = hashfunc(pKey, iKeyLen);

	ret = pthread_mutex_lock(&mutex);
	if (ret < 0) {
		Attr_API(391679, 1);
		snprintf(errmsg, sizeof(errmsg), "Mutex lock failed: %s",
				errnotostr(errno));
		return -3;
	}

	pDataNode = Search(pKey, iKeyLen, dwHashKey);
	if (pDataNode) {
		ret = lt.EraseData(pDataNode->ddwLtPos);
		if (ret) {
			snprintf(errmsg, sizeof(errmsg), "Link table erase failed: %s",
					lt.GetErrorMsg());
			ret = -4;
			goto unlock_ret;
		}
		memset(pDataNode, 0, sizeof(*pDataNode));
		ret = 1;
		goto unlock_ret;
	}
	ret = 0;
	unlock_ret: if (pthread_mutex_unlock(&mutex) < 0) {
		Attr_API(391680, 1);
		snprintf(errmsg, sizeof(errmsg), "Mutex unlock failed: %s",
				errnotostr(errno));
	}
	return ret;
}

MhtIterator MultiHashTable::Next(MhtIterator it) {
	if (header == NULL || ht == NULL) {
		snprintf(errmsg, sizeof(errmsg), "Hash table is not initialized");
		return -1;
	}

	ull64_t ddwMaxUsedNode =
			header->ddwMaxUsedRow < header->ddwRowNum ?
					header->auRowsStartNode[header->ddwMaxUsedRow]
							+ header->auRowsNodeNum[header->ddwMaxUsedRow] :
					header->ddwHashNodeNum;
	if (ddwMaxUsedNode > header->ddwHashNodeNum)
		ddwMaxUsedNode = header->ddwHashNodeNum;

	ull64_t ddwNodeSize = header->ddwHashNodeSize;
	for (it++; it < ddwMaxUsedNode; ++it) {
		if (!is_hnode_empty(HT_GET_AT(ht, it, ddwNodeSize)))
			return it;
	}
	return End();
}

int MultiHashTable::Get(MhtIterator it, MhtData &data) {
	if (header == NULL || ht == NULL) {
		snprintf(errmsg, sizeof(errmsg), "Hash table is not initialized");
		return -1;
	}

	MhtNode *pDataNode = HT_GET_AT(ht, it, header->ddwHashNodeSize);
	data.klen = pDataNode->cKeyLen;
	if (data.klen > HASH_MAX_KEY_LENGTH)
		data.klen = HASH_MAX_KEY_LENGTH;
	memcpy(data.key, pDataNode->acKey, data.klen);

	data.access_time = pDataNode->dwAccessTime;

	int ret = lt.GetData(pDataNode->ddwLtPos, data.data, *(int*) &(data.dlen =
			sizeof(data.data)));
	if (ret < 0) {
		snprintf(errmsg, sizeof(errmsg), "LinkTable GetData failed: %s",
				lt.GetErrorMsg());
		return -2;
	}

	return 0;
}

int MultiHashTable::GetMaxKeyLen() {
	if (header == NULL || ht == NULL) {
		snprintf(errmsg, sizeof(errmsg), "Hash table is not initialized");
		return -1;
	}
	return (int) (header->ddwHashNodeSize - sizeof(MhtNode));
}

int MultiHashTable::GetDataBlockSize() {
	if (header == NULL || ht == NULL) {
		snprintf(errmsg, sizeof(errmsg), "Hash table is not initialized");
		return -1;
	}
	return (int) header->ddwDataBlockSize;
}

int MultiHashTable::GetHashRowNum() {
	if (header == NULL || ht == NULL) {
		snprintf(errmsg, sizeof(errmsg), "Hash table is not initialized");
		return -1;
	}
	return (int) header->ddwRowNum;
}

int MultiHashTable::PrintInfo(const string &prefix) {
	if (header == NULL || ht == NULL) {
		cout << prefix << "Hash table is not initialized" << endl;
		return -1;
	}
	cout << prefix << "Hash table info:" << endl;
	cout << prefix << "    ddwTotalShmSize:   " << header->ddwTotalShmSize
			<< endl;
	cout << prefix << "    ddwHashNodeNum:    " << header->ddwHashNodeNum
			<< endl;
	cout << prefix << "    ddwHashNodeSize:   " << header->ddwHashNodeSize
			<< endl;
	cout << prefix << "    ddwDataBlockSize:  " << header->ddwDataBlockSize
			<< endl;
	cout << prefix << "    ddwDataBlockNum:   " << header->ddwDataBlockNum
			<< endl;
	cout << prefix << "    ddwRowNum:         " << header->ddwRowNum << endl;
	for (int i = 0; i < (int) header->ddwRowNum; i++)
		cout << prefix << "    auRowsNodeNum[" << i << "]:"
				<< (i < 10 ? "  " : (i < 100 ? " " : ""))
				<< header->auRowsNodeNum[i] << endl;
	cout << prefix << "    ddwMaxUsedRow:     " << header->ddwMaxUsedRow
			<< endl;
	cout << prefix << "    ddwEraseIterator:  " << header->ddwEraseIterator
			<< endl;
	cout << prefix << "Link table info:" << endl;
	lt.PrintInfo(prefix + "    ");
	return 0;
}

int MultiHashTable::ReportInfo() {
	if (header == NULL || ht == NULL) {
		return -1;
	}
	Attr_API_Set(389153, header->ddwTotalShmSize);
	Attr_API_Set(389154, header->ddwHashNodeNum);
	Attr_API_Set(389155, header->ddwHashNodeSize);
	Attr_API_Set(389156, header->ddwDataBlockSize);
	Attr_API_Set(389157, header->ddwDataBlockNum);
	Attr_API_Set(389158, header->ddwRowNum);
	Attr_API_Set(389159, header->ddwMaxUsedRow);
	return lt.ReportInfo();
}
