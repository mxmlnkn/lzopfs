#pragma once

#include <stdexcept>

#include <zlib.h>

#include "Block.h"
#include "Buffer.h"
#include "Debug.h"
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


void checkZlibStreamOperation( int         errorCode,
                               const char* fileName,
                               int         lineNumber,
                               const char* functionName,
                               const char* stringifiedArgument );

#define CHECK_ZLIB( x ) ::checkZlibStreamOperation( x, __FILENAME__, __LINE__, AVAILABLE_FUNCTION, #x )


class GzipHeaderReader;
class SavingGzipReader;


namespace GzipReaderInternal {

/**
 * This is the base class for GzipBlockReader and DiscardingGzipReader
 * and should not be used directly.
 */
class GzipReaderBase
{
public:
    struct Exception : std::runtime_error
    {

        Exception( const std::string& s ) :
            std::runtime_error( s ) { }
    };

    enum Wrapper
    {
        /* @see https://www.zlib.net/manual.html windowBits */
        Gzip = 16 + MAX_WBITS,
        Raw = -MAX_WBITS,
    };

private:
    // Disable copying
    GzipReaderBase( const GzipReaderBase& o ) = delete;

    GzipReaderBase& operator=( const GzipReaderBase& o ) = delete;

protected:
    /** checks for Zlib error (!= Z_OK) and prints message and throws exception if so */
    void throwEx( const std::string& s,
                  int                err );

    inline void resetOutBuf()
    {
        mStream.next_out = &outBuf()[0];
        mStream.avail_out = outBuf().size();

        if ( mStream.next_out == nullptr ) {
            std::cerr << "Output buffer location is null. Can't work with this!\n";
        }

        if ( mStream.avail_out == 0 ) {
            std::cerr << "Available outbut buffer is empty. Can't work with this!\n";
        }
    }

    /** only used by GzipBlockReader */
    void setDict( const Buffer& dict );

    void prime( uint8_t byte,
                size_t  bits );

    int step( int flush = Z_BLOCK ); // One iteration of inflate

    int stepThrow( int flush = Z_BLOCK );

    void initialize();

    virtual void moreData( Buffer& buf ) = 0;

    virtual size_t chunkSize() const;

    inline virtual Wrapper wrapper() const { return Raw; }

    virtual Buffer& outBuf() = 0;

    inline virtual void writeOut()
    {
        throw std::runtime_error( "out of space in gzip output buffer" );
    }

    /** debug function */
    inline static void
    dumpStream( const z_stream& stream )
    {
        std::cerr
        << "z_stream:\n"
        << "  next input from                : " << (void*) stream.next_in << "\n"
        << "  available input bytes          : " << stream.avail_in << "\n"
        << "  input bytes already read       : " << stream.total_in << "\n"
        << "\n"
        << "  next output to                 : " << (void*) stream.next_out << "\n"
        << "  remaining free output space    : " << stream.avail_out << "\n"
        << "  output bytes already written   : " << stream.total_out << "\n"
        << "\n"
        << "  last error message             : " << ( stream.msg == nullptr ? "(null)" : stream.msg ) << "\n"
        << "  data type guess                : " << stream.data_type << "\n"
        << "  checksum for uncompressed data : " << stream.adler << "\n"
        << "\n";
    }

public:
    GzipReaderBase();

    inline virtual
    ~GzipReaderBase()
    {
        if ( mInitialized ) {
            inflateEnd( &mStream );
        }
    }

    void
    swap( GzipReaderBase& o );

    int
    block();     // Read to next block

    inline virtual void
    reset( Wrapper w )
    {
        dumpStream( mStream );
        CHECK_ZLIB( inflateReset2( &mStream, w ) );
    };

    virtual off_t
    ipos() const = 0;

    size_t
    ibits() const
    {
        return mStream.data_type & 7;
    }

