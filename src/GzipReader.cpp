#include "GzipReader.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "CompressedFile.h"
#include "GzipFile.h"


void checkZlibStreamOperation( int         errorCode,
                               const char* fileName,
                               int         lineNumber,
                               const char* functionName,
                               const char* stringifiedArgument )
{
    if ( errorCode == Z_OK ) {
        return;
    }

    std::stringstream msg;
    msg << "\n[" << fileName << ":" << lineNumber << "]\n";
    msg << "  Function: " << functionName << "\n";
    msg << "  Expression evaluated for error code: " << stringifiedArgument << "\n";
    msg << "  ZLib error code: " << errorCode << "\n";

    throw GzipReaderInternal::GzipReaderBase::Exception( msg.str() );
}


namespace GzipReaderInternal {

void GzipReaderBase::setDict( const Buffer& dict )
{
    initialize();
    if ( !dict.empty() ) {
        CHECK_ZLIB( inflateSetDictionary( &mStream, &dict[0], dict.size() ) );
    }
}

void GzipReaderBase::prime( uint8_t byte,
                            size_t  bits )
{
    initialize();
    if ( bits != 0 ) {
        CHECK_ZLIB( inflatePrime( &mStream, bits, byte >> ( 8 - bits ) ) );
    }
}

int GzipReaderBase::step( int flush )
{
    initialize();
    if ( mStream.avail_in == 0 ) {
        moreData( mInput );
        mStream.avail_in = mInput.size();
        mStream.next_in = &mInput[0];
    }
    if ( mStream.avail_out == 0 ) {
        writeOut();
    }

    mOutBytes += mStream.avail_out;
    int err = inflate( &mStream, flush );
    mOutBytes -= mStream.avail_out;
    return err;
}

int GzipReaderBase::stepThrow( int flush )
{
    int err = step( flush );
    if ( err != Z_STREAM_END ) {
        CHECK_ZLIB( err );
    }
    return err;
}

void GzipReaderBase::initialize()
{
    if ( mInitialized ) {
        return;
    }

    CHECK_ZLIB( inflateInit2( &mStream, wrapper() ) );
    resetOutBuf();

    mInitialized = true;
}

size_t GzipReaderBase::chunkSize() const
{
    return CompressedFile::ChunkSize;
}

GzipReaderBase::GzipReaderBase()
{
    mStream.zfree = Z_NULL;
    mStream.zalloc = Z_NULL;
    mStream.opaque = Z_NULL;
    mStream.avail_in = 0;
}

void GzipReaderBase::swap( GzipReaderBase& o )
{
    initialize();
    o.initialize();
    std::swap( mStream, o.mStream );
    std::swap( mInput, o.mInput );
    std::swap( mOutBytes, o.mOutBytes );
}

int GzipReaderBase::block()
{
    int err;
    do {
        err = step( Z_BLOCK );
        if ( ( err != Z_OK ) &&( err != Z_STREAM_END ) ) {
            return err;
        }
        if ( mStream.data_type & 128 ) {
            break;
        }
    } while ( err != Z_STREAM_END );

    return err;
}

void PositionedGzipReader::skipFooter()
{
    if ( wrapper() == Gzip ) {
        return;         // footer should've been processed
    }
    const size_t footerSize = 8;
    if ( mStream.avail_in < footerSize ) {
        mFH.seek( footerSize - mStream.avail_in, SEEK_CUR );
        mStream.avail_in = 0;
    } else {
        mStream.avail_in -= footerSize;
        mStream.next_in += footerSize;
    }
}

} // namespace GzipReaderInternal

GzipBlockReader::GzipBlockReader( const FileHandle& fh,
                                  Buffer&           ubuf,
                                  const Block&      b,
                                  const Buffer&     dict,
                                  size_t            bits ) :
    mOutBuf( ubuf ),
    mCFH( fh ),
    mPos( b.coff - ( bits ? 1 : 0 ) )
{
    setDict( dict );
    if ( bits ) {
        uint8_t byte;
        mCFH.pread( mPos, &byte, sizeof( byte ) );
        mPos += sizeof( byte );
        prime( byte, bits );
    }
}

void SavingGzipReader::save()
{
    if ( !mSave ) {
        mSave = new PositionedGzipReader( mFH );
    }
    swap( *mSave );

    mSaveSeek = mFH.tell();

    // Fixup state to correspond to where we were
    reset( Raw );

    mInput = mSave->mInput;
    mStream.avail_in = mSave->mStream.avail_in;
    mStream.next_in = &mInput[0] + mInput.size() - mStream.avail_in;

    mOutBuf.resize( windowSize() );
    resetOutBuf();

    size_t bits = mSave->ibits();
    prime( mStream.next_in[-1], bits );

    mInitOutPos = mSave->opos();
    mOutBytes = 0;
}

void SavingGzipReader::restore()
{
    if ( !mSave ) {
        throw std::runtime_error( "no saved state" );
    }

    swap( *mSave );
    mFH.seek( mSaveSeek, SEEK_SET );
}

void SavingGzipReader::copyWindow( Buffer& buf )
{
    buf.resize( outBuf().size() );
    std::rotate_copy( outBuf().begin(), outBuf().end() - mStream.avail_out,
                      outBuf().end(), buf.begin() );
}

size_t
SavingGzipReader::windowSize() const
{
    return GzipFile::WindowSize;
}
