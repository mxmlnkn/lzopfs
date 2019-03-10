#include <cstddef>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "BlockCache.h"
#include "CompressedFile.h"
#include "FileList.h"
#include "GzipFile.h"
#include "OpenCompressedFile.h"
#include "PathUtils.h"
#include "ThreadPool.h"


namespace {

typedef uint64_t FuseFH;

const size_t CacheSize = 1024 * 1024 * 32;

struct FSData
{
    FileList *files;
    ThreadPool pool;
    BlockCache cache;

    FSData( FileList* f ) : files( f ), pool(), cache( pool )
    {
        cache.maxSize( CacheSize );
    }

    ~FSData() { delete files; }
};

FSData * fsdata()
{
    return reinterpret_cast<FSData*>( fuse_get_context()->private_data );
}

extern "C" void * lf_init( struct fuse_conn_info *conn )
{
    void *priv = fuse_get_context()->private_data;
    return new FSData( reinterpret_cast<FileList*>( priv ) );
}

extern "C" int lf_getattr( const char * path,
                           struct stat *stbuf )
{
    memset( stbuf, 0, sizeof( *stbuf ) );

    CompressedFile *file;
    if ( strcmp( path, "/" ) == 0 ) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 3;
    } else if ( ( file = fsdata()->files->find( path ) ) ) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = file->uncompressedSize();
    } else {
        return -ENOENT;
    }
    return 0;
}

struct DirFiller
{
    void *buf;
    fuse_fill_dir_t filler;

    DirFiller( void *          b,
               fuse_fill_dir_t f ) : buf( b ), filler( f ) { }
    void operator()( const std::string& path )
    {
        filler( buf, path.c_str() + 1, NULL, 0 );
    }

};

extern "C" int lf_readdir( const char *           path,
                           void *                 buf,
                           fuse_fill_dir_t        filler,
                           off_t                  offset,
                           struct fuse_file_info *fi )
{
    if ( strcmp( path, "/" ) != 0 ) {
        return -ENOENT;
    }

    filler( buf, ".", NULL, 0 );
    filler( buf, "..", NULL, 0 );
    fsdata()->files->forNames( DirFiller( buf, filler ) );
    return 0;
}

extern "C" int lf_open( const char *           path,
                        struct fuse_file_info *fi )
{
    CompressedFile *file;
    if ( !( file = fsdata()->files->find( path ) ) ) {
        return -ENOENT;
    }
    if ( ( fi->flags & O_ACCMODE ) != O_RDONLY ) {
        return -EACCES;
    }

    try {
        fi->fh = FuseFH( new OpenCompressedFile( file, fi->flags ) );
        return 0;
    } catch ( FileHandle::Exception& e ) {
        return e.error_code;
    }
}

extern "C" int lf_release( const char *           path,
                           struct fuse_file_info *fi )
{
    delete reinterpret_cast<OpenCompressedFile*>( fi->fh );
    fi->fh = 0;
    return 0;
}

extern "C" int lf_read( const char *           path,
                        char *                 buf,
                        size_t                 size,
                        off_t                  offset,
                        struct fuse_file_info *fi )
{
    int ret = -1;
    try {
        ret = reinterpret_cast<OpenCompressedFile*>( fi->fh )->read(
            fsdata()->cache, buf, size, offset );
    } catch ( std::runtime_error& e ) {
        fprintf( stderr, "%s: %s\n", typeid( e ).name(), e.what() );
        exit( 1 );
    }
    return ret;
}

typedef std::vector<std::string> paths_t;
struct OptData
{
    const char *nextSource;
    paths_t* files;

    unsigned gzipBlockFactor;
};

static struct fuse_opt lf_opts[] = {
    { "--gzip-block-factor=%lu", offsetof( OptData, gzipBlockFactor ), 0 },
    {NULL, -1U, 0},
};

