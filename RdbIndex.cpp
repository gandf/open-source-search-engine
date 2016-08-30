#include "gb-include.h"

#include "RdbIndex.h"
#include "BigFile.h"
#include "Titledb.h"	// For MAX_DOCID
#include "Process.h"
#include "BitOperations.h"
#include "Conf.h"
#include <set>
#include <unordered_set>
#include <algorithm>
#include "RdbTree.h"
#include "RdbBuckets.h"
#include "ScopedLock.h"

#include <iterator>

static const int32_t s_defaultMaxPendingTimeMs = 5000;
static const uint32_t s_defaultMaxPendingSize = 2000000;
static const uint32_t s_generateMaxPendingSize = 20000000;

RdbIndex::RdbIndex()
	: m_file()
	, m_fixedDataSize(0)
	, m_useHalfKeys(false)
	, m_ks(0)
	, m_rdbId(RDB_NONE)
	, m_docIds(new docids_t)
	, m_docIdsMtx()
	, m_pendingDocIds(new docids_t)
	, m_pendingDocIdsMtx()
	, m_prevPendingDocId(MAX_DOCID + 1)
	, m_lastMergeTime(gettimeofdayInMilliseconds())
	, m_needToWrite(false) {
}

// dont save index on deletion!
RdbIndex::~RdbIndex() {
}

void RdbIndex::reset() {
	m_file.reset();

	m_prevPendingDocId = MAX_DOCID + 1;

	m_docIds->clear();
	m_pendingDocIds->clear();

	m_needToWrite = false;
}

/// @todo ALC collapse RdbIndex::set into constructor
void RdbIndex::set(const char *dir, const char *indexFilename,
                   int32_t fixedDataSize , bool useHalfKeys , char keySize, char rdbId) {
	logTrace(g_conf.m_logTraceRdbIndex, "BEGIN. dir [%s], indexFilename [%s]", dir, indexFilename);

	reset();
	m_fixedDataSize = fixedDataSize;
	m_file.set ( dir , indexFilename );
	m_useHalfKeys = useHalfKeys;
	m_ks = keySize;
	m_rdbId = rdbId;
}

bool RdbIndex::close(bool urgent) {
	bool status = true;
	if (m_needToWrite) {
		status = writeIndex();
	}

	// clears and frees everything
	if (!urgent) {
		reset();
	}

	return status;
}

bool RdbIndex::writeIndex() {
	logTrace(g_conf.m_logTraceRdbIndex, "BEGIN. filename [%s]", m_file.getFilename());

	if (g_conf.m_readOnlyMode) {
		logTrace(g_conf.m_logTraceRdbIndex, "END. Read-only mode, not writing index. filename [%s]. Returning true.",
		         m_file.getFilename());
		return true;
	}

	if (!m_needToWrite) {
		logTrace(g_conf.m_logTraceRdbIndex, "END. no need, not writing index. filename [%s]. Returning true.",
		         m_file.getFilename());
		return true;
	}

	// open a new file
	if (!m_file.open(O_RDWR | O_CREAT | O_TRUNC)) {
		logError("END. Could not open %s for writing: %s. Returning false.", m_file.getFilename(), mstrerror(g_errno))
		return false;
	}

	// write index data
	bool status = writeIndex2();

	// on success, we don't need to write it anymore
	if (status) {
		m_needToWrite = false;
	}

	logTrace(g_conf.m_logTraceRdbIndex, "END. filename [%s], returning %s", m_file.getFilename(), status ? "true" : "false");

	return status;
}

bool RdbIndex::writeIndex2() {
	logTrace(g_conf.m_logTraceRdbIndex, "BEGIN. filename [%s]", m_file.getFilename());

	g_errno = 0;

	int64_t offset = 0LL;

	// make sure we always write the newest tree
	// remove const as m_file.write does not accept const buffer
	docids_ptr_t tmpDocIds = std::const_pointer_cast<docids_t>(mergePendingDocIds());

	/// @todo ALC we may want to store size of data used to generate index file here so that we can validate index file
	/// eg: store total keys in buckets/tree; size of rdb file
	// first 8 bytes are the total docIds in the index file
	size_t docid_count = tmpDocIds->size();

	m_file.write(&docid_count, sizeof(docid_count), offset);
	if (g_errno) {
		logError("Failed to write to %s (docid_count): %s", m_file.getFilename(), mstrerror(g_errno))
		return false;
	}

	offset += sizeof(docid_count);

	m_file.write(&(*tmpDocIds)[0], docid_count * sizeof((*tmpDocIds)[0]), offset);
	if (g_errno) {
		logError("Failed to write to %s (docids): %s", m_file.getFilename(), mstrerror(g_errno))
		return false;
	}

	logTrace(g_conf.m_logTraceRdbIndex, "END - OK, returning true.");

	return true;
}


