// =====================================================
// EngineTaskRunner.h
//
// Roda o "corpo" de uma engine de script (Lua/Wren/JS)
// dentro de uma task FreeRTOS dedicada, com stack
// próprio, isolada da loopTask. Bloqueia a chamada até
// terminar (comportamento síncrono do ponto de vista de
// quem chama), mas protege o resto do sistema de um
// estouro de stack de um app.
// =====================================================

#pragma once

#include <Arduino.h>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


class EngineTaskRunner
{
public:
    // stackSize em bytes
    static void run(
        const char* taskName,
        uint32_t stackSize,
        std::function<void()> body
    )
    {
        struct Args
        {
            std::function<void()> body;
            SemaphoreHandle_t doneSignal;
        };

        auto* args = new Args{
            std::move(body),
            xSemaphoreCreateBinary()
        };

        if (!args->doneSignal)
        {
            Serial.println("[EngineTaskRunner] Falha ao criar semaforo");
            delete args;
            return;
        }

        BaseType_t created = xTaskCreate(
            [](void* param)
            {
                auto* a = static_cast<Args*>(param);

                UBaseType_t freeStart =
                    uxTaskGetStackHighWaterMark(nullptr);

                a->body();

                UBaseType_t freeEnd =
                    uxTaskGetStackHighWaterMark(nullptr);

                Serial.print("[EngineTaskRunner] Stack livre - inicio: ");
                Serial.print(freeStart * sizeof(StackType_t));
                Serial.print(" bytes | minimo atingido: ");
                Serial.print(freeEnd * sizeof(StackType_t));
                Serial.println(" bytes");

                if (freeEnd * sizeof(StackType_t) < 512)
                {
                    Serial.println(
                        "[EngineTaskRunner] AVISO: proximo do limite de stack"
                    );
                }

                xSemaphoreGive(a->doneSignal);
                vTaskDelete(nullptr);
            },
            taskName,
            stackSize / sizeof(StackType_t),
            args,
            1,
            nullptr
        );

        if (created != pdPASS)
        {
            Serial.println(
                "[EngineTaskRunner] Falha ao criar task (RAM insuficiente?)"
            );
            vSemaphoreDelete(args->doneSignal);
            delete args;
            return;
        }

        xSemaphoreTake(args->doneSignal, portMAX_DELAY);

        vSemaphoreDelete(args->doneSignal);
        delete args;
    }
};