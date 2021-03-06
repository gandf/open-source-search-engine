#include "RdbIndexQuery.h"
#include "RdbBase.h"
#include <algorithm>

RdbIndexQuery::RdbIndexQuery(RdbBase *base)
	: RdbIndexQuery(base ? (base->getGlobalIndex() ? base->getGlobalIndex() : docidsconst_ptr_t()) : docidsconst_ptr_t(),
	                base ? (base->getTreeIndex() ? base->getTreeIndex()->getDocIds() : docidsconst_ptr_t()) : docidsconst_ptr_t(),
	                base ? base->getNumFiles() : 0,
	                base ? base->hasPendingGlobalIndexJob() : false) {
}

RdbIndexQuery::RdbIndexQuery(docidsconst_ptr_t globalIndexData, docidsconst_ptr_t treeIndexData, int32_t numFiles, bool hasPendingGlobalIndexJob)
	: m_globalIndexData(globalIndexData)
	, m_treeIndexData(treeIndexData)
	, m_numFiles(numFiles)
	, m_hasPendingGlobalIndexJob(hasPendingGlobalIndexJob) {
}

RdbIndexQuery::~RdbIndexQuery() {
}

int32_t RdbIndexQuery::getFilePos(uint64_t docId, bool isMerging) const {
	if (m_treeIndexData.get()) {
		auto it = std::lower_bound(m_treeIndexData->cbegin(), m_treeIndexData->cend(), docId << RdbIndex::s_docIdOffset);
		if (it != m_treeIndexData->cend() && ((*it >> RdbIndex::s_docIdOffset) == docId)) {
			return m_numFiles;
		}
	}

	auto it = std::lower_bound(m_globalIndexData->cbegin(), m_globalIndexData->cend(), docId << RdbBase::s_docIdFileIndex_docIdDelKeyOffset);
	if (it != m_globalIndexData->cend() && ((*it >> RdbBase::s_docIdFileIndex_docIdDelKeyOffset) == docId)) {
		return static_cast<int32_t>(*it & RdbBase::s_docIdFileIndex_filePosMask);
	}

	// if we're merging, docId should always be present in global index
	if (isMerging) {
		// mismatch in idx & data files?
		logError("Unable to find docId=%lu in global index", docId);
		gbshutdownLogicError();
	}

	/// this happens when pending docIds are not merged into RdbTree index
	return m_numFiles;
}

bool RdbIndexQuery::documentIsInFile(uint64_t docId, int32_t fileNum) const {
	if (m_treeIndexData.get()) {
		auto it = std::lower_bound(m_treeIndexData->cbegin(), m_treeIndexData->cend(), docId << RdbIndex::s_docIdOffset);
		if (it != m_treeIndexData->cend() && ((*it >> RdbIndex::s_docIdOffset) == docId)) {
			return (fileNum == m_numFiles);
		}
	}


	auto it = std::lower_bound(m_globalIndexData->cbegin(), m_globalIndexData->cend(), docId << RdbBase::s_docIdFileIndex_docIdDelKeyOffset);
	if (it != m_globalIndexData->cend() && ((*it >> RdbBase::s_docIdFileIndex_docIdDelKeyOffset) == docId)) {
		return (static_cast<int32_t>(*it & RdbBase::s_docIdFileIndex_filePosMask) == fileNum);
	}

	return false;
}

void RdbIndexQuery::printIndex() const {
	if (m_treeIndexData.get()) {
		for (auto key : *m_treeIndexData) {
			logf(LOG_TRACE, "db: docId=%" PRId64" index=%" PRId32" isDel=%d",
			     key >> RdbIndex::s_docIdOffset,
			     m_numFiles,
			     ((key & RdbIndex::s_delBitMask) == 0));
		}
	}

	for (auto key : *m_globalIndexData) {
		logf(LOG_TRACE, "db: docId=%" PRId64" index=%" PRId64" isDel=%d key=%" PRIx64,
		     (key & RdbBase::s_docIdFileIndex_docIdMask) >> RdbBase::s_docIdFileIndex_docIdDelKeyOffset,
		     key & RdbBase::s_docIdFileIndex_filePosMask,
		     ((key & RdbBase::s_docIdFileIndex_delBitMask) == 0),
			 key);
	}
}
