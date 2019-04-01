/*HEADER**********************************************************************
 *
 * Copyright 2011 TSS
 *
 *****************************************************************************
 *
 * Comments:
 *
 *       Provide MFS file system on a SDCARD
 *
 *
 *
 *END************************************************************************/

#include <string.h>
#include <mqx.h>
#include <bsp.h>
#include <nio.h>
#include <nio_mem.h>
#include <stdio.h>
#include <mfs.h>
#include <part_mgr.h>
#include <fcntl.h>
#include <unistd.h>
#include <fs_supp.h>

#include <esdhc.h>
#include <sdcard.h>
#include <sdcard_esdhc.h>

#ifdef BOARD_SDHC_INSTANCE
#define CDET_PIN sdhcCdPin
#else
#define CDET_PIN sdcardCdPin
#endif

#include "mfs_filesystem.h"
#include "mfs_sdcard_settings.h"
#include "main.h"

static int dev_fd = -1, a_fd = -1;
static char partman_name[] = "pm:";
static int partition_handle;


int8_t mfs_mount_filesystem(void)
{
    static LWSEM_STRUCT lwsem_mfs;
    _task_id mfs_tid;
    _mqx_uint rc;

    rc = _lwsem_create(&lwsem_mfs, 0);
    if (rc != MQX_OK) {
        fprintf(stderr, "lwsem create failed!\n");
        return -1;
    }

    mfs_tid = _task_create(0, MFS_FILESYSTEM_IDX, (uint32_t)&lwsem_mfs);
    if (mfs_tid == MQX_NULL_TASK_ID) {
        fprintf(stderr, "Unable to start MSF FileSystem task.\n");
        return -1;
    }

    rc = _lwsem_wait(&lwsem_mfs);
    if (rc != MQX_OK) {
        fprintf(stderr, "lwsem wait failed!\n");
        return -1;
    }

    return 0;
}

void mfs_sdcard_detect_isr(void *lwsem_ptr)
{
    GPIO_DRV_ClearPinIntFlag(CDET_PIN->pinName);

    /* Post card_detect semaphore */
    _lwsem_post(lwsem_ptr);
}

