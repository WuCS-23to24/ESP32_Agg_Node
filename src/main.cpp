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
std::queue<TransmissionData_t *> received_packets; // shared everywhere, declared extern in data_packet.h
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

// ISR for handling various semaphores
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
    disconnect_semaphore = xSemaphoreCreateBinary();
    scan_semaphore = xSemaphoreCreateBinary();
    send_semaphore = xSemaphoreCreateBinary();
    receive_body_semaphore = xSemaphoreCreateBinary();

    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(A0, OUTPUT); // for acoustic out
    Serial.printf("SERVICE UUID - %s\n", UUID_generator.get_service_uuid());
    Serial.printf("CHARACTERISTIC UUID - %s\n", UUID_generator.get_characteristic_uuid());
    bluetooth = Bluetooth<uuids>(UUID_generator);

    setup_timer(set_semaphore, 250, 80); // prepare interrupt timer to control semaphores

    // CPU Core 0: body as wire receive
    xTaskCreatePinnedToCore(receive_body, "receive_body", 4096, NULL, 19, &body_task_handle, 0); // priority at least 19
    // CPU Core 1: BLE server/client and acoustic send
    xTaskCreatePinnedToCore(main_loop, "main_loop", 4096*4, NULL, 19, &main_task_handle, 1); // priority at least 19
}

void main_loop(void *arg) // don't use default loop() because it has low priority
{
    while (1)
    {
        // Send and BLE connect
        if (xSemaphoreTake(send_semaphore, 0) == pdTRUE)
        {
            if (bluetooth.clientIsConnected())
            {
                // Priority: BLE transmit
                digitalWrite(LED_BUILTIN, HIGH);
                if (received_packets.size() > 0)
                {
                    bluetooth.callback_class->setData(*received_packets.front());
                    delete received_packets.front(); // free previously allocated TransmissionData pointer
                    received_packets.pop();
                    bluetooth.sendData();
                }
                bluetooth.tryConnectToServer();
            }
            else
            {
                // Alternative: Acoustic transmit
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
    }
}

void loop() // intentionally blank
{
}