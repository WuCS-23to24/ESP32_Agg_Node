#include <stdint.h>
#include <Arduino.h>
#include "data_packet.h"

class Acoustic
{
    private:
        TransmissionData _data;
        const int carrier_frequency = 10000;
        const int symbol_period = 10;

        void transmitFloat(float float_data)
        { 
            // START SEQUENCE: 1/2 period on, 1/2 period off
            tone(A0, carrier_frequency);
            delay(symbol_period/2);
            
            // DATA SEQUENCE: 1 period on for 1, 1 period off for 0 (NRZ)
            for (int i = 0; i < 32; i++)
            {
                if (bitRead(*(uint32_t *)&float_data, i) == 1)
                {
                    tone(A0, carrier_frequency);
                    delay(symbol_period);
                }
                else
                {
                    noTone(A0);
                    digitalWrite(A0, LOW);
                    delay(symbol_period);
                }
            }
            
            // STOP SEQUENCE: 1/2 period off, 1/2 period on
            noTone(A0);
            delay(symbol_period/2);
            tone(A0, carrier_frequency);
            delay(symbol_period/2);
            noTone(A0);
            digitalWrite(A0, LOW);
        }
        
    public:
        TransmissionData getData()
        {
            return _data;
        }

        void setData(TransmissionData data)
        {
            _data = data;
        }

        void transmitFrame()
        {
            transmitFloat(_data.temp_data);
        }

};