/*HEADER**********************************************************************
*
* Copyright 20013 Freescale Semiconductor, Inc.
*
* This software is owned or controlled by Freescale Semiconductor.
* Use of this software is governed by the Freescale MQX RTOS License
* distributed with this Material.
* See the MQX_RTOS_LICENSE file distributed for more details.
*
* Brief License Summary:
* This software is provided in source form for you to use free of charge,
* but it is not open source software. You are allowed to use this software
* but you cannot redistribute it or derivative works of it in source form.
* The software may be used only in connection with a product containing
* a Freescale microprocessor, microcontroller, or digital signal processor.
* See license agreement file for full license terms including other
* restrictions.
*****************************************************************************
*
* Comments:
*
*   This file contains the functions that are used to initialize TFS
*   It also contains the TFS driver functions.
*
*
*END************************************************************************/
#include <stdint.h>
#include <assert.h>
#include <fsl_os_abstraction.h>
#include "nio_tfs.h"

#include "nio.h"

#include "fcntl.h"
#include "ioctl.h"

/** TFS device context
 */
typedef struct
{
    const NIO_TFS_DIR_ENTRY *ROOT;  ///< pointer to root of trivial file system
    semaphore_t LOCK;          ///< device lock - provide necessary attomic operations
} NIO_TFS_DEV_CONTEXT_STRUCT;

/** TFS File descriptor context
 */
typedef struct
{
    const NIO_TFS_DIR_ENTRY *ENTRY;
    uint32_t LOCATION;            ///< actual position in file
    int FLAGS;                  ///< file attributes
    uint32_t SIZE;                ///< file size
    int ERROR;                  ///< last error code
    semaphore_t LOCK;
} NIO_TFS_FP_CONTEXT_STRUCT;



/** Isolates the device name and file name.
 * \param devname < string contain device name and file name
 * \return A pointer to character following the semicolon (':') in devname string
 * or if the string does not contain a semicolon return NULL
 */
static const char* nio_tfs_parse_devname(const char *devname)
{
    const char *p;

    assert(NULL != devname);

    for (p = devname; (':' != *p) && (*p); p++)
    {
    }

    return (':' == *p) ? (const char*)p + 1 : NULL;
}

/** Compares file paths.
 * Not case sensitive, both delimiters '/' and '\' are supported.
 * \return zero if paths are same
 */
static int nio_tfs_cmp(const char *path1, const char *path2)
{
    int res;
    unsigned int ch1, ch2;

    if (path1 == path2) {
        res = 0;
    }
    else if (path1 == NULL) {
        res = -1;
    }
    else if (path2 == NULL) {
        res = 1;
    }
    else {
        do {
            ch1 = *path1++;
            ch2 = *path2++;

            if ('\\' == ch1)
            {
                // backslash conversion
                ch1 = '/';
            }
            else
            {
                // conversion to uppercase - convert chars from 'a' - 'z' range to 'A' - 'Z', others stay untouched
                ch1 = (ch1 - 'a' < ('z' - 'a')) ? ch1 - 'a' + 'A' : ch1;
            }

            if ('\\' == ch2)
            {
                // backslash conversion
                ch2 = '/';
            }
            else
            {
                // conversion to uppercase - convert chars from 'a' - 'z' range to 'A' - 'Z', others stay untouched
                ch2 = (ch2 - 'a' < ('z' - 'a')) ? ch2 - 'a' + 'A' : ch2;
            }
        } while (ch1 && ch2 && (ch1 == ch2));

        res = ch1 - ch2;
    }

    return res;
}

/** Initialize the Trivial File System.
 */
static int nio_tfs_init(void *init_data, void **dev_context, int *error)
{
    NIO_TFS_DEV_CONTEXT_STRUCT *context;

    assert(NULL != dev_context);
    
    if (NULL == init_data) {
        if (error) {
            *error = NIO_EINVAL;
        }
        return -1;
    }

    context = (NIO_TFS_DEV_CONTEXT_STRUCT*)OSA_MemAlloc(sizeof(NIO_TFS_DEV_CONTEXT_STRUCT));
    if (NULL != context)
    {
        OSA_SemaCreate(&context->LOCK, 1);

        context->ROOT = ((NIO_TFS_INIT_DATA_STRUCT*)init_data)->ROOT;

        *dev_context = (void*)context;
    }
    else if (error) {
        *error = NIO_ENOMEM;
    }

    return context ? 0 : -1;
}

