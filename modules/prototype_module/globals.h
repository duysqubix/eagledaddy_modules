
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#define MAX_RF_DATA_LEN 16
#define MAX_RX_PACKET_LEN 64
#define MAX_SCHEDULE_TIMES 4

#define EEMEM_MOTOR_TIME_ADDR 0x00
#define EEMEM_MOTOR_SCH_ADDR 0x01

typedef unsigned long long uint64_t;
typedef unsigned long uint32_t;
typedef unsigned int uint16_t;
typedef unsigned char uint8_t;

typedef struct {
    uint64_t source_addr;
    uint8_t recv_opts;
    uint8_t rf_data[MAX_RF_DATA_LEN];
} RecieveFrame;

typedef struct {
    uint16_t module_id;
    uint8_t cmd;
    uint8_t args[MAX_RF_DATA_LEN-3];
} MasterRequest;


typedef struct{
    uint8_t hour;
    uint8_t min;
} ScheduleTime;

const uint16_t MOD_ID = 0x001a;
uint8_t HEADER[3], RX_PACKET[MAX_RX_PACKET_LEN];
uint8_t RX_BUF;

#endif
