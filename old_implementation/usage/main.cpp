/*
    usage/main.cpp
    Q@khaa.pk
 */

#include "main.hh"

int main(int argc, char* argv[])
{ 
    ARG arg_corpus, arg_help, arg_verbose;
    cc_tokenizer::csv_parser<cc_tokenizer::String<char>, char> argsv_parser(cc_tokenizer::String<char>(COMMAND));
    cc_tokenizer::String<char> data;

    FIND_ARG(argv, argc, argsv_parser, "?", arg_help);
    if (arg_help.i)
    {
        HELP(argsv_parser, arg_help, ALL);
        HELP_DUMP(argsv_parser, arg_help);

        return 0;
    }

    if (argc < 2)
    {        
        HELP(argsv_parser, arg_help, "help");                
        HELP_DUMP(argsv_parser, arg_help);

        return 0;                     
    }

    FIND_ARG(argv, argc, argsv_parser, "verbose", arg_verbose);
    FIND_ARG(argv, argc, argsv_parser, "corpus", arg_corpus);
    
    if (arg_corpus.i)
    {
        FIND_ARG_BLOCK(argv, argc, argsv_parser, arg_corpus);
        if (arg_corpus.argc)
        {            
            try 
            {
                data = cc_tokenizer::cooked_read<char>(argv[arg_corpus.i + 1]);
                if (arg_verbose.i)
                {
                    std::cout<< "Corpus: " << argv[arg_corpus.i + 1] << std::endl;
                }
            }
            catch (ala_exception e)
            {
                std::cout<<e.what()<<std::endl;
                return -1;
            }            
        }
        else
        { 
            ARG arg_corpus_help;
            HELP(argsv_parser, arg_corpus_help, "--corpus");                
            HELP_DUMP(argsv_parser, arg_corpus_help);

            return 0; 
        }                
    }
    else
    {
        try
        {        
            data = cc_tokenizer::cooked_read<char>(SKIP_GRAM_DEFAULT_CORPUS_FILE);
            if (arg_verbose.i)
            {
                std::cout<< "Corpus: " << SKIP_GRAM_DEFAULT_CORPUS_FILE << std::endl;
            }
        }
        catch (ala_exception e)
        {
            std::cout<<e.what()<<std::endl;
            return -1;
        }
    }

    // Initialize the data parser with the CSV data.    
    cc_tokenizer::csv_parser<cc_tokenizer::String<char>, char> data_parser(data);
    // Create a vocabulary object from the parsed data.
    class Corpus vocab(data_parser);
    // Initialize the SkipGramPairs object with the vocabulary.
    // Uncomment the ternary conditional operator and all of the newly get built pairs are displayed.
    PAIRS pairs(vocab/*, arg_verbose.i ? true : false*/);

    /*
     * Instructions:
     * Below, this file contains two blocks of code that perform the same task of displaying the pairs.
     * To test the functionality, please follow these steps:
    *
    * 1. Uncomment the first block of code and keep the second block commented.
    * 2. Compile and run the code to observe the output.
    * 3. Comment the first block of code again.
    * 4. Uncomment the second block of code and keep the first block commented.
    * 5. Compile and run the code to observe the output.
    * 
    * Only one block of code should be uncommented at a time for proper testing.
    */

    /* BLOCK 1 */
    // Iterate through all word pairs.
    /*
    while (pairs.go_to_next_word_pair() != cc_tokenizer::string_character_traits<char>::eof())
    {
        // Get the current word pair.
        WORDPAIRS_PTR pair = pairs.get_current_word_pair();

        // Display left context words. Start from the farthest one from the center word.
        for (cc_tokenizer::string_character_traits<char>::size_type i = 0; i < SKIP_GRAM_WINDOW_SIZE; i++)
        {   
            // Check if the left context word index is valid.
            if ((*pair->getLeft())[(SKIP_GRAM_WINDOW_SIZE - 1) - i] < INDEX_ORIGINATES_AT_VALUE) 
            {
                std::cout<< "NONE" << " ";    
            }
            else 
            {
                // Display the left context word.
                std::cout<< vocab[(*pair->getLeft())[(SKIP_GRAM_WINDOW_SIZE - 1) - i]].c_str() << " ";
            }
        }
        
        // Display center/target word.
        std::cout<< "[ " << vocab[pair->getCenterWord()].c_str() << " ]" << " ";
                
        // Display right context words.        
        for (cc_tokenizer::string_character_traits<char>::size_type i = 0; i < SKIP_GRAM_WINDOW_SIZE; i++)
        {
            // Check if the right context word index is valid.
            if ((*(pair->getRight()))[i] < INDEX_ORIGINATES_AT_VALUE)
            {
                std::cout<< "NONE" << " ";
            }
            else
            {
                // Display the right context word.
                std::cout<< vocab[(*(pair->getRight()))[i]].c_str() << " ";
            }
        }
        std::cout<< std::endl;
    }
     */
    
    /* BLOCK 2 */
    /*             
    for (cc_tokenizer::string_character_traits<char>::size_type i = 1; i <= pairs.get_number_of_word_pairs(); i++)
    {
        WORDPAIRS_PTR pair = pairs.get_word_pair_by_number(i); 
       
        // Display left context words. Start from the farthest one from the center word.
        for (cc_tokenizer::string_character_traits<char>::size_type j = 0; j < SKIP_GRAM_WINDOW_SIZE; j++)
        { 
            // Check if the left context word index is valid.
            if (pair->getLeft((SKIP_GRAM_WINDOW_SIZE - 1) - j) < INDEX_ORIGINATES_AT_VALUE) 
            {
                std::cout<< "NONE" << " ";    
            }
            else 
            {
                // Display the left context word.
                std::cout<< vocab[pair->getLeft((SKIP_GRAM_WINDOW_SIZE - 1) - j)].c_str() << " ";
            }
        }

        // Display center/target word.
        std::cout<< "[ " << vocab[pair->getCenterWord()].c_str() << " ]" << " ";
                
        // Display right context words.        
        for (cc_tokenizer::string_character_traits<char>::size_type j = 0; j < SKIP_GRAM_WINDOW_SIZE; j++)
        {
            // Check if the right context word index is valid.
            if ((*pair->getRight())[j] < INDEX_ORIGINATES_AT_VALUE)
            {
                std::cout<< "NONE" << " ";
            }
            else
            {
                // Display the right context word.
                std::cout<< vocab[(*pair->getRight())[j]].c_str() << " ";
            }
        }
        std::cout<< std::endl;
    }
     */
                            
    return 0;
}