extern "C" int lf_opt_proc( void *            data,
                            const char *      arg,
                            int               key,
                            struct fuse_args *outargs )
{
    OptData *optd = reinterpret_cast<OptData*>( data );
    if ( key == FUSE_OPT_KEY_NONOPT ) {
        if ( optd->nextSource ) {
            try {
                fprintf( stderr, "%s\n", optd->nextSource );
                optd->files->push_back( PathUtils::realpath( optd->nextSource ) );
            } catch ( std::runtime_error& e ) {
                fprintf( stderr, "%s: %s\n", typeid( e ).name(), e.what() );
                exit( 1 );
            }
        }
        optd->nextSource = arg;
        return 0;
    }
    return 1;
}

} // anon namespace

int main( int   argc,
          char *argv[] )
{
    /* just for convenience convert the arguments to a C++ vector of strings */
    std::vector<std::string> args;
    args.reserve( argc );
    /* ignore the first argument (the call path to this binary) */
    for ( int i = 1; i < argc; ++i ) {
        args.push_back( argv[i] );
    }

    if ( args.empty() || ( args[0] == "-h" ) || ( args[0] == "--help" ) ) {
        std::cout
            << "Version: 0.8.9\n"
            << "\n"
            << "This program will mount all specified compressed files into a specified folder.\n"
            << "The mounted files are not extracted but still can be accessed at random\n"
            << "positions without having to start extracting from the beginning!\n"
            /** @todo extract this information from FileList::OpenerList */
            << "Supported file formats are currently: lzo, bzip2, gzip, lzma (xz)\n"
            << "\n"
            << "Usage:\n"
            << "  lzopfs [options] [fuse-options] <file-to-mount> [<other-file-to-mount> [...]] <mount point>\n"
            << "\n"
            << "Options:\n"
            << "  -h|--help       Display this help message\n"
            << "  -H|--fuse-help  Display FUSE options help (for advanced users)\n";

        return 0;
    }

    struct fuse_operations ops;
    memset( &ops, 0, sizeof( ops ) );
    ops.getattr = lf_getattr;
    ops.readdir = lf_readdir;
    ops.open = lf_open;
    ops.release = lf_release;
    ops.read = lf_read;
    ops.init = lf_init;

    /* @todo might be better to just use the FUSE argument parsing?
     * @see https://github.com/libfuse/libfuse/wiki/Option-Parsing */
    if ( !args.empty() && ( ( args[0] == "-H" )||( args[0] == "--fuse-help" ) ) ) {
        struct fuse_args fuseArgs = FUSE_ARGS_INIT( 0, NULL );
        fuse_opt_add_arg( &fuseArgs, argv[0] );
        fuse_opt_add_arg( &fuseArgs, "--help" );
        return fuse_main( fuseArgs.argc, fuseArgs.argv, &ops, nullptr );
    }

    try {
        umask( 0 );

        paths_t files;
        OptData optd = { 0, &files, 0 };
        struct fuse_args args = FUSE_ARGS_INIT( argc, argv );
        fuse_opt_parse( &args, &optd, lf_opts, lf_opt_proc );
        if ( optd.nextSource ) {
            fuse_opt_add_arg( &args, optd.nextSource );
        }

        if ( optd.gzipBlockFactor ) {
            GzipFile::gMinDictBlockFactor = optd.gzipBlockFactor;
        }

        const auto flist = new FileList( CacheSize );
        for ( const auto& filePath : files ) {
            flist->add( filePath );
        }

        if ( flist->size() == 0 ) {
            std::cerr << "Could not open any of the specified files:\n";
            for ( const auto& filePath : files ) {
                std::cerr << "   " << filePath.c_str() << "\n";
            }
            std::cerr << "Will not mount FUSE system!\n";
            return 1;
        }

        std::cerr << "Ready\n";
        return fuse_main( args.argc, args.argv, &ops, flist );
    } catch ( std::runtime_error& e ) {
        fprintf( stderr, "%s: %s\n", typeid( e ).name(), e.what() );
        exit( 1 );
    }
}
