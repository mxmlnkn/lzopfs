#pragma once

#include "CompressedFile.h"

#include <string>
#include <unordered_map>
#include <vector>

#include <stdint.h>


class FileList
{
private:
    typedef CompressedFile* (*OpenFunc)( const std::string& path,
                                         uint64_t           maxBlock );

    typedef std::vector<OpenFunc> OpenerList;

private:
    // Disable copying
    FileList( const FileList& o ) = delete;

    FileList& operator=( const FileList& o ) = delete;

private:
    /** Returns list of file open functors. One for each compression type. */
    static OpenerList initOpeners();

public:
    FileList( uint64_t maxBlockSize = UINT64_MAX ) :
        mMaxBlockSize( maxBlockSize ) { }

    virtual ~FileList();

    CompressedFile * find( const std::string& dest );

    void add( const std::string& source );

    template <typename Op>
    void forNames( Op op )
    {
        for ( const auto& nameAndObject : mMap ) {
            op( nameAndObject.first );
        }
    }

    inline size_t size() const { return mMap.size(); }

private:
    static const OpenerList Openers;

    /**
     * Holds the path of the extracted file with the mount point as root
     * and a pointer to the CompressedFile object handling access to the
     * corrsponding archive.
     * This design will only work for archives with a one-file per
     * archive basis like gzip, bzip2, ... but not zip
     */
    std::unordered_map<std::string, CompressedFile*> mMap;
    uint64_t mMaxBlockSize;
};
