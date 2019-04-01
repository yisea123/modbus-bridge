/*HEADER**********************************************************************
*
* Copyright 2008 Freescale Semiconductor, Inc.
* Copyright 2004-2008 Embedded Access Inc.
* Copyright 1989-2008 ARC International
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
*   This file contains the source for an MFS shell function.
*
*
*END************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <mqx.h>
#include <shell.h>
#include <sh_prv.h>

#if SHELLCFG_USES_MFS
#include <mfs.h>

#include "fs_supp.h"

static int Shell_mfs_rename(int fd, char *old_ptr, char *new_ptr)
{
   MFS_RENAME_PARAM           rename_struct;

   rename_struct.OLD_PATHNAME = old_ptr;
   rename_struct.NEW_PATHNAME = new_ptr;

   return 0 > ioctl(fd, IO_IOCTL_RENAME_FILE, (void *)&rename_struct);
}

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    :  Shell_rename
* Returned Value   :  uint32_t error code
* Comments  :  Renames or moves a file.
*
*END*---------------------------------------------------------------------*/

int32_t  Shell_rename(int32_t argc, char *argv[] )
{ /* Body */
   bool                    print_usage, temp, shorthelp = FALSE;
   int32_t                     error = 0, return_code = SHELL_EXIT_SUCCESS;
   int fd;
   SHELL_CONTEXT_PTR    shell_ptr = Shell_get_context( argv );
   char              *abs_path_old = NULL;
   char              *abs_path_new = NULL;


   print_usage = Shell_check_help_request(argc, argv, &shorthelp );

   if (!print_usage)  {
      if (argc != 3)  {
         fprintf(shell_ptr->STDOUT, "Error, invalid number of parameters\n");
         return_code = SHELL_EXIT_ERROR;
         print_usage=TRUE;
      } else  {
         if (MFS_alloc_2paths(&abs_path_old,&abs_path_new) != MFS_NO_ERROR) {
            fprintf(shell_ptr->STDOUT, "Error, unable to allocate memory for paths\n" );
            return_code = SHELL_EXIT_ERROR;
         }
         else
         {
            _io_get_dev_for_path(abs_path_old,&temp,PATHNAME_SIZE,(char *)argv[1],Shell_get_current_filesystem_name(shell_ptr));
            _io_get_dev_for_path(abs_path_new,&temp,PATHNAME_SIZE,(char *)argv[2],Shell_get_current_filesystem_name(shell_ptr));
            if (strcmp(abs_path_new,abs_path_old))
            {
               fprintf(shell_ptr->STDOUT, "Error, rename is possible only within one device\n" );
               return_code = SHELL_EXIT_ERROR;
            } else {
            fd = _io_get_fs_by_name(abs_path_new);
               if (0 > fd)  {
                  fprintf(shell_ptr->STDOUT, "Error, file system not mounted\n" );
                  return_code = SHELL_EXIT_ERROR;
               } else {
                  error = _io_rel2abs(abs_path_old,shell_ptr->CURRENT_DIR,(char *) argv[1],PATHNAME_SIZE,Shell_get_current_filesystem_name(shell_ptr));
                  if(!error)
                  {
                     error = _io_rel2abs(abs_path_new,shell_ptr->CURRENT_DIR,(char *) argv[2],PATHNAME_SIZE,Shell_get_current_filesystem_name(shell_ptr));
                     if(!error)
                     {
                        error = Shell_mfs_rename(fd, abs_path_old, abs_path_new );
                     }
                  }
                  if (error)  {
                     fprintf(shell_ptr->STDOUT, "Error renaming file %s\n", argv[1]);
                  }
               }
            }
         }
      }
   }

   MFS_free_path(abs_path_old);
   MFS_free_path(abs_path_new);


   if (print_usage)  {
      if (shorthelp)  {
         fprintf(shell_ptr->STDOUT, "%s <oldname> <newname> \n", argv[0]);
      } else  {
         fprintf(shell_ptr->STDOUT, "Usage: %s <oldname> <newname>\n", argv[0]);
         fprintf(shell_ptr->STDOUT, "   <oldname> = name of file to change\n");
         fprintf(shell_ptr->STDOUT, "   <newname> = new name of file\n");
      }
   }
   return return_code;
} /* Endbody */

#endif //SHELLCFG_USES_MFS
/* EOF*/
