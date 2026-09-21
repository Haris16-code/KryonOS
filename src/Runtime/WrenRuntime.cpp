#include "Runtime/WrenRuntime.h"
#include "Runtime/WrenBindings.h"

WrenVM* WrenRuntime::vm = nullptr;
int WrenRuntime::apiLineCount = 0;

// =====================================================
// Output
// =====================================================

void WrenRuntime::writeFn(
    WrenVM* vm,
    const char* text
)
{
    (void)vm;

    Serial.print(text);
}


// =====================================================
// Error
// =====================================================

void WrenRuntime::errorFn(
    WrenVM* vm,
    WrenErrorType type,
    const char* module,
    int line,
    const char* msg
)
{
    (void)vm;

    const char* typeName = "Unknown Error";

    switch (type)
    {
        case WREN_ERROR_COMPILE:      typeName = "Compile Error"; break;
        case WREN_ERROR_STACK_TRACE:  typeName = "Stack Trace";   break;
        case WREN_ERROR_RUNTIME:      typeName = "Runtime Error"; break;
        default: break;
    }

    int realLine = line;
    if (type == WREN_ERROR_STACK_TRACE &&
        WrenRuntime::apiLineCount > 0 &&
        line > WrenRuntime::apiLineCount)
    {
        realLine = line - WrenRuntime::apiLineCount;
    }

    // Serial (sem mudanças)
    Serial.print("[Wren] ");
    Serial.print(typeName);
    if (module)
    {
        Serial.print(" ");
        Serial.print(module);
        if (realLine > 0) { Serial.print(":"); Serial.print(realLine); }
    }
    Serial.print(" - ");
    Serial.println(msg ? msg : "no message");

    // TFT — continua chamando showError, só que agora ele acumula
    String errorText;
    if (module)
    {
        errorText += module;
        if (realLine > 0) { errorText += ":"; errorText += String(realLine); }
        errorText += "\n";
    }
    if (msg) errorText += msg;

    WrenBindings::showError(typeName, errorText.c_str());
}

// =====================================================
// Module Loader
// =====================================================

WrenLoadModuleResult WrenRuntime::loadModule(
    WrenVM* vm,
    const char* name
)
{
    (void)vm;
    (void)name;

    WrenLoadModuleResult result{};

    result.source = nullptr;
    result.onComplete = nullptr;
    result.userData = nullptr;

    return result;
}


// =====================================================
// Foreign Methods
// =====================================================

WrenForeignMethodFn WrenRuntime::bindForeignMethod(
    WrenVM* vm,
    const char* module,
    const char* className,
    bool isStatic,
    const char* signature
)
{
    return WrenBindings::bindForeignMethod(
        vm,
        module,
        className,
        isStatic,
        signature
    );
}


// =====================================================
// Foreign Classes
// =====================================================

WrenForeignClassMethods WrenRuntime::bindForeignClass(
    WrenVM* vm,
    const char* module,
    const char* className
)
{
    return WrenBindings::bindForeignClass(
        vm,
        module,
        className
    );
}


// =====================================================
// Begin
// =====================================================

// WrenRuntime.cpp - Alocador otimizado

// ============================================
// Alocador que prioriza PSRAM
// ============================================