void mfs_filesystem_task(uint32_t param)
{
    bool inserted = FALSE, last = FALSE;
    int32_t error_code;
    uint32_t error;
    uint32_t isFormatted = 0;
    int esdhc_handle = -1;
    char partition_name[] = "pm:1";
    SDCARD_INIT_STRUCT sdcard_init_data;
    LWSEM_STRUCT lwsem;

    LWSEM_STRUCT *lwsem_mfs = (LWSEM_STRUCT *)param;

    /* Create and post card detection semaphore */
    error = _lwsem_create(&lwsem, 0);
    if (error != MQX_OK)
    {
        fprintf(stderr, "lwsem create failed!\n");
        _task_block();
    }

    error = _lwsem_post(&lwsem);
    if(error != MQX_OK)
    {
        fprintf(stderr, "lwsem post failed!\n");
        _task_block();
    }

#ifdef BOARD_SDHC_GPIO_INSTANCE
    configure_gpio_pins(GPIO_EXTRACT_PORT(BOARD_SDHC_GPIO_INSTANCE));
#endif

    /* initialize SDHC pins */
    configure_sdhc_pins(BOARD_SDHC_INSTANCE);

    /* install SDHC low-level driver */
    _nio_dev_install("esdhc:", &nio_esdhc_dev_fn, (void*)&_bsp_esdhc_init, NULL);

    /* get an instance of the SDHC driver */
    esdhc_handle = open("esdhc:", 0);
    if (esdhc_handle < 0)
    {
        fprintf(stderr, "Could not open esdhc!\n");
        _task_block();
    }

    /* prepare init data structure */
    sdcard_init_data.com_dev = esdhc_handle;
    sdcard_init_data.const_data = (SDCARD_CONST_INIT_STRUCT_PTR)&sdcard_esdhc_init_data;

    /* install device */
    if (_nio_dev_install("sdcard:", &nio_sdcard_dev_fn, (void*)&sdcard_init_data, NULL) == NULL)
    {
        /* Number of sectors is returned by ioctl IO_IOCTL_GET_NUM_SECTORS function */
        /* If another disc structure is desired, use MFS_FORMAT_DATA structure to   */
        /* define it and call standart format function instead default_format       */
        fprintf(stderr, "Error installing memory device\n");
        _task_block();
    }

    /* install isr for card detection handling and initialize gpio pin */
    _int_install_isr(g_portIrqId[GPIO_EXTRACT_PORT(CDET_PIN->pinName)], mfs_sdcard_detect_isr, &lwsem);
    GPIO_DRV_InputPinInit(CDET_PIN);

    for (;;)
    {
        /* Wait for card insertion or removal */
        error = _lwsem_wait(&lwsem);
        if (error != MQX_OK) {
            fprintf(stderr, "lwsem_wait failed!\n");
            _task_block();
        }

        /* Check if card is present */
        if (CDET_PIN->config.pullSelect == kPortPullDown) {
            inserted = GPIO_DRV_ReadPinInput(CDET_PIN->pinName);
        } else {
            inserted = !GPIO_DRV_ReadPinInput(CDET_PIN->pinName);
        }

        if (last != inserted)
        {
            last = inserted;

            /* Card detection switch debounce delay */
            _time_delay(100);

            if (inserted)
            {
                /* Open the device which MFS will be installed on */
                dev_fd = open("sdcard:", O_RDWR);
                if (dev_fd <= 0)
                {
                    fprintf(stderr, "Unable to open SDCARD device\n");
                    _task_block();
                }

                /* Install partition manager over SD card driver */
                error_code = _io_part_mgr_install(dev_fd, partman_name, 0);
                if (error_code != MFS_NO_ERROR)
                {
                    fprintf(stderr, "Error installing partition manager: %s\n",
                        MFS_Error_text((uint32_t)error_code));
                    _task_block();
                }

                /* Open partition */
                partition_handle = open(partition_name, O_RDWR);
                if (partition_handle >= 0)
                {
                    fprintf(stdout, "Installing MFS over partition...\n");

                    /* Validate partition */
                    error_code = ioctl(partition_handle, IO_IOCTL_VAL_PART, NULL);
                    if (error_code != MFS_NO_ERROR)
                    {
                        fprintf(stderr, "Error validating partition: %s\n", MFS_Error_text((uint32_t)error_code));
                        fprintf(stderr, "Not installing MFS.\n");
                        _task_block();
                    }

                    /* Install MFS over partition */
                    error_code = _io_mfs_install(partition_handle, "a:", 0);
                    if (error_code != MFS_NO_ERROR) {
                        fprintf(stderr, "Error initializing MFS over partition: %s\n",
                            MFS_Error_text((uint32_t)error_code));
                    }

                }
                else
                {
                    fprintf(stdout, "Installing MFS over SD card driver...\n");

                    /* Install MFS over SD card driver */
                    error_code = _io_mfs_install(dev_fd, "a:", (_file_size)0);
                    if (error_code != MFS_NO_ERROR) {
                        fprintf(stderr, "Error initializing MFS: %s\n",
                            MFS_Error_text((uint32_t)error_code));
                    }
                }

                /* Open the filesystem and format detect, if format is required */
                a_fd = open("a:", O_RDWR);
                if (a_fd <= 0)
                {
                    fprintf(stderr, "Error while opening a:\\ (%s)",
                        MFS_Error_text((uint32_t)errno));
                    _task_block();
                }

                _io_register_file_system(a_fd, "a:");

                /* We check if the device is formatted with the ioctl command. */
                error_code = ioctl(a_fd, IO_IOCTL_CHECK_FORMATTED, &isFormatted);
                if (error_code < 0) {
                    fprintf(stderr, "Error while opening a:\\ (%s)", MFS_Error_text((uint32_t)error_code));
                    _task_block();
                } else {
                    if (!isFormatted) {
                        fprintf(stderr, "NOT A DOS DISK! You must format to continue.\n");
                    }
                }

                error = _lwsem_post(lwsem_mfs);
                if(error != MQX_OK) {
                    fprintf(stderr, "lwsem post failed!\n");
                    _task_block();
                }
            }
            else
            {
                /* Close the filesystem */
                if ((a_fd >= 0) && (MQX_OK != close(a_fd))) {
                    fprintf(stderr, "Error closing filesystem.\n");
                }
                a_fd = -1;

                /* Force uninstall filesystem */
                error_code = _nio_dev_uninstall_force("a:", NULL);
                if (error_code < 0) {
                    fprintf(stderr, "Error uninstalling filesystem.\n");
                }

                /* Close partition */
                if ((partition_handle >= 0) && (MQX_OK != close(partition_handle))) {
                    fprintf(stderr, "Error closing partition.\n");
                }
                partition_handle = -1;

                /* Uninstall partition manager */
                error_code = _nio_dev_uninstall_force(partman_name, NULL);
                if (error_code < 0) {
                    fprintf(stderr, "Error uninstalling partition manager.\n");
                }

                /* Close the SD card device */
                if ((dev_fd >= 0) && (MQX_OK != close(dev_fd))) {
                    fprintf(stderr, "Error closing SD card device.\n");
                }

                dev_fd = -1;

                fprintf(stdout, "SD card uninstalled.\n");
            }
        }
    }
}


