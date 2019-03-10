#pragma once

#include <stdint.h>


struct Block
{
    uint32_t usize, csize;
    uint64_t uoff, coff;

    Block( uint32_t us = 0,
           uint32_t cs = 0,
           uint64_t uo = 0,
           uint64_t co = 0 ) :
        usize( us ),
        csize( cs ),
        uoff( uo ),
        coff( co ) { }

    virtual ~Block() { }
};
