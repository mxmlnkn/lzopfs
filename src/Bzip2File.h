#pragma once

#include "Block.h"
#include "Buffer.h"
#include "Debug.h"
#include "CompressedFile.h"

#include <list>

class Bzip2File : public IndexedCompFile
{
protected:
    void checkFileType( FileHandle &fh ) override;

    void buildIndex( FileHandle& fh ) override;

    struct BlockBoundary
    {
        uint64_t magic;
        char level;
        off_t coff;
        size_t bits;

        BlockBoundary( uint64_t m,
                       char     l,
                       off_t    c,
                       size_t   b ) :
            magic( m ),
            level( l ),
            coff( c ),
            bits( b ) { }
    };
    typedef std::list<BlockBoundary> BoundList;

    struct Bzip2Block : public Block
    {
        size_t bits, endbits;
        char level;

        Bzip2Block() { }

        Bzip2Block( BlockBoundary& rStart,
                    BlockBoundary& rEnd,
                    off_t          rUoff,
                    size_t         rUsize,
                    char           rLev ) :
            Block( rUsize, rEnd.coff - rStart.coff, rUoff, rStart.coff ),
            bits( rStart.bits ),
            endbits( rEnd.bits ),
            level( rLev ) { }
    };

    void findBlockBoundaryCandidates( FileHandle& fh,
                                      BoundList&  bl ) const;

    void createAlignedBlock( const FileHandle& fh,
                             Buffer&           b,
                             char              level,
                             off_t             coff,
                             size_t            bits,
                             off_t             end,
                             size_t            endbits ) const;

    void decompress( const Buffer& in,
                     Buffer& out ) const;

    Block* newBlock() const override { return new Bzip2Block(); }

    bool readBlock( FileHandle& fh,
                    Block*      b ) override;

    void writeBlock( FileHandle&  fh,
                     const Block *b ) const override;

public:
    static const char Magic[];
    static const uint64_t BlockMagic = 0x314159265359;
    static const size_t BlockMagicBytes = 6;
    static const uint64_t BlockMagicMask = ( 1LL << ( BlockMagicBytes * 8 ) ) - 1;
    static const uint64_t EOSMagic = 0x177245385090;

    static CompressedFile* open( const std::string& path,
                                 uint64_t           maxBlock )
    { return new Bzip2File( path, maxBlock ); }

    Bzip2File( const std::string& path,
               uint64_t           maxBlock ) :
        IndexedCompFile( path ) { initialize( maxBlock ); }

    std::string destName() const override;

    void decompressBlock( const FileHandle& fh,
                          const Block&      b,
                          Buffer&           ubuf ) const override;

};
