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
    size_t* left_context_keys; // Array of keys for the left context tokens
    size_t* right_context_keys; // Array of keys for the right context tokens
    size_t  target_key;        // Key for the target/center token

    ContextPair() : left_context_keys{nullptr}, right_context_keys{nullptr}, target_key{0}
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
