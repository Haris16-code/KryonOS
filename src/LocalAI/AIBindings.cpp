#include "AIBindings.h"
#include "AIEngine.h"
#include <esp_heap_caps.h>

namespace LocalAI {

duk_ret_t AIBindings::js_ai_load(duk_context *ctx) {
    if (!duk_is_string(ctx, 0)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    const char* path = duk_get_string(ctx, 0);
    bool ok = AIEngine::getInstance().loadModel(String(path));
    if (!ok) {
        duk_push_boolean(ctx, false);
        return 1;
    }

    ModelHeader hdr = AIEngine::getInstance().getHeader();
    duk_push_object(ctx);
    duk_push_uint(ctx, hdr.model_type);
    duk_put_prop_string(ctx, -2, "type");
    duk_push_uint(ctx, hdr.num_params);
    duk_put_prop_string(ctx, -2, "params");
    duk_push_number(ctx, (double)AIEngine::getInstance().getMemoryUsed() / (1024.0 * 1024.0));
    duk_put_prop_string(ctx, -2, "sizeMB");
    duk_push_uint(ctx, hdr.quant_type);
    duk_put_prop_string(ctx, -2, "quant");

    return 1;
}

duk_ret_t AIBindings::js_ai_unload(duk_context *ctx) {
    bool ok = AIEngine::getInstance().unload();
    duk_push_boolean(ctx, ok);
    return 1;
}

duk_ret_t AIBindings::js_ai_isLoaded(duk_context *ctx) {
    duk_push_boolean(ctx, AIEngine::getInstance().isLoaded());
    return 1;
}

duk_ret_t AIBindings::js_ai_getInfo(duk_context *ctx) {
    if (!AIEngine::getInstance().isLoaded()) {
        duk_push_null(ctx);
        return 1;
    }

    ModelHeader hdr = AIEngine::getInstance().getHeader();
    duk_push_object(ctx);
    duk_push_uint(ctx, hdr.model_type);
    duk_put_prop_string(ctx, -2, "type");
    duk_push_uint(ctx, hdr.num_params);
    duk_put_prop_string(ctx, -2, "params");
    duk_push_number(ctx, (double)AIEngine::getInstance().getMemoryUsed() / (1024.0 * 1024.0));
    duk_put_prop_string(ctx, -2, "sizeMB");
    duk_push_uint(ctx, hdr.quant_type);
    duk_put_prop_string(ctx, -2, "quant");
    duk_push_uint(ctx, (duk_uint_t)AIEngine::getInstance().getMemoryUsed());
    duk_put_prop_string(ctx, -2, "memUsed");
    duk_push_uint(ctx, (duk_uint_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    duk_put_prop_string(ctx, -2, "psramFree");
    duk_push_uint(ctx, hdr.image_width > 0 ? hdr.image_width : 96);
    duk_put_prop_string(ctx, -2, "width");
    duk_push_uint(ctx, hdr.image_height > 0 ? hdr.image_height : 96);
    duk_put_prop_string(ctx, -2, "height");
    duk_push_uint(ctx, hdr.vocab_size);
    duk_put_prop_string(ctx, -2, "classes");

    return 1;
}

duk_ret_t AIBindings::js_ai_step(duk_context *ctx) {
    if (!AIEngine::getInstance().isLoaded()) {
        duk_push_null(ctx);
        return 1;
    }

    if (duk_is_number(ctx, 0)) {
        int token = duk_get_int(ctx, 0);
        int nextToken = AIEngine::getInstance().step(token);
        duk_push_int(ctx, nextToken);
        return 1;
    } else if (duk_is_array(ctx, 0)) {
        duk_size_t len = duk_get_length(ctx, 0);
        int lastToken = 0;
        for (duk_size_t i = 0; i < len; i++) {
            duk_get_prop_index(ctx, 0, (duk_uarridx_t)i);
            int tok = duk_get_int(ctx, -1);
            duk_pop(ctx);
            lastToken = AIEngine::getInstance().step(tok);
        }
        duk_push_int(ctx, lastToken);
        return 1;
    }

    duk_push_null(ctx);
    return 1;
}

duk_ret_t AIBindings::js_ai_encode(duk_context *ctx) {
    if (!duk_is_string(ctx, 0)) {
        duk_push_array(ctx);
        return 1;
    }
    const char* text = duk_get_string(ctx, 0);
    std::vector<int> tokens = AIEngine::getInstance().encode(String(text));

    duk_push_array(ctx);
    for (size_t i = 0; i < tokens.size(); i++) {
        duk_push_int(ctx, tokens[i]);
        duk_put_prop_index(ctx, -2, (duk_uarridx_t)i);
    }
    return 1;
}

duk_ret_t AIBindings::js_ai_decode(duk_context *ctx) {
    if (!duk_is_number(ctx, 0)) {
        duk_push_string(ctx, "");
        return 1;
    }
    int token = duk_get_int(ctx, 0);
    String word = AIEngine::getInstance().decode(token);
    duk_push_string(ctx, word.c_str());
    return 1;
}

duk_ret_t AIBindings::js_ai_generate(duk_context *ctx) {
    if (!AIEngine::getInstance().isLoaded()) {
        duk_push_boolean(ctx, false);
        return 1;
    }

    String prompt = "";
    duk_idx_t tokenCb = -1;
    duk_idx_t completeCb = -1;
    duk_idx_t errorCb = -1;

    if (duk_is_object(ctx, 0)) {
        duk_get_prop_string(ctx, 0, "prompt");
        if (duk_is_string(ctx, -1)) prompt = duk_get_string(ctx, -1);
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "temperature");
        if (duk_is_number(ctx, -1)) AIEngine::getInstance().setParam("temperature", (float)duk_get_number(ctx, -1));
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "top_k");
        if (duk_is_number(ctx, -1)) AIEngine::getInstance().setParam("top_k", (float)duk_get_number(ctx, -1));
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "top_p");
        if (duk_is_number(ctx, -1)) AIEngine::getInstance().setParam("top_p", (float)duk_get_number(ctx, -1));
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "scale");
        if (duk_is_number(ctx, -1)) AIEngine::getInstance().setParam("scale", (float)duk_get_number(ctx, -1));
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "max_tokens");
        if (duk_is_number(ctx, -1)) AIEngine::getInstance().setParam("max_tokens", (float)duk_get_number(ctx, -1));
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "x");
        if (duk_is_number(ctx, -1)) AIEngine::getInstance().setParam("x", (float)duk_get_number(ctx, -1));
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "y");
        if (duk_is_number(ctx, -1)) AIEngine::getInstance().setParam("y", (float)duk_get_number(ctx, -1));
        duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "onToken");
        if (duk_is_function(ctx, -1)) tokenCb = duk_get_top_index(ctx);
        else duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "onComplete");
        if (duk_is_function(ctx, -1)) completeCb = duk_get_top_index(ctx);
        else duk_pop(ctx);

        duk_get_prop_string(ctx, 0, "onError");
        if (duk_is_function(ctx, -1)) errorCb = duk_get_top_index(ctx);
        else duk_pop(ctx);
    } else if (duk_is_string(ctx, 0)) {
        prompt = duk_get_string(ctx, 0);
        if (duk_get_top(ctx) >= 2 && duk_is_function(ctx, 1)) tokenCb = 1;
        if (duk_get_top(ctx) >= 3 && duk_is_function(ctx, 2)) completeCb = 2;
    }

    bool ok = AIEngine::getInstance().startGenerate(prompt, ctx, tokenCb, completeCb, errorCb);
    duk_push_boolean(ctx, ok);
    return 1;
}