static void* wren_psram_first_allocate(void* memory, size_t newSize, void* userData)
{
    (void)userData;
    
    // ============================================
    // LIBERAÇÃO
    // ============================================
    if (newSize == 0)
    {
        if (memory)
        {
            free(memory);  // free() funciona para ambos (PSRAM e RAM)
        }
        return NULL;
    }
    
    // ============================================
    // NOVA ALOCAÇÃO
    // ============================================
    if (!memory)
    {
        // 1. Tenta PSRAM PRIMEIRO (prioridade máxima)
        if (psramFound())
        {
            void* result = heap_caps_malloc(newSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (result)
            {
                return result;
            }
        }
        
        // 2. Fallback para RAM interna
        return malloc(newSize);
    }
    
    // ============================================
    // REALOCAÇÃO
    // ============================================
    
    // Verifica se o ponteiro está na PSRAM
    if (psramFound())
    {
        // Tenta realocar na PSRAM
        void* result = heap_caps_realloc(memory, newSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (result)
        {
            return result;
        }
    }
    
    // Tenta realocar na RAM interna
    void* result = realloc(memory, newSize);
    if (result)
    {
        return result;
    }
    
    // Último recurso: tenta PSRAM novamente
    if (psramFound())
    {
        result = heap_caps_realloc(memory, newSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (result)
        {
            return result;
        }
    }
    
    return NULL;
}

// ============================================
// Inicialização otimizada
// ============================================

bool WrenRuntime::begin()
{
    if (vm)
    {
        shutdown();
    }

    WrenConfiguration config;
    wrenInitConfiguration(&config);

    // ============================================
    // Callbacks
    // ============================================
    config.reallocateFn = wren_psram_first_allocate;  // Alocador PSRAM-first
    
    config.writeFn = writeFn;
    config.errorFn = errorFn;
    config.loadModuleFn = loadModule;

    config.bindForeignMethodFn = bindForeignMethod;
    config.bindForeignClassFn = bindForeignClass;

    // ============================================
    // Ajuste dinâmico do heap
    // ============================================
    
    size_t freeInternalRAM = ESP.getFreeHeap();
    size_t freePSRAM = ESP.getFreePsram();
    size_t totalPSRAM = ESP.getPsramSize();
    
    Serial.printf("[Wren] RAM interna livre: %d bytes (%.1f KB)\n", 
                  freeInternalRAM, freeInternalRAM / 1024.0);
    Serial.printf("[Wren] PSRAM livre: %d bytes (%.1f KB)\n", 
                  freePSRAM, freePSRAM / 1024.0);
    Serial.printf("[Wren] PSRAM total: %d bytes (%.1f KB)\n", 
                  totalPSRAM, totalPSRAM / 1024.0);
    
    if (psramFound() && freePSRAM > 0)
    {
        // ============================================
        // PSRAM DISPONÍVEL - Usa TODA a PSRAM livre
        // ============================================
        
        // Usa 90% da PSRAM livre (deixa margem de segurança)
        size_t heapSize = (freePSRAM * 90) / 100;
        
        // Garante um mínimo razoável
        if (heapSize < 64 * 1024)
        {
            heapSize = 64 * 1024;  // 64 KB mínimo
        }
        
        config.initialHeapSize = heapSize;
        config.minHeapSize = 16 * 1024;  // 16 KB mínimo
        config.heapGrowthPercent = 50;
        
        Serial.printf("[Wren] Heap inicial: %d bytes (%.1f KB) na PSRAM\n", 
                      heapSize, heapSize / 1024.0);
        Serial.printf("[Wren] Heap mínimo: %d bytes (%.1f KB)\n", 
                      config.minHeapSize, config.minHeapSize / 1024.0);
    }
    else
    {
        // ============================================
        // SEM PSRAM - Usa 80% da RAM interna livre
        // ============================================
        
        // Usa 80% da RAM interna livre
        size_t heapSize = (freeInternalRAM * 80) / 100;
        
        // Garante um mínimo razoável
        if (heapSize < 16 * 1024)
        {
            heapSize = 16 * 1024;  // 16 KB mínimo
        }
        
        // Mas não excede 100 KB (para não esgotar a RAM)
        if (heapSize > 100 * 1024)
        {
            heapSize = 100 * 1024;
        }
        
        config.initialHeapSize = heapSize;
        config.minHeapSize = 8 * 1024;  // 8 KB mínimo
        config.heapGrowthPercent = 30;
        
        Serial.printf("[Wren] Heap inicial: %d bytes (%.1f KB) na RAM interna\n", 
                      heapSize, heapSize / 1024.0);
        Serial.printf("[Wren] Heap mínimo: %d bytes (%.1f KB)\n", 
                      config.minHeapSize, config.minHeapSize / 1024.0);
    }

    // ============================================
    // Cria VM
    // ============================================
    
    vm = wrenNewVM(&config);

    if (!vm)
    {
        Serial.println("[Wren] Failed to create VM");
        
        // Tenta com configuração mínima
        wrenInitConfiguration(&config);
        config.reallocateFn = wren_psram_first_allocate;
        config.writeFn = writeFn;
        config.errorFn = errorFn;
        config.loadModuleFn = loadModule;
        config.bindForeignMethodFn = bindForeignMethod;
        config.bindForeignClassFn = bindForeignClass;
        
        config.initialHeapSize = 16 * 1024;  // 16 KB
        config.minHeapSize = 4 * 1024;       // 4 KB
        config.heapGrowthPercent = 20;
        
        Serial.println("[Wren] Tentando com heap mínimo...");
        
        vm = wrenNewVM(&config);
        
        if (!vm)
        {
            Serial.println("[Wren] Failed to create VM (mínimo)");
            return false;
        }
    }

    WrenBindings::init(vm);

    Serial.println("[Wren] VM initialized");

    return true;
}


// =====================================================
// Execute
// =====================================================

bool WrenRuntime::execute(
    const char* source,
    int lineOffset
)
{
    if (!vm)
    {
        Serial.println("[Wren] VM is not initialized");
        return false;
    }

    if (!source)
    {
        Serial.println("[Wren] Source is null");
        return false;
    }

    apiLineCount = lineOffset;

    WrenInterpretResult result =
        wrenInterpret(
            vm,
            "main",
            source
        );

    switch (result)
    {
        case WREN_RESULT_SUCCESS:
            Serial.println("[Wren] Script finished.");
            return true;

        case WREN_RESULT_COMPILE_ERROR:
            Serial.println("[Wren] Compilation failed.");
            return false;

        case WREN_RESULT_RUNTIME_ERROR:
            Serial.println("[Wren] Runtime failed.");
            return false;
    }

    return false;
}

// =====================================================
// Shutdown
// =====================================================

void WrenRuntime::shutdown()
{
    if (!vm)
        return;

    wrenFreeVM(vm);

    vm = nullptr;

    Serial.println(
        "[Wren] VM destroyed"
    );
}


// =====================================================
// Update
// =====================================================

void WrenRuntime::update()
{
    // Futuramente:
    //
    // processamento de eventos
    // timers
    // callbacks
    // etc.
}