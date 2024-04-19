#include <Arduino.h>
#include <BLEDevice.h>
#include <vector>

#include "auxiliary.h"
#include "bluetooth.hpp"
#include "body.hpp"
#include "hardware_timer.hpp"
#include "data_packet.h"
#include "acoustic.hpp"


uuids UUID_generator;
Bluetooth<uuids> bluetooth;
Acoustic acoustic;
volatile SemaphoreHandle_t disconnect_semaphore;
volatile SemaphoreHandle_t scan_semaphore;
volatile SemaphoreHandle_t send_semaphore;
volatile SemaphoreHandle_t receive_body_semaphore;
volatile int8_t DISCON_ISR = 0;
volatile int8_t SCAN_ISR = 0;
volatile int8_t SEND_ISR = 0;
volatile int8_t RECEIVE_BODY_ISR = 0;
portMUX_TYPE isr_mux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t body_task_handle = NULL;
TaskHandle_t main_task_handle = NULL;

void main_loop(void *arg);

void ARDUINO_ISR_ATTR set_semaphore()
{
    taskENTER_CRITICAL_ISR(&isr_mux);
    if (DISCON_ISR == 20)
    {
        xSemaphoreGiveFromISR(disconnect_semaphore, NULL);
    }
    else
    {
        DISCON_ISR++;
    }

    if (SCAN_ISR == 6)
    {
        xSemaphoreGiveFromISR(scan_semaphore, NULL);
    }
    else
    {
        SCAN_ISR++;
    }

    if (SEND_ISR == 2)
    {
        xSemaphoreGiveFromISR(send_semaphore, NULL);
    }
    else
    {
        SEND_ISR++;
    }
    if (RECEIVE_BODY_ISR == 2)
    {
        xSemaphoreGiveFromISR(receive_body_semaphore, NULL);

    }
    else
    {
        RECEIVE_BODY_ISR++;
    }
    taskEXIT_CRITICAL_ISR(&isr_mux);
}

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(A0, OUTPUT); // for acoustic out
    //UUID_generator.initialize_random_values();
    //UUID_generator.generate_hashes();
    Serial.printf("SERVICE UUID - %s\n", UUID_generator.get_service_uuid());
    Serial.printf("CHARACTERISTIC UUID - %s\n", UUID_generator.get_characteristic_uuid());
    bluetooth = Bluetooth<uuids>(UUID_generator);

    disconnect_semaphore = xSemaphoreCreateBinary();
    scan_semaphore = xSemaphoreCreateBinary();
    send_semaphore = xSemaphoreCreateBinary();
    receive_body_semaphore = xSemaphoreCreateBinary();
    setup_timer(set_semaphore, 250, 80);
    xTaskCreatePinnedToCore(receive_body, "receive_body", 4096, NULL, 19, &body_task_handle, 0); // priority at least 19
    xTaskCreatePinnedToCore(main_loop, "main_loop", 4096*4, NULL, 19, &main_task_handle, 1); // priority at least 19
}

void main_loop(void *arg) // don't use default loop() because it has low priority
{
    while (1)
    {
        if (xSemaphoreTake(send_semaphore, 0) == pdTRUE)
        {
            if (bluetooth.clientIsConnected())
            {
                digitalWrite(LED_BUILTIN, HIGH);
                if (received_packets.size() > 0)
                {
                    bluetooth.callback_class->setData(received_packets.front());
                    received_packets.pop();
                    bluetooth.sendData();
                }
                bluetooth.tryConnectToServer();
            }
            else
            {
                digitalWrite(LED_BUILTIN, LOW);
                acoustic.setData(bluetooth.callback_class->getData());
                acoustic.transmitFrame();
            }
        }
        if (xSemaphoreTake(scan_semaphore, 0) == pdTRUE && bluetooth.clientIsConnected())
        {
            bluetooth.scan();
        }
        if (xSemaphoreTake(disconnect_semaphore, 0) == pdTRUE )
        {
            bluetooth.removeOldServers();
        }
        //vTaskDelay(1 / portTICK_RATE_MS); // delay 1 ms
    }
}

void loop()
{
}