    off_t
    obytes() const
    {
        return mOutBytes;
    }

public:
    z_stream mStream;     /**< libz struct holding the opened gzip stream */
    Buffer mInput;
    bool mInitialized = false;
    off_t mOutBytes = 0;
};


class DiscardingGzipReader : public GzipReaderInternal::GzipReaderBase
{
protected:
    FileHandle& mFH;
    Buffer mOutBuf;

public:
    inline
    DiscardingGzipReader( FileHandle& fh ) :
        mFH( fh )
    {}

    inline void
    swap( DiscardingGzipReader& o )
    {
        GzipReaderBase::swap( o );
        std::swap( mOutBuf, o.mOutBuf );
    }

    inline void
    moreData( Buffer& buf ) override
    {
        mFH.tryRead( buf, chunkSize() );
    }

    inline void
    writeOut() override
    {
        resetOutBuf();
    }

    inline Buffer&
    outBuf() override
    {
        return mOutBuf;
    }

    inline off_t
    ipos() const override
    {
        return mFH.tell() - mStream.avail_in;
    }
};


class PositionedGzipReader : public DiscardingGzipReader
{
    friend class ::SavingGzipReader;

protected:
    off_t mInitOutPos;
    Wrapper mWrap;

protected:
    Wrapper wrapper() const override { return mWrap; }

public:
    inline PositionedGzipReader( FileHandle& fh,
                                 off_t       opos = 0 ) :
        DiscardingGzipReader( fh ),
        mInitOutPos( opos ),
        mWrap( opos ? Raw : Gzip )
    {}

    inline void
    swap( PositionedGzipReader& o )
    {
        DiscardingGzipReader::swap( o );
        std::swap( mInitOutPos, o.mInitOutPos );
        std::swap( mWrap, o.mWrap );
    }

    inline void
    reset( Wrapper w ) override
    {
        mWrap = w;
        DiscardingGzipReader::reset( w );
    }

    off_t opos() const { return mInitOutPos + obytes(); }

    void skipFooter();

};


} // namespace GzipReaderInternal


class GzipHeaderReader : public GzipReaderInternal::DiscardingGzipReader
{
public:
    GzipHeaderReader( FileHandle& fh ) :
        DiscardingGzipReader( fh ) { mOutBuf.resize( 1 ); }

    inline size_t chunkSize() const override { return 512; }

    inline Wrapper wrapper() const override { return Gzip; }

    inline void header( gz_header& hdr )
    {
        initialize();
        CHECK_ZLIB( inflateGetHeader( &mStream, &hdr ) );
        while ( !hdr.done ) {
            stepThrow();
        }
    }

};


class GzipBlockReader : public GzipReaderInternal::GzipReaderBase
{
protected:
    Buffer& mOutBuf;
    const FileHandle& mCFH;
    off_t mPos;

public:
    GzipBlockReader( const FileHandle& fh,
                     Buffer&           ubuf,
                     const Block&      b,
                     const Buffer&     dict,
                     size_t            bits );

    void moreData( Buffer& buf ) override
    {
        mCFH.tryPRead( mPos, buf, chunkSize() );
        mPos += buf.size();
    }

    inline void
    read()
    {
        while ( mStream.avail_out ) {
            stepThrow( Z_NO_FLUSH );
        }
    }

    Buffer& outBuf() override { return mOutBuf; }
    off_t ipos() const override { return mPos; }
};


class SavingGzipReader : public GzipReaderInternal::PositionedGzipReader
{
protected:
    PositionedGzipReader* mSave = nullptr;
    off_t mSaveSeek;

public:
    SavingGzipReader( FileHandle& fh,
                      off_t       opos = 0 ) :
        PositionedGzipReader( fh, opos )
    {
        std::cerr << "Resize buffer to " << windowSize() << "\n";
        mOutBuf.resize( windowSize() );
    }

    virtual ~SavingGzipReader()
    {
        if ( mSave != nullptr ) {
            delete mSave;
        }
    }

    // Save the current state, and flush the dictionary
    void save();

    void restore();

    void copyWindow( Buffer& buf );

protected:
    size_t
    windowSize() const;
};
