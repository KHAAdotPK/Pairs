/*
    lib/src/Pairs.hh
    Q@hackers.pk
 */

#ifndef CSV_PAIRS_LIB_PAIRS_HH 
#define CSV_PAIRS_LIB_PAIRS_HH

class Pairs
{
    std::string _ifile_name;
    std::ifstream _ifile;
    bool _is_open;

  public:

    void reset(void)
    {
        if (_is_open)
        {
             _ifile.clear(); // Clear any error flags
             _ifile.seekg(0); // Move to the beginning of the file            
        }
    }

    void close(void)
    {
        if (_is_open)
        {
            _ifile.close();
            _is_open = false;
        }
    }

    explicit Pairs() : _ifile_name(""), _ifile(), _is_open(false)
    {        
    }

    explicit Pairs(const std::string& iname) : _ifile_name(iname), _is_open(false)
    {
        _ifile.open(_ifile_name, std::ios::in | std::ios::binary);
        if (!_ifile)
        {
            throw std::runtime_error("Pairs::Pairs(const std::string&) Error: failed to open file for reading"); 
        }

        _is_open = true; 
    }

    /*
     * Build lines using a doubly-linked list of lines and tokens.
     * 
     * NOTE:
     * This method is deprecated/replaced by Parser::build_lines_table().
     * 
     * DESIGN DRAWBACKS (Why this is replaced):
     * - Requires O(N_lines + N_tokens) separate heap allocations, causing heap fragmentation.
     * - High memory overhead due to next/prev pointers in LINES_NEW and TOKEN_NEW.
     * - Poor CPU cache locality since list nodes are not guaranteed to be contiguous in memory,
     *   leading to frequent cache misses during iterations.
     * 
     * @param parser Reference to the Parser object.
     * @param hash_table The vocabulary hash table.
     * @return Head pointer of the doubly-linked list of lines.
     */
    LINES_NEW* build_lines(Parser& parser, const WordRecord_new* const *const hash_table)
    {
        size_t key = 0, probe = 0;

        LINES_NEW* lines_head = nullptr;
        LINES_NEW* lines_tail = nullptr;
        
        TOKEN_NEW* token_tail = nullptr;

        for (auto& line: *this)
        {
            if (lines_head == nullptr)
            {
                lines_head = new LINES_NEW();
                lines_tail = lines_head;                
            }
            else
            {
                lines_tail->next = new LINES_NEW();
                lines_tail->next->prev = lines_tail;
                lines_tail = lines_tail->next;
            }
            
            for (auto& token: line)
            {
                if (token.empty()) // Skip empty tokens
                {
                    continue;
                }

                key = Keys::generate_key(token, parser.get_bucket_count());

                if (hash_table[key] == nullptr) // Token is not present in hash table, skip
                {
                    continue;
                }

                if (hash_table[key]->get_word() == token) // Token is present in hash table, insert into pairs
                {
                    if (token_tail == nullptr)
                    {
                        lines_tail->tokens = new TOKEN_NEW();
                        token_tail = lines_tail->tokens;
                    }
                    else
                    {
                        token_tail->next = new TOKEN_NEW();
                        token_tail->next->prev = token_tail;
                        token_tail = token_tail->next;
                    }
                    
                    token_tail->key = key;
                    lines_tail->n++; // Increment the token count for this line                    

                    continue;
                }

                // Collision resolution using linear probing
                probe = (key + 1) % parser.get_bucket_count();
                
                while (probe != key)
                {
                    if (hash_table[probe] == nullptr) // token is not in the hash table, skip
                    {
                        break;
                    }

                    if (hash_table[probe]->get_word() == token) // Token is in the hash table
                    {
                        if (token_tail == nullptr)
                        {
                            lines_tail->tokens = new TOKEN_NEW();
                            token_tail = lines_tail->tokens;
                        }
                        else
                        {
                            token_tail->next = new TOKEN_NEW();
                            token_tail->next->prev = token_tail;
                            token_tail = token_tail->next;
                        }

                        token_tail->key = probe; // Store the actual key where the token was found after probing
                        lines_tail->n++; // Increment the token count for this line

                        break;
                    }

                    // Move to the next bucket
                    probe = (probe + 1) % parser.get_bucket_count(); // linear probing  
                }
            }

            token_tail = nullptr;
        }                            

        return lines_head;;
    }