duk_ret_t AIBindings::js_ai_blitFrame(duk_context *ctx) {
    int x = -1;
    int y = -1;
    if (duk_get_top(ctx) >= 1 && duk_is_number(ctx, 0)) x = duk_get_int(ctx, 0);
    if (duk_get_top(ctx) >= 2 && duk_is_number(ctx, 1)) y = duk_get_int(ctx, 1);
    bool ok = AIEngine::getInstance().blitLastFrame(x, y);
    duk_push_boolean(ctx, ok);
    return 1;
}

duk_ret_t AIBindings::js_ai_setParam(duk_context *ctx) {
    if (!duk_is_string(ctx, 0) || !duk_is_number(ctx, 1)) {
        duk_push_boolean(ctx, false);
        return 1;
    }
    const char* key = duk_get_string(ctx, 0);
    float val = (float)duk_get_number(ctx, 1);
    bool ok = AIEngine::getInstance().setParam(String(key), val);
    duk_push_boolean(ctx, ok);
    return 1;
}

duk_ret_t AIBindings::js_ai_reset(duk_context *ctx) {
    bool ok = AIEngine::getInstance().reset();
    duk_push_boolean(ctx, ok);
    return 1;
}

duk_ret_t AIBindings::js_ai_getLastError(duk_context *ctx) {
    duk_push_string(ctx, AIEngine::getInstance().getLastError().c_str());
    return 1;
}

