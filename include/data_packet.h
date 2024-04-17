#ifndef DATA_PACKET_H
#define DATA_PACKET_H

typedef struct __attribute__((__packed__)) TransmissionData
{
    float temp_data;
    float latitude;
    float longitude;
    float altitude;
} TransmissionData_t;

#endif