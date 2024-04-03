#include <Arduino.h>
#include <BLEDevice.h>
#include <vector>

#include "auxiliary.h"
#include "bluetooth.hpp"
#include "hardware_timer.hpp"


uuids UUID_generator;
Bluetooth<uuids> bluetooth;
volatile SemaphoreHandle_t disconnect_semaphore;
volatile SemaphoreHandle_t scan_semaphore;
volatile int8_t DISCON_ISR = 0;
volatile int8_t SCAN_ISR = 0;
portMUX_TYPE isr_mux = portMUX_INITIALIZER_UNLOCKED;

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
    taskEXIT_CRITICAL_ISR(&isr_mux);
}

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    //UUID_generator.initialize_random_values();
    //UUID_generator.generate_hashes();
    Serial.printf("SERVICE UUID - %s\n", UUID_generator.get_service_uuid());
    Serial.printf("CHARACTERISTIC UUID - %s\n", UUID_generator.get_characteristic_uuid());
    bluetooth = Bluetooth<uuids>(UUID_generator);

    disconnect_semaphore = xSemaphoreCreateBinary();
    scan_semaphore = xSemaphoreCreateBinary();
    setup_timer(set_semaphore, 250, 80);
}

void loop()
{
    if (bluetooth.clientIsConnected()) // only worry about finding sensors after repeater is connected
    {
        digitalWrite(LED_BUILTIN, HIGH);
        if (received_packets.size() > 0)
        {
            // admittedly cursed
            bluetooth.callback_class->setData(* received_packets.front());
            received_packets.pop();
            bluetooth.sendData();
        }
        bluetooth.tryConnectToServer();
        // ALWAYS scan, because devices could become available at any time
    }
    else
    {
        digitalWrite(LED_BUILTIN, LOW);
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
