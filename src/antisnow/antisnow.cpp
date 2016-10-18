/*
 * antisnow.cpp
 *
 *  Created on: 2016-9-17
 *      Author: tiankonguse
 */

#include "antisnow.h"
#include "minSpinLock.h"

namespace SHM_CACHE {

AntiSnow::AntiSnow() {
	pstNode = NULL;
	iErrorNo = 0;
	iAntiSnowShmSize = sizeof(AntiSnowNode) * MAX_ANTISNOW_NODE;
	shm.init(ANTISNOW_SHM_ID, iAntiSnowShmSize);
}

int AntiSnow::report(int iPos, int iValue) {

	if (unlikely(iPos >= MAX_ANTISNOW_NODE)) {
		snprintf(lastErrorBuf, sizeof(lastErrorBuf),
				"iPos=[%d]>=MAX_ANTISNOW_NODE[%d] pos too max", iPos,
				MAX_ANTISNOW_NODE);
		strLastError = lastErrorBuf;
		return -__LINE__;
	}

	//第一次创建
	if (unlikely(pstNode == NULL)) {
		iErrorNo = shm.getShmInit((void**) &pstNode, 0666 | IPC_CREAT);
		if (iErrorNo < 0) {
			snprintf(lastErrorBuf, sizeof(lastErrorBuf), "ret=[%d] msg[%s]",
					iErrorNo, shm.getLastError());
			strLastError = lastErrorBuf;
			return -__LINE__;
		}
		iErrorNo = 0;
	}

	int retNum = 0;
	unsigned int iTime = time(NULL);
	MINLOCK(pstNode[iPos].iLock);
	if (pstNode[iPos].iTime < iTime) {
		pstNode[iPos].iTime = iTime;
		pstNode[iPos].iNum = iValue;
	} else {
		pstNode[iPos].iNum += iValue;
	}
	retNum = pstNode[iPos].iNum;
	MINUNLOCK(pstNode[iPos].iLock);
	return retNum;
}

}

