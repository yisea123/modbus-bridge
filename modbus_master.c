#include <stdint.h>
#include <stdio.h>

#include <mqx.h>
#include <rtcs.h>

#include <modbus.h>

#include "device_modbus.h"

MODBUS_CONF modbus_conf;

typedef struct polling_block {
    uint8_t fc;
    uint32_t address;
    uint16_t quantity;
    uint32_t scan_time;
    uint32_t map;
    uint8_t *tab_rp_bits;
    uint16_t *tab_rp_registers;

    struct polling_block *next;
} POLLING_BLOCK;

static bool get_varval(char *src, char *var_name, char *dst, uint32_t length);
static size_t zstrlen(char *line);

POLLING_BLOCK *next_polling_block(POLLING_BLOCK *block)
{
    return (block->next);
}

int8_t get_fc_address(uint32_t reg_number, uint8_t address_length, uint32_t *address)
{
    uint8_t fc;
    uint32_t addr_length_divisor;
    int8_t fc_table[] = {
            FC_READ_COILS, // FC01=0xxxx
            FC_READ_DISCRETE_INPUTS, // FC02=1xxxx
            -1, // illegal
            FC_READ_INPUT_REGISTERS, // FC04=3xxxx
            FC_READ_HOLDING_REGISTERS  // FC03=4xxxx
    };

    if (address_length == 6) {
        addr_length_divisor = ADDRESS_LENGTH_6_DIVISOR;
    } else if (address_length == 5) {
        addr_length_divisor = ADDRESS_LENGTH_5_DIVISOR;
    } else { /* default */
        addr_length_divisor = ADDRESS_LENGTH_4_DIVISOR;
    }

    fc = reg_number / addr_length_divisor;
    if (fc != 0 && fc != 1 && fc != 3 && fc != 4) {
        return -1; /* invalid register number */
    }

    *address = reg_number - (fc * addr_length_divisor) - 1;

    return fc_table[fc];
}

POLLING_BLOCK *new_polling_block(uint32_t reg_number, uint16_t quantity, uint32_t scan_time, uint32_t map, uint8_t address_length)
{
    POLLING_BLOCK *newblock;

    newblock = (POLLING_BLOCK *)_mem_alloc_system(sizeof(POLLING_BLOCK));
    if (newblock == NULL) {
        return NULL;
    }

    newblock->fc = get_fc_address(reg_number, address_length, &newblock->address);
    if (newblock->fc == -1) {
        _mem_free(newblock);
        return NULL;
    }

    uint8_t map_fc = get_fc_address(map, address_length, &newblock->map);
    if (map_fc == -1) {
        _mem_free(newblock);
        return NULL;
    }

    /* It's just to be sure that mapping address and block address
       are both of the same function code. */
    if (map_fc != newblock->fc) {
        _mem_free(newblock);
        return NULL;
    }

    newblock->quantity = quantity;
    newblock->scan_time = scan_time;
    newblock->tab_rp_bits = NULL;
    newblock->tab_rp_registers = NULL;
    newblock->next = newblock;
    return newblock;
}

void add_block_to_list(POLLING_BLOCK *newblock, POLLING_BLOCK **block_list)
{
    POLLING_BLOCK *aux;

    if (*block_list == NULL) {
        *block_list = newblock;
    } else {
        /* Insert the new block at the beginning of the list. */
        aux = *block_list;
        while (aux->next != *block_list) {
            aux = aux->next;
        }
        newblock->next = *block_list;
        aux->next = newblock;
        *block_list = newblock;
    }
}

int16_t address_offset(uint16_t address, POLLING_BLOCK *block_list)
{
    int16_t offset = 0;
    POLLING_BLOCK *block;

    block = block_list;
    while (!(address >= block->map && address <= block->map + block->quantity) && block->next != block_list) {
        block = block->next;
    }

    /* map <= address <= map + nb */
    if (address >= block->map && address <= block->map + block->quantity) {
        offset = block->address - block->map;
    }

    return offset;
}

void modbus_master_task(uint32_t param)
{
    modbus_t *ctx;
    int32_t rc;
    uint8_t use_backend, address_length;

    if (!(use_backend = modbus_conf.backend)) {
        _task_block();
    }

    if (!(address_length = modbus_conf.address_length)) {
        _task_block();
    }

    if (use_backend == TCP) {
        if (strlen(modbus_conf.ip) == 0 ||
            modbus_conf.port == 0) {
            _task_block();
        }
        ctx = modbus_new_tcp(modbus_conf.ip, modbus_conf.port);
    } else if (use_backend == TCP_PI) {
        ctx = modbus_new_tcp_pi("::1", "1502");
    } else {
        if (!modbus_conf.baudrate || !modbus_conf.parity ||
            !modbus_conf.databit || !modbus_conf.stopbit) {
            _task_block();
        }
        ctx = modbus_new_rtu("nio_ser2:", modbus_conf.baudrate, modbus_conf.parity,
            modbus_conf.databit, modbus_conf.stopbit);
    }
    if (ctx == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        _task_block();
    }

    modbus_set_debug(ctx, FALSE);
    modbus_set_error_recovery(ctx, MODBUS_ERROR_RECOVERY_LINK|MODBUS_ERROR_RECOVERY_PROTOCOL);
#if 0
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    modbus_set_response_timeout(ctx, &timeout);
#endif
    if (use_backend == RTU) {
        if (modbus_conf.slave_id == 0) {
            modbus_conf.slave_id = 1;
        }
        modbus_set_slave(ctx, modbus_conf.slave_id);
    }

    if (use_backend == TCP || use_backend == TCP_PI) {
        while (modbus_connect(ctx) == -1) {
            fprintf(stderr, "Unable to connect to IP %s\n", modbus_conf.ip);
            _time_delay(5000); /* wait for 5 seconds before trying a new connection attempt */
        }
    } else { /* RTU */
        if (modbus_connect(ctx) == -1) {
            fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
            modbus_free(ctx);
            _task_block();
        }
    }

    POLLING_BLOCK *block_list = NULL;
    POLLING_BLOCK *block;
    char line[200], varval[50];
    FILE *fd;

    fd = fopen("a:\\polling_blocks.txt", "r");
    if (fd == NULL) {
        modbus_free(ctx);
        _task_block();
    }

    while (fgets(line, sizeof(line), fd) != NULL)
    {
        /* fgets breaks when it reach either '\r' and '\n' characters. If txt file
           has been edited on windows environment, every line on the file ends with both
           newline characters \r\n; unix environment, on the other hand, only use \n character
           at the end of each line. This code is supposed to be multi-platform. It discards "empty"
           lines (the ones that contain only \n character). */
        uint8_t size = zstrlen(line);
        if (size == 0) {
            continue;
        }

        get_varval(line, "address", varval, sizeof(varval));
        uint32_t reg_number = atoi(varval);
        get_varval(line, "quantity", varval, sizeof(varval));
        uint16_t quantity = atoi(varval);
        get_varval(line, "scantime", varval, sizeof(varval));
        uint32_t scan_time = atoi(varval);
        get_varval(line, "mapping", varval, sizeof(varval));
        uint32_t mapping = atoi(varval);

        block = new_polling_block(reg_number, quantity, scan_time, mapping, address_length);
        if (block == NULL) {
            // Unable to allocate memory for polling block
            modbus_free(ctx);
            _task_block();
        }

        switch (block->fc)
        {
        case FC_READ_COILS:
        case FC_READ_DISCRETE_INPUTS:
            block->tab_rp_bits = (uint8_t *)_mem_alloc_system(block->quantity * sizeof(uint8_t));
            if (block->tab_rp_bits == NULL) {
                fprintf(stderr, "Unable to allocate memory for registers.\n");
                modbus_free(ctx);
                _task_block();
            }
            memset(block->tab_rp_bits, 0, block->quantity * sizeof(uint8_t));
            break;
        case FC_READ_HOLDING_REGISTERS:
        case FC_READ_INPUT_REGISTERS:
            block->tab_rp_registers = (uint16_t *)_mem_alloc_system(block->quantity * sizeof(uint16_t));
            if (block->tab_rp_registers == NULL) {
                fprintf(stderr, "Unable to allocate memory for registers.\n");
                modbus_free(ctx);
                _task_block();
            }
            memset(block->tab_rp_registers, 0, block->quantity * sizeof(uint16_t));
            break;
        default:
            ;
        }

        add_block_to_list(block, &block_list);
    }
    fclose(fd);

    if (!(block = block_list)) {
        modbus_free(ctx);
        _task_block();
    }

    _queue_id master_qid;
    MODBUS_MESSAGE_PTR modbus_message;
    uint8_t j, i;

    master_qid = _msgq_open(_MODBUSQUEUE_base+1, 0);
    if (master_qid == 0) {
        // TODO: free the block list memory
        modbus_free(ctx);
        _task_block();
    }

    for (;;)
    {
        /* TODO: ScanTime */

        switch (block->fc)
        {
        case FC_READ_COILS:
            rc = modbus_read_bits(ctx, block->address, block->quantity, block->tab_rp_bits);
            if (rc == block->quantity) { // TODO: copy bits properly
                _mutex_lock(&mb_mapping_mutex);
                memcpy(mb_mapping->tab_bits + block->map, block->tab_rp_bits, block->quantity);
                _mutex_unlock(&mb_mapping_mutex);
            }
            break;
        case FC_READ_DISCRETE_INPUTS:
            rc = modbus_read_input_bits(ctx, block->address, block->quantity, block->tab_rp_bits);
            if (rc == block->quantity) { // TODO: copy input bits properly
                _mutex_lock(&mb_mapping_mutex);
                memcpy(mb_mapping->tab_input_bits + block->map, block->tab_rp_bits, block->quantity);
                _mutex_unlock(&mb_mapping_mutex);
            }
            break;
        case FC_READ_HOLDING_REGISTERS:
            rc = modbus_read_registers(ctx, block->address, block->quantity, block->tab_rp_registers);
            if (rc == block->quantity) {
                _mutex_lock(&mb_mapping_mutex);
                for (j = 0, i = block->map; i < block->map + block->quantity; i++, j++) {
                    mb_mapping->tab_registers[i] = block->tab_rp_registers[j];
                }
                _mutex_unlock(&mb_mapping_mutex);
            }
            break;
        case FC_READ_INPUT_REGISTERS:
            rc = modbus_read_input_registers(ctx, block->address, block->quantity, block->tab_rp_registers);
            if (rc == block->quantity) {
                _mutex_lock(&mb_mapping_mutex);
                for (j = 0, i = block->map; i < block->map + block->quantity; i++, j++) {
                    mb_mapping->tab_input_registers[i] = block->tab_rp_registers[j];
                }
                _mutex_unlock(&mb_mapping_mutex);
            }
            break;
        default:
            ;
        }

        /* Poll MODBUS queue for a message. */
        if ((modbus_message = (MODBUS_MESSAGE_PTR)_msgq_poll(master_qid)))
        {
            switch (modbus_message->fc)
            {
            case FC_WRITE_SINGLE_REGISTER: /* Write Single Holding Register. FC06 */
                modbus_write_register(ctx, modbus_message->address + address_offset(modbus_message->address, block_list),
                        modbus_message->values[0]);
                break;
            case FC_WRITE_MULTIPLE_REGISTERS: /* Write Multiple Holding Register. FC16 */
                modbus_write_registers(ctx, modbus_message->address + address_offset(modbus_message->address, block_list),
                        modbus_message->quantity, modbus_message->values);
                break;
            case FC_WRITE_SINGLE_COIL: /* Write Single Coil. FC05 */
                // TODO: modbus_write_bit // write single coil. COIL. FC05
                break;
            case FC_WRITE_MULTIPLE_COILS: /* Write Multiple Coils. FC15 */
                // TODO: modbus_write_bits // write multiple coils. COIL. FC15
                break;
            default:
                ;
            }

            /* Free the memory allocated by MODBUS Slave */
            if (modbus_message->values) {
                _mem_free(modbus_message->values);
            }
            _msg_free(modbus_message);
        }

        block = next_polling_block(block);

        _time_delay(1); /* 1 ms period */
    }

#if 0
    close:
    /* Free the memory */
    free(tab_rp_bits);
    free(tab_rp_registers);

    /* Close the connection */
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
#endif
}

/* key=value,key=value */
static bool get_varval(char *src, char *var_name, char *dst, uint32_t length)
{
    char *name;
    bool result;
    uint32_t index;
    uint32_t n_length;

    result = false;
    dst[0] = 0;
    name = src;

    n_length = strlen(var_name);

    while ((name = strstr(name, var_name)) != 0)
    {
        if (name[n_length] == '=')
        {
            name += n_length + 1;

            index = strcspn(name, ",");
            if (index >= length)
            {
                index = length-1;
            }
            strncpy(dst, name, index);
            dst[index] = '\0';
            result = true;
            break;
        }
        else
        {
            name = strchr(name, ',');
        }
    }

    return(result);
}

static size_t zstrlen(char *line)
{
    char *s = line;

    while (*s && *s != '\r' && *s != '\n') s++;
    *s = '\0';
    return (s - line);
}