bool RdbIndex::readIndex() {
	logTrace(g_conf.m_logTraceRdbIndex, "BEGIN. filename [%s]", m_file.getFilename());

	// bail if does not exist
	if (!m_file.doesExist()) {
		logError("Index file [%s] does not exist.", m_file.getFilename());
		logTrace(g_conf.m_logTraceRdbIndex, "END. Returning false");
		return false;
	}

	// . open the file
	// . do not open O_RDONLY because if we are resuming a killed merge
	//   we will add to this index and write it back out.
	if (!m_file.open(O_RDWR)) {
		logError("Could not open index file %s for reading: %s.", m_file.getFilename(), mstrerror(g_errno));
		logTrace(g_conf.m_logTraceRdbIndex, "END. Returning false");
		return false;
	}

	bool status = readIndex2();

	m_file.closeFds();

	logTrace(g_conf.m_logTraceRdbIndex, "END. Returning %s", status ? "true" : "false");

	// return status
	return status;
}

bool RdbIndex::readIndex2() {
	logTrace(g_conf.m_logTraceRdbIndex, "BEGIN. filename [%s]", m_file.getFilename());

	g_errno = 0;

	int64_t offset = 0;
	size_t docid_count = 0;

	// first 8 bytes are the size of the DATA file we're indexing
	m_file.read(&docid_count, sizeof(docid_count), offset);
	if ( g_errno ) {
		logError("Had error reading offset=%" PRId64" from %s: %s", offset, m_file.getFilename(), mstrerror(g_errno));
		return false;
	}

	docids_ptr_t tmpDocIds(new docids_t);

	offset += sizeof(docid_count);
	tmpDocIds->resize(docid_count);

	m_file.read(&(*tmpDocIds)[0], docid_count * sizeof((*tmpDocIds)[0]), offset);
	if (g_errno) {
		logError("Had error reading offset=%" PRId64" from %s: %s", offset, m_file.getFilename(), mstrerror(g_errno));
		return false;
	}

	// replace with new index
	ScopedLock sl(m_docIdsMtx);
	m_docIds.swap(tmpDocIds);

	logTrace(g_conf.m_logTraceRdbIndex, "END. Returning true with %zu docIds loaded", m_docIds->size());
	return true;
}

void RdbIndex::addRecord(char *key) {
	ScopedLock sl(m_pendingDocIdsMtx);
	addRecord_unlocked(key, false);
}

docidsconst_ptr_t RdbIndex::mergePendingDocIds() {
	m_lastMergeTime = gettimeofdayInMilliseconds();

	// merge pending docIds into docIds
	std::sort(m_pendingDocIds->begin(), m_pendingDocIds->end());
	m_pendingDocIds->erase(std::unique(m_pendingDocIds->begin(), m_pendingDocIds->end()), m_pendingDocIds->end());

	docids_ptr_t tmpDocIds(new docids_t(*m_docIds));
	auto midIt = std::prev(tmpDocIds->end());
	tmpDocIds->insert(tmpDocIds->end(), m_pendingDocIds->begin(), m_pendingDocIds->end());
	std::inplace_merge(tmpDocIds->begin(), midIt, tmpDocIds->end());

	// replace existing
	ScopedLock sl(m_docIdsMtx);
	m_docIds.swap(tmpDocIds);

	return m_docIds;
}

