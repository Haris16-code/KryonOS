#ifndef TEXT_RUNNER_H
#define TEXT_RUNNER_H

#include "../ModelHeader.h"
#include <Arduino.h>
#include <vector>

namespace LocalAI {

class TextRunner {
public:
    TextRunner();
    ~TextRunner();

    bool init(const ModelHeader& header, const uint8_t* weights_blob, const char* vocab_table);
    void reset();
    void freeMemory();

    // Tokenizer
    std::vector<int> encode(const String& text);
    String decode(int tokenId);

    // Forward inference step with repetition penalty
    int step(int tokenId, float temperature = 0.7f, int top_k = 40, float top_p = 0.9f);
    const float* getLogits() const { return m_logits; }
    int getVocabSize() const { return m_header.vocab_size; }
    int getCurrentPos() const { return m_pos; }

private:
    void forward(int token, int pos);

    ModelHeader m_header;
    const uint8_t* m_weights;
    const char* m_vocabTable;
    std::vector<const char*> m_vocabIndex;

    int m_pos;
    std::vector<int> m_recentTokens;

    // INT8 KV-Cache in PSRAM
    int8_t* m_keyCache;     // (n_layers * seq_len * dim)
    int8_t* m_valCache;     // (n_layers * seq_len * dim)
    float*  m_keyScales;    // (n_layers * seq_len)
    float*  m_valScales;    // (n_layers * seq_len)

    // Working scratchpads
    float* m_x;             // (dim)
    float* m_xb;            // (dim)
    float* m_xb2;           // (dim)
    float* m_qkv;           // (3 * dim)
    float* m_att;           // (seq_len)
    float* m_mlp_hidden;    // (4 * dim)
    float* m_logits;        // (vocab_size)
    int8_t* m_q_int8;       // (dim) INT8 scratchpad
    int8_t* m_mlp_int8;     // (4 * dim) INT8 scratchpad
};

} // namespace LocalAI

#endif // TEXT_RUNNER_H
