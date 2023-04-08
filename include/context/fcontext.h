#pragma once

#include <stddef.h>
#include <stdint.h>

namespace nemo {

extern "C" {

struct stack_t {
    void* sp;
    size_t size;

    stack_t() :
        sp(0), size(0) {
    }
};

struct fp_t {
    uint32_t fc_freg[2];

    fp_t() :
        fc_freg() {
    }
};

struct fcontext_t {
    uint64_t fc_greg[8];
    stack_t fc_stack;
    fp_t fc_fp;

    fcontext_t() :
        fc_greg(),
        fc_stack(),
        fc_fp() {
    }
};

intptr_t jump_fcontext(fcontext_t* ofc, fcontext_t const* nfc, intptr_t vp, bool preserve_fpu = true);
fcontext_t * make_fcontext(void * sp, size_t size, void (*fn)(intptr_t));

} //extern "C"

} //namespace nemo