void RdbIndex::addRecord_unlocked(char *key, bool isGenerateIndex) {
	if (m_rdbId == RDB_POSDB || m_rdbId == RDB2_POSDB2) {
		if (key[0] & 0x02 || !(key[0] & 0x04)) {
			//it is a 12-byte docid+pos or 18-byte termid+docid+pos key
			uint64_t doc_id = extract_bits(key, 58, 96);
			if (doc_id != m_prevPendingDocId) {
				m_pendingDocIds->push_back(doc_id);
				m_prevPendingDocId = doc_id;
				m_needToWrite = true;
			}
		}
	} else {
		logError("Not implemented for dbname=%s", getDbnameFromId(m_rdbId));
		gbshutdownLogicError();
	}

	bool doMerge = false;
	if (isGenerateIndex) {
		if (m_pendingDocIds->size() >= s_generateMaxPendingSize) {
			doMerge = true;
		}
	} else {
		if ((m_pendingDocIds->size() >= s_defaultMaxPendingSize) ||
		    (gettimeofdayInMilliseconds() - m_lastMergeTime >= s_defaultMaxPendingTimeMs)) {
			doMerge = true;
		}
	}

	if (doMerge) {
		(void)mergePendingDocIds();
	}
}

void RdbIndex::printIndex() {
	//@todo: IMPLEMENT!
	logError("NOT IMPLEMENTED YET");
}

bool RdbIndex::generateIndex(RdbTree *tree, collnum_t collnum, const char *dbname) {
	reset();

	if (g_conf.m_readOnlyMode) {
		return false;
	}

	log(LOG_INFO, "db: Generating index for %s tree", dbname);

	// use extremes
	const char *startKey = KEYMIN();
	const char *endKey = KEYMAX();
	int32_t numPosRecs  = 0;
	int32_t numNegRecs = 0;

	RdbList list;
	if (!tree->getList(collnum, startKey, endKey, -1, &list, &numPosRecs, &numNegRecs, m_useHalfKeys)) {
		return false;
	}

	char key[MAX_KEY_BYTES];

	ScopedLock sl(m_pendingDocIdsMtx);
	for (list.resetListPtr(); !list.isExhausted(); list.skipCurrentRecord()) {
		list.getCurrentKey(key);
		addRecord_unlocked(key, true);
	}
	return true;
}

bool RdbIndex::generateIndex(RdbBuckets *buckets, collnum_t collnum, const char *dbname) {
	reset();

	if (g_conf.m_readOnlyMode) {
		return false;
	}

	log(LOG_INFO, "db: Generating index for %s buckets", dbname);

	// use extremes
	const char *startKey = KEYMIN();
	const char *endKey = KEYMAX();
	int32_t numPosRecs  = 0;
	int32_t numNegRecs = 0;

	RdbList list;
	if (!buckets->getList(collnum, startKey, endKey, -1, &list, &numPosRecs, &numNegRecs, m_useHalfKeys)) {
		return false;
	}

	char key[MAX_KEY_BYTES];

	ScopedLock sl(m_pendingDocIdsMtx);
	for (list.resetListPtr(); !list.isExhausted(); list.skipCurrentRecord()) {
		list.getCurrentKey(key);
		addRecord_unlocked(key, true);
	}

	return true;
}

