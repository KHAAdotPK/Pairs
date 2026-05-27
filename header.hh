/*
 * header.hh
 * Q@hackers.pk
 */

#ifndef PAIRS_HEADER_HH
#define PAIRS_HEADER_HH

/*
    This header file serves as a central inclusion point for all necessary
    headers related to the Pairs class and its dependencies. It includes
    the Serialisation header from the Corpus library, which defines the
    structures and functions needed for reading and writing corpus data,
    as well as the Pairs header itself, which defines the Pairs class.
    
    By including this header in source files that need to use the Pairs class,
    we ensure that all necessary declarations and definitions are available
    without needing to include multiple headers separately.
*/
//#include "./../Hash/header.hh"
#include "./../Parser/header.hh"
//#include "./../lib/Corpus/lib/src/Serialisation.hh"
//#include "./../lib/Parser/lib/src/Iterator.hh"
#include "lib/src/ContextPairs.hh"
#include "lib/src/Pairs.hh"

#endif // PAIRS_HEADER_HH