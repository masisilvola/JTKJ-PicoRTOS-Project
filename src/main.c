
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048 

//Add here necessary states
enum state { IDLE=1 };
enum state programState = IDLE;

static void example_task(void *arg){
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

    for (;;) {
        // Luetaan anturin mittausarvot
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temp) == 0) {
            // Tunnistetaan asento Z-akselin kiihtyvyydestä (painovoima g)
            const char *asento = "Kallellaan / Tuntematon";
            if (az > 0.7f) {
                asento = "OIKEIN PAIN (Z ~ +1g)";
            } else if (az < -0.7f) {
                asento = "VAAROIN PAIN / YLOSALAISIN (Z ~ -1g)";
            }

            printf("[%s] Accel: X=%.2f Y=%.2f Z=%.2f g | Gyro: X=%.1f Y=%.1f Z=%.1f dps | T=%.1f C\n",
                   asento, ax, ay, az, gx, gy, gz, temp);
        } else {
            printf("Virhe luettaessa sensoridataa.\n");
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Luetaan 500 ms valein
    }
}

    //for(;;){
    //    tight_loop_contents(); // Modify with application code here.
      //  vTaskDelay(pdMS_TO_TICKS(2000));


int main() {
    stdio_init_all();
    //Uncomment this lines if you want to wait till the serial monitor is connected
    while (!stdio_usb_connected()){
        sleep_ms(10);
    }
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    TaskHandle_t myExampleTask = NULL;
    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(example_task,       // (en) Task function
                "example",              // (en) Name of the task 
                DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,               // (en) Arguments of the task 
                2,                  // (en) Priority of this task
                &myExampleTask);    // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Example Task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}