duk_ret_t AIBindings::js_ai_createSampleModel(duk_context *ctx) {
    const char* path = duk_is_string(ctx, 0) ? duk_get_string(ctx, 0) : "/local/sample_ai.krai";
    bool ok = AIEngine::getInstance().createSampleModel(String(path));
    duk_push_boolean(ctx, ok);
    return 1;
}

void AIBindings::registerBindings(duk_context *ctx) {
    // 1. Get or create global 'System' object
    duk_push_global_object(ctx);
    
    duk_get_prop_string(ctx, -1, "System");
    if (!duk_is_object(ctx, -1)) {
        duk_pop(ctx); // pop non-object
        duk_push_object(ctx);
        duk_put_prop_string(ctx, -2, "System");
        duk_get_prop_string(ctx, -1, "System");
    }

    // 2. Non-destructively get or create 'System.ai' object
    duk_get_prop_string(ctx, -1, "ai");
    if (!duk_is_object(ctx, -1)) {
        duk_pop(ctx);
        duk_push_object(ctx);
        duk_put_prop_string(ctx, -2, "ai");
        duk_get_prop_string(ctx, -1, "ai");
    }

    // Attach Local AI methods onto System.ai
    duk_push_c_function(ctx, js_ai_load, 1);              duk_put_prop_string(ctx, -2, "load");
    duk_push_c_function(ctx, js_ai_unload, 0);            duk_put_prop_string(ctx, -2, "unload");
    duk_push_c_function(ctx, js_ai_isLoaded, 0);          duk_put_prop_string(ctx, -2, "isLoaded");
    duk_push_c_function(ctx, js_ai_getInfo, 0);           duk_put_prop_string(ctx, -2, "getInfo");
    duk_push_c_function(ctx, js_ai_step, 1);              duk_put_prop_string(ctx, -2, "step");
    duk_push_c_function(ctx, js_ai_encode, 1);            duk_put_prop_string(ctx, -2, "encode");
    duk_push_c_function(ctx, js_ai_decode, 1);            duk_put_prop_string(ctx, -2, "decode");
    duk_push_c_function(ctx, js_ai_generate, 1);          duk_put_prop_string(ctx, -2, "generate");
    duk_push_c_function(ctx, js_ai_blitFrame, 2);         duk_put_prop_string(ctx, -2, "blitFrame");
    duk_push_c_function(ctx, js_ai_setParam, 2);          duk_put_prop_string(ctx, -2, "setParam");
    duk_push_c_function(ctx, js_ai_reset, 0);             duk_put_prop_string(ctx, -2, "reset");
    duk_push_c_function(ctx, js_ai_getLastError, 0);      duk_put_prop_string(ctx, -2, "getLastError");
    duk_push_c_function(ctx, js_ai_createSampleModel, 1); duk_put_prop_string(ctx, -2, "createSampleModel");

    duk_pop(ctx); // pop System.ai

    // 3. Create System.localAI alias object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_ai_load, 1);              duk_put_prop_string(ctx, -2, "load");
    duk_push_c_function(ctx, js_ai_unload, 0);            duk_put_prop_string(ctx, -2, "unload");
    duk_push_c_function(ctx, js_ai_isLoaded, 0);          duk_put_prop_string(ctx, -2, "isLoaded");
    duk_push_c_function(ctx, js_ai_getInfo, 0);           duk_put_prop_string(ctx, -2, "getInfo");
    duk_push_c_function(ctx, js_ai_step, 1);              duk_put_prop_string(ctx, -2, "step");
    duk_push_c_function(ctx, js_ai_encode, 1);            duk_put_prop_string(ctx, -2, "encode");
    duk_push_c_function(ctx, js_ai_decode, 1);            duk_put_prop_string(ctx, -2, "decode");
    duk_push_c_function(ctx, js_ai_generate, 1);          duk_put_prop_string(ctx, -2, "generate");
    duk_push_c_function(ctx, js_ai_blitFrame, 2);         duk_put_prop_string(ctx, -2, "blitFrame");
    duk_push_c_function(ctx, js_ai_setParam, 2);          duk_put_prop_string(ctx, -2, "setParam");
    duk_push_c_function(ctx, js_ai_reset, 0);             duk_put_prop_string(ctx, -2, "reset");
    duk_push_c_function(ctx, js_ai_getLastError, 0);      duk_put_prop_string(ctx, -2, "getLastError");
    duk_push_c_function(ctx, js_ai_createSampleModel, 1); duk_put_prop_string(ctx, -2, "createSampleModel");
    duk_put_prop_string(ctx, -2, "localAI");

    duk_pop(ctx); // pop System

    // 4. Create global LocalAI alias object
    duk_push_object(ctx);
    duk_push_c_function(ctx, js_ai_load, 1);              duk_put_prop_string(ctx, -2, "load");
    duk_push_c_function(ctx, js_ai_unload, 0);            duk_put_prop_string(ctx, -2, "unload");
    duk_push_c_function(ctx, js_ai_isLoaded, 0);          duk_put_prop_string(ctx, -2, "isLoaded");
    duk_push_c_function(ctx, js_ai_getInfo, 0);           duk_put_prop_string(ctx, -2, "getInfo");
    duk_push_c_function(ctx, js_ai_step, 1);              duk_put_prop_string(ctx, -2, "step");
    duk_push_c_function(ctx, js_ai_encode, 1);            duk_put_prop_string(ctx, -2, "encode");
    duk_push_c_function(ctx, js_ai_decode, 1);            duk_put_prop_string(ctx, -2, "decode");
    duk_push_c_function(ctx, js_ai_generate, 1);          duk_put_prop_string(ctx, -2, "generate");
    duk_push_c_function(ctx, js_ai_blitFrame, 2);         duk_put_prop_string(ctx, -2, "blitFrame");
    duk_push_c_function(ctx, js_ai_setParam, 2);          duk_put_prop_string(ctx, -2, "setParam");
    duk_push_c_function(ctx, js_ai_reset, 0);             duk_put_prop_string(ctx, -2, "reset");
    duk_push_c_function(ctx, js_ai_getLastError, 0);      duk_put_prop_string(ctx, -2, "getLastError");
    duk_push_c_function(ctx, js_ai_createSampleModel, 1); duk_put_prop_string(ctx, -2, "createSampleModel");
    duk_put_prop_string(ctx, -2, "LocalAI");

    duk_pop(ctx); // pop global object
}

} // namespace LocalAI
