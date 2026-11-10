#ifndef AI_BINDINGS_H
#define AI_BINDINGS_H

#include "../Runtime/duktape.h"

namespace LocalAI {

class AIBindings {
public:
    static void registerBindings(duk_context *ctx);

    static duk_ret_t js_ai_load(duk_context *ctx);
    static duk_ret_t js_ai_unload(duk_context *ctx);
    static duk_ret_t js_ai_isLoaded(duk_context *ctx);
    static duk_ret_t js_ai_getInfo(duk_context *ctx);
    static duk_ret_t js_ai_step(duk_context *ctx);
    static duk_ret_t js_ai_encode(duk_context *ctx);
    static duk_ret_t js_ai_decode(duk_context *ctx);
    static duk_ret_t js_ai_generate(duk_context *ctx);
    static duk_ret_t js_ai_blitFrame(duk_context *ctx);
    static duk_ret_t js_ai_setParam(duk_context *ctx);
    static duk_ret_t js_ai_reset(duk_context *ctx);
    static duk_ret_t js_ai_getLastError(duk_context *ctx);
    static duk_ret_t js_ai_createSampleModel(duk_context *ctx);
};

} // namespace LocalAI

#endif // AI_BINDINGS_H