    /*
     * Build target-context word pairs from the given line array.
     * 
     * =========================================================================
     * PADDING & CONTEXT HANDLING DESIGN
     * =========================================================================
     * 
     * 1. THE HASH KEY INDEX PROBLEM & PAIRS_PADDING_KEY:
     *    During parsing, tokens are hashed into a vocabulary hash table of size 
     *    `bucket_count`. A token's hash index (key) is generated using modulo 
     *    arithmetic, which means '0' is a completely valid and occupied index in 
     *    the hash table. 
     *    Consequently, we CANNOT use '0' to denote out-of-bounds/padding slots for 
     *    context words at the beginning or end of lines. Doing so would conflate 
     *    padding with whatever vocabulary word happens to hash to index 0.
     *    
     *    To solve this, we pad out-of-bounds context slots with `PAIRS_PADDING_KEY` 
     *    which is defined as `((size_t)-1)` (the maximum value of `size_t`). This is 
     *    guaranteed to never conflict with any valid hash table index.
     * 
     * 2. THE TRANSLATION LAYER (HASH INDEX -> WORD ID):
     *    Downstream training loops (especially on the GPU/CUDA) require contiguous, 
     *    compact token identifiers (Word IDs) ranging from 1 to V (vocabulary size) 
     *    rather than sparse, scattered hash indices.
     *    
     *    Therefore, before training, these pairs undergo a translation phase where:
     *      - Any valid hash index is mapped to its unique 1-based `word_id` 
     *        (via `WordRecord->get_word_id()`).
     *      - The `PAIRS_PADDING_KEY` sentinel is safely mapped to `0`.
     * 
     * 3. WHY WORD ID ORIGINS AT 1 & GPU EMBEDDING ZERO-PADDING:
     *    Because `word_id`s originate at 1 (using `TOKEN_ID_ORIGINATE_AT_VALUE = 1`), 
     *    index `0` is completely free to serve as the official padding token ID.
     *    
     *    In GPU embedding layers, index `0` is assigned a "0-vectored" embedding row 
     *    (all weights in this embedding row are initialized and kept at 0.0). 
     *    This allows the GPU to run branchless lookups: we don't need conditional 
     *    branching (`if (id != pad)`) to mask out padding tokens. The GPU can blindly 
     *    fetch and accumulate embedding vectors for all tokens (including padding), 
     *    and the zero-vector at index 0 will naturally add nothing to the projection, 
     *    avoiding warp divergence and ensuring optimal execution speed.
     * 
     * @param parser Reference to the Parser object.
     * @param lines_array Contiguous array of WORDS pointers containing line token keys.
     * @return Array of pointers to ContextPairs for each line.
     */
    ContextPairs** build_pairs(Parser& parser, WORDS** lines_array)
    {
        /*
            Pointer to array of pointers to ContextPairs structures
            Each instance of ContextPairs structure has internal array which holds the instances of struct ContextPair             
         */
        struct ContextPairs** contexts = nullptr;

        try
        {
            contexts = new ContextPairs*[parser.get_nol()];            
        }
        catch (const std::bad_alloc& e)
        {
            throw std::runtime_error("Pairs::build_pairs((Parser&, WORDS**) Error: failed to allocate memory for contexts.");
        }
        
        for (size_t i = 0; i < parser.get_nol(); i++)
        {            
            // This pointer will hold all context pairs of single line
            struct ContextPairs* context = nullptr;

            // Pointer to current line's word hash keys
            WORDS* line = lines_array[i];
            
            try
            {
                context = new ContextPairs();
            }
            catch (const std::bad_alloc& e)
            {
                throw std::runtime_error("Pairs::build_pairs((Parser&, WORDS**) Error: failed to allocate memory for context_pairs of single line.");
            }

            context->n = lines_array[i]->n;

            try
            {
                /*
                    Number of pairs = number of tokens
                 */
                context->pairs = new ContextPair*[context->n];
            }
            catch (const std::bad_alloc& e)
            {
                throw std::runtime_error("Pairs::build_pairs((Parser&, WORDS**) Error: failed to allocate memory for context_pairs array of single line.");
            }
            
            // Add the context of single line to the array of contexts
            contexts[i] = context;

            // Iterate over all tokens in the current line
            for (size_t j = 0; j < context->n; j++)
            {
                // This pointer will hold context pair for single token
                struct ContextPair* pair = nullptr;

                try
                {
                    pair = new ContextPair();

                    // Add the context pair for single token to the array of context pairs
                    context->pairs[j] = pair;
                }
                catch (const std::bad_alloc& e)
                {
                    throw std::runtime_error("Pairs::build_pairs((Parser&, WORDS**) Error: failed to allocate memory for context pair of single token.");
                }
                
                try
                {
                    pair->left_context_keys = new size_t[CONTEXT_WINDOW_SIZE];
                    pair->right_context_keys = new size_t[CONTEXT_WINDOW_SIZE];
                }
                catch (const std::bad_alloc& e)
                {
                    throw std::runtime_error("Pairs::build_pairs((Parser&, WORDS**) Error: failed to allocate memory for left/right context arrays of single token.");
                }
                
                pair->target_key = line->keys[j]; // Set the key for the target/center token

                /*for (size_t k = CONTEXT_WINDOW_SIZE - 1; k >= 0; k--)
                {                    
                    if (k < j)
                    {                    
                        //pair->left_context_keys[k] = line->keys[j - k];
                    }
                    else
                    {
                        //pair->left_context_keys[k] = 0;                        
                    }                    
                }*/

                /*
                    pair->target_key = target_token->key; // Set target key in pair
 
                    for (size_t j = 0; j < CONTEXT_WINDOW_SIZE && target_token->prev != nullptr; j++)
                    {
                        pair->left_context_keys[CONTEXT_WINDOW_SIZE - 1 - j] = target_token->prev->key;
                        target_token = target_token->prev;
                }

                */

                // Left Context Keys
                /*
                    How this loop works:
                        - We want to fill the left_context_keys array from right to left with the keys of the tokens to the left of the target/center token
                        - The first element of the left_context_keys array will be the key of the first token/word to the left of the target/center token
                        - The second element of the left_context_keys array will be the key of the token/word to the left of the token/word to the left of the target/center token
                        - And so 
                        
                    Out-of-bounds/padding indices are set to PAIRS_PADDING_KEY ((size_t)-1) because 0 is a valid hash key index.
                    - WARNING: Downstream CPU/GPU code must check for PADDING_KEY before accessing 
                      arrays or embedding tables to prevent Out-of-Bounds crashes    
                 */
                for (size_t k = 0; k < CONTEXT_WINDOW_SIZE; k++)
                {
                    if (k < j)
                    {
                        pair->left_context_keys[CONTEXT_WINDOW_SIZE - 1 - k] = line->keys[j - k - 1];
                    }
                    else
                    {
                        pair->left_context_keys[CONTEXT_WINDOW_SIZE - 1 - k] = PAIRS_PADDING_KEY; // 0 is a valid hash key index
                    }
                }

                // Right Context Keys
                /*
                    How this loop works:
                        - We want to fill the right_context_keys array from left to right with the keys of the tokens to the right of the target/center token
                        - The first element of the right_context_keys array will be the key of the first token/word to the right of the target/center token
                        - The second element of the right_context_keys array will be the key of the token/word to the right of the token/word to the right of the target/center token
                        - And so on

                    Out-of-bounds/padding indices are set to PAIRS_PADDING_KEY ((size_t)-1) because 0 is a valid hash key index.
                    - WARNING: Downstream CPU/GPU code must check for PADDING_KEY before accessing 
                      arrays or embedding tables to prevent Out-of-Bounds crashes       
                 */
                for (size_t k = 0; k < CONTEXT_WINDOW_SIZE; k++)
                {
                    if ((j + k + 1) < context->n)
                    {
                        pair->right_context_keys[k] = line->keys[j + k + 1];
                    }
                    else
                    {
                        pair->right_context_keys[k] = PAIRS_PADDING_KEY; // 0 is a valid hash key index
                    }
                }
                
                /*size_t k = CONTEXT_WINDOW_SIZE - 1;
                while (1)
                {                    
                    if (k < (j - 1))
                    {
                        pair->left_context_keys[k] = line->keys[j - k - 1];
                    }
                    else
                    {
                        pair->left_context_keys[k] = 0;
                    }

                    if (k == 0)
                    {
                        break;
                    }

                    k--;
                }*/
            }
        }
        
        return contexts;
    }
    
