#include <stdint.h>
#include <stdio.h>

#include <mqx.h>
#include <mutex.h>
#include <rtcs.h>

#include <modbus.h>

#include "device_modbus.h"

MUTEX_STRUCT mb_mapping_mutex;
modbus_mapping_t *mb_mapping;

#define MODBUS_MSGPOOL_INIT            64
#define MODBUS_MSGPOOL_GROW            4
#define MODBUS_MSGPOOL_MAX             2048

#define MODBUS_QUEUE_BASE              8

uint32_t _MODBUS_msgpool_init = MODBUS_MSGPOOL_INIT;
uint32_t _MODBUS_msgpool_grow = MODBUS_MSGPOOL_GROW;
uint32_t _MODBUS_msgpool_max = MODBUS_MSGPOOL_MAX;

uint32_t _MODBUSQUEUE_base = MODBUS_QUEUE_BASE;
static _queue_id slave_qid;
static _pool_id MODBUS_msg_pool;

int8_t MODBUS_message_issue(uint8_t fc, uint16_t address, uint16_t quantity, uint16_t *values);

/* Modbus TCP server device slave */
void modbus_slave_task(uint32_t param)
{
    int socket;
    modbus_t *ctx;
    int rc;
    uint8_t use_backend;
    uint8_t *query;
    uint32_t header_length;

    LWSEM_STRUCT *lwsem = (LWSEM_STRUCT *)param;

    use_backend = TCP;

    if (use_backend == TCP) {
        if (modbus_conf.slave_port == 0) {
            modbus_conf.slave_port = 502;
        }
        ctx = modbus_new_tcp("127.0.0.1", modbus_conf.slave_port);
        if (ctx == NULL) {
            _task_block();
        }
        query = _mem_alloc_system(MODBUS_TCP_MAX_ADU_LENGTH);
        if (query == NULL) {
            modbus_free(ctx);
            _task_block();
        }
    } else if (use_backend == TCP_PI) {
        ctx = modbus_new_tcp_pi("::0", "1502");
        if (ctx == NULL) {
            _task_block();
        }
        query = _mem_alloc_system(MODBUS_TCP_MAX_ADU_LENGTH);
        if (query == NULL) {
            modbus_free(ctx);
            _task_block();
        }
    } else {
        /* RTU over serial line is not supported */
        _task_block();
    }
    header_length = modbus_get_header_length(ctx);
    modbus_set_debug(ctx, FALSE);

    uint32_t offset;

    uint16_t coils_address = modbus_conf.map.coils_address - COILS_ADDRESS_OFFSET;

    if (modbus_conf.address_length == 4) {
        offset = DISCRETE_INPUTS_4_OFFSET;
    } else if (modbus_conf.address_length == 6) {
        offset = DISCRETE_INPUTS_6_OFFSET;
    } else { // Default: address length = 5
        offset = DISCRETE_INPUTS_5_OFFSET;
    }
    uint16_t discrete_inputs_address = 0;
    if (modbus_conf.map.discrete_inputs_address != 0) {
        discrete_inputs_address = modbus_conf.map.discrete_inputs_address - offset;
    }

    if (modbus_conf.address_length == 4) {
        offset = HOLDING_REGISTERS_4_OFFSET;
    } else if (modbus_conf.address_length == 5) {
        offset = HOLDING_REGISTERS_5_OFFSET;
    } else if (modbus_conf.address_length == 6) {
        offset = HOLDING_REGISTERS_6_OFFSET;
    } else {
        // wrong address length
        _mem_free(query);
        modbus_free(ctx);
        _task_block();
    }
    uint16_t holding_registers_address = 0;
    if (modbus_conf.map.holding_registers_address != 0) {
        holding_registers_address = modbus_conf.map.holding_registers_address - offset;
    }

    if (modbus_conf.address_length == 4) {
        offset = INPUT_REGISTERS_4_OFFSET;
    } else if (modbus_conf.address_length == 5) {
        offset = INPUT_REGISTERS_5_OFFSET;
    } else if (modbus_conf.address_length == 6) {
        offset = INPUT_REGISTERS_6_OFFSET;
    } else {
        // wrong address length
        _mem_free(query);
        modbus_free(ctx);
        _task_block();
    }
    uint16_t input_registers_address = 0;
    if (modbus_conf.map.input_registers_address != 0) {
        input_registers_address = modbus_conf.map.input_registers_address - offset;
    }

    slave_qid = _msgq_open(_MODBUSQUEUE_base, 0);
    if (slave_qid == 0) {
        _mem_free(query);
        modbus_free(ctx);
        _task_block();
    }

    MODBUS_msg_pool = _msgpool_create(sizeof(MODBUS_MESSAGE), _MODBUS_msgpool_init, _MODBUS_msgpool_grow, _MODBUS_msgpool_max);
    if (MODBUS_msg_pool == MSGPOOL_NULL_POOL_ID) {
        _mem_free(query);
        modbus_free(ctx);
        _msgq_close(slave_qid);
        _task_block();
    }

    _mutex_init(&mb_mapping_mutex, NULL);

    mb_mapping = modbus_mapping_new(
        coils_address + modbus_conf.map.coils_quantity,
        discrete_inputs_address + modbus_conf.map.discrete_inputs_quantity,
        holding_registers_address + modbus_conf.map.holding_registers_quantity,
        input_registers_address + modbus_conf.map.input_registers_quantity);
    if (mb_mapping == NULL)
    {
        fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        _mem_free(query);
        _msgq_close(slave_qid);
        _msgpool_destroy(MODBUS_msg_pool);
        _task_block();
    }

    _lwsem_post(lwsem); /* Modbus Map has been created */

    if (use_backend == TCP) {
        socket = modbus_tcp_listen(ctx, 1);
    } else { /* use_backend == TCP_PI */
        socket = modbus_tcp_pi_listen(ctx, 1);
    }

    for (;;)
    {
        if (use_backend == TCP) {
            modbus_tcp_accept(ctx, &socket);
        } else { /* use_backend == TCP_PI */
            modbus_tcp_pi_accept(ctx, &socket);
        }

        for (;;)
        {
            rc = modbus_receive(ctx, query);
            if (rc == -1) {
                /* Connection closed by the client or error */
                break;
            }

            uint16_t address, quantity;
            uint16_t *values;

            /* Write Single Holding Register. FC06 */
            if (query[header_length] == FC_WRITE_SINGLE_REGISTER) {
                quantity = 1;
                address = MODBUS_GET_INT16_FROM_INT8(query, header_length + 1);

                if (address >= mb_mapping->nb_registers) {
                    /* MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS */
                } else {
                    values = (uint16_t *)_mem_alloc_system(quantity * sizeof(uint16_t));
                    if (values == NULL) {
                        _task_block(); // TODO: what to do here?
                    }

                    *values = MODBUS_GET_INT16_FROM_INT8(query, header_length + 3);
                    MODBUS_message_issue(FC_WRITE_SINGLE_REGISTER, address, quantity, values);
                }
            }
            /* Write Multiple Holding Register. FC16 */
            else if (query[header_length] == FC_WRITE_MULTIPLE_REGISTERS) {
                uint8_t n, i;
                uint16_t offset;

                address = MODBUS_GET_INT16_FROM_INT8(query, header_length + 1); /* The data address of the first register. */
                quantity = MODBUS_GET_INT16_FROM_INT8(query, header_length + 3); /* The number of registers to write. */

                if ((address + quantity) > mb_mapping->nb_registers) {
                    /* MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS */
                } else {
                    n = query[header_length + 5]; /* The number of data bytes to follow. */
                    n /= 2; /* (1 register x 2 bytes each = 2 bytes) */
                    offset = header_length + 6;

                    values = (uint16_t *)_mem_alloc_system(quantity * sizeof(uint16_t));
                    if (values == NULL) {
                        _task_block(); // TODO: what to do here?
                    }

                    for (i = 0; i < n; i++) {
                        values[i] = MODBUS_GET_INT16_FROM_INT8(query, offset);
                        offset += 2; /* Next value. */
                    }
                    MODBUS_message_issue(FC_WRITE_MULTIPLE_REGISTERS, address, quantity, values);
                }
            }
            /* Write Single Coil. FC05 */
            else if (query[header_length] == FC_WRITE_SINGLE_COIL) {
                // TODO: issue single coil message
            }
            /* Write Multiple Coils. FC15 */
            else if (query[header_length] == FC_WRITE_MULTIPLE_COILS) {
                // TODO: issue multiple coil message
            }

            _mutex_lock(&mb_mapping_mutex);
            rc = modbus_reply(ctx, query, rc, mb_mapping);
            _mutex_unlock(&mb_mapping_mutex);
            if (rc == -1) {
                break;
            }
        }

    }

#if 0
    closesocket(socket);

    modbus_mapping_free(mb_mapping);
    _mem_free(query);
    modbus_free(ctx);

    _task_block();
#endif
}

int8_t MODBUS_message_issue(uint8_t fc, uint16_t address, uint16_t quantity, uint16_t *values)
{
    uint32_t n_free_msgs;
    MODBUS_MESSAGE_PTR message;

    n_free_msgs = _msg_available(MODBUS_msg_pool);
    if (n_free_msgs > 0)
    {
        message = _msg_alloc(MODBUS_msg_pool);
        if (message == NULL) {
            return -1;
        }

        message->HEAD.SOURCE_QID = slave_qid;
        message->HEAD.TARGET_QID = _msgq_get_id(0, _MODBUSQUEUE_base+1);
        message->HEAD.SIZE = sizeof(*message);
        message->fc = fc;
        message->address = address;
        message->quantity = quantity;
        message->values = values;

        if (!_msgq_send(message)) {
            return -1;
        }
    }

    return 0;
}
