/*
    lib/WordEmbedding-Algorithms/Word2Vec/Skip-Gram/skip-gram-pairs.hh
    Q@khaa.pk
 */

#include "header.hh" 

/*
    TODO: Address handling for the last token/word of the corpus when it serves as the center/target word in a pair. 
    Given that there are no words/tokens to its right, the 'right' array contains values other than INDEX_NOT_FOUND_AT_VALUE. 
    For such a center/target word, it's crucial to verify the values in the 'right' array. 
    Specifically, we need to ensure that they don't exceed the total number of center/target words or all words (including redundancies).

    Conditions to check:
    
    1. Check if the first element of 'right' array points to a valid index in the vocabulary:
       if (vocab[(*(pair->getRight()))[0]].size() == 0) {}

    2. Check if the second element of 'right' array points to a valid index in the vocabulary:
       if (vocab[(*(pair->getRight()))[1]].size() == 0) {}

    3. Verify if the index referenced by the first element of 'right' array exceeds the total number of tokens:
       if ( (*(pair->getRight()))[0] > vocab.numberOfToken() ) {}

    4. Verify if the index referenced by the second element of 'right' array exceeds the total number of tokens:
       if ( (*(pair->getRight()))[1] > vocab.numberOfToken() ) {}    
 */

/*
    This should be moved to the respective methods being used here...
    std::cout<< vocab[pair->getCenterWord()].c_str() << std::endl;\
    std::cout<< pair->getLeft(0) << std::endl;\
    if ((*(pair->getRight()))[0] == INDEX_NOT_FOUND_AT_VALUE)\
    {\
        std::cout<<" YES YES YES YES YES YES YES YES YES YES " << std::endl;\
    }\
    std::cout<< vocab[(*(pair->getRight()))[0]].c_str() << std::endl;\
    std::cout<< vocab[(*(pair->getRight()))[1]].c_str() << std::endl;\
 */

#ifndef R3PLICA_WILD_FLOWERS_AND_WILD_HORSES_GROW_PAIRS_HH
#define R3PLICA_WILD_FLOWERS_AND_WILD_HORSES_GROW_PAIRS_HH

struct Pairs;

typedef struct ContextWord
{   
    cc_tokenizer::string_character_traits<char>::size_type operator[](cc_tokenizer::string_character_traits<char>::size_type index) const
    {
        if (index >= SKIP_GRAM_CONTEXT_WINDOW_SIZE)
        {            
            return INDEX_NOT_FOUND_AT_VALUE;
        }
        
        return array[index];   
    }

    cc_tokenizer::string_character_traits<char>::size_type size(void) 
    {
        return n;
    }

    // TODO, make all properties private
    cc_tokenizer::string_character_traits<char>::size_type n;
    cc_tokenizer::string_character_traits<char>::size_type array[SKIP_GRAM_CONTEXT_WINDOW_SIZE];    
} CONTEXTWORDS;

typedef CONTEXTWORDS* CONTEXTWORDS_PTR;

typedef struct WordPairs 
{
    // Declare a friend    
    friend struct Pairs;

    WordPairs(WordPairs& ref) : centerWord(ref.centerWord), left(ref.left), right(ref.right)
    {
        next = NULL;
        prev = NULL;
    } 

    WordPairs& operator= (const WordPairs& other)
    {
        if (this != &other) // Check for self-assignment
        { 
            this->centerWord = other.centerWord;
            this->left = other.left;
            this->right = other.right;
        }

        return *this;
    }

    /*
        (*(pair->getLeft()))[0]
         vocab[(*(pair->getLeft()))[0]].c_str()
     */
    cc_tokenizer::string_character_traits<char>::size_type getLeft(cc_tokenizer::string_character_traits<char>::size_type index)
    {
        if (index >= SKIP_GRAM_CONTEXT_WINDOW_SIZE)
        {
            return INDEX_NOT_FOUND_AT_VALUE;
        }

        // The furthest context word from the center/target word is at index ((SKIP_GRAM_CONTEXT_WINDOW_SIZE - 1) - index)
        //return left->array[index];
        return (*left)[index];
    }

    CONTEXTWORDS_PTR getLeft(void) 
    {
        return left;
    }

    /*
        (*(pair->getRight()))[0]
        vocab[(*(pair->getRight()))[0]].c_str()
     */
    cc_tokenizer::string_character_traits<char>::size_type getRight(cc_tokenizer::string_character_traits<char>::size_type index)
    {   
        /*if (this->next == NULL)
        {
            return INDEX_NOT_FOUND_AT_VALUE;
        }*/

        if (index >= SKIP_GRAM_CONTEXT_WINDOW_SIZE)
        {
            return INDEX_NOT_FOUND_AT_VALUE;
        }

        // The furthest context word from the center/target word is at index (SKIP_GRAM_CONTEXT_WINDOW_SIZE - 1)
        //return right->array[index];
        return (*right)[index];
    }

    CONTEXTWORDS_PTR getRight(void) 
    {
        return right;
    }

    cc_tokenizer::string_character_traits<char>::size_type getCenterWord(void)
    {
        return centerWord;    
    }

    private:
        CONTEXTWORDS_PTR left;
        cc_tokenizer::string_character_traits<char>::size_type centerWord;
        CONTEXTWORDS_PTR right;

        struct WordPairs* next;
        struct WordPairs* prev;

} WORDPAIRS;