    void build_pairs_old(Parser& parser, WORDS** lines_array)
    {
        /*for (size_t i = 0; i < parser.get_nol(); i++)
        {
            WORDS* ptr = lines_array[i];

            std::cout<< "n = " << ptr->n << ", ";

            for (size_t j = 0; j < ptr->n; j++)
            {
                std::cout<< ptr->keys[j] << " ";
            }
            std::cout<< std::endl;
        }*/
       
        /*
            Pointer to array of pointers to ContextPairs structures
            Each instance of ContextPairs structure has internal array which holds the instances of struct ContextPair             
         */
        struct ContextPairs** contexts = nullptr;
        try
        {
            contexts = new ContextPairs*[parser.get_nol()];
        }
        catch (const std::bad_alloc& e)
        {
            throw std::runtime_error("Pairs::build_pairs((Parser&, WORDS**) Error: failed to allocate memory for contexts.");
        }
                
        for (size_t i = 0; i < parser.get_nol(); i++)
        {
            WORDS* line = lines_array[i];

            // Number of context pairs in one line is equal to the number of tokens in the line
            //contexts[i]->n = line->n;

            std::cout<< "Hello World\n";

            for (size_t j = 0; j < line->n; j++)
            {
                struct ContextPair* pair = nullptr;
                try
                {
                    pair = new ContextPair();
                }
                catch (const std::bad_alloc& e)
                {
                    throw std::runtime_error("Pairs::build_pairs((Parser&, WORDS**) Error: failed to allocate memory for pair.");
                }

                try 
                {
                    pair->left_context_keys = new size_t[CONTEXT_WINDOW_SIZE]();
                    pair->right_context_keys = new size_t[CONTEXT_WINDOW_SIZE]();
                }
                catch (const std::bad_alloc& e)
                {
                    throw std::runtime_error("Pairs::build_pairs((Parser&, WORDS**) Error: failed to allocate memory for context pair keys.");
                }

                contexts[i]->pairs[j] = pair; // Store the pointer to the pair in the array of pairs

                pair->target_key = line->keys[j]; // Set target key in pair     
                
                // Left context keys
                for (size_t k = CONTEXT_WINDOW_SIZE - 1; k >= 0 && j > 0; k--)
                {
                    std::cout<< k << ", ";

                    /*if (k <= (j - 1))
                    {
                        pair->left_context_keys[k] = line->keys[j - k - 1];  
                    }*/
                    /*else
                    {
                        pair->left_context_keys[k] = 0; // Padding for out of bounds
                    }*/
                }
                std::cout<< std::endl;
            }
        }
    }