/** Deinitialize the Trivial File System.
 */
static int nio_tfs_deinit(void *dev_context, int *error)
{
    assert(NULL != dev_context);

    OSA_SemaDestroy(&((NIO_TFS_DEV_CONTEXT_STRUCT*)dev_context)->LOCK);
    OSA_MemFree(dev_context);

    return 0;
}


/** Closes given file descriptor.
 */
static int nio_tfs_close(void *dev_context, void *fp_context, int *error)
{
    assert((NULL != dev_context) && (NULL != fp_context));

    if (fp_context)
    {
        ((NIO_TFS_FP_CONTEXT_STRUCT*)fp_context)->ENTRY = NULL;
        OSA_MemFree(fp_context);
    }

    return 0;
}

static const NIO_TFS_DIR_ENTRY* nio_tfs_open_intern(NIO_TFS_DEV_CONTEXT_STRUCT *dev_context, const char *pathname, int *error)
{
    const NIO_TFS_DIR_ENTRY *entry = NULL;

    assert(NULL != dev_context && NULL != error);

    if ((pathname == NULL) || (*pathname == '\0'))
    {
        *error = NIO_EINVAL;
    }
    else
    {
        entry = dev_context->ROOT;
        
        assert (NULL != entry);

        while (entry->NAME != NULL)
        {
            if (0 == nio_tfs_cmp(entry->NAME, pathname))
            {
                break;
            }
            entry++;
        }

        if (NULL == entry->NAME)
        {
            // file not found
            *error = NIO_ENOENT;
            entry = NULL;
        }
    }

    return entry;
}

/** Open file on TFS driver.
 * \param dev_context
 * \param dev_name
 * \param flags
 * \param fp_context
 * \return
 */
static int nio_tfs_open(void *dev_context, const char *dev_name, int flags, void **fp_context, int *error)
{
    NIO_TFS_FP_CONTEXT_STRUCT *fpc;
    const char *name;
    int err = 0;

    assert((NULL != dev_context) && (NULL != dev_name) && (NULL != fp_context));

    if (NULL != (fpc = OSA_MemAlloc(sizeof(NIO_TFS_FP_CONTEXT_STRUCT))))
    {
        memset(fpc, 0, sizeof(NIO_TFS_FP_CONTEXT_STRUCT));
        name = nio_tfs_parse_devname(dev_name);

        // check filename
        if (NULL == name)
        {
            // no such file or directory
            err = NIO_ENOENT;
        } else {
            if (O_RDONLY == (flags & O_ACCMODE))
            {
                // read only
                fpc->ENTRY = nio_tfs_open_intern(dev_context, name, &err);
            }
            else if (O_ACCMODE == (flags & O_ACCMODE))
            {
                err = NIO_EROFS;
            }
            else
            {
                err = NIO_EINVAL;
            }
        }

        if (NULL != fpc->ENTRY)
        {
            // initialise the file information fields
            fpc->SIZE = fpc->ENTRY->SIZE;

            *fp_context = fpc;
        }
        else
        {
            // if fpc->ENTRY is NULL then an err occured
            assert (0 != err);

            fpc->ERROR = err;
            nio_tfs_close(dev_context, fpc, NULL);
        }
    }

    if (err) {
        if (error) {
            *error = err;
        }
        return -1;
    }

    return 0;
}

/** Read data from file on TFS.
 */
static int nio_tfs_read(void *dev_context, void *fp_context, void *buf, size_t nbytes, int *error)
{
    NIO_TFS_FP_CONTEXT_STRUCT *fpc = (NIO_TFS_FP_CONTEXT_STRUCT*)fp_context;
    size_t len;

    assert((NULL != dev_context) && (NULL != fp_context));

    if (buf) {
        if (nbytes)
        {
            // lock file - only one read can be processed in one time
            OSA_SemaWait(&fpc->LOCK, OSA_WAIT_FOREVER);

            // check for EOF
            if (fpc->LOCATION < fpc->SIZE)
            {
                // normalize size
                if (nbytes > fpc->SIZE - fpc->LOCATION)
                {
                    len = fpc->SIZE - fpc->LOCATION;
                }
                else
                {
                    len = nbytes;
                }

                // read data from file
                memcpy(buf, fpc->ENTRY->DATA + fpc->LOCATION, len);
                fpc->LOCATION += len;
            }
            else
            {
                // EOF
                len = 0;
            }

            OSA_SemaPost(&fpc->LOCK);
        }
        else
        {
             //no data to copy
             len = 0;
        }
    }
    else
    {
         if (error) {
             *error = NIO_EFAULT;
         }     
         return -1;
    }

    return len;
}

