#pragma once

#include "Block.h"
#include "Buffer.h"
#include "CompressedFile.h"
#include "GzipReader.h"


class GzipFile : public IndexedCompFile
{
protected:
    struct GzipBlock : public Block
    {
        size_t bits;
        Buffer dict;

        GzipBlock( off_t  rUoff,
                   off_t  rCoff,
                   size_t rBits ) :
            Block( 0, 0, rUoff, rCoff ),
            bits( rBits )
            {}
    };

    void setLastBlockSize( off_t uoff,
                           off_t coff );

    Buffer& addBlock( off_t  uoff,
                      off_t  coff,
                      size_t bits );

    void checkFileType( FileHandle &fh ) override;

    void buildIndex( FileHandle& fh ) override;

    Block* newBlock() const override { return new GzipBlock( 0, 0, 0 ); }

    bool readBlock( FileHandle& fh,
                    Block*      b ) override; // True unless EOF

    void writeBlock( FileHandle&  fh,
                             const Block *b ) const override;

public:
    static const size_t WindowSize;
    static uint64_t gMinDictBlockFactor;

    /**
     * This is the interface which will be used, e.g., by FileList.h to
     * create a new GzipFile object.
     * Has to be deallocated with delete after being finished!
     */
    static CompressedFile* open( const std::string& path,
                                 uint64_t           maxBlock )
    {
        return new GzipFile( path, maxBlock );
    }

    GzipFile( const std::string& path,
              uint64_t           maxBlock );

    std::string destName() const override;

    void decompressBlock( const FileHandle& fh,
                          const Block&      b,
                          Buffer&           ubuf ) const override;

};