    struct ContextPairs** build_pairs(Parser& parser, const LINES_NEW* const lines, const WordRecord_new* const *const hash_table)
    {
        /*
            Pointer to array of pointers to ContextPairs structures
            Each instance of ContextPairs structure has internal array which holds the instances of struct ContextPair             
         */
        struct ContextPairs** contexts = new ContextPairs*[parser.get_nol()];

        const LINES_NEW* lines_tail = lines;

        size_t j = 0; 

        while (lines_tail != nullptr) // For each line in the corpus
        {
            struct ContextPairs* context_pairs = new ContextPairs(); // Allocate a ContextPairs struct for this line

            context_pairs->n = lines_tail->n; // Number of tokens in the line is the number of context pairs for that line
            context_pairs->pairs = new ContextPair*[context_pairs->n]; // Allocate an array of pointers to ContextPair structs for each word/token of this line

            size_t i = 0;

            TOKEN_NEW* tokens_tail = lines_tail->tokens;
                                    
            while (tokens_tail != nullptr)
            {
                struct ContextPair* pair = new ContextPair();
                pair->left_context_keys = new size_t[CONTEXT_WINDOW_SIZE]();
                pair->right_context_keys = new size_t[CONTEXT_WINDOW_SIZE]();

                context_pairs->pairs[i] = pair;

                TOKEN_NEW* target_token = tokens_tail;

                pair->target_key = target_token->key; // Set target key in pair
 
                for (size_t j = 0; j < CONTEXT_WINDOW_SIZE && target_token->prev != nullptr; j++)
                {
                    pair->left_context_keys[CONTEXT_WINDOW_SIZE - 1 - j] = target_token->prev->key;
                    target_token = target_token->prev;
                }

                // Reset target_token to the original token
                target_token = tokens_tail;

                //std::cout<< i << std::endl;

                // Move to right
                for (size_t j = 0; j < CONTEXT_WINDOW_SIZE && target_token->next != nullptr; j++)
                {
                    pair->right_context_keys[j] = target_token->next->key;
                    target_token = target_token->next;
                }
                            
                i = i + 1;
                tokens_tail = tokens_tail->next; // Start from the first token in the line                
            }            

            contexts[j] = context_pairs;                 
            lines_tail = lines_tail->next;

            j = j + 1; 
        }

        return contexts;
    }
    
    // Iterator access
    Iterator begin()
    { 
        return Iterator(&_ifile);
    }
    Iterator end()
    { 
        return Iterator();
    }

    // Utility
    bool is_open() const
    { 
        return _is_open;
    }
    const std::string& ifilename() const
    { 
        return _ifile_name;
    }

};
#endif // CSV_PAIRS_LIB_PAIRS_HH