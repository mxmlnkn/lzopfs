#pragma once

#include <cstdio>
#include <iostream>
#include <sstream>


#if defined( _MSC_VER )
    #define AVAILABLE_FUNCTION __FUNCSIG__
#elif defined( __GNUC__ )
    #define AVAILABLE_FUNCTION __PRETTY_FUNCTION__
#else
    #define AVAILABLE_FUNCTION __func__
#endif

#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

#ifndef NDEBUG
    #define DEBUG( fmt, ... ) fprintf( stderr, fmt "\n", ## __VA_ARGS__ )
#else
    #define DEBUG( fmt, ... ) (void)0;
#endif

#ifndef NDEBUG
    class DummyDebugOutStream
    {
    public:
        template<class T>
        const DummyDebugOutStream&
        operator<< ( const T& ) const
        {
            return *this;
        };
    };

    extern DummyDebugOutStream dummyDebugOutStream;

    #define DOUT ::dummyDebugOutStream
#else
    #define DOUT std::cerr
#endif
