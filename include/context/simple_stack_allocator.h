#pragma once

#include <stddef.h>

#include "common/macro.h"

namespace nemo {

template<size_t Max, size_t Default, size_t Min>
class SimpleStackAllocator {
public:
    constexpr static size_t MaximumStacksize() { 
        return Max; 
    }

    constexpr static size_t DefaultStacksize() { 
        return Default; 
    }

    constexpr static size_t MinimumStacksize() { 
        return Min; 
    }

    static void* Allocate(size_t size) {
        NEMO_ASSERT(MinimumStacksize() <= size);
        NEMO_ASSERT(MaximumStacksize() >= size);

        void* limit = ::malloc(size * sizeof(char)) ;
        if (!limit) {
            throw std::bad_alloc();
        }

        return static_cast<char*>(limit) + size;
    }

    static void* Allocate() {
        return Allocate(Default);
    }

    static void Deallocate(void* vp, size_t size) {
        NEMO_ASSERT(vp);
        NEMO_ASSERT(MinimumStacksize() <= size);
        NEMO_ASSERT(MaximumStacksize() >= size);

        void* limit = static_cast<char*>(vp) - size;
        ::free(limit);
    }

    static void Deallocate(void* vp) {
        Deallocate(vp, Default);
    }
};

} //namespace nemo