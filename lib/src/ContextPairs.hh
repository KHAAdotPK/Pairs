/*
    lib/src/ContextPairs.hh

    Defines the ContextPairs and ContextPair structs, which are used to store the generated context-pairs for each line in the corpus.
    Each context-pair is defined by a target token and its left and right context tokens. 
    
    Maintainer: Sohail
 */

#ifndef CSV_CONTEXT_PAIRS_LIB_PAIRS_HH
#define CSV_CONTEXT_PAIRS_LIB_PAIRS_HH

#ifndef CONTEXT_WINDOW_SIZE
#define CONTEXT_WINDOW_SIZE 2 // Number of tokens to the left and the number of tokens to the right of the target/center token to include in the context of that center/target word/token   
#endif

/*
 * Represents a single training example built around one target token.
 *
 * Each ContextPair contains the target token id together with the left and
 * right context token ids for a fixed-size context window. The left and right
 * arrays are stored as contiguous size_t buffers, and each element is a compact
 * token id/index used downstream by the model pipeline.
 *
 * Ownership model:
 *   - this object owns the two context arrays
 *   - the arrays are allocated as heap buffers and must be released by the
 *     caller when the pair is no longer needed
 *   - target_id is a scalar value for the center/target token
 *
 * The naming intentionally uses "*_ids" because these values are not raw hash
 * keys or vocabulary strings; they are numeric ids that are later consumed by
 * training or embedding lookups.
 */
struct ContextPair
{
    // All names ending on keys, should end at ids

    size_t* left_context_ids;  // Array of ids for the left context tokens, used as offsets into the vocabulary/index table
    size_t* right_context_ids; // Array of ids for the right context tokens, used as offsets into the vocabulary/index table
    size_t  target_id;         // Id for the target/center token

    ContextPair() : left_context_ids{nullptr}, right_context_ids{nullptr}, target_id{0}
    {
    }
};

/*
 * Represents all context pairs generated for one line / sentence.
 *
 * A ContextPairs instance stores an array of ContextPair objects for one row in
 * the corpus. The total count is kept in n, and each logical pair is referenced
 * by a pointer in the pairs array.
 *
 * This container is used as the unit of serialization and training preparation:
 *   - one ContextPairs object corresponds to one line
 *   - each element inside pairs[] corresponds to one target word in that line
 *   - left_context_ids/right_context_ids encode the surrounding window for the
 *     target token
 *
 * Ownership model:
 *   - ContextPairs owns the pairs[] array itself
 *   - each element in pairs[] points to a ContextPair instance that owns its
 *     left/right context buffers
 *   - memory is expected to be cleaned up by the producer/consumer when the
 *     structure is discarded
 */
struct ContextPairs
{
    size_t       n;     // Number of context pairs in this line
    ContextPair** pairs; // Array of context pairs for the line

    ContextPairs() : n{0}, pairs{nullptr}
    {
    }
};

// ContextPairsHeader
// ------------------
// File header written once at the start of a serialized context-pairs file.
//
// This header allows the reader to reconstruct the in-memory pair table without
// needing any other metadata or external dependency. The loader reads this
// header first, obtains the number of saved entries (nol) and the context
// window size (cws), and then reads the remaining payload to rebuild the
// ContextPairs structure directly.
//
// Layout:
//   1) ContextPairsHeader  -> metadata for the serialized stream
//   2) ContextPair[]       -> the saved pair records for each line/entry
//
// This makes the on-disk format self-describing and enables simple, standalone
// persistence/loading logic for pair data.
struct ContextPairsHeader
{
    size_t nol; // Number of lines / number of saved context-pair entries
    size_t cws; // Context window size used when generating the pairs
};
typedef ContextPairsHeader PAIRS_FILE_HEADER;


#endif // CSV_CONTEXT_PAIRS_LIB_PAIRS_HH
