#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048 

// Tilat asennon tunnistukselle
typedef enum {
    ASENTO_TUNTEMATON = 0,
    ASENTO_YLOS,
    ASENTO_ALAS
} device_orientation_t;

static void example_task(void *arg) {
    (void)arg;

    // Alustetaan ICM-42670 oletusarvoilla (LN-tila, ±4g, ±250 dps)
    if (ICM42670_start_with_default_values() != 0) {
        printf("ICM-42670 alustus epaonnistui!\n");
    } else {
        printf("ICM-42670 alustettu onnistuneesti.\n");
    }

    float ax = 0.0f, ay = 0.0f, az = 0.0f;
    float gx = 0.0f, gy = 0.0f, gz = 0.0f;
    float temp = 0.0f;

    device_orientation_t NYKYINEN_TILA = ASENTO_TUNTEMATON;   

    for (;;) {
        // Luetaan anturin mittausarvot
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temp) == 0) {
            
            // 1. Tunnistetaan asento Z-akselin kiihtyvyydestä
            device_orientation_t ASENTO = ASENTO_TUNTEMATON;
            if (az > 0.9f) {
                ASENTO = ASENTO_YLOS;
            } else if (az < -0.9f) {
                ASENTO = ASENTO_ALAS;
            }
            
            // 2. Tulostetaan viesti VAIN jos siirrytään tuntemattomasta tunnettuun asentoon
            if (NYKYINEN_TILA == ASENTO_TUNTEMATON && ASENTO != ASENTO_TUNTEMATON) {
                if (ASENTO == ASENTO_YLOS) {
                    printf("Asento: YLOS\n");
                } else if (ASENTO == ASENTO_ALAS) {
                    printf("Asento: ALAS\n");
                }
            }

            // 3. Päivitetään tila seuraavaa kierrosta varten
            NYKYINEN_TILA = ASENTO;

        } else {
            printf("Anturin lukeminen epaonnistui\n");
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Luetaan 500 ms välein
    }
}

int main() {
    stdio_init_all();
    
    // Odotetaan kunnes USB-sarjayhteys on muodostettu
    while (!stdio_usb_connected()) {
        sleep_ms(10);
    }
    
    init_hat_sdk();
    sleep_ms(300); // Odotetaan hetki alustusten valmistumista

    TaskHandle_t myExampleTask = NULL;
    
    // Luodaan FreeRTOS-tehtävä
    BaseType_t result = xTaskCreate(
        example_task,
        "example",
        DEFAULT_STACK_SIZE,
        NULL,
        2,
        &myExampleTask
    );

    if (result != pdPASS) {
        printf("Example Task creation failed\n");
        return 0;
    }

    // Käynnistetään scheduler
    vTaskStartScheduler();

    return 0;
}