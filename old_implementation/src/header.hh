/*
    src/header.hh
    Q@khaa.pk
 */

#ifndef MOTHER_MOTHER_WHEN_YOU_SENT_ME_OFF_TO_SEE_THE_WORLD_WERE_YOU_SCARED_THAT_I_MIGHT_GET_HURT_HEADER_HH
#define MOTHER_MOTHER_WHEN_YOU_SENT_ME_OFF_TO_SEE_THE_WORLD_WERE_YOU_SCARED_THAT_I_MIGHT_GET_HURT_HEADER_HH

#define PAIRS_TRAINING_PHASE true
#define PAIRS_VALIDATION_PHASE false 
#define PAIRS_VOCABULARY_TRAINING_SPLIT(c) ((cc_tokenizer::string_character_traits<char>::size_type)((c)*0.80))
#define PAIRS_VOCABULARY_VALIDATION_SPLIT(c) ((cc_tokenizer::string_character_traits<char>::size_type)((c)*0.20))

#define PAIRS_VOCABULARY_TRAINING_SPLIT_OLD(c) ((((c)/100.0))*80)
#define PAIRS_VOCABULARY_VALIDATION_SPLIT_OLD(c) (((c)/100)*20) 

#include "../../Numcy/header.hh"
#include "../../corpus/corpus.hh"

#include "hyper-parameters.hh"
#include "pairs.hh"

#endif

