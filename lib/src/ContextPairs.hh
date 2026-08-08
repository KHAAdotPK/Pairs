/*
    lib/src/ContextPairs.hh
    Q@hackers.pk
 */

#ifndef CSV_CONTEXT_PAIRS_LIB_PAIRS_HH
#define CSV_CONTEXT_PAIRS_LIB_PAIRS_HH

#ifndef CONTEXT_WINDOW_SIZE
#define CONTEXT_WINDOW_SIZE 2 // Number of tokens to the left and the number of tokens to the right of the target/center token to include in the context of that center/target word/token   
#endif

struct ContextPair
{
    // All names ending on keys, should end at ids

    size_t* left_context_ids; // Array of ids for the left context tokens, these are displacement into index table
    size_t* right_context_ids; // Array of ids for the right context tokens, these are displacement into index table
    size_t  target_id;        // Id for the target/center token

    ContextPair() : left_context_ids{nullptr}, right_context_ids{nullptr}, target_id{0}
    {
    }
};

struct ContextPairs
{
    size_t       n;     // Number of context pairs
    ContextPair** pairs; // Array of context pairs  
    
    ContextPairs() : n{0}, pairs{nullptr}
    {
    }
};

#endif // CSV_CONTEXT_PAIRS_LIB_PAIRS_HH
