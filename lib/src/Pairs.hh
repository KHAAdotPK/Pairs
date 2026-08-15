/*
    lib/src/Pairs.hh
    
    Declaration of the Pairs class, which is responsible for reading and processing pairs of words from a file.

    Maintainer: Sohail.
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
     * Serializes the generated context-pair data to a binary file.
     *
     * The on-disk format is intentionally simple and self-describing:
     *   1) header.nol   -> number of lines in the corpus
     *   2) header.cws   -> context window size used while creating pairs
     *   3) for each line: number of pairs in that line
     *   4) for each pair: target_id, left_context_ids[cws], right_context_ids[cws]
     *
     * The method validates the input before writing so that a corrupted or
     * partially initialized ContextPairs graph cannot silently produce a bad
     * binary payload. If a null pointer, invalid record, or write failure is
     * detected, it throws std::runtime_error and aborts the save operation.
     *
     * @param contexts Pointer array of ContextPairs produced for each line.
     * @param parser Parser metadata used to determine the total line count.
     * @param ofile_name Output file path.
     *
     * @throws std::runtime_error If the file cannot be opened, the in-memory
     *         structure is incomplete, or the stream fails while writing.
     */
    void save_pairs(const ContextPairs* const* contexts, const Parser& parser, const std::string& ofile_name)
    {
        if (contexts == nullptr && parser.get_nol() != 0)
        {
            throw std::runtime_error("Pairs::save_pairs(const ContextPairs* const*, const Parser&, const std::string&) Error: contexts pointer is null while parser reports non-zero line count");
        }

        const size_t expected_nol = parser.get_nol();
        const size_t expected_cws = CONTEXT_WINDOW_SIZE;

        std::ofstream ofile(ofile_name, std::ios::out | std::ios::binary);
        if (!ofile.is_open())
        {
            throw std::runtime_error("Pairs::save_pairs(const ContextPairs* const*, const Parser&, const std::string&) Error: failed to open file for writing");
        }

        const PAIRS_FILE_HEADER header = { expected_nol, expected_cws };

        // Write header
        ofile.write(reinterpret_cast<const char*>(&header.nol), sizeof(header.nol));
        ofile.write(reinterpret_cast<const char*>(&header.cws), sizeof(header.cws));

        for (size_t i = 0; i < expected_nol; i++)
        {
            if (contexts[i] == nullptr)
            {
                throw std::runtime_error(std::string("Pairs::save_pairs(const ContextPairs* const*, const Parser&, const std::string&) Error: null ContextPairs at contexts[") + std::to_string(i) + "]");
            }

            // Write number of pairs for the line before serializing them.
            const size_t line_pair_count = contexts[i]->n;
            ofile.write(reinterpret_cast<const char*>(&line_pair_count), sizeof(size_t));

            for (size_t j = 0; j < line_pair_count; j++)
            {
                const ContextPair* pair = contexts[i]->pairs[j];
                if (pair == nullptr)
                {
                    throw std::runtime_error(std::string("Pairs::save_pairs(const ContextPairs* const*, const Parser&, const std::string&) Error: null ContextPair at contexts[") + std::to_string(i) + "]->pairs[" + std::to_string(j) + "]");
                }

                if (pair->left_context_ids == nullptr)
                {
                    throw std::runtime_error(std::string("Pairs::save_pairs(const ContextPairs* const*, const Parser&, const std::string&) Error: null left_context_ids at contexts[") + std::to_string(i) + "]->pairs[" + std::to_string(j) + "]");
                }

                if (pair->right_context_ids == nullptr)
                {
                    throw std::runtime_error(std::string("Pairs::save_pairs(const ContextPairs* const*, const Parser&, const std::string&) Error: null right_context_ids at contexts[") + std::to_string(i) + "]->pairs[" + std::to_string(j) + "]");
                }

                ofile.write(reinterpret_cast<const char*>(&pair->target_id), sizeof(size_t));
                ofile.write(reinterpret_cast<const char*>(pair->left_context_ids), sizeof(size_t) * expected_cws);
                ofile.write(reinterpret_cast<const char*>(pair->right_context_ids), sizeof(size_t) * expected_cws);
            }
        }

        if (!ofile)
        {
            throw std::runtime_error("Pairs::save_pairs(const ContextPairs* const*, const Parser&, const std::string&) Error: write operation failed before completion");
        }

        ofile.flush();
        if (!ofile)
        {
            throw std::runtime_error("Pairs::save_pairs(const ContextPairs* const*, const Parser&, const std::string&) Error: flush failed after writing pairs data");
        }

        ofile.close();
    }

    /*
     * Loads a previously saved binary pair table and reconstructs the in-memory
     * ContextPairs structure.
     *
     * The loader expects the exact file layout written by save_pairs(). It reads
     * the file header first, validates the stored context window size, then
     * reads each line's pair count and every target/context vector. Any missing
     * or malformed data causes an exception and cleans up any partially created
     * memory before propagating the error.
     *
     * This method is designed to fail safely: a corrupt or mismatched file does
     * not leave the caller with a half-initialized pair table. It returns a
     * fully allocated ContextPairs** only when the file has been read and
     * validated completely.
     *
     * @param ifile_name Input binary file path.
     * @return Newly allocated ContextPairs array equivalent to the serialized data.
     *
     * @throws std::runtime_error If the file cannot be opened, the header is
     *         invalid, the context window does not match the current build, or a
     *         record is truncated or malformed.
     */
    ContextPairs** load_pairs(const std::string& ifile_name)
    {
        std::ifstream ifile(ifile_name, std::ios::in | std::ios::binary);
        if (!ifile.is_open())
        {
            throw std::runtime_error("Pairs::load_pairs(const std::string&) Error: failed to open file for reading");
        }

        PAIRS_FILE_HEADER header = {0, 0};

        if (!ifile.read(reinterpret_cast<char*>(&header.nol), sizeof(header.nol)))
        {
            throw std::runtime_error("Pairs::load_pairs(const std::string&) Error: failed to read file header (nol)");
        }

        if (!ifile.read(reinterpret_cast<char*>(&header.cws), sizeof(header.cws)))
        {
            throw std::runtime_error("Pairs::load_pairs(const std::string&) Error: failed to read file header (cws)");
        }

        if (header.nol == 0 && header.cws == 0)
        {
            throw std::runtime_error("Pairs::load_pairs(const std::string&) Error: invalid empty file header");
        }

        if (header.cws != CONTEXT_WINDOW_SIZE)
        {
            throw std::runtime_error(std::string("Pairs::load_pairs(const std::string&) Error: saved context window size mismatch: file cws=") + std::to_string(header.cws) + ", expected cws=" + std::to_string(CONTEXT_WINDOW_SIZE));
        }

        ContextPairs** contexts = nullptr;

        try
        {
            contexts = new ContextPairs*[header.nol];
            for (size_t i = 0; i < header.nol; i++)
            {
                contexts[i] = new ContextPairs();
                ContextPairs* context = contexts[i];

                if (!ifile.read(reinterpret_cast<char*>(&context->n), sizeof(context->n)))
                {
                    throw std::runtime_error(std::string("Pairs::load_pairs(const std::string&) Error: failed to read line pair count at index ") + std::to_string(i));
                }

                context->pairs = nullptr;
                if (context->n > 0)
                {
                    context->pairs = new ContextPair*[context->n];
                    for (size_t j = 0; j < context->n; j++)
                    {
                        ContextPair* pair = new ContextPair();
                        pair->left_context_ids = new size_t[header.cws];
                        pair->right_context_ids = new size_t[header.cws];

                        if (!ifile.read(reinterpret_cast<char*>(&pair->target_id), sizeof(pair->target_id)))
                        {
                            delete pair->left_context_ids;
                            delete pair->right_context_ids;
                            delete pair;
                            throw std::runtime_error(std::string("Pairs::load_pairs(const std::string&) Error: failed to read target_id at line ") + std::to_string(i) + ", pair " + std::to_string(j));
                        }

                        if (!ifile.read(reinterpret_cast<char*>(pair->left_context_ids), sizeof(size_t) * header.cws))
                        {
                            delete pair->left_context_ids;
                            delete pair->right_context_ids;
                            delete pair;
                            throw std::runtime_error(std::string("Pairs::load_pairs(const std::string&) Error: failed to read left context at line ") + std::to_string(i) + ", pair " + std::to_string(j));
                        }

                        if (!ifile.read(reinterpret_cast<char*>(pair->right_context_ids), sizeof(size_t) * header.cws))
                        {
                            delete pair->left_context_ids;
                            delete pair->right_context_ids;
                            delete pair;
                            throw std::runtime_error(std::string("Pairs::load_pairs(const std::string&) Error: failed to read right context at line ") + std::to_string(i) + ", pair " + std::to_string(j));
                        }

                        context->pairs[j] = pair;
                    }
                }
            }
        }
        catch (...)
        {
            for (size_t i = 0; i < header.nol; i++)
            {
                if (contexts == nullptr || contexts[i] == nullptr)
                {
                    continue;
                }

                if (contexts[i]->pairs != nullptr)
                {
                    for (size_t j = 0; j < contexts[i]->n; j++)
                    {
                        if (contexts[i]->pairs[j] == nullptr)
                        {
                            continue;
                        }

                        delete[] contexts[i]->pairs[j]->left_context_ids;
                        delete[] contexts[i]->pairs[j]->right_context_ids;
                        delete contexts[i]->pairs[j];
                    }
                    delete[] contexts[i]->pairs;
                }

                delete contexts[i];
            }

            delete[] contexts;
            throw;
        }

        return contexts;
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
     * Build target-context word pairs from the given line array using the provided vocabulary hash table.
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
     * This overload uses the supplied vocabulary hash table to resolve each token's
     * hash key into a compact word ID before filling the left/right context arrays.
     * It is intended for the newer parser pipeline where token keys are already
     * materialized in the line structures and need to be translated into training-ready
     * context pairs.
     *
     * @param parser Reference to the Parser object.
     * @param lines_array Contiguous array of WORDS pointers containing line token keys.
     * @param hash_table Vocabulary hash table used to resolve hash keys into word IDs.
     * @return Array of pointers to ContextPairs for each line.
     */
    ContextPairs** build_pairs(Parser& parser, WORDS** lines_array, WordRecord_new** hash_table)
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
                    pair->left_context_ids = new size_t[CONTEXT_WINDOW_SIZE];
                    pair->right_context_ids = new size_t[CONTEXT_WINDOW_SIZE];
                }
                catch (const std::bad_alloc& e)
                {
                    throw std::runtime_error("Pairs::build_pairs((Parser&, WORDS**) Error: failed to allocate memory for left/right context arrays of single token.");
                }
                
                //pair->target_key = line->keys[j]; // Set the key for the target/center token

