#ifndef __DEVICE_MODBUS_H__
#define __DEVICE_MODBUS_H__

#include <stdint.h>

#include <mutex.h>
#include <message.h>
#include <modbus.h>

enum {
    TCP = 1,
    TCP_PI,
    RTU
};

#define ADDRESS_LENGTH_4_DIVISOR    1000
#define ADDRESS_LENGTH_5_DIVISOR    10000
#define ADDRESS_LENGTH_6_DIVISOR    100000

#define COILS_ADDRESS_OFFSET        0

#define DISCRETE_INPUTS_4_OFFSET    1000
#define HOLDING_REGISTERS_4_OFFSET  4000
#define INPUT_REGISTERS_4_OFFSET    3000

#define DISCRETE_INPUTS_5_OFFSET    10000
#define HOLDING_REGISTERS_5_OFFSET  40000
#define INPUT_REGISTERS_5_OFFSET    30000

#define DISCRETE_INPUTS_6_OFFSET    100000
#define HOLDING_REGISTERS_6_OFFSET  400000
#define INPUT_REGISTERS_6_OFFSET    300000

/* Function codes */
#define FC_READ_COILS                  0x01 // FC01=0xxxx
#define FC_READ_DISCRETE_INPUTS        0x02 // FC02=1xxxx
#define FC_READ_HOLDING_REGISTERS      0x03 // FC03=4xxxx
#define FC_READ_INPUT_REGISTERS        0x04 // FC04=3xxxx
#define FC_WRITE_SINGLE_COIL           0x05
#define FC_WRITE_SINGLE_REGISTER       0x06
#define FC_WRITE_MULTIPLE_COILS        0x0F
#define FC_WRITE_MULTIPLE_REGISTERS    0x10

#define MODBUS_QUEUE_BASE              8

typedef struct modbus_memory_map {
    uint32_t coils_address; /* 0xxxx */
    uint16_t coils_quantity;
    uint32_t discrete_inputs_address; /* 1xxxx */
    uint16_t discrete_inputs_quantity;
    uint32_t holding_registers_address; /* 4xxxx */
    uint16_t holding_registers_quantity;
    uint32_t input_registers_address; /* 3xxxx */
    uint16_t input_registers_quantity;
} MODBUS_MEMORY_MAP;

typedef struct modbus_conf {
    uint32_t slave_id;
    uint8_t address_length;
    uint8_t backend; /* network type */

    char ip[16];
    uint32_t port;

    uint32_t baudrate;
    int8_t parity;
    uint8_t databit;
    uint8_t stopbit;

    uint32_t slave_port;

    MODBUS_MEMORY_MAP map;
} MODBUS_CONF;

typedef struct modbus_message {
    MESSAGE_HEADER_STRUCT HEAD;
    uint8_t fc;
    uint16_t address;
    uint16_t quantity;
    uint16_t *values;
} MODBUS_MESSAGE, *MODBUS_MESSAGE_PTR;

extern uint32_t _MODBUSQUEUE_base;

extern MODBUS_CONF modbus_conf;
extern MUTEX_STRUCT mb_mapping_mutex;
extern modbus_mapping_t *mb_mapping;

#endif /* __DEVICE_MODBUS_H__ */
