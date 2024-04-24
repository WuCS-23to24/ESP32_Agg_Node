#include <queue>

#ifndef DATA_PACKET_H
#define DATA_PACKET_H

typedef struct __attribute__((__packed__)) TransmissionData
{
    float temp_data;
    float latitude;
    float longitude;
    float altitude;
} TransmissionData_t;

typedef union TransmissionDataConverter_u {

    TransmissionData_t message;
    uint8_t bytes[sizeof(TransmissionData)];

} TransmissionDataConverter_t;

extern std::queue<TransmissionData_t *> received_packets;
#endif