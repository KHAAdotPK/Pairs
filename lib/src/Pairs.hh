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