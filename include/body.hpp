#include <Arduino.h>
#include <cstdint>
#include <queue>
#include "esp_task_wdt.h"

#include "data_packet.h"

const int F_CARRIER = 10000;
const int SYMBOL_HALF_MS = 5;

uint16_t msToHalfSymbolsEstimated(unsigned long time_ms);

void receive_body(void *arg)
{
    // make task_wdt wait as long as possible to effectively disable (not ideal)
    esp_task_wdt_init(UINT32_MAX, false); 
    unsigned long t_start_ms = 0;
    unsigned long t_end_ms = 0;
    unsigned long t_delta_ms = 0;

    uint16_t sample_array[16] = {0};
    uint32_t sample_sum = 0;
    uint16_t digital_input = 0;
    uint16_t digital_input_prev = 0;

    const int THRESHOLD_T = 16*512;
    const int THRESHOLD_B = 16*128;

    bool start_flag;
    uint16_t half_symbols;
    uint32_t decode_data;
    uint16_t decode_bit_position;

    while(1)
    {

        // shift and sum previous samples
        sample_sum = 0;
        for (int i = 0; i < 15; i++)
        {
            sample_array[i] = sample_array[i + 1];
            sample_sum += sample_array[i];
        }
        
        // get new sample, add to sum
        sample_array[15] = analogRead(A0);
        sample_sum += sample_array[15];
        
        // convert to digital with hysteresis
        digital_input_prev = digital_input; 
        if (sample_sum < THRESHOLD_B)
        {
            digital_input = 0;
        }
        else if (sample_sum > THRESHOLD_T)
        {
            digital_input = 1;
        }

        // process rising edge
        if (digital_input > digital_input_prev)
        {
            t_end_ms = millis();
            t_delta_ms = t_end_ms - t_start_ms;
            t_start_ms = t_end_ms;
            
            half_symbols = msToHalfSymbolsEstimated(t_delta_ms);
            
            if (start_flag)
            {
                if (half_symbols > 64)
                {
                    Serial.printf("ERROR: Excessive duration LOW following START sequence. Frame discarded.\n");
                    start_flag = false;
                }
                half_symbols--;
                while (half_symbols > 0 && half_symbols < 65)
                {
                    Serial.printf("0 ");
                    decode_bit_position++;
                    half_symbols -= 2;
                }
            }
        }
        // process falling edge
        else if (digital_input < digital_input_prev)
        {
            t_end_ms = millis();
            t_delta_ms = t_end_ms - t_start_ms;
            t_start_ms = t_end_ms;
            
            half_symbols = msToHalfSymbolsEstimated(t_delta_ms);
            
            if (half_symbols == 1 && start_flag)
            {
                Serial.printf("1 ");
                bitSet(decode_data, decode_bit_position);
                decode_bit_position++;
            }
            if (half_symbols == 7)
            {
                Serial.printf("START ");
                start_flag = true;
                decode_data = 0;
                decode_bit_position = 0;
                
            }
            else if (half_symbols == 3)
            {
                Serial.printf("STOP\n");
                start_flag = false;
                if (decode_bit_position != 32)
                {
                    Serial.printf("ERROR: Sequence length incorrect. Frame discarded.\n");
                }
                else
                {
                    Serial.printf("Frame successfully decoded. Data value: 0x%08X.\n", decode_data);
                    decode_bit_position = 0;
                    auto packet = new TransmissionData_t;
                    packet->temp_data = decode_data;
                    received_packets.push(packet);
                }
            }
        }
    }
}

uint16_t msToHalfSymbolsEstimated(unsigned long time_ms)
{
    uint16_t half_symbols;
    uint16_t remainder_ms;
    
    half_symbols = time_ms/SYMBOL_HALF_MS;
    remainder_ms = time_ms%SYMBOL_HALF_MS;
    if (remainder_ms > SYMBOL_HALF_MS/2)
    {
        half_symbols++;
    }
return half_symbols;
}