typedef WORDPAIRS* WORDPAIRS_PTR;
/*
    In the context of word embedding models like Skip-gram,
    word pairs refer to a central word and the surrounding words within a specific window size (SKIP_GRAM_CONTEXT_WINDOWS_SIZE).

    Skip-gram aims to predict the surrounding words based on the central word. 
    There are two main types of word pairs to consider in Skip-gram:
    1. Positive pairs: These pairs consist of the central word and its surrounding words within the window. 
       Since these words tend to co-occur frequently, they represent a positive relationship for the model to learn.
    2. Negative pairs: These are artificially generated pairs that don't typically appear together.
       They help the model distinguish between words with similar but not identical meanings.
       Negative pairs are often formed by randomly selecting words from the vocabulary that are not within the window of the central word.

    By processing a large corpus of text and analyzing these positive and negative word pairs, 
    Skip-gram learns the underlying relationships between words and builds a meaningful vector representation for each word.      
 */
typedef struct Pairs 
{   
    Pairs() : head(NULL), current_pair(NULL), n(0), the_80_20_split_counter(0), reference_count(NUMCY_DEFAULT_REFERENCE_COUNT)
    {

    }

    Pairs(CORPUS& vocab, bool verbose = false) : head(NULL), current_pair(NULL), n(0), the_80_20_split_counter(0), reference_count(NUMCY_DEFAULT_REFERENCE_COUNT)
    {
        WORDPAIRS_PTR current_word_pair = NULL;
        //cc_tokenizer::string_character_traits<char>::size_type i_centerWord;
        
        /*
            At each i + INDEX_ORIGINATES_AT_VALUE we've the center word
            Context words are pair of words around this center word...
            SKIP_GRAM_CONTEXT_WINDOW_SIZE center-word SKIP_GRAM_CONTEXT_WINDOW_SIZE
            
            We must monitor redundancy as we construct the entire vocabulary from the corpus.
            It's crucial to maintain records of any redundant words, that is why the use of numberOfTokens() method instead of numberOfUniqueTokens()
         */
        for (cc_tokenizer::string_character_traits<char>::size_type i = 0; i < vocab.numberOfTokens(); i++)
        {               
            if (head == NULL)
            {   
                try            
                {                
                    head = reinterpret_cast<WORDPAIRS_PTR>(cc_tokenizer::allocator<char>().allocate(sizeof(WORDPAIRS)));
                    head->next = NULL;
                    head->prev = NULL;

                    head->centerWord = vocab[vocab(i + INDEX_ORIGINATES_AT_VALUE, true)]; // i originated at 0, 
                                                                                          // vocabulary indices originate at INDEX_ORIGINATES_AT_VALUE
                                                                                          // We are traversing the vocabulary with redundency flag on. That is becuase of our choice for the outer for loop
                    //i_centerWord = i;

                    // Later on find the surrounding context words
                    head->left = NULL;
                    head->right = NULL;

                    /*
                        Pointer "current_word_pair" is a displacement in to our linked list and it points to the word pair 
                        which will hold the center word and the all its context words
                     */
                    current_word_pair = head;

                    tail = current_word_pair;

                    n = n + 1; // Increment number of word pair counter
                }
                catch (std::bad_alloc& e)
                {
                    head = NULL;
                    
                    throw ala_exception(e.what());
                }
                catch(std::length_error& e)
                {
                    head = NULL;
                    
                    throw ala_exception(e.what());
                }
            }
            else
            {                
                //current_word_pair = head;

                current_word_pair = tail;

                // We've new center word. Add its link at the end.
                // Get to the tail of the linked list, we already know that head is not null
                //while (current_word_pair->next != NULL)
                //{
                //    current_word_pair = current_word_pair->next;
                //}
                
                try
                {
                    // Add new link to the already existing linked list
                    current_word_pair->next = reinterpret_cast<WORDPAIRS_PTR>(cc_tokenizer::allocator<char>().allocate(sizeof(WORDPAIRS)));
                    current_word_pair->next->next = NULL;
                    current_word_pair->next->prev = current_word_pair;
                    /*
                        Pointer "current_word_pair" is a displacement in to our linked list and it points to the word pair 
                        which will hold the center word and the all its context words
                     */
                    current_word_pair = current_word_pair->next;

                    current_word_pair->centerWord = vocab[vocab(i + INDEX_ORIGINATES_AT_VALUE, true)]; // i originated at 0,
                                                                                                       // vocabulary indices originate at INDEX_ORIGINATES_AT_VALUE 
                                                                                                       // We traversing the vocabulary with redundency flag on. That is becuase of our choice for the outer for loop 
                    //i_centerWord = i;

                    // Later on find the surrounding context words
                    current_word_pair->left = NULL;
                    current_word_pair->right = NULL;

                    tail = current_word_pair;

                    n = n + 1; // Increment number of word pair counter
                }
                catch (std::bad_alloc& e)
                {
                     current_word_pair->next = NULL;

                     throw ala_exception(e.what()); 
                }
                catch(std::length_error& e)
                {
                    head = NULL;
                    
                    throw ala_exception(e.what());
                }                
            }

            // Find context words around the center word 
            // Pointer "current_word_pair" is a displacement in to our linked list and it points to the word pair which will hold the center word and the all its context words

            // context words on left, number of these at max would be SKIP_GRAM_CONTEXT_WINDOW_SIZE
            CONTEXTWORDS_PTR left = NULL;
            try
            {                            
                left = reinterpret_cast<CONTEXTWORDS_PTR>(cc_tokenizer::allocator<char>().allocate(sizeof(CONTEXTWORDS)));
                left->n = 0;
                //memset(left->array, INDEX_NOT_FOUND_AT_VALUE, SKIP_GRAM_CONTEXT_WINDOW_SIZE);
                for (cc_tokenizer::string_character_traits<char>::size_type i = 0; i < SKIP_GRAM_CONTEXT_WINDOW_SIZE; i++)
                {
                    left->array[i] = INDEX_NOT_FOUND_AT_VALUE;                    
                }
            }
            catch (std::bad_alloc& e)
            {
                left = NULL;

                throw ala_exception(e.what());
            }
            catch(std::length_error& e)
            {
                head = NULL;
                    
                throw ala_exception(e.what());
            }

            // Context words on the left of center word. Start on the left most and then move closer to the target/center word
            for (cc_tokenizer::string_character_traits<char>::size_type j = SKIP_GRAM_CONTEXT_WINDOW_SIZE; j > 0; /*SKIP_GRAM_CONTEXT_WINDOW_SIZE;*/ j--)
            {            
                if (vocab(((i + INDEX_ORIGINATES_AT_VALUE) - j), true).size())
                {
                    left->array[j - 1] = vocab[vocab(((i + INDEX_ORIGINATES_AT_VALUE) - j), true)];
                    left->n = left->n + 1;

                    if (verbose)
                    {
                        //std::cout<< vocab(((i + INDEX_ORIGINATES_AT_VALUE) - j), true).c_str() << "  ";
                        std::cout<< vocab(left->array[j - 1], false).c_str() << "  ";
                    }
                }
                else
                {
                    left->array[j - 1] = INDEX_NOT_FOUND_AT_VALUE;

                    if (verbose)
                    {
                        std::cout<< "NONE" << "  ";
                    }
                }                
            }

            current_word_pair->left = left;

            left = NULL;

            if (verbose)
            {
                // The center word
                //std::cout<< "[ " << vocab(i + INDEX_ORIGINATES_AT_VALUE, true).c_str() << " ] ";
                std::cout<< "[ " << vocab(current_word_pair->centerWord, false).c_str() << " ] ";
            }

            CONTEXTWORDS_PTR right = NULL;
            try
            {
                right = reinterpret_cast<CONTEXTWORDS_PTR>(cc_tokenizer::allocator<char>().allocate(sizeof(CONTEXTWORDS)));
                right->n = 0;
                //memset(right->array, INDEX_NOT_FOUND_AT_VALUE, SKIP_GRAM_CONTEXT_WINDOW_SIZE);
                for (cc_tokenizer::string_character_traits<char>::size_type i = 0; i < SKIP_GRAM_CONTEXT_WINDOW_SIZE; i++)
                {
                    right->array[i] = INDEX_NOT_FOUND_AT_VALUE;
                }
            }
            catch(std::bad_alloc& e)
            {
                right = NULL;

                throw ala_exception(e.what()); 
            }
            catch(std::length_error& e)
            {
                head = NULL;
                    
                throw ala_exception(e.what());
            } 

            // Context words to the right of center word only when there are atleast one context word to the right
            // We need to stop this loop at the last word as target/center word and not look for context words on the right
            for (cc_tokenizer::string_character_traits<char>::size_type j = 1; j <= SKIP_GRAM_CONTEXT_WINDOW_SIZE && (i + INDEX_ORIGINATES_AT_VALUE + j) /* Not go beyond the last (center/target) word */ <= vocab.numberOfTokens(); j++)
            //for (cc_tokenizer::string_character_traits<char>::size_type j = i_centerWord + 1; ((j < vocab.numberOfTokens()) && (j < (i_centerWord + SKIP_GRAM_CONTEXT_WINDOW_SIZE))); j++)
            {                
                if (vocab(((i + INDEX_ORIGINATES_AT_VALUE) + j), true).size())
                //if (vocab(INDEX_ORIGINATES_AT_VALUE + j, true).size())
                {
                    /*if (verbose)
                    {
                        std::cout<< vocab(((i + INDEX_ORIGINATES_AT_VALUE) + j), true).c_str() << "  ";

                        //std::cout<< "-------->>>>>>>> "  << vocab[vocab((i + INDEX_ORIGINATES_AT_VALUE) + j, true)] << std::endl;
                    }*/

                    right->array[j - 1] = vocab[vocab(((i + INDEX_ORIGINATES_AT_VALUE) + j), true)];
                    //right->array[j - (i_centerWord + 1)] = vocab[vocab(INDEX_ORIGINATES_AT_VALUE + j, true)];
                    right->n = right->n + 1;

                    if (verbose)
                    {
                        std::cout<< vocab(right->array[j - 1], false).c_str() << "  ";

                        //std::cout<< "-------->>>>>>>> "  << vocab[vocab((i + INDEX_ORIGINATES_AT_VALUE) + j, true)] << std::endl;
                    }

                    /*if (current_word_pair->centerWord == vocab[vocab(((i + INDEX_ORIGINATES_AT_VALUE) + j), true)])
                    {
                        std::cout<< "YES YES" << std::endl;
                    }*/
                }
                else
                {
                    right->array[j - /*(i_centerWord + 1)*/ /*i*/ 1] = INDEX_NOT_FOUND_AT_VALUE;

                    if (verbose)
                    {
                        std::cout<< "NONE" << "  ";
                    }
                }

                /*if (right->array[j - 1] == INDEX_NOT_FOUND_AT_VALUE)
                {
                    std::cout<< "NONE" << " ";
                }*/
            }

            current_word_pair->right = right;

            /*
                TODO, rewrite all of the code of this constructors, in the code wold after thoughts are dangerous.
             */
            if (verbose)
            {
                for (cc_tokenizer::string_character_traits<char>::size_type k = 0; k < SKIP_GRAM_CONTEXT_WINDOW_SIZE; k++)
                {
                    if (right->array[k] == INDEX_NOT_FOUND_AT_VALUE)
                    {
                        std::cout<< "NONE" << " ";
                    }
                }

                std::cout<< std::endl;  
            }

            right = NULL;

            if (verbose)
            {
                //std::cout<< std::endl;            
            }        
        }        
    }

    ~Pairs()
    {                
        if (this->reference_count > 0)
        {
            return;
        }

        if (head == NULL)
        {
            return;
        }

        WORDPAIRS_PTR current_wordpair = head;

        while (current_wordpair->next != NULL)
        {
            current_wordpair = current_wordpair->next;
        }

        while (current_wordpair->prev != NULL)
        {
            current_wordpair = current_wordpair->prev;

            cc_tokenizer::allocator<char>().deallocate(reinterpret_cast<char*>(current_wordpair->next->left));
            cc_tokenizer::allocator<char>().deallocate(reinterpret_cast<char*>(current_wordpair->next->right));

            cc_tokenizer::allocator<char>().deallocate(reinterpret_cast<char*>(current_wordpair->next));
        }

        cc_tokenizer::allocator<char>().deallocate(reinterpret_cast<char*>(current_wordpair->left));
        cc_tokenizer::allocator<char>().deallocate(reinterpret_cast<char*>(current_wordpair->right));
        cc_tokenizer::allocator<char>().deallocate(reinterpret_cast<char*>(current_wordpair));
        
        this->head = NULL;
        this->n = 0;
        this->current_pair = NULL;
        this->reference_count = 0;
        this->the_80_20_split_counter = 0;
    }
    
    /**
     * @brief Serializes the entire linked list of word-context pairs to a binary file on disk.
     *
     * This function writes the pre-computed training pairs (left context array, center word index,
     * and right context array) for every center word occurrence in the corpus to a binary file.
     * The data is written in a fixed-size format assuming a constant context window size
     * (SKIP_GRAM_CONTEXT_WINDOW_SIZE on each side).
     *
     * The format per pair is:
     *   - SKIP_GRAM_CONTEXT_WINDOW_SIZE × size_type : left context indices
     *   - 1 × size_type                             : center word index (unique vocabulary index)
     *   - SKIP_GRAM_CONTEXT_WINDOW_SIZE × size_type : right context indices
     *
     * This allows fast subsequent training runs by avoiding expensive pair reconstruction
     * from the corpus and redundant vocabulary lookups.
     *
     * @param fname The filename (as a custom String<char>) to write the binary data to.
     *
     * @throws ala_exception If the file cannot be opened, or if any write operation fails
     *                       (incomplete write of an array or center word).
     *
     * @note The file is opened in "wb" mode (binary write). The caller is responsible for
     *       ensuring the Pairs object is fully constructed before calling this method.
     */
    void write (cc_tokenizer::String<char>& fname) throw (ala_exception)
    {
        FILE* fp = fopen(fname.c_str(), "wb");
        size_t written_count = 0;
        
        if (fp == NULL)
        {
            cc_tokenizer::String<char> message1 = cc_tokenizer::String<char>("Pairs::write(cc_tokenizer::String<char>&) Error: Could not open file \"");
            cc_tokenizer::String<char> message2 = cc_tokenizer::String<char>("\" for writing.");

            throw ala_exception(message1 + fname + message2);
        }

        WORDPAIRS_PTR current_pair = head;

        while (current_pair != NULL)
        {
            written_count = fwrite(current_pair->left->array, sizeof(cc_tokenizer::string_character_traits<char>::size_type), SKIP_GRAM_CONTEXT_WINDOW_SIZE, fp);
            if (written_count != SKIP_GRAM_CONTEXT_WINDOW_SIZE/**sizeof(cc_tokenizer::string_character_traits<char>::size_type)*/)
            {
                cc_tokenizer::String<char> message1 = cc_tokenizer::String<char>("Pairs::write(cc_tokenizer::String<char>&) Error: Could not write to file \"");
                cc_tokenizer::String<char> message2 = cc_tokenizer::String<char>("\" for writing.");

                fclose(fp);

                throw ala_exception(message1 + fname + message2);
            }

            //std::cout<< "Pairs::write(cc_tokenizer::String<char>&) Success: Successfully wrote " << written_count << " pairs to file \"" << fname.c_str() << "\"." << std::endl;

            written_count = fwrite(&(current_pair->centerWord), sizeof(cc_tokenizer::string_character_traits<char>::size_type), 1, fp);
            if (written_count != 1)
            {
                cc_tokenizer::String<char> message1 = cc_tokenizer::String<char>("Pairs::write(cc_tokenizer::String<char>&) Error: Could not write to file \"");
                cc_tokenizer::String<char> message2 = cc_tokenizer::String<char>("\" for writing.");

                fclose(fp);

                throw ala_exception(message1 + fname + message2);
            }

            //std::cout<< "Pairs::write(cc_tokenizer::String<char>&) Success: Successfully wrote " << written_count << " pairs to file \"" << fname.c_str() << "\"." << std::endl;

            written_count = fwrite(current_pair->right->array, sizeof(cc_tokenizer::string_character_traits<char>::size_type), SKIP_GRAM_CONTEXT_WINDOW_SIZE, fp);
            if (written_count != SKIP_GRAM_CONTEXT_WINDOW_SIZE/**sizeof(cc_tokenizer::string_character_traits<char>::size_type)*/)
            {
                cc_tokenizer::String<char> message1 = cc_tokenizer::String<char>("Pairs::write(cc_tokenizer::String<char>&) Error: Could not write to file \"");
                cc_tokenizer::String<char> message2 = cc_tokenizer::String<char>("\" for writing.");

                fclose(fp);

                throw ala_exception(message1 + fname + message2);
            }
            
            //std::cout<< "Pairs::write(cc_tokenizer::String<char>&) Success: Successfully wrote " << written_count << " pairs to file \"" << fname.c_str() << "\"." << std::endl;
                                    
            current_pair = current_pair->next;
        }

        fclose(fp);
    }

    /**
     * @brief Deserializes word-context training pairs from a binary file previously written by write().
     *
     * This function reconstructs the complete linked list of WORDPAIRS nodes by reading the binary
     * data saved by the write() method. It rebuilds the structure in memory exactly as it was
     * during pair generation, including left/right context arrays and correct context counts (n).
     *
     * The file format is assumed to match the one produced by write():
     *   - Fixed-size records of left contexts + center index + right contexts
     *
     * The number of pairs is computed from the file size to avoid storing an explicit count.
     * After loading each pair, the function recounts valid (non-INDEX_NOT_FOUND_AT_VALUE)
     * context entries and updates left->n and right->n accordingly.
     *
     * This enables extremely fast startup for repeated training runs — pair generation,
     * which can be expensive due to redundant vocabulary lookups, is skipped entirely.
     *
     * @param fname The filename (as a custom String<char>) containing the serialized pairs.
     *
     * @throws ala_exception If the file cannot be opened, file size cannot be determined,
     *                       any read operation fails (incomplete record), or memory allocation fails.
     *
     * @note The file is opened in "rb" mode (binary read). The existing list (if any) is
     *       assumed to be empty; this method does not clear any prior content.
     * @note Context counts (left->n and right->n) are recalculated after loading to ensure
     *       correctness even if padding values (INDEX_NOT_FOUND_AT_VALUE) are present.
     */
    void read (cc_tokenizer::String<char>& fname) throw (ala_exception)
    {
        FILE* fp = fopen(fname.c_str(), "rb");
        struct stat file_stat;
        
        size_t read_count;

        if (fp == NULL)
        {
            cc_tokenizer::String<char> message1 = cc_tokenizer::String<char>("Pairs::read(cc_tokenizer::String<char>&) Error: Could not open file \"");
            cc_tokenizer::String<char> message2 = cc_tokenizer::String<char>("\" for reading.");

            throw ala_exception(message1 + fname + message2);
        }
        
        if (fstat(fileno(fp), &file_stat) != 0)
        {
            cc_tokenizer::String<char> message1 = cc_tokenizer::String<char>("Pairs::read(cc_tokenizer::String<char>&) Error: Could not get file size of file \"");
            cc_tokenizer::String<char> message2 = cc_tokenizer::String<char>("\" for reading.");

            fclose(fp);

            throw ala_exception(message1 + fname + message2);
        }

        size_t file_size = file_stat.st_size;

        //std::cout<< "Pairs::read(cc_tokenizer::String<char>&) Success: Successfully read " << file_size << " pairs from file \"" << fname.c_str() << "\"." << std::endl;

        cc_tokenizer::string_character_traits<char>::size_type number_of_pairs = file_size / ((SKIP_GRAM_CONTEXT_WINDOW_SIZE * sizeof(cc_tokenizer::string_character_traits<char>::size_type))*2 + sizeof(cc_tokenizer::string_character_traits<char>::size_type));

        //std::cout<< "Pairs::read(cc_tokenizer::String<char>&) Success: Successfully read " << number_of_pairs << " pairs from file \"" << fname.c_str() << "\"." << std::endl;

        for (cc_tokenizer::string_character_traits<char>::size_type i = 0; i < number_of_pairs; i++)
        {
            try
            {
                if (head == NULL)
                {               
                    head = reinterpret_cast<WORDPAIRS_PTR>(cc_tokenizer::allocator<char>().allocate(sizeof(WORDPAIRS)));
                    head->next = NULL;
                    head->prev = NULL;
                   
                    // Later on find the surrounding context words
                    head->left = reinterpret_cast<CONTEXTWORDS_PTR>(cc_tokenizer::allocator<char>().allocate(sizeof(CONTEXTWORDS)));
                    head->right = reinterpret_cast<CONTEXTWORDS_PTR>(cc_tokenizer::allocator<char>().allocate(sizeof(CONTEXTWORDS)));
                    
                    tail = head;                           
                }
                else
                {
                    tail->next = reinterpret_cast<WORDPAIRS_PTR>(cc_tokenizer::allocator<char>().allocate(sizeof(WORDPAIRS)));
                    tail->next->next = NULL;
                    tail->next->prev = tail;

                    // Later on find the surrounding context words
                    tail->next->left = reinterpret_cast<CONTEXTWORDS_PTR>(cc_tokenizer::allocator<char>().allocate(sizeof(CONTEXTWORDS)));
                    tail->next->right = reinterpret_cast<CONTEXTWORDS_PTR>(cc_tokenizer::allocator<char>().allocate(sizeof(CONTEXTWORDS)));                    

                    tail = tail->next;
                }

                n = n + 1; // Increment number of word pair counter
                
                read_count = fread(tail->left->array, sizeof(cc_tokenizer::string_character_traits<char>::size_type), SKIP_GRAM_CONTEXT_WINDOW_SIZE, fp);

                if (read_count != SKIP_GRAM_CONTEXT_WINDOW_SIZE)
                {
                    cc_tokenizer::String<char> message1 = cc_tokenizer::String<char>("Pairs::read(cc_tokenizer::String<char>&) Error: Could not read from file \"");
                    cc_tokenizer::String<char> message2 = cc_tokenizer::String<char>("\" for reading.");

                    fclose(fp);

                    throw ala_exception(message1 + fname + message2);
                }

                read_count = fread(&(tail->centerWord), sizeof(cc_tokenizer::string_character_traits<char>::size_type), 1, fp);

                if (read_count != 1)
                {
                    cc_tokenizer::String<char> message1 = cc_tokenizer::String<char>("Pairs::read(cc_tokenizer::String<char>&) Error: Could not read from file \"");
                    cc_tokenizer::String<char> message2 = cc_tokenizer::String<char>("\" for reading.");

                    fclose(fp);

                    throw ala_exception(message1 + fname + message2);
                }

                read_count = fread(tail->right->array, sizeof(cc_tokenizer::string_character_traits<char>::size_type), SKIP_GRAM_CONTEXT_WINDOW_SIZE, fp);

                if (read_count != SKIP_GRAM_CONTEXT_WINDOW_SIZE)
                {
                    cc_tokenizer::String<char> message1 = cc_tokenizer::String<char>("Pairs::read(cc_tokenizer::String<char>&) Error: Could not read from file \"");
                    cc_tokenizer::String<char> message2 = cc_tokenizer::String<char>("\" for reading.");

                    fclose(fp);

                    throw ala_exception(message1 + fname + message2);
                }

                tail->left->n = 0;
                tail->right->n = 0;

                for (cc_tokenizer::string_character_traits<char>::size_type j = 0; j < SKIP_GRAM_CONTEXT_WINDOW_SIZE; j++)
                {
                    if (tail->left->array[j] != INDEX_NOT_FOUND_AT_VALUE)
                    {
                        tail->left->n++;
                    }
                        
                    if (tail->right->array[j] != INDEX_NOT_FOUND_AT_VALUE)
                    {
                        tail->right->n++;
                    }                         
                }
            }
            catch (std::bad_alloc& e)
            {
                fclose(fp);

                throw ala_exception(cc_tokenizer::String<char>("Pairs::read(cc_tokenizer::String<char>&) Error: ") + e.what()); 
            }
            catch(std::length_error& e)
            {
                fclose(fp);

                throw ala_exception(cc_tokenizer::String<char>("Pairs::read(cc_tokenizer::String<char>&) Error: ") + e.what());
            }                
        }

        fclose(fp);
    }

    void incrementReferenceCount(void) 
    {            
        if (this->head != NULL)
        {            
            this->reference_count++;
        }
    }

    void decrementReferenceCount(void)
    {        
        if (this->reference_count > 0)
        {
            this->reference_count--;
        }

        /*if (this->reference_count > 0)
        {
            return;
        }*/

        /*WORDPAIRS_PTR current_wordpair = head;

        while (current_wordpair->next != NULL)
        {
            current_wordpair = current_wordpair->next;
        }

        while (current_wordpair->prev != NULL)
        {
            current_wordpair = current_wordpair->prev;

            cc_tokenizer::allocator<char>().deallocate(reinterpret_cast<char*>(current_wordpair->next->left));
            cc_tokenizer::allocator<char>().deallocate(reinterpret_cast<char*>(current_wordpair->next->right));

            cc_tokenizer::allocator<char>().deallocate(reinterpret_cast<char*>(current_wordpair->next));
        }

        cc_tokenizer::allocator<char>().deallocate(reinterpret_cast<char*>(current_wordpair->left));
        cc_tokenizer::allocator<char>().deallocate(reinterpret_cast<char*>(current_wordpair->right));
        cc_tokenizer::allocator<char>().deallocate(reinterpret_cast<char*>(current_wordpair));*/    
    }

    Pairs& operator= (Pairs& other)
    {
        // Self assinment check
        if (this == &other)
        {
            return *this;
        }

        this->decrementReferenceCount();

        this->head = other.head;
        this->current_pair = NULL;
        this->n = other.n;
        this->the_80_20_split_counter = other.the_80_20_split_counter;
        this->reference_count = other.reference_count;

        this->incrementReferenceCount();

        return *this;
    }

     /*
        PLEASE NOTE: current_pair gets NULL value at the end of loop on its own. 
     */
    cc_tokenizer::string_character_traits<char>::int_type go_to_next_word_pair(void) 
    {
        if (current_pair == NULL)
        {
            current_pair = head;
        }
        else 
        {
            current_pair = current_pair->next;
        }

        if (current_pair == NULL)
        {
            return cc_tokenizer::string_character_traits<char>::eof();
        }

        return ~cc_tokenizer::string_character_traits<char>::eof();
    }
   
    /*
        PLEASE NOTE: current_pair gets NULL value at the end of loop on its own.
        the_80_20_split_counter, originates at 0
     */
    cc_tokenizer::string_character_traits<char>::int_type go_to_next_word_pair(bool phase) 
    {
        if (current_pair == NULL) // It does not matter which phase it is, all phases initialized in the same way 
        {
            current_pair = head;
        
            if (phase == PAIRS_VALIDATION_PHASE)
            {
                cc_tokenizer::string_character_traits<char>::size_type number_of_pairs_for_training = /*(this->get_number_of_word_pairs()*PAIRS_VOCABULARY_TRAINING_SPLIT)*/ PAIRS_VOCABULARY_TRAINING_SPLIT(this->get_number_of_word_pairs());

                while (number_of_pairs_for_training > 0 && current_pair != NULL)
                {
                    current_pair = current_pair->next;
                    number_of_pairs_for_training = number_of_pairs_for_training - 1;
                }
            }
        }
        else 
        {
            if (phase == PAIRS_TRAINING_PHASE)
            {
                this->the_80_20_split_counter = this->the_80_20_split_counter + 1;
                
                if (this->the_80_20_split_counter < /*(this->get_number_of_word_pairs()*PAIRS_VOCABULARY_TRAINING_SPLIT)*/ PAIRS_VOCABULARY_TRAINING_SPLIT(this->get_number_of_word_pairs()))
                {
                    current_pair = current_pair->next;    
                }
                else // Crossed the last node of training pairs
                {                    
                    current_pair = NULL;
                }
            }
            else // phase is for validation
            {
                current_pair = current_pair->next; // just set to next link, the last link will set the curent_pair to NULL
            }  
        }
                
        if (current_pair == NULL)
        {
            this->the_80_20_split_counter = 0; // It does not matter whcvh phase it is, it is safe to reset this counter anyways as it originates at this value
            return cc_tokenizer::string_character_traits<char>::eof();
        }

        // Sucess, pointer to nth pair is stored in the current_pair, access it usng the get_current_word_pair() method
        return ~cc_tokenizer::string_character_traits<char>::eof();
    }

    cc_tokenizer::string_character_traits<char>::size_type get_number_of_word_pairs(void)
    {
        return n;
    }

    WORDPAIRS_PTR get_current_word_pair(void) 
    {
        return current_pair;
    }

    WORDPAIRS_PTR get_word_pair_by_number(cc_tokenizer::string_character_traits<char>::size_type i) throw (ala_exception) 
    {
        if (head == NULL)
        {
            throw ala_exception("");
        }

        if (i > get_number_of_word_pairs())
        {
            throw ala_exception("");
        }

        cc_tokenizer::string_character_traits<char>::size_type j = 1;
        WORDPAIRS_PTR current = head;

        do
        {
            if (j == i)
            {
                return current;
            }

            j = j + 1;
            current = current->next;
        } while (current != NULL);
        
        return NULL;
    }

    /*
        Shuffling Word Pairs: Before iterating through word pairs. 
        It shuffles the pairs object (assumed to be an iterable containing word index pairs) using Numcy::Random::shuffle.
        This helps prevent the model from overfitting to the order of word pairs in the corpus.

        Both of the arguments originate at 0.
        The method swaps the values of two links with each other... 
        @a, zero originated link number
        @b, zero originated link number
     */
    void shuffle(cc_tokenizer::string_character_traits<char>::size_type a, cc_tokenizer::string_character_traits<char>::size_type b)
    {           
        cc_tokenizer::string_character_traits<char>::size_type i = 0;
        WORDPAIRS_PTR pair_a = NULL, pair_b = NULL;

        while (go_to_next_word_pair() != cc_tokenizer::string_character_traits<char>::eof())
        {            
            if (a == i)
            {
               pair_a = get_current_word_pair(); 
            }
            else if (b == i)
            {
                pair_b = get_current_word_pair();
            }

            if (pair_a != NULL && pair_b != NULL)
            {                
                WORDPAIRS pair(*pair_a);
                *pair_a = *pair_b;
                *pair_b = pair;
                /*
                    We are breaking the loop with go_to_next_pair() abruptly(in the middle),
                    that is why we need to set the counter which keeps track of current word pair explicitly 
                 */
                current_pair = NULL;    
                break;          
            }

            i = i + 1;
        }
                        
        /*
        if (head == NULL)
        {
            return;
        }

        cc_tokenizer::string_character_traits<char>::size_type i = 0;
        WORDPAIRS_PTR current_pair = head, pair_a = NULL, pair_b = NULL;
        
        do {
            if (a == i)
            {
               pair_a = current_pair; 
            }
            else if (b == i)
            {
                pair_b = current_pair;
            }

            if (pair_a != NULL && pair_b != NULL)
            {
                break;
            }

            i = i + 1;

            current_pair = current_pair->next;
            
        } while (current_pair != NULL);

        if (current_pair == NULL)
        {
            return;
        }
                         
        WORDPAIRS pair(*pair_a);
        *pair_a = *pair_b;
        *pair_b = pair;
         */        
    }

    private:
        WORDPAIRS_PTR head, tail;
        WORDPAIRS_PTR current_pair;

        cc_tokenizer::string_character_traits<char>::size_type n;
        cc_tokenizer::string_character_traits<char>::size_type the_80_20_split_counter; // Originates at 0

        cc_tokenizer::string_character_traits<char>::size_type reference_count;

} PAIRS;

typedef PAIRS* SKIPGRAMPAIRS_PTR;
typedef PAIRS& SKIPGRAMPAIRS_REF;
//typedef Pairs& SKIPGRAMPAIRS_REF;
typedef const PAIRS& SKIPGRAMPAIRS_CONST_REF;

#endif