#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

// Tehtävän pinon oletuskoko (sanoina/tavuina arkkitehtuurista riippuen)
#define DEFAULT_STACK_SIZE 2048 

// Morse-viestipuskurin maksimipituus merkkeinä
#define MAX_MSG_LEN 128

// Tilakoneen tilat laitteen asennon tunnistukseen (Z-akseli)
typedef enum {
    ASENTO_TUNTEMATON = 0, 
    ASENTO_YLOS,          
    ASENTO_ALAS            
} device_orientation_t;

// Puskuri morse-merkkien keräämistä varten ennen lähetystä
static char msg_buffer[MAX_MSG_LEN];
static size_t msg_len = 0; // Kerättyjen merkkien määrä alussa

// 1. Tehtävä: Lukee painikkeita ja käsittelee viestin lähetyksen sekä välilyönnit
static void button_task(void *arg) {
    (void)arg;

    bool last_sw1_state = true; // Pull-up-kytketty: lepotila on 1 (true)
    bool last_sw2_state = true; 

    for (;;) {
        bool sw1 = gpio_get(SW1_PIN);
        bool sw2 = gpio_get(SW2_PIN);

        // välilyönti 
        if (!sw1 && last_sw1_state) {
            if (msg_len < MAX_MSG_LEN - 1) {
                msg_buffer[msg_len++] = ' '; 
                msg_buffer[msg_len] = '\0';
            }
        }

        // bufferin lähetys ja tyhjennys 
        if (!sw2 && last_sw2_state) {
            if (msg_len > 0) {
                printf("%s\n", msg_buffer);
                fflush(stdout); // Tyhjennetään USB-lähetyspuskuri heti

                // Nollataan puskuri uutta viestiä varten
                msg_len = 0;
                msg_buffer[0] = '\0';
            }
        }

        // Päivitetään painikkeiden edelliset tilat seuraavaa kierrosta varten
        last_sw1_state = sw1;
        last_sw2_state = sw2;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// 2. Tehtävä: Lukee IMU-anturia ja tunnistaa asennon (piste / viiva)
static void imu_task(void *arg) {
    (void)arg;

    // Alustetaan 6-akselinen ICM-42670 IMU-anturi oletusarvoilla
    if (ICM42670_start_with_default_values() != 0) {
        printf("ICM-42670 alustus epaonnistui!\n");
    } else {
        printf("ICM-42670 alustettu onnistuneesti.\n");
    }

    float ax = 0.0f, ay = 0.0f, az = 0.0f; // Kiihtyvyydet (g)
    float gx = 0.0f, gy = 0.0f, gz = 0.0f; // Kulmanopeudet (dps)
    float temp = 0.0f;                     // Lämpötila (°C) (TURHA)

    device_orientation_t NYKYINEN_TILA = ASENTO_TUNTEMATON;   

    for (;;) {
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temp) == 0) {
            
            // Määritetään asento Z-akselin kiihtyvyyden perusteella
            device_orientation_t ASENTO = ASENTO_TUNTEMATON;
            if (az > 0.9f) {
                ASENTO = ASENTO_YLOS; // Laite pystyssä ylöspäin
            } else if (az < -0.9f) {
                ASENTO = ASENTO_ALAS; // Laite ylösalaisin
            }
            
            // Lisätään merkki puskuriin VAIN kun laite käännetään neutraalista asentoon
            if (NYKYINEN_TILA == ASENTO_TUNTEMATON && ASENTO != ASENTO_TUNTEMATON) {
                if (msg_len < MAX_MSG_LEN - 1) {
                    if (ASENTO == ASENTO_YLOS) {
                        msg_buffer[msg_len++] = '.'; // Piste
                    } else if (ASENTO == ASENTO_ALAS) {
                        msg_buffer[msg_len++] = '-'; // Viiva
                    }
                    msg_buffer[msg_len] = '\0';
                }
            }

            // Päivitetään asento seuraavaa anturilukua varten
            NYKYINEN_TILA = ASENTO;

        } else {
            printf("Anturin lukeminen epaonnistui\n");
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

int main() {
    // Alustetaan standardi I/O (USB-sarjaportti)
    stdio_init_all();
    
    // Estetään automaattinen rivinvaihdon lisäys/muunnos Pico SDK:ssa
    stdio_set_translate_crlf(&stdio_usb, false);
    
    // Odotetaan kunnes USB-sarjayhteys on muodostettu tietokoneeseen
    while (!stdio_usb_connected()) {
        sleep_ms(10);
    }
    
    // Alustetaan HAT-laajennuskortin SDK ja komponentit
    init_hat_sdk();
    
    // Alustetaan painikkeet SDK:n funktioilla ja varmistetaan ylösvetovastukset
    init_sw1();
    init_sw2();
    gpio_pull_up(SW1_PIN);
    gpio_pull_up(SW2_PIN);
    
    sleep_ms(300); // Lyhyt odotus laitealustusten asettumiseen

    TaskHandle_t buttonTaskHandle = NULL;
    TaskHandle_t imuTaskHandle = NULL;
    
    // Luodaan painikkeiden tehtävä
    BaseType_t res1 = xTaskCreate(
        button_task,
        "ButtonTask",
        DEFAULT_STACK_SIZE,
        NULL,
        2,
        &buttonTaskHandle
    );

    // Luodaan IMU-anturin tehtävä
    BaseType_t res2 = xTaskCreate(
        imu_task,
        "ImuTask",
        DEFAULT_STACK_SIZE,
        NULL,
        2,
        &imuTaskHandle
    );

    if (res1 != pdPASS || res2 != pdPASS) {
        printf("Tehtävien luonti epäonnistui\n");
        return 0;
    }

    // Käynnistetään FreeRTOS-tehtävänjakaja
    vTaskStartScheduler();

    return 0;
}