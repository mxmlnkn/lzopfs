#pragma once

#include <zlib.h>

#include "Block.h"
#include "Buffer.h"
#include "FileHandle.h"


/**
 * @file
 *
 * @verbatim
 * class structures:
 *   GzipReaderBase
 *     GzipBlockReader
 *     DiscardingGzipReader
 *       GzipHeaderReader
 *       PositionedGzipReader
 *         SavingGzipReader
 * @endverbatim
 */


namespace GzipReaderInternal {

/**
 * This is the base class for GzipBlockReader and DiscardingGzipReader
 * and should not be used directly.
 */
class GzipReaderBase {
public:
	struct Exception : std::runtime_error {
		Exception(const std::string& s) : std::runtime_error(s) { }
	};

	enum Wrapper {
        /* @see https://www.zlib.net/manual.html windowBits */
		Gzip = 16 + MAX_WBITS,
		Raw = -MAX_WBITS,
	};

private:
	// Disable copying
	GzipReaderBase(const GzipReaderBase& o) = delete;
	GzipReaderBase& operator=(const GzipReaderBase& o) = delete;

protected:
    /** checks for Zlib error (!= Z_OK) and prints message and throws exception if so */
	void throwEx(const std::string& s, int err);

	void resetOutBuf() {
		mStream.next_out = &outBuf()[0];
		mStream.avail_out = outBuf().size();
	}

	void setDict(const Buffer& dict);
	void prime(uint8_t byte, size_t bits);

	int step(int flush = Z_BLOCK); // One iteration of inflate
	int stepThrow(int flush = Z_BLOCK);

	virtual void initialize(bool force = false);
	virtual void moreData(Buffer& buf) = 0;
	virtual size_t chunkSize() const;
	virtual Wrapper wrapper() const { return Raw; }
	virtual Buffer& outBuf() = 0;
	virtual void writeOut(size_t n)
		{ throw std::runtime_error("out of space in gzip output buffer"); }

public:
	GzipReaderBase();
	inline virtual ~GzipReaderBase() { if (mInitialized) inflateEnd(&mStream); }

	void swap(GzipReaderBase& o);

	int block(); // Read to next block
	void reset(Wrapper w);

	virtual off_t ipos() const = 0;
	size_t ibits() const { return (mStream.data_type & 7); }
	off_t obytes() const { return mOutBytes; }

public:
	z_stream mStream; /**< libz struct holding the opened gzip stream */
	Buffer mInput;

protected:
	bool mInitialized = false;
	off_t mOutBytes = 0;
};

class DiscardingGzipReader : public GzipReaderInternal::GzipReaderBase {
protected:
	FileHandle& mFH;
	Buffer mOutBuf;

public:
	DiscardingGzipReader(FileHandle& fh) : mFH(fh) { }

	void swap(DiscardingGzipReader& o) {
		GzipReaderBase::swap(o);
		std::swap(mOutBuf, o.mOutBuf);
	}

	virtual void moreData(Buffer& buf);
	virtual void writeOut(size_t n) { resetOutBuf(); }
	virtual Buffer& outBuf() { return mOutBuf; }
	virtual off_t ipos() const { return mFH.tell() - mStream.avail_in; }
};


class PositionedGzipReader : public DiscardingGzipReader {
protected:
	off_t mInitOutPos;
	Wrapper mWrap;

	virtual Wrapper wrapper() const { return mWrap; }

	friend class SavingGzipReader;

public:
	PositionedGzipReader(FileHandle& fh, off_t opos = 0)
		: DiscardingGzipReader(fh), mInitOutPos(opos),
		mWrap(opos ? Raw : Gzip) { }

	void swap(PositionedGzipReader& o) {
		DiscardingGzipReader::swap(o);
		std::swap(mInitOutPos, o.mInitOutPos);
		std::swap(mWrap, o.mWrap);
	}

	virtual void reset(Wrapper w)
		{ mWrap = w; DiscardingGzipReader::reset(w); }
	off_t opos() const { return mInitOutPos + obytes(); }

	void skipFooter();
};

} // namespace GzipReaderInternal


class GzipHeaderReader : public GzipReaderInternal::DiscardingGzipReader {
public:
	GzipHeaderReader(FileHandle& fh) : DiscardingGzipReader(fh) { mOutBuf.resize(1); }
	inline virtual size_t chunkSize() const { return 512; }
	inline virtual Wrapper wrapper() const { return Gzip; }
	inline void header(gz_header& hdr) {
		initialize();
		throwEx("header", inflateGetHeader(&mStream, &hdr));
		while (!hdr.done)
			stepThrow();
	}
};

class GzipBlockReader : public GzipReaderInternal::GzipReaderBase {
protected:
	Buffer& mOutBuf;
	const FileHandle& mCFH;
	off_t mPos;

public:
	GzipBlockReader(const FileHandle& fh, Buffer& ubuf, const Block& b,
		const Buffer& dict, size_t bits);
	virtual void moreData(Buffer& buf);
	Buffer& outBuf() { return mOutBuf; }
	void read();
	virtual off_t ipos() const { return mPos; }
};

class SavingGzipReader : public GzipReaderInternal::PositionedGzipReader {
protected:
	PositionedGzipReader *mSave;
	off_t mSaveSeek;

	size_t windowSize() const;

public:
	SavingGzipReader(FileHandle& fh, off_t opos = 0)
		: PositionedGzipReader(fh, opos), mSave(0)
		{ mOutBuf.resize(windowSize()); }
	virtual ~SavingGzipReader() { if (mSave) delete mSave; }

	// Save the current state, and flush the dictionary
	void save();
	void restore();

	void copyWindow(Buffer& buf);
};
