#pragma once

#include <lzma.h>

#include "CompressedFile.h"


class PixzFile : public CompressedFile
{
protected:
    struct PixzBlock : public Block
    {
        lzma_check check;
    };

    class Iterator : public BlockIteratorInner
    {
        lzma_index_iter *mIter;
        PixzBlock mBlock;
        bool mEnd;

        virtual void makeBlock();

        // Disable copy
        Iterator( const Iterator& o );

        Iterator& operator=( const Iterator& o );

    public:
        Iterator( lzma_index_iter *i ) :
            mIter( i ),
            mEnd( false ) { makeBlock(); }

        Iterator() :
            mIter( 0 ),
            mEnd( true ) { }

        virtual ~Iterator()
        {
            if ( mIter ) {
                delete mIter;
            }
        }

        void incr() override;

        const Block& deref() const override { return mBlock; }

        bool end() const override { return mEnd; }

        BlockIteratorInner * dup() const override;
    };

    static const uint64_t MemLimit;

    lzma_index *mIndex;

    lzma_ret code( lzma_stream&      s,
                   const FileHandle& fh,
                   off_t             off = -1 ) const;

    lzma_index * readIndex( FileHandle& fh );

    void streamInit( lzma_stream& s ) const;

public:
    static CompressedFile* open( const std::string& path,
                                 uint64_t           maxBlock )
    { return new PixzFile( path, maxBlock ); }

    PixzFile( const std::string& path,
              uint64_t           maxBlock );

    virtual ~PixzFile();

    std::string destName() const override;

    BlockIterator findBlock( off_t off ) const override;

    void decompressBlock( const FileHandle& fh,
                          const Block&      b,
                          Buffer&           ubuf ) const override;

    off_t uncompressedSize() const override;

};
