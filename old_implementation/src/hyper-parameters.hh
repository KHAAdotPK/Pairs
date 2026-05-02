/*
    lib/WordEmbedding-Algorithms/ML/Word2Vec/Skip-gram/hyper-parameters.hh
    Q@khaa.pk
 */

#ifndef SKIP_GRAM_HYPER_PARAMETERS_PAIRS_HEADER_HH
#define SKIP_GRAM_HYPER_PARAMETERS_PAIRS_HEADER_HH

/*
   Size of window of context words around a target/center word, and use the context words to predict the target word(in CBOW/Skip-Gram model) 
   In the Skip-gram model, the model predicts the context words given a target word
 */
#ifndef SKIP_GRAM_CONTEXT_WINDOW_SIZE 
#define SKIP_GRAM_CONTEXT_WINDOW_SIZE 2
#endif

#endif