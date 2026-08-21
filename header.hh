/*
    lib/Pairs/header.hh

    This header file serves as a central inclusion point for all necessary
    headers related to the Pairs class and its dependencies. It includes
    the Serialisation header from the Corpus library, which defines the
    structures and functions needed for reading and writing corpus data,
    as well as the Pairs header itself, which defines the Pairs class.
    
    By including this header in source files that need to use the Pairs class,
    we ensure that all necessary declarations and definitions are available
    without needing to include multiple headers separately.

    Maintainer: Sohail.
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
 *   map hash keys to the parser's active vocabulary ID space, whose first valid
 *   entry is `TOKEN_ID_ORIGINATE_AT_VALUE` (default: 5), not 1.
 * - The reserved padding/sentinel slot remains `PARSER_PADDING_VALUE` (default: 0),
 *   and that value is translated to the zero-vectored embedding row downstream.
 * - The active vocabulary IDs occupy the range
 *     [TOKEN_ID_ORIGINATE_AT_VALUE, TOKEN_ID_ORIGINATE_AT_VALUE + bucket_used).
 * - On the GPU, the zero embedding row is still used for padding, but the parser's
 *   real vocabulary IDs are offset by `TOKEN_ID_ORIGINATE_AT_VALUE`.
 */
#ifdef PARSER_PADDING_VALUE
#define PAIRS_PADDING_KEY PARSER_PADDING_VALUE // Use the same padding value as defined in Parser/header.hh
#else
#define PAIRS_PADDING_KEY 0 // Fallback to 0 if PARSER_PADDING_VALUE is not defined
                            // In both cases, this is the index into the embedding table.
                            // This slot of the embedding table is initialized to all zeros and remains zero throughout training.
                            
/*
    This macro is intentionally left commented out because the current
    implementation uses compact word IDs for the left/right context arrays.

    In an earlier design, raw hash keys were stored in those arrays, and 0 was
    a valid hash-key value, so a distinct sentinel was needed. That earlier
    sentinel was defined as:

        #define PAIRS_PADDING_KEY ((size_t)-1)

    In the present design, the padding value is represented by 0 instead.
 */
//#define PAIRS_PADDING_KEY ((size_t)-1) // Sentinel used by the earlier hash-key-based design
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