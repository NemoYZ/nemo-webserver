#pragma once

#include <boost/lexical_cast.hpp>

namespace nemo {

template<typename Target, typename Source>
inline Target LexicalCast(const Source& source) {
    return ::boost::lexical_cast<Target>(source);
}

} //namespace nemo