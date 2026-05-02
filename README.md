# Pairs — Skip-gram Training Pair Generator

[![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)](https://isocpp.org/)
[![Status](https://img.shields.io/badge/status-rewrite%20in%20progress-orange)]()

---

## What Is This Package

`Pairs` generates every training example that the Skip-gram model will ever consume,
in a single pass over a pre-built vocabulary table, before training begins.

A Skip-gram training example is exactly one pair:

```
(center_word_id, context_word_id)
```

For the sentence `"the cat sat on the mat"` with a context window of size 2,
the center word `"sat"` produces four training pairs:

```
(sat, the)    ← 2 positions to the left
(sat, cat)    ← 1 position to the left
(sat, on)     ← 1 position to the right
(sat, the)    ← 2 positions to the right
```

Both values in each pair are `size_t` vocabulary indices — direct row indices into
the embedding matrix `E` of shape `[vocab_size × d_model]`. No strings. No one-hot
vectors. No corpus re-reading.

---

## Role in the NLP Pipeline

```
corpus file
    │
    ▼
Parser::build_hash_table()          ← one pass, builds TABLES
    │
    ▼
TABLES                              ← vocabulary + corpus layout, fully in memory
    ├── hash_to_word_record[]       ← token string → WordRecord (word_id, occurrences)
    ├── word_id_to_hash[]           ← word_id → bucket index
    └── lines → LINE → TOKEN        ← full corpus in order, token_ids pre-assigned
    │
    ▼
Pairs(TABLES*)                      ← this package, reads TABLES, emits pairs
    │
    ▼
WORDPAIRS linked list               ← the training dataset
    │
    ▼
Skip-gram training loop             ← consumes pairs, updates E via backprop
```

---

## Why the Old Implementation Existed — and Why It Is Being Rewritten

### The Old Implementation

The original `Pairs` package (`skip-gram-pairs.hh`) took a `CORPUS&` object as its
input. `CORPUS` was a flat, string-based vocabulary that carried no pre-built positional
index. To generate pairs, the constructor had to:

1. Loop over `vocab.numberOfTokens()` — every token occurrence including repetitions.
2. For each position `i`, call `vocab(i + INDEX_ORIGINATES_AT_VALUE, true)` to get the
   vocabulary index of the center word — a lookup involving string comparison or
   scanning inside `CORPUS`.
3. For each context position `i - j` and `i + j` (where `j` runs 1 to
   `SKIP_GRAM_CONTEXT_WINDOW_SIZE`), call `vocab(...)` again to find the context word's
   vocabulary index — more scanning.
4. Allocate a separate `CONTEXTWORDS` struct on the heap for the left side and another
   for the right side, then fill them with the retrieved indices.
5. Use `INDEX_NOT_FOUND_AT_VALUE` as a sentinel to mark positions where no context word
   existed (start/end of corpus), because there was no structural way to know where a
   line ended.

For a corpus of `T` total tokens with window size `W`, the constructor performed
roughly `T × 2W` vocabulary lookups — each one a string scan through `CORPUS`.

On top of this, the package included `write()` and `read()` methods to serialise the
generated pairs to a binary file on disk — because pair generation was expensive enough
that re-running it from scratch on every training session was unacceptable.

### The Root Cause

The old `CORPUS` object had no positional index. It did not know where a word appeared.
It did not preserve line structure. It did not pre-assign integer IDs to tokens. Every
piece of information the pair generator needed had to be discovered by scanning, every
time, from strings.

### What Changed

`Parser::build_hash_table()` now returns a `TABLES*` that pre-computes and stores
everything the pair generator needs, during the single corpus pass that was already
happening for vocabulary construction:

| What the old code had to discover | Where it lives in TABLES |
|---|---|
| Vocabulary index of center word | `TOKEN::token_id` — already assigned |
| Vocabulary index of context word | `TOKEN::token_id` of neighbouring TOKEN nodes |
| Whether a context position is valid | `LINE::n` — exact token count per line |
| Line boundary detection | `LINE` node boundary — structural, not sentinel-based |
| Word string for debugging | `hash_table[index_table[token_id]]->word` — O(1) |
| Frequency / repetition | Every occurrence already in the `LINE → TOKEN` list |

The new constructor does not scan `CORPUS`. It does not call any vocabulary lookup
function. It does not allocate separate `CONTEXTWORDS` structs per pair. It walks a
linked list that is already in memory, reading `size_t` values that are already the
correct vocabulary indices.

---

## Old vs New — Side by Side

### Data Structures

**Old `WORDPAIRS`:**
```cpp
struct WordPairs
{
    CONTEXTWORDS_PTR left;      // heap-allocated, up to W indices, left side
    size_t centerWord;          // vocabulary index of center word
    CONTEXTWORDS_PTR right;     // heap-allocated, up to W indices, right side
    struct WordPairs* next;
    struct WordPairs* prev;
};

struct ContextWord
{
    size_t n;                                    // how many valid entries
    size_t array[SKIP_GRAM_CONTEXT_WINDOW_SIZE]; // padded with INDEX_NOT_FOUND_AT_VALUE
};
```

**New `WORDPAIRS`:**
```cpp
struct WordPairs
{
    size_t center_id;    // TOKEN::token_id of center word — row index into E
    size_t context_id;   // TOKEN::token_id of context word — row index into E

    struct WordPairs* next;
    struct WordPairs* prev;
};
```

A pair is two integers. Nothing else is needed. The left/right distinction disappears
because Skip-gram treats all context words identically regardless of which side of the
center word they are on — both generate the same training signal. A center word with
`W=2` context words on each side produces 4 separate `WORDPAIRS` nodes, one per context
word. This is correct and matches how Skip-gram is defined mathematically.

---

### Constructor

**Old constructor — O(T × W) vocabulary lookups, string-based:**
```cpp
Pairs(CORPUS& vocab, bool verbose = false)
{
    for (size_t i = 0; i < vocab.numberOfTokens(); i++)
    {
        // String lookup to get center word vocabulary index
        centerWord = vocab[vocab(i + INDEX_ORIGINATES_AT_VALUE, true)];

        // String lookup for each left context position
        for (size_t j = SKIP_GRAM_CONTEXT_WINDOW_SIZE; j > 0; j--)
        {
            if (vocab((i + INDEX_ORIGINATES_AT_VALUE) - j, true).size())
                left->array[j-1] = vocab[vocab((i + INDEX_ORIGINATES_AT_VALUE) - j, true)];
            else
                left->array[j-1] = INDEX_NOT_FOUND_AT_VALUE; // sentinel for boundary
        }
        // ... same string lookups for right context ...
    }
}
```

**New constructor — O(T × W) array reads, zero lookups:**
```cpp
Pairs(TABLES* tables)
{
    LINE* current_line = tables->lines;

    while (current_line != nullptr)
    {
        size_t n = current_line->n;

        // Collect token_ids for this line — one pass, no lookup
        size_t* token_ids = new size_t[n];
        TOKEN* t = current_line->tokens;
        for (size_t k = 0; k < n; k++, t = t->next)
            token_ids[k] = t->token_id;

        // Slide window — pure arithmetic, no strings, no sentinels
        for (size_t i = 0; i < n; i++)
        {
            size_t left  = (i >= W) ? i - W : 0;
            size_t right = (i + W < n) ? i + W : n - 1;

            for (size_t j = left; j <= right; j++)
            {
                if (j == i) continue;
                // allocate WORDPAIRS node
                // set center_id  = token_ids[i]
                // set context_id = token_ids[j]
                // append to list, increment n
            }
        }

        delete[] token_ids;
        current_line = current_line->next;
    }
}
```

The inner body is two array reads and a linked list append. No string. No hash.
No sentinel. No `INDEX_NOT_FOUND_AT_VALUE` anywhere.

---

### Boundary Handling

**Old:** Used `INDEX_NOT_FOUND_AT_VALUE` as a sentinel to mark missing context
positions. Every consumer of a pair had to check for this sentinel before using an
index. The original source explicitly carried a TODO comment flagging that the last
token of the corpus was not handled correctly when it served as a center word with no
right context.

**New:** No sentinel needed. The window is clipped arithmetically against `LINE::n`.
If the center word is at position `0`, the left boundary is `0`. If it is at position
`n-1`, the right boundary is `n-1`. No invalid index is ever stored. The line boundary
is the structural boundary — the window never crosses from one `LINE` node to the next,
which is the correct Skip-gram behaviour (each sentence/line is treated independently).

---

### Serialisation — `write()` and `read()`

The old implementation included full binary serialisation because pair generation was
expensive enough to justify saving results to disk and reloading on subsequent training
runs. The binary format per record was:

```
[left[0], ..., left[W-1],  centerWord,  right[0], ..., right[W-1]]
  W × size_t               1 × size_t   W × size_t
```

Fixed-size records allowed `read()` to compute pair count from file size without
storing an explicit count header.

**The new implementation may not need `write()`/`read()` at all.** Since pair
generation is now O(T × W) array reads with zero vocabulary lookups, it is fast enough
to re-run from `TABLES` at the start of every training session. If serialisation is
still desired for distributed training or checkpoint restart, the new binary format is
simpler:

```
[center_id, context_id]   // two size_t values per record, one record per pair
```

Half the size of the old format. No sentinel values. No left/right arrays to store.

---

### Training / Validation Split

The old `go_to_next_word_pair(bool phase)` method and `the_80_20_split_counter` field
implement an 80/20 train/validation split by dividing the linked list at position
`n × 0.80`. This logic is correct and carries forward unchanged:

```cpp
#define PAIRS_VOCABULARY_TRAINING_SPLIT(c)   ((size_t)((c) * 0.80))
#define PAIRS_VOCABULARY_VALIDATION_SPLIT(c) ((size_t)((c) * 0.20))
```

Training iterates pairs `[0, n×0.80)`. Validation iterates pairs `[n×0.80, n)`.
The iterator resets correctly at the end of each phase.

---

### Shuffle

The old `shuffle(a, b)` swaps the content of two `WORDPAIRS` nodes at zero-based
positions `a` and `b` by walking the list to find them, then swapping `centerWord`,
`left`, and `right` using the copy constructor.

In the new implementation, swapping a node means swapping two `size_t` values
(`center_id` and `context_id`) — no heap pointer bookkeeping involved. The
walk-and-swap pattern is otherwise identical. Alternatively, since a pair is now just
two integers, a flat array of `WORDPAIRS` structs would make shuffle O(1) via direct
index access instead of O(n) list traversal.

---

### Reference Count

`reference_count`, `incrementReferenceCount()`, and `decrementReferenceCount()` carry
forward unchanged. The ownership model mirrors `TABLES::ref_count` — the `Pairs`
object is heap-allocated, ownership transfers to the caller, and the reference count
controls deallocation.

---

## One-Hot Vector Replacement

This is the most important conceptual role of the `Pairs` package in the full model.

The classical mathematical formulation of Skip-gram uses one-hot input vectors:

```
vocab_size = 50,000
center word "sat" has word_id = 42

one-hot = [0, 0, ..., 1, ..., 0]   ← 50,000 floats, one 1 at position 42

embedding = E × one-hot             ← matrix × vector: O(vocab_size × d_model)
          = E[42]                   ← which reduces to just row 42 of E
```

The matrix multiplication is entirely wasteful — 49,999 of the 50,000 multiplications
produce zero. The one-hot vector exists only as a mathematical convenience in the
derivation. It should never be materialised in code.

`Pairs` eliminates it entirely. The training loop receives a `WORDPAIRS` node with
`center_id = 42` and looks up `E[42]` directly:

```cpp
// One-hot approach — never do this
float one_hot[vocab_size] = {0};
one_hot[pair->center_id] = 1.0f;
float* embedding = matmul(E, one_hot);   // O(vocab_size × d_model), wasteful

// Pairs approach — direct row access
float* embedding = E[pair->center_id];   // O(1)
```

`center_id` and `context_id` are `TOKEN::token_id` values from `TABLES`, which are
dense sequential integers in `[0, vocab_size)`. They are already valid row indices into
`E`. No translation step. No sentinel check. No one-hot vector at any stage.

The full forward pass for one training pair reduces to:

```
center_embedding  = E[pair->center_id]   // O(1) row lookup
context_embedding = E[pair->context_id]  // O(1) row lookup
score = dot(center_embedding, context_embedding)
loss  = cross_entropy(score, label)      // label=1 for positive pair
// backprop updates E[center_id] and E[context_id] only — O(d_model)
```

`vocab_size` never appears in any hot path.

---

## Hyper-Parameters

Defined in `hyper-parameters.hh`. Carry forward unchanged.

```cpp
#ifndef SKIP_GRAM_CONTEXT_WINDOW_SIZE
#define SKIP_GRAM_CONTEXT_WINDOW_SIZE 2
#endif
```

`W = SKIP_GRAM_CONTEXT_WINDOW_SIZE` controls how many words to the left and right of
each center word are considered context. A center word at position `i` generates at
most `2W` training pairs (fewer near line boundaries). Total pairs in the dataset:

```
total_pairs ≈ total_tokens × 2W   (minus boundary shortfall)
```

Where `total_tokens = TABLES::total_tokens`.

---

## Implementation Checklist

When rewriting this package against `TABLES`, the following must be implemented or
carried forward:

- [ ] `struct WordPairs` — `center_id`, `context_id`, `next`, `prev`
- [ ] `struct Pairs` constructor taking `TABLES*` — double loop over `LINE`/`TOKEN`
- [ ] Window clipping against `LINE::n` — no sentinel values, pure arithmetic
- [ ] Line boundary enforcement — window does not cross `LINE` node boundaries
- [ ] `go_to_next_word_pair()` — plain iterator, carry forward as-is
- [ ] `go_to_next_word_pair(bool phase)` — 80/20 split iterator, carry forward as-is
- [ ] `get_current_word_pair()` — carry forward as-is
- [ ] `get_number_of_word_pairs()` — carry forward as-is
- [ ] `shuffle(a, b)` — carry forward, simplify swap (no heap pointers in new pair)
- [ ] `reference_count` + increment/decrement — carry forward as-is
- [ ] `~Pairs()` destructor — walk list tail-to-head, deallocate each node
- [ ] `write()` — optional; if kept, new format is `[center_id, context_id]` per record
- [ ] `read()` — optional; paired with new `write()`
- [ ] `PAIRS_TRAINING_PHASE` / `PAIRS_VALIDATION_PHASE` macros — carry forward
- [ ] `PAIRS_VOCABULARY_TRAINING_SPLIT` / `PAIRS_VOCABULARY_VALIDATION_SPLIT` — carry forward

---

## What Is NOT Needed in the New Implementation

These existed in the old code to compensate for what `CORPUS` could not provide.
None are needed when building against `TABLES`.

| Old element | Why it existed | Why it is gone |
|---|---|---|
| `CONTEXTWORDS` struct | Stored up to W indices with count and sentinel padding | A pair is one center + one context — emit one node per pair |
| `left[]` and `right[]` arrays | Distinguished left-side from right-side context words | Skip-gram treats both identically; side does not matter |
| `INDEX_NOT_FOUND_AT_VALUE` sentinel | Marked invalid positions at corpus boundaries | `LINE::n` gives exact bounds; no invalid position is ever generated |
| `vocab(i, true)` calls | Translated corpus position to vocabulary index | `TOKEN::token_id` is already the vocabulary index |
| `vocab[index]` string retrieval | Got word string for index translation | Never needed; `token_id` is the index directly |
| `write()` / `read()` | Pair generation was too slow to redo each session | Generation from `TABLES` is O(T × W) array reads — fast to redo |
| Corpus boundary TODO comment | Last token's right context was not handled safely | `LINE::n` makes every boundary safe and structurally enforced |

---

## Related Packages

| Package | Role |
|---|---|
| [Parser](https://github.com/KHAAdotPK/Parser.git) | Builds `TABLES` from corpus in one pass |
| [Naqsh](https://github.com/KHAAdotPK/Naqsh.git) | Urdu text cleaner, plugs into Parser's Iterator |
| [imprint](https://github.com/KHAAdotPK/imprint.git) | English text cleaner, counterpart to Naqsh |
| [Numcy](https://github.com/KHAAdotPK/Numcy.git) | Numerical compute layer used by training loop |

---

## License

This project is governed by a license, the details of which can be located in the
accompanying file named `LICENSE`.