// . attempts to auto-generate from data file, f
// . returns false and sets g_errno on error
bool RdbIndex::generateIndex(BigFile *f) {
	reset();

	if (g_conf.m_readOnlyMode) {
		return false;
	}

	log(LOG_INFO, "db: Generating index for %s/%s", f->getDir(), f->getFilename());

	if (!f->doesPartExist(0)) {
		g_errno = EBADENGINEER;
		log(LOG_WARN, "db: Cannot generate index for this headless data file");
		return false;
	}

	// scan through all the recs in f
	int64_t offset = 0;
	int64_t fileSize = f->getFileSize();

	// if file is length 0, we don't need to do much
	// g_errno should be set on error
	if (fileSize == 0 || fileSize < 0) {
		return (fileSize == 0);
	}

	// don't read in more than 10 megs at a time initially
	int64_t bufSize = fileSize;
	if (bufSize > 10 * 1024 * 1024) {
		bufSize = 10 * 1024 * 1024;
	}
	char *buf = (char *)mmalloc(bufSize, "RdbIndex");

	// use extremes
	const char *startKey = KEYMIN();
	const char *endKey = KEYMAX();

	// a rec needs to be at least this big
	// negative keys do not have the dataSize field
	int32_t minRecSize = (m_fixedDataSize == -1) ? 0 : m_fixedDataSize;

	// POSDB
	if (m_ks == 18) {
		minRecSize += 6;
	} else if (m_useHalfKeys) {
		minRecSize += m_ks - 6;
	} else {
		minRecSize += m_ks;
	}

	// for parsing the lists into records
	char key[MAX_KEY_BYTES];
	int64_t next = 0LL;

	ScopedLock sl(m_pendingDocIdsMtx);
	// read in at most "bufSize" bytes with each read
	for (; offset < fileSize;) {
		// keep track of how many bytes read in the log
		if (offset >= next) {
			if (next != 0) {
				logf(LOG_INFO, "db: Read %" PRId64" bytes [%s]", next, f->getFilename());
			}

			next += 500000000; // 500MB
		}

		// our reads should always block
		int64_t readSize = fileSize - offset;
		if (readSize > bufSize) {
			readSize = bufSize;
		}

		// if the readSize is less than the minRecSize, we got a bad cutoff
		// so we can't go any more
		if (readSize < minRecSize) {
			mfree(buf, bufSize, "RdbMap");
			return true;
		}

		// otherwise, read it in
		if (!f->read(buf, readSize, offset)) {
			mfree(buf, bufSize, "RdbMap");
			log(LOG_WARN, "db: Failed to read %" PRId64" bytes of %s at offset=%" PRId64". Map generation failed.",
			    bufSize, f->getFilename(), offset);
			return false;
		}

		RdbList list;

		// set the list
		list.set(buf, readSize, buf, readSize, startKey, endKey, m_fixedDataSize, false, m_useHalfKeys, m_ks);

		// . HACK to fix useHalfKeys compression thing from one read to the nxt
		// . "key" should still be set to the last record we read last read
		if (offset > 0) {
			// ... fix for posdb!!!
			if (m_ks == 18) {
				list.m_listPtrLo = key + (m_ks - 12);
			} else {
				list.m_listPtrHi = key + (m_ks - 6);
			}
		}

		bool advanceOffset = true;

		// . parse through the records in the list
		for (; !list.isExhausted(); list.skipCurrentRecord()) {
			char *rec = list.getCurrentRec();
			if (rec + 64 > list.getListEnd() && offset + readSize < fileSize) {
				// set up so next read starts at this rec that MAY have been cut off
				offset += (rec - buf);

				advanceOffset = false;
				break;
			}

			// WARNING: when data is corrupted these may cause segmentation faults?
			list.getCurrentKey(key);
			int32_t recSize = list.getCurrentRecSize();

			// don't chop keys
			if (recSize < 6) {
				log(LOG_WARN, "db: Got negative recsize of %" PRId32" at offset=%" PRId64, recSize,
				    offset + (rec - buf));

				// @todo ALC do we want to abort here?
				return false;
			}

			// do we have a breech?
			if (rec + recSize > buf + readSize) {
				// save old
				int64_t oldOffset = offset;

				// set up so next read starts at this rec that got cut off
				offset += (rec - buf);

				// . if we advanced nothing, then we'll end up looping forever
				// . this will == 0 too, for big recs that did not fit in our
				//   read but we take care of that below
				// . this can happen if merge dumped out half-ass
				// . the write split a record...
				if (rec - buf == 0 && recSize <= bufSize) {
					log(LOG_WARN,
					    "db: Index generation failed because last record in data file was split. Power failure while writing?");

					// @todo ALC do we want to abort here?
					return false;
				}

				// is our buf big enough to hold this type of rec?
				if (recSize > bufSize) {
					mfree(buf, bufSize, "RdbIndex");
					bufSize = recSize;
					buf = (char *)mmalloc(bufSize, "RdbIndex");
					if (!buf) {
						log(LOG_WARN, "db: Got error while generating the index file: %s. offset=%" PRIu64".",
						    mstrerror(g_errno), oldOffset);
						return false;
					}
				}
				// read again starting at the adjusted offset
				advanceOffset = false;
				break;
			}

			addRecord_unlocked(key, true);
		}

		if (advanceOffset) {
			offset += readSize;
		}
	}

	// don't forget to free this
	mfree(buf, bufSize, "RdbMap");

	// otherwise, we're done
	return true;
}

docidsconst_ptr_t RdbIndex::getDocIds() {
	ScopedLock sl(m_docIdsMtx);
	return m_docIds;
}