static _nio_off_t nio_tfs_lseek(void *dev_context, void *fp_context, _nio_off_t offset, int whence, int *error)
{
    NIO_TFS_FP_CONTEXT_STRUCT *fpc = (NIO_TFS_FP_CONTEXT_STRUCT*)fp_context;
    _nio_off_t res = 0;
    int err = 0;

    assert((NULL != dev_context) && (NULL != fp_context));

    OSA_SemaWait(&fpc->LOCK, OSA_WAIT_FOREVER);

    switch (whence) {
    case SEEK_SET:
        if (0 > offset)
        {
            err = NIO_EINVAL;
        }
        else
        {
            fpc->LOCATION = offset;
            res = fpc->LOCATION;
        }
        break;
    case SEEK_CUR:
        if (offset >= 0)
        {
            if ((UINT32_MAX - fpc->LOCATION) < offset)
            {
                err = NIO_EOVERFLOW;
            }
            else
            {
                res = fpc->LOCATION = fpc->LOCATION + (uint32_t)offset;
            }
        }
        else
        {
            if (fpc->LOCATION < -offset)
            {
                err = NIO_EOVERFLOW;
            }
            else
            {
                res = fpc->LOCATION = fpc->LOCATION - (uint32_t)-offset;
            }
        }
        break;
    case SEEK_END:
        if (offset >= 0)
        {
            if ((UINT32_MAX - fpc->SIZE) < offset)
            {
                err = NIO_EOVERFLOW;
            }
            else
            {
                res = fpc->LOCATION = fpc->SIZE + (uint32_t)offset;
            }
        }
        else
        {
            if (fpc->SIZE < -offset)
            {
                err = NIO_EOVERFLOW;
            }
            else
            {
                res = fpc->LOCATION = fpc->SIZE - (uint32_t)-offset;
            }
        }
        break;
    default:
        err = NIO_EINVAL;
        break;
    }

    OSA_SemaPost(&fpc->LOCK);

    if (err) {
        if (error) {
            *error = err;
        }
        return -1;
    }

    return res;
}
 
static int nio_tfs_ioctl(void *dev_context, void *fp_context, int *error, unsigned long int request, va_list ap)
{
    NIO_TFS_FP_CONTEXT_STRUCT *fpc = (NIO_TFS_FP_CONTEXT_STRUCT*)fp_context;
    int err = 0;

    assert((NULL != dev_context) && (NULL != fp_context));

    switch (request)
    {
    case IOCTL_NIO_TFS_GET_NAME:
    {
        char **name = va_arg(ap, char**);

        *name = fpc->ENTRY->NAME;
        break;
    }
    case IOCTL_NIO_TFS_GET_ATTRIBUTES:
    {
        int *attr = va_arg(ap, int*);

        *attr = fpc->ENTRY->FLAGS;
        break;
    }
    case IOCTL_NIO_TFS_GET_LENGTH:
    {
        size_t *size = va_arg(ap, size_t*);
        *size = fpc->SIZE;
        break;
    }
    case IOCTL_NIO_TFS_GET_LAST_ERROR:
    {
        int *last_err = va_arg(ap, int*);
        *last_err = fpc->ERROR;
        break;
    }
    default:
        err = NIO_ENOTSUP;
        break;
    }
    
    if (error) {
        *error = err;
    }

    return err ? -1 : 0;
}
const NIO_DEV_FN_STRUCT nio_tfs_dev_fn =
{
    .OPEN = nio_tfs_open,
    .READ = nio_tfs_read,
    .WRITE = NULL,
    .LSEEK = nio_tfs_lseek,
    .IOCTL = nio_tfs_ioctl,
    .CLOSE = nio_tfs_close,
    .INIT = nio_tfs_init,
    .DEINIT = nio_tfs_deinit,
};
