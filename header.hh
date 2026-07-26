/*
 * header.hh
 * Q@hackers.pk
 */

#ifndef PAIRS_HEADER_HH
#define PAIRS_HEADER_HH

/*
 * =========================================================================
 * PAIRS_PADDING_KEY & TRANSLATION DESIGN
 * =========================================================================
 * 
 * We use `((size_t)-1)` as the out-of-bounds/padding key sentinel value 
 * instead of std::numeric_limits because it is 100% compatible with both 
 * Host (CPU) and CUDA Device (GPU) compilation.
 * 
 * WHY A NON-ZERO PADDING SENTINEL IS USED AT THIS LAYER:
 * - Hash indices (keys) are stored in `WORDS->keys` arrays representing the 
 *   modulo-compressed indexes of the vocabulary hash table.
 * - Because `0` is a valid and occupied bucket index in the hash table, 
 *   using `0` as a padding indicator at this stage would lead to conflicts 
 *   with whatever word happens to hash to index 0.
 * 
 * DOWNSTREAM TRANSLATION LAYER:
 * - Before these pairs are used in model training, a translation step must 
 *   convert hash keys to continuous, 1-based Word IDs (1 to V).
 * - Since Word IDs are 1-based (i.e., starting from 1), Word ID `0` is free.
 * - The translation layer maps `PAIRS_PADDING_KEY` to Word ID `0`.
 * - On the GPU, Word ID `0` is mapped to a zero-vectored embedding row, 
 *   allowing branchless embedding lookups without warp divergence.
 */
#ifndef PAIRS_PADDING_KEY
#define PAIRS_PADDING_KEY ((size_t)-1) 
#endif

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