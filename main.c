/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of Freescale Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * [File Name]     main.c
 * [Platform]      MK64FN1M0VLL12
 * [Project]       RTU
 * [Version]       1.00
 * [Author]        Juan Francisco Carol
 * [Date]          10/28/2016
 * [Language]      'C'
 * [History]       1.00 - Original Release
 *
 */

//-----------------------------------------------------------------------
// Standard C/C++ Includes
//-----------------------------------------------------------------------
#include <errno.h>
//-----------------------------------------------------------------------
// KSDK Includes
//-----------------------------------------------------------------------
#include "main.h"
#include <rtcs.h>
//-----------------------------------------------------------------------
// Application Includes
//-----------------------------------------------------------------------
#include "mfs_filesystem.h"
#include "network.h"
#include "device_modbus.h"
#include "inih.h"
//-----------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------
void main_task(uint32_t param);
void modbus_slave_task(uint32_t param);
void modbus_master_task(uint32_t param);
void mfs_filesystem_task(uint32_t param);
//-----------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------
#define MAIN_TASK_PRIORITY      8
#define MAIN_TASK_STACK_SIZE    (2000)

#define TASK_TEMPLATE_LIST_END  {0, 0, 0, 0, 0, 0, 0}

const TASK_TEMPLATE_STRUCT  MQX_template_list[] =
{
    {MAIN_IDX, main_task, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, "main", MQX_AUTO_START_TASK, 0},
    {MODBUS_SLAVE_IDX, modbus_slave_task, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, "modbus_slave", 0, 0},
    {MODBUS_MASTER_IDX, modbus_master_task, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, "modbus_master", 0, 0},
    {MFS_FILESYSTEM_IDX, mfs_filesystem_task, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, "mfs_filesystem", 0, 0},
    TASK_TEMPLATE_LIST_END
};

static bool is_valid_ipv4(const char *ip)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ip, &(sa.sin_addr), sizeof(sa.sin_addr));
    return (result != 0);
}

static int inih_handler(void *user, const char *section, const char *name, const char *value)
{
    MODBUS_CONF *config = (MODBUS_CONF *)user;

#define MATCH_SECTION(s)    strcmp(section, s) == 0
#define MATCH_KEY(n)        strcmp(name, n) == 0

    if (MATCH_SECTION("master"))
    {
        if (MATCH_KEY("slave_id")) {
            config->slave_id = atoi(value);
        } else if (MATCH_KEY("baudrate")) {
            config->baudrate = atoi(value);
        } else if (MATCH_KEY("parity")) {
            if (value[0] == 'N' || // None
                value[0] == 'E' || // Even
                value[0] == 'O') { // Odd
                config->parity = value[0];
            } else {
                config->parity = 'N'; // Default
            }
        } else if (MATCH_KEY("databit")) {
            uint8_t databit = atoi(value);
            switch (databit)
            {
            case 5:
            case 6:
            case 7:
            case 8:
                config->databit = databit;
                break;
            default:
                config->databit = 8; // Default
            }
        } else if (MATCH_KEY("stopbit")) {
            uint8_t stopbit = atoi(value);
            switch (stopbit)
            {
            case 1:
            case 2:
                config->stopbit = stopbit;
                break;
            default:
                config->stopbit = 1; // Default
            }
        } else if (MATCH_KEY("address_length")) {
            config->address_length = atoi(value);
        } else if (MATCH_KEY("network_type")) {
            if (strcmp(value, "TCP") == 0) {
                config->backend = TCP;
            } else { /* default */
                config->backend = RTU;
            }
        } else if (MATCH_KEY("ip")) {
            if (is_valid_ipv4(config->ip)) {
                strcpy(config->ip, value);
            }
        } else if (MATCH_KEY("port")) {
            config->port = atoi(value);
        }
    } else if (MATCH_SECTION("slave")) {
        if (MATCH_KEY("port")) {
            config->slave_port = atoi(value);
        } else if (MATCH_KEY("coils_address")) {
            config->map.coils_address = atoi(value);
        } else if (MATCH_KEY("coils_quantity")) {
            config->map.coils_quantity = atoi(value);
        } else if (MATCH_KEY("discrete_inputs_address")) {
            config->map.discrete_inputs_address = atoi(value);
        } else if (MATCH_KEY("discrete_inputs_quantity")) {
            config->map.discrete_inputs_quantity = atoi(value);
        } else if (MATCH_KEY("holding_registers_address")) {
            config->map.holding_registers_address = atoi(value);
        } else if (MATCH_KEY("holding_registers_quantity")) {
            config->map.holding_registers_quantity = atoi(value);
        } else if (MATCH_KEY("input_registers_address")) {
            config->map.input_registers_address = atoi(value);
        } else if (MATCH_KEY("input_registers_quantity")) {
            config->map.input_registers_quantity = atoi(value);
        }
    } else {
        return 0;  /* unknown section/name, error */
    }

    return 1;
}

void main_task(uint32_t param)
{
    static LWSEM_STRUCT lwsem;

    fprintf(stdout, "Starting up....\n");

    if (mfs_mount_filesystem() == -1) {
        fprintf(stderr, "Unable to mount MFS filesystem.\n");
        _task_block();
    }

    if (network_init() == -1) {
        fprintf(stderr, "Unable to initialize network.\n");
        _task_block();
    }

    if (network_telnetsrv_start() == -1) {
        fprintf(stderr, "Unable to start Telnet Server.\n");
        _task_block();
    }

    memset(&modbus_conf, 0, sizeof(MODBUS_CONF));
    if (ini_parse("a:\\modbus.ini", inih_handler, &modbus_conf) == -1) {
        fprintf(stderr, "Can't parse Modbus INI file.\n");
        _task_block();
    }

    _mqx_uint rc = _lwsem_create(&lwsem, 0);
    if (rc != MQX_OK) {
        fprintf(stderr, "lwsem create failed!\n");
        _task_block();
    }

    if (_task_create(0, MODBUS_SLAVE_IDX, (uint32_t)&lwsem) == MQX_NULL_TASK_ID) {
        fprintf(stderr, "Unable to start Modbus Slave.\n");
        _task_block();
    }

    rc = _lwsem_wait(&lwsem);
    if (rc != MQX_OK) {
        fprintf(stderr, "lwsem wait failed!\n");
        _task_block();
    }

    if (_task_create(0, MODBUS_MASTER_IDX, 0) == MQX_NULL_TASK_ID) {
        fprintf(stderr, "Unable to start Modbus Slave.\n");
        _task_block();
    }

    fprintf(stdout, "Up and running!\n");
    _task_block();
}