#ifndef MAX_VOCAB_SIZE                
                pair->target_id = hash_table[line->keys[j]]->get_word_id(); // Translate hash key to compact word ID for training
#else
                pair->target_id = (hash_table[line->keys[j]]->get_word_id() <= MAX_VOCAB_SIZE) ? hash_table[line->keys[j]]->get_word_id() : PARSER_UNKNOWN_VALUE;
#endif                

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
                        size_t key = line->keys[j - k - 1];
                        size_t word_id = hash_table[key]->get_word_id(); // Translate hash key to compact word ID for training
#ifndef  MAX_VOCAB_SIZE                                               
                        pair->left_context_keys[CONTEXT_WINDOW_SIZE - 1 - k] = word_id; 
#else                   
                        if (word_id <= MAX_VOCAB_SIZE) // Ensure the word ID is within the valid range
                        {
                            pair->left_context_ids[CONTEXT_WINDOW_SIZE - 1 - k] = word_id; 
                        }
                        else
                        {
                            pair->left_context_ids[CONTEXT_WINDOW_SIZE - 1 - k] = PARSER_UNKNOWN_VALUE; // Set to 1 for unknown words, as 1 is the reserved index for unknown words in the embedding table. Downstream code should handle this appropriately.
                        }                                                
#endif
                        //pair->left_context_ids[CONTEXT_WINDOW_SIZE - 1 - k] = line->keys[j - k - 1];
                    }
                    else
                    {
                        pair->left_context_ids[CONTEXT_WINDOW_SIZE - 1 - k] = /*0*/ PAIRS_PADDING_KEY; // Set to 0 for padding, as 0 is a valid hash key index. Downstream code should handle this appropriately.
                                                                                  // This should not be hard coded to 0, but rather use a defined constant for padding, e.g., PAIRS_PADDING_KEY.

                        //pair->left_context_ids[CONTEXT_WINDOW_SIZE - 1 - k] = PAIRS_PADDING_KEY; // 0 is a valid hash key index
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
                        size_t key = line->keys[j + k + 1];
                        size_t word_id = hash_table[key]->get_word_id(); // Translate hash key to compact word ID for training
                        //pair->right_context_keys[k] = hash_table[key]->get_word_id();

                        //pair->right_context_keys[k] = line->keys[j + k + 1];
#ifndef  MAX_VOCAB_SIZE 
                        pair->right_context_keys[k] = word_id; // No MAX_VOCAB_SIZE check, just assign the word ID directly
                        //pair->right_context_keys[k] = (word_id <= MAX_VOCAB_SIZE) ? word_id : PARSER_UNKNOWN_VALUE; // Set to 1 for unknown words, as 1 is the reserved index for unknown words in the embedding table. Downstream code should handle this appropriately.            
#else
                        pair->right_context_ids[k] = (word_id <= MAX_VOCAB_SIZE) ? word_id : PARSER_UNKNOWN_VALUE; // Set to 1 for unknown words, as 1 is the reserved index for unknown words in the embedding table. Downstream code should handle this appropriately.
                        //pair->right_context_keys[k] = word_id; // No MAX_VOCAB_SIZE check, just assign the word ID directly
#endif

                    }
                    else
                    {
                        pair->right_context_ids[k] = /*0*/ PAIRS_PADDING_KEY; // Set to 0 for padding, as 0 is a valid hash key index. Downstream code should handle this appropriately.
                                                         // This should not be hard coded to 0, but rather use a defined constant for padding, e.g., PAIRS_PADDING_KEY.

                        //pair->right_context_keys[k] = PAIRS_PADDING_KEY; // 0 is a valid hash key index
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
        
    /*
     * Debugging-only overload used to verify that the earlier parser and line-table
     * structures were built correctly before moving on to training-oriented pair generation.
     */
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
                    pair->left_context_ids = new size_t[CONTEXT_WINDOW_SIZE];
                    pair->right_context_ids = new size_t[CONTEXT_WINDOW_SIZE];
                }
                catch (const std::bad_alloc& e)
                {
                    throw std::runtime_error("Pairs::build_pairs((Parser&, WORDS**) Error: failed to allocate memory for left/right context arrays of single token.");
                }
                
                pair->target_id = line->keys[j]; // Set the key for the target/center token

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
                        pair->left_context_ids[CONTEXT_WINDOW_SIZE - 1 - k] = line->keys[j - k - 1];
                    }
                    else
                    {
                        pair->left_context_ids[CONTEXT_WINDOW_SIZE - 1 - k] = PAIRS_PADDING_KEY; // 0 is a valid hash key index
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
                        pair->right_context_ids[k] = line->keys[j + k + 1];
                    }
                    else
                    {
                        pair->right_context_ids[k] = PAIRS_PADDING_KEY; // 0 is a valid hash key index
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
    
    void build_pairs_older(Parser& parser, WORDS** lines_array)
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
                    pair->left_context_ids = new size_t[CONTEXT_WINDOW_SIZE]();
                    pair->right_context_ids = new size_t[CONTEXT_WINDOW_SIZE]();
                }
                catch (const std::bad_alloc& e)
                {
                    throw std::runtime_error("Pairs::build_pairs((Parser&, WORDS**) Error: failed to allocate memory for context pair keys.");
                }

                contexts[i]->pairs[j] = pair; // Store the pointer to the pair in the array of pairs

                pair->target_id = line->keys[j]; // Set target key in pair     
                
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

    struct ContextPairs** build_pairs_old(Parser& parser, const LINES_NEW* const lines, const WordRecord_new* const *const hash_table)
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
                pair->left_context_ids = new size_t[CONTEXT_WINDOW_SIZE]();
                pair->right_context_ids = new size_t[CONTEXT_WINDOW_SIZE]();

                context_pairs->pairs[i] = pair;

                TOKEN_NEW* target_token = tokens_tail;

                pair->target_id = target_token->key; // Set target key in pair
 
                for (size_t j = 0; j < CONTEXT_WINDOW_SIZE && target_token->prev != nullptr; j++)
                {
                    pair->left_context_ids[CONTEXT_WINDOW_SIZE - 1 - j] = target_token->prev->key;
                    target_token = target_token->prev;
                }

                // Reset target_token to the original token
                target_token = tokens_tail;

                //std::cout<< i << std::endl;

                // Move to right
                for (size_t j = 0; j < CONTEXT_WINDOW_SIZE && target_token->next != nullptr; j++)
                {
                    pair->right_context_ids[j] = target_token->next->key;
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

/*
 * =========================================================================
 * DESIGN SHORTCOMINGS & PERFORMANCE OPTIMIZATION PATHS
 * =========================================================================
 * 
 * While the current implementation of `Pairs` building works correctly, there 
 * are several low-level performance bottlenecks and structural shortcomings 
 * that can be optimized to improve performance on large datasets.
 * 
 * 1. HEAP FRAGMENTATION FROM NESTED DYNAMIC ALLOCATIONS:
 *    - In `build_pairs()`, the code performs multiple allocations per token:
 *        - `new ContextPair()`
 *        - `new size_t[CONTEXT_WINDOW_SIZE]` (for left_context_keys)
 *        - `new size_t[CONTEXT_WINDOW_SIZE]` (for right_context_keys)
 *    - For a large corpus with millions of tokens, this leads to millions of 
 *      tiny allocations. This causes significant heap overhead, allocator lock 
 *      contention, and heap fragmentation.
 *    - **Optimization**: Flatten the memory layout. Allocate a single contiguous 
 *      block of memory for all left and right context keys of the entire corpus 
 *      (or line), and have pointers/offsets indexing into this giant pre-allocated 
 *      buffer.
 * 
 * 2. CPU CACHE LOCALITY ISSUES (POINTER CHASING):
 *    - `ContextPairs**` and `ContextPair**` store pointers to structures. Because 
 *      these structures are allocated independently on the heap, they are scattered 
 *      across memory. 
 *    - When iterating through pairs during training, the CPU has to "chase" pointers, 
 *      leading to L1/L2 cache misses.
 *    - **Optimization**: Migrate from arrays of pointers (e.g. `ContextPair**`) to 
 *      contiguous vectors/arrays of structures (e.g. `ContextPair*` array), or 
 *      preferably a fully-flat struct-of-arrays (SoA) design which is also highly 
 *      conducive to direct copying to GPU memory.
 * 
 * 3. EXPLICIT ERROR HANDLING & bad_alloc RECOVERY:
 *    - Using `try/catch` around every allocation provides safe failure logging, but 
 *      fails to perform cleanup of previously allocated lines/pairs on bad_alloc.
 *      If memory allocation fails mid-way, the already-allocated contexts will leak.
 *    - **Optimization**: Implement a custom RAII resource manager or destructor that 
 *      can clean up partially built structures on exception, or use smart pointers 
 *      like `std::unique_ptr` where appropriate (though raw buffers are often preferred 
 *      in high-performance GPU pipelines, in which case a single giant contiguous allocation 
 *      solves both issues).
 */
#endif // CSV_PAIRS_LIB_PAIRS_HH