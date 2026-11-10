#include "TextRunner.h"
#include "../Ops/Ops.h"
#include <esp_heap_caps.h>
#include <string.h>
#include <math.h>

namespace LocalAI {

TextRunner::TextRunner()
    : m_weights(nullptr),
      m_vocabTable(nullptr),
      m_pos(0),
      m_keyCache(nullptr),
      m_valCache(nullptr),
      m_keyScales(nullptr),
      m_valScales(nullptr),
      m_x(nullptr),
      m_xb(nullptr),
      m_xb2(nullptr),
      m_qkv(nullptr),
      m_att(nullptr),
      m_mlp_hidden(nullptr),
      m_logits(nullptr),
      m_q_int8(nullptr),
      m_mlp_int8(nullptr) {
    memset(&m_header, 0, sizeof(m_header));
}

TextRunner::~TextRunner() {
    freeMemory();
}

void TextRunner::freeMemory() {
    if (m_keyCache)   { heap_caps_free(m_keyCache);   m_keyCache = nullptr; }
    if (m_valCache)   { heap_caps_free(m_valCache);   m_valCache = nullptr; }
    if (m_keyScales)  { heap_caps_free(m_keyScales);  m_keyScales = nullptr; }
    if (m_valScales)  { heap_caps_free(m_valScales);  m_valScales = nullptr; }

    if (m_x)          { heap_caps_free(m_x);          m_x = nullptr; }
    if (m_xb)         { heap_caps_free(m_xb);         m_xb = nullptr; }
    if (m_xb2)        { heap_caps_free(m_xb2);        m_xb2 = nullptr; }
    if (m_qkv)        { heap_caps_free(m_qkv);        m_qkv = nullptr; }
    if (m_att)        { heap_caps_free(m_att);        m_att = nullptr; }
    if (m_mlp_hidden) { heap_caps_free(m_mlp_hidden); m_mlp_hidden = nullptr; }
    if (m_logits)     { heap_caps_free(m_logits);     m_logits = nullptr; }
    if (m_q_int8)     { heap_caps_free(m_q_int8);     m_q_int8 = nullptr; }
    if (m_mlp_int8)   { heap_caps_free(m_mlp_int8);   m_mlp_int8 = nullptr; }

    m_vocabIndex.clear();
    m_recentTokens.clear();
    m_pos = 0;
}

bool TextRunner::init(const ModelHeader& header, const uint8_t* weights_blob, const char* vocab_table) {
    freeMemory();
    m_header = header;
    m_weights = weights_blob;
    m_vocabTable = vocab_table;

    // Index vocabulary table in PSRAM (Zero memory duplication)
    m_vocabIndex.reserve(header.vocab_size);
    if (vocab_table && strlen(vocab_table) > 0) {
        const char* ptr = vocab_table;
        for (int i = 0; i < (int)header.vocab_size; i++) {
            m_vocabIndex.push_back(ptr);
            size_t slen = strlen(ptr);
            ptr += slen + 1;
            if (slen == 0 && i < (int)header.vocab_size - 1) {
                for (int j = i + 1; j < (int)header.vocab_size; j++) {
                    m_vocabIndex.push_back("");
                }
                break;
            }
        }
    } else {
        // Fallback token index
        static std::vector<String> s_fallbackTokens;
        s_fallbackTokens.clear();
        s_fallbackTokens.reserve(header.vocab_size);

        s_fallbackTokens.push_back("<|system|>");    // 0
        s_fallbackTokens.push_back("<|user|>");      // 1
        s_fallbackTokens.push_back("<|assistant|>"); // 2
        s_fallbackTokens.push_back("<|end|>");       // 3
        s_fallbackTokens.push_back("<pad>");         // 4
        s_fallbackTokens.push_back("<call:>");       // 5

        for (int b = 6; b < (int)header.vocab_size; b++) {
            if (b < 128) {
                char ch = (char)(b >= 32 ? b : (b == 10 ? '\n' : ' '));
                s_fallbackTokens.push_back(String(ch));
            } else {
                s_fallbackTokens.push_back(" ");
            }
        }

        for (int i = 0; i < (int)header.vocab_size; i++) {
            m_vocabIndex.push_back(s_fallbackTokens[i].c_str());
        }
    }

    // Allocate INT8 KV-Cache in PSRAM
    size_t kv_size = (size_t)header.n_layers * header.seq_len * header.dim * sizeof(int8_t);
    size_t scale_size = (size_t)header.n_layers * header.seq_len * sizeof(float);

    m_keyCache = (int8_t*)heap_caps_malloc(kv_size, MALLOC_CAP_SPIRAM);
    m_valCache = (int8_t*)heap_caps_malloc(kv_size, MALLOC_CAP_SPIRAM);
    m_keyScales = (float*)heap_caps_malloc(scale_size, MALLOC_CAP_SPIRAM);
    m_valScales = (float*)heap_caps_malloc(scale_size, MALLOC_CAP_SPIRAM);

    if (!m_keyCache || !m_valCache || !m_keyScales || !m_valScales) {
        Serial.println("[TextRunner] Failed to allocate KV-cache in PSRAM");
        freeMemory();
        return false;
    }

    memset(m_keyCache, 0, kv_size);
    memset(m_valCache, 0, kv_size);

    int dim = header.dim;
    int vocab = header.vocab_size;
    int seq = header.seq_len;

    // Allocate activation scratchpads
    m_x          = (float*)heap_caps_malloc(dim * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    m_xb         = (float*)heap_caps_malloc(dim * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    m_xb2        = (float*)heap_caps_malloc(dim * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    m_qkv        = (float*)heap_caps_malloc(3 * dim * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    m_att        = (float*)heap_caps_malloc(seq * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    m_mlp_hidden = (float*)heap_caps_malloc(4 * dim * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    m_logits     = (float*)heap_caps_malloc(vocab * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    m_q_int8     = (int8_t*)heap_caps_malloc(dim * sizeof(int8_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    m_mlp_int8   = (int8_t*)heap_caps_malloc(4 * dim * sizeof(int8_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    // Fallback to Octal PSRAM if internal SRAM is full
    if (!m_x)          m_x          = (float*)heap_caps_malloc(dim * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!m_xb)         m_xb         = (float*)heap_caps_malloc(dim * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!m_xb2)        m_xb2        = (float*)heap_caps_malloc(dim * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!m_qkv)        m_qkv        = (float*)heap_caps_malloc(3 * dim * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!m_att)        m_att        = (float*)heap_caps_malloc(seq * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!m_mlp_hidden) m_mlp_hidden = (float*)heap_caps_malloc(4 * dim * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!m_logits)     m_logits     = (float*)heap_caps_malloc(vocab * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!m_q_int8)     m_q_int8     = (int8_t*)heap_caps_malloc(dim * sizeof(int8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!m_mlp_int8)   m_mlp_int8   = (int8_t*)heap_caps_malloc(4 * dim * sizeof(int8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!m_x || !m_xb || !m_xb2 || !m_qkv || !m_att || !m_mlp_hidden || !m_logits || !m_q_int8 || !m_mlp_int8) {
        Serial.println("[TextRunner] Failed to allocate activation scratchpads");
        freeMemory();
        return false;
    }

    reset();
    return true;
}

void TextRunner::reset() {
    m_pos = 0;
    m_recentTokens.clear();
    if (m_keyCache && m_header.n_layers > 0) {
        size_t kv_size = (size_t)m_header.n_layers * m_header.seq_len * m_header.dim * sizeof(int8_t);
        memset(m_keyCache, 0, kv_size);
        memset(m_valCache, 0, kv_size);
    }
}

std::vector<int> TextRunner::encode(const String& text) {
    std::vector<int> tokens;
    if (m_vocabIndex.empty() || text.length() == 0) return tokens;

    int text_len = text.length();
    int cur_idx = 0;

    while (cur_idx < text_len) {
        int best_token = -1;
        int best_match_len = 0;

        for (int i = 0; i < (int)m_vocabIndex.size(); i++) {
            const char* entry = m_vocabIndex[i];
            int elen = strlen(entry);
            if (elen > 0 && elen <= (text_len - cur_idx)) {
                if (strncmp(text.c_str() + cur_idx, entry, elen) == 0) {
                    if (elen > best_match_len) {
                        best_match_len = elen;
                        best_token = i;
                    }
                }
            }
        }

        if (best_token >= 0) {
            tokens.push_back(best_token);
            cur_idx += best_match_len;
        } else {
            tokens.push_back((uint8_t)text[cur_idx]);
            cur_idx++;
        }
    }

    return tokens;
}

String TextRunner::decode(int tokenId) {
    if (tokenId >= 0 && tokenId < (int)m_vocabIndex.size()) {
        const char* s = m_vocabIndex[tokenId];
        // Filter out system tags from display
        if (strcmp(s, "<|system|>") == 0 || strcmp(s, "<|user|>") == 0 ||
            strcmp(s, "<|assistant|>") == 0 || strcmp(s, "<pad>") == 0) {
            return String("");
        }
        if (strcmp(s, "<|end|>") == 0) {
            return String("\n");
        }
        return String(s);
    }
    if (tokenId >= 0 && tokenId < 256) {
        char ch = (char)tokenId;
        return String(ch);
    }
    return String("");
}

void TextRunner::forward(int token, int pos) {
    int dim = m_header.dim;
    int n_layers = m_header.n_layers;
    int n_heads = m_header.n_heads;
    int head_size = (n_heads > 0) ? (dim / n_heads) : dim;
    int seq_len = m_header.seq_len;

    if (token < 0 || token >= (int)m_header.vocab_size) token = 0;
    if (pos >= seq_len) pos = seq_len - 1;

    size_t weight_offset = 0;

    // 1. Token Embeddings (WTE)
    float wte_scale = *(const float*)(m_weights + weight_offset);
    weight_offset += sizeof(float);
    const int8_t* wte_data = (const int8_t*)(m_weights + weight_offset);
    weight_offset += (size_t)m_header.vocab_size * dim;

    // 2. Position Embeddings (WPE)
    float wpe_scale = *(const float*)(m_weights + weight_offset);
    weight_offset += sizeof(float);
    const int8_t* wpe_data = (const int8_t*)(m_weights + weight_offset);
    weight_offset += (size_t)seq_len * dim;

    // Combine Token + Positional Embeddings
    const int8_t* tok_emb = wte_data + ((size_t)token * dim);
    const int8_t* pos_emb = wpe_data + ((size_t)pos * dim);
    for (int i = 0; i < dim; i++) {
        m_x[i] = ((float)tok_emb[i] * wte_scale) + ((float)pos_emb[i] * wpe_scale);
    }

    // 3. Transformer Blocks
    for (int l = 0; l < n_layers; l++) {
        // A. LayerNorm 1 (Weight + Bias)
        const float* ln1_w = (const float*)(m_weights + weight_offset);
        weight_offset += dim * sizeof(float);
        const float* ln1_b = (const float*)(m_weights + weight_offset);
        weight_offset += dim * sizeof(float);
        Ops::layernorm(m_xb, m_x, ln1_w, ln1_b, dim);

        // B. QKV Projection (c_attn: [3 * dim, dim])
        float c_attn_scale = *(const float*)(m_weights + weight_offset);
        weight_offset += sizeof(float);
        const int8_t* c_attn_w = (const int8_t*)(m_weights + weight_offset);
        weight_offset += (size_t)(3 * dim) * dim;

        float xb_scale = Ops::quantize_f32_to_int8(m_q_int8, m_xb, dim);
        Ops::gemv_int8_f32(m_qkv, c_attn_w, m_q_int8, nullptr, dim, 3 * dim);
        float total_qkv_scale = c_attn_scale * xb_scale;
        for (int i = 0; i < 3 * dim; i++) {
            m_qkv[i] *= total_qkv_scale;
        }

        float* q = m_qkv;
        float* k = m_qkv + dim;
        float* v = m_qkv + 2 * dim;

        // C. Store K & V into INT8 KV-Cache in PSRAM
        size_t cache_idx = ((size_t)l * seq_len + pos) * dim;
        float k_scale = Ops::quantize_f32_to_int8(m_keyCache + cache_idx, k, dim);
        float v_scale = Ops::quantize_f32_to_int8(m_valCache + cache_idx, v, dim);
        m_keyScales[l * seq_len + pos] = k_scale;
        m_valScales[l * seq_len + pos] = v_scale;

        // D. Multi-Head Attention
        float sqrt_head = 1.0f / sqrtf((float)head_size);
        for (int h = 0; h < n_heads; h++) {
            float* q_head = q + (h * head_size);

            for (int t = 0; t <= pos; t++) {
                size_t k_idx = ((size_t)l * seq_len + t) * dim + (h * head_size);
                const int8_t* k_head_int8 = m_keyCache + k_idx;
                float cur_k_scale = m_keyScales[l * seq_len + t];

                float score = 0.0f;
                for (int i = 0; i < head_size; i++) {
                    score += q_head[i] * ((float)k_head_int8[i] * cur_k_scale);
                }
                m_att[t] = score * sqrt_head;
            }

            Ops::softmax(m_att, pos + 1);

            float* xb_head = m_xb + (h * head_size);
            for (int i = 0; i < head_size; i++) xb_head[i] = 0.0f;

            for (int t = 0; t <= pos; t++) {
                float a = m_att[t];
                size_t v_idx = ((size_t)l * seq_len + t) * dim + (h * head_size);
                const int8_t* v_head_int8 = m_valCache + v_idx;
                float cur_v_scale = m_valScales[l * seq_len + t];

                for (int i = 0; i < head_size; i++) {
                    xb_head[i] += a * ((float)v_head_int8[i] * cur_v_scale);
                }
            }
        }

        // E. Attention Output Projection (c_proj: [dim, dim]) + Residual Add
        float c_proj_scale = *(const float*)(m_weights + weight_offset);
        weight_offset += sizeof(float);
        const int8_t* c_proj_w = (const int8_t*)(m_weights + weight_offset);
        weight_offset += (size_t)dim * dim;

        float xb_att_scale = Ops::quantize_f32_to_int8(m_q_int8, m_xb, dim);
        Ops::gemv_int8_f32(m_xb2, c_proj_w, m_q_int8, nullptr, dim, dim);
        float total_proj_scale = c_proj_scale * xb_att_scale;
        for (int i = 0; i < dim; i++) {
            m_xb2[i] *= total_proj_scale;
        }
        Ops::vec_add(m_x, m_x, m_xb2, dim);

        // F. LayerNorm 2 (Weight + Bias)
        const float* ln2_w = (const float*)(m_weights + weight_offset);
        weight_offset += dim * sizeof(float);
        const float* ln2_b = (const float*)(m_weights + weight_offset);
        weight_offset += dim * sizeof(float);
        Ops::layernorm(m_xb, m_x, ln2_w, ln2_b, dim);

        // G. MLP FC Expansion (c_fc: [4 * dim, dim]) + GELU
        float mlp_fc_scale = *(const float*)(m_weights + weight_offset);
        weight_offset += sizeof(float);
        const int8_t* mlp_fc_w = (const int8_t*)(m_weights + weight_offset);
        weight_offset += (size_t)(4 * dim) * dim;

        float xb_mlp_scale = Ops::quantize_f32_to_int8(m_q_int8, m_xb, dim);
        Ops::gemv_int8_f32(m_mlp_hidden, mlp_fc_w, m_q_int8, nullptr, dim, 4 * dim);
        float total_mlp_fc_scale = mlp_fc_scale * xb_mlp_scale;
        for (int i = 0; i < 4 * dim; i++) {
            m_mlp_hidden[i] *= total_mlp_fc_scale;
        }
        Ops::silu(m_mlp_hidden, 4 * dim);

        // H. MLP Output Projection (c_proj: [dim, 4 * dim]) + Residual Add
        float mlp_proj_scale = *(const float*)(m_weights + weight_offset);
        weight_offset += sizeof(float);
        const int8_t* mlp_proj_w = (const int8_t*)(m_weights + weight_offset);
        weight_offset += (size_t)dim * (4 * dim);

        float mlp_act_scale = Ops::quantize_f32_to_int8(m_mlp_int8, m_mlp_hidden, 4 * dim);
        Ops::gemv_int8_f32(m_xb2, mlp_proj_w, m_mlp_int8, nullptr, 4 * dim, dim);
        float total_mlp_proj_scale = mlp_proj_scale * mlp_act_scale;
        for (int i = 0; i < dim; i++) {
            m_xb2[i] *= total_mlp_proj_scale;
        }
        Ops::vec_add(m_x, m_x, m_xb2, dim);
    }

    // 4. Final LayerNorm (Weight + Bias)
    const float* lnf_w = (const float*)(m_weights + weight_offset);
    weight_offset += dim * sizeof(float);
    const float* lnf_b = (const float*)(m_weights + weight_offset);
    weight_offset += dim * sizeof(float);
    Ops::layernorm(m_xb, m_x, lnf_w, lnf_b, dim);

    // 5. LM Head Logits (lm_head: [vocab_size, dim])
    float lm_head_scale = *(const float*)(m_weights + weight_offset);
    weight_offset += sizeof(float);
    const int8_t* lm_head_w = (const int8_t*)(m_weights + weight_offset);

    float final_xb_scale = Ops::quantize_f32_to_int8(m_q_int8, m_xb, dim);
    Ops::gemv_int8_f32(m_logits, lm_head_w, m_q_int8, nullptr, dim, m_header.vocab_size);
    float total_lm_scale = lm_head_scale * final_xb_scale;
    for (int i = 0; i < (int)m_header.vocab_size; i++) {
        m_logits[i] *= total_lm_scale;
    }
}

int TextRunner::step(int tokenId, float temperature, int top_k, float top_p) {
    forward(tokenId, m_pos);
    m_pos++;
    if (m_pos >= (int)m_header.seq_len) {
        m_pos = m_header.seq_len - 1;
    }

    // Apply Repetition Penalty to prevent loop traps (e.g. "laughlaughlaugh...")
    for (size_t r = 0; r < m_recentTokens.size(); r++) {
        int past_id = m_recentTokens[r];
        if (past_id >= 0 && past_id < (int)m_header.vocab_size) {
            if (m_logits[past_id] > 0.0f) {
                m_logits[past_id] /= 1.35f;
            } else {
                m_logits[past_id] *= 1.35f;
            }
        }
    }

    int next_token = Ops::sample_top_p(m_logits, m_header.vocab_size, temperature, top_k, top_p);

    // Keep sliding window of last 16 tokens for repetition suppression
    m_recentTokens.push_back(next_token);
    if (m_recentTokens.size() > 16) {
        m_recentTokens.erase(m_recentTokens.begin());
    }

    return next_token;
}

} // namespace LocalAI
