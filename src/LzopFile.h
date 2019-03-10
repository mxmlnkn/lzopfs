#pragma once

#include "Buffer.h"
#include "CompressedFile.h"


class LzopFile : public IndexedCompFile
{
protected:
    typedef uint32_t Checksum;

    enum Flags
    {
        AdlerDec = 1 << 0,
        AdlerComp = 1 << 1,
        ExtraField = 1 << 6,
        CRCDec = 1 << 8,
        CRCComp = 1 << 9,
        MultiPart = 1 << 10,
        Filter = 1 << 11,
        HeaderCRC = 1 << 12,
    };

    enum ChecksumType
    {
        Adler,
        CRC
    };

    static const unsigned char Magic[];
    static const uint16_t LzopDecodeVersion;

    uint32_t mFlags;

    void readHeaders( FileHandle& fh,
                      uint32_t&   flags );

    off_t findBlocks( FileHandle& fh,
                      uint32_t    flags,
                      off_t       uoff );

    Checksum checksum( ChecksumType  type,
                       const Buffer& buf );

    void checkFileType( FileHandle &fh ) override { readHeaders( fh, mFlags ); }

    void buildIndex( FileHandle& fh ) override;

public:
    static CompressedFile* open( const std::string& path,
                                 uint64_t           maxBlock )
    { return new LzopFile( path, maxBlock ); }

    LzopFile( const std::string& path,
              uint64_t           maxBlock );

    std::string destName() const override;

    void decompressBlock( const FileHandle& fh,
                          const Block&      b,
                          Buffer&           ubuf ) const override;

};
