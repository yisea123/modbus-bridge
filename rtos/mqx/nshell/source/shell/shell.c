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
 *   This file contains the RTCS shell.
 *
 *
 *END************************************************************************/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <mqx.h>
#include <mqx_inc.h> /* internal kernel structures */
#include "shell.h"
#include "sh_prv.h"

#include <nio_tty.h>
#include <unistd.h>
#include <ioctl.h>

#include "fs/fs_supp.h"

static void insert_at_pos(char *linebuf, uint32_t linebufsize, char data, uint32_t pos)
{   /* insert a character from command at position */
    uint32_t i;
    for (i=linebufsize-1; i>pos; i--) linebuf[i] = linebuf[i-1];
    linebuf[pos] = data;
}

static void delete_at_pos(char *linebuf, uint32_t linebufsize, uint32_t pos)
{   /* delete a character from command at position */
    uint32_t i;
    for (i=pos; i<linebufsize-1; i++) linebuf[i] = linebuf[i+1];
    linebuf[linebufsize-1] = '\0'; // insert null at end of linebuf for history
}

static void delete_all(char *linebuf, uint32_t linebufsize)
{   /* delete all characters of command */
    uint32_t i,j,len;
    len = strlen(linebuf);
    for (i=0,j=len; j<linebufsize; i++,j++) linebuf[i] = linebuf[j];
    for ( ; i<linebufsize; i++) linebuf[i] = '\0'; // insert null(s) at end of linebuf for history
}

static void terminate_history(char *linebuf, uint32_t linebufsize)
{
    /* make sure that history located after first null character of linebuf is still correctly */
    /* terminated : the last entry in history at end of linebuf must be terminated or cleared. */
    /* For this, start from end of linebuf to beginning, writing null characters until a null */
    /* is found */
    uint32_t i;
    for (i=linebufsize-1; i>=0; i--) {
        if (linebuf[i] == '\0') break;
        linebuf[i] = '\0';
    }
}

static void get_history_entry(char *linebuf, uint32_t linebufsize, uint32_t entry)
{
    /* recall an history entry (entry is between 1 and get_history_size()) by copying it */
    /* to the beginning of linebuf (before first null) */
    int32_t histpos = -1; // -1 to have 0 at first null (where history starts)
    uint32_t i;
    if (entry < 1) return;
    for (i=0; i<linebufsize; i++) {
        if (linebuf[i] == '\0') histpos++; // end of command or history string (null)
        if (histpos == entry) {
            i--; // go back to last character of history string before null
            /* now copy one character at once to make it work even if history entry size is near linebufsize */
            while (linebuf[i]) insert_at_pos(linebuf, linebufsize, linebuf[i], 0);
            break;
        }
    }
    terminate_history(linebuf, linebufsize);
}

static uint32_t get_history_size(char *linebuf, uint32_t linebufsize)
{   /* compute number of entries of history currently in linebuf (after first null) */
    int32_t histsize = -1; // -1 to have 0 at first null (where history starts)
    uint32_t i;
    char lastchar = ' ';
    for (i=0; i<linebufsize; i++) {
        if (linebuf[i] == '\0') {
            if (lastchar == '\0') break; // two consecutive null : no need to continue (end of history)
            histsize++; // increment size because at end of command or history string (null)
        }
        lastchar = linebuf[i];
    }
    return histsize;
}

static bool already_in_history(char *linebuf, uint32_t linebufsize)
{   /* check if command currently in linebuf (before first null) is already present in */
    /* the first entry of history */
    uint32_t i,j;
    for (i=0,j=strlen(linebuf)+1; j<linebufsize; i++,j++) {
        if (linebuf[i] != linebuf[j]) return false;
        if (linebuf[i] == '\0') break;
    }
    return true;
}

/*FUNCTION*-----------------------------------------------------------------------
 *
 *  Function Name : Shell_getline_internal
 *  Returned Value: int32_t (0:ENTER, 1:ESCAPE+ESCAPE or CTRL-C, -1:error or EOF)
 *  Comments  :  Edit a line from stdin with history and arrow keys support
 *               I/O echo (and translation ?) must be disabled on stdin.
 *
 *               Edition is limited to linemaxsize-1 characters on the line.
 *               Number of entries in history depend on the size of commands in history
 *               and on the line buffer whose size is supplied in linebufsize.
 *               Data in linebuf on linebufsize bytes is :
 *               <linestring>NULL<history1>NULL<history2>NULL<historyN>NULL...NULL
 *
 *               ENTER ; validate edition
 *               CTRL-C or ESCAPE+ESCAPE : abort edition
 *               BACKSPACE (BS or DEL) : delete character on the left of cursor
 *               CTRL-A : move cursor to beginning of line
 *               CTRL-E : move cursor to end of line
 *               CTRL-B / ARROW-LEFT : move cursor back one character
 *               CTRL-F / ARROW-RIGHT : move cursor forward one character
 *               CTRL-P / ARROW-UP : older history entry
 *               CTRL-N / ARROW-DOWN : newer history entry
 *
 *               A line starting with a # is a comment and is not added to history
 *
 *               If the first key pressed is ! , the last command is recalled
 *               (compat with older shell behavior where !<ENTER> runs last command)
 *
 *               For history management reasons, linebuf must be all filled with 00s
 *               before being used. It must not be modified by an external code
 *               even after the first null character (the history is there).
 *
 *END*-----------------------------------------------------------------------------*/
#define ENTER 13
#define ESCAPE 27
#define BACKSPACE 8
#define DELETE 127
#define CTRL_A 1
#define CTRL_B 2
#define CTRL_C 3
#define CTRL_E 5
#define CTRL_F 6
#define CTRL_N 14
#define CTRL_P 16

static int32_t Shell_getline_internal(char *linebuf, uint32_t linebufsize, uint32_t linemaxsize, FILE * fp)
{
    _mqx_int key;
    uint32_t pos = 0;
    uint32_t linelen = 0;
    uint32_t i;
    bool left,right,abort, up, down, first, last;
    bool firstkey = true;
    uint32_t historypos = 0; // not in history (history is from 1 to historysize)
    uint32_t historysize;
    uint32_t historylinelen;

    if (linemaxsize > linebufsize) linemaxsize = linebufsize;

    /* Process previous line still stored in linebuf and add it to history if needed */
    if ((linebuf[0] != '\0') && (linebuf[0] != '#') && (!already_in_history(linebuf, linebufsize))) {
        /* If line is not empty and not a comment (starting with #) and different from previous one, */
        /* add to history by inserting a null at linebuf[0] */
        insert_at_pos(linebuf, linebufsize, '\0', pos);
        terminate_history(linebuf, linebufsize); // make sure last entry in history is still null terminated
    } else {
        /* do not add to history -> clear previous line */
        delete_all(linebuf, linebufsize);
    }

    while (1)
    {
        left = right = abort = up = down = first = last = 0;

        key = fgetc(fp);
        switch(key) {
        case EOF :
            return -1;
        case ESCAPE :
            /* process Escape sequence */
            switch(fgetc(fp)) { // get second character of sequence
            case EOF :
                return -1;
            case '[' :
                switch(fgetc(fp)) { // get third character of sequence
                case EOF :
                    return -1;
                case 'A' : // ARROW_UP key
                    up = 1;
                    key = 0;
                    break;
                case 'B' : // ARROW_DOWN key
                    down = 1;
                    key = 0;
                    break;
                case 'C' : // ARROW_RIGHT key
                    right = 1;
                    key = 0;
                    break;
                case 'D' : // ARROW_LEFT key
                    left = 1;
                    key = 0;
                    break;
                default :  // unknown -> ignore
                    continue;
                }
                break;
                case 'O':
                    switch(fgetc(fp)) { // get third character of sequence
                    case EOF :
                        return -1;
                    case 'H' : // HOME key
                        first = 1;
                        key = 0;
                        break;
                    case 'F' : // END key
                        last = 1;
                        key = 0;
                        break;
                    default :  // unknown -> ignore
                        continue;
                    }
                    break;
                    case ESCAPE: // ESCAPE ESCAPE sequece
                        abort = 1;
                        key = 0;
                        break;
                    default :  // unknown -> ignore
                        continue;
            }
            break;
            case '!' :
                if (firstkey) {
                    up = 1; // emulate an arrow-up key to recall last command
                    key = 0;
                }
                break;
        }

        firstkey = false;

        /* Process ENTER, but also treat \n as ENTER for RTCS telnet server */
        /* (\n is read by fgetc() from server when ENTER is pressed on client) */
        if ((key == ENTER) || (key == '\n')) {
            fputc('\r', fp);
            fputc('\n', fp);
            return 0;
        }
        else if ((key == CTRL_C) || abort) {
            /* edition aborted -> return an empty line */
            for (i=pos; i>0; i--) fputc('\b', fp); // move cursor to first character
            for (i=0; i<linelen; i++) fputc(' ', fp); // overwrite all line with spaces
            for (i=linelen; i>0; i--) fputc('\b', fp); // return to first charactr
            pos = linelen = 0;
            delete_all(linebuf, linebufsize);
            fputc('\r', fp);
            fputc('\n', fp);
            return 1;
        }
        else if ((key == CTRL_N) || down) {
            delete_all(linebuf, linebufsize); // delete line
            historysize = get_history_size(linebuf, linebufsize); // get current history size
            if (historypos > 0)
                historypos--; // recall a newer entry in history than current one
            if (historypos > historysize)
                historypos = historysize; // above last entry -> move to older one
            if (historypos >= 1) {
                /* in history -> get entry */
                get_history_entry(linebuf, linebufsize, historypos);
            } else {
                /* no more in history -> empty line */
                delete_all(linebuf, linebufsize);
            }
            historylinelen = strlen(linebuf); // history entry or a null is now at the beginning of linebuf
            for (i=pos; i>0; i--) fputc('\b', fp); // move cursor to first character
            for (i=0; i<historylinelen; i++) fputc(linebuf[i], fp); // print lin
            for (i=historylinelen; i<linelen; i++) fputc(' ', fp); // erase any extra characters still on line
            for (i=linelen; i>historylinelen; i--) fputc('\b', fp); // move cursor back to end of line
            pos = linelen = historylinelen;
        }
        else if ((key == CTRL_P) || up) {
            delete_all(linebuf, linebufsize);
            historysize = get_history_size(linebuf, linebufsize);
            if (historypos <= historysize)
                historypos++; // recall an older entry in history than current one
            if (historypos > historysize)
                historypos = historysize + 1; // above last entry -> limit historypos
            if (historypos <= historysize) {
                /* in history -> get entry */
                get_history_entry(linebuf, linebufsize, historypos);
            } else {
                /* no more in history -> empty line */
                delete_all(linebuf, linebufsize);
            }
            historylinelen = strlen(linebuf);
            for (i=pos; i>0; i--) fputc('\b', fp);
            for (i=0; i<historylinelen; i++) fputc(linebuf[i], fp);
            for (i=historylinelen; i<linelen; i++) fputc(' ', fp);
            for (i=linelen; i>historylinelen; i--) fputc('\b', fp);
            pos = linelen = historylinelen;
        }
        else if ((key == CTRL_B) || left) {
            /* move cursor left */
            if (pos > 0) {
                fputc('\b', fp);
                pos--;
            }
        }
        else if ((key == CTRL_F) || right) {
            /* move cursor right */
            if (pos < linelen) {
                fputc(linebuf[pos], fp);
                pos++;
            }
        }
        else if ((key == CTRL_A) || first) {
            /* move cursor to beginning of line */
            for (i=pos; i>0; i--) fputc('\b', fp);
            pos = 0;
        }
        else if ((key == CTRL_E) || last) {
            /* move cursor to end of line */
            for (i=pos; i<linelen; i++) fputc(linebuf[i], fp);
            pos = linelen;
        }
        else if ((key == BACKSPACE) || (key == DELETE)) {
            /* delete charcter on the left of cursor */
            if (pos > 0) {
                pos--;
                linelen--;
                delete_at_pos(linebuf, linebufsize, pos);
                fputc('\b', fp);
                for (i=pos; i<linelen; i++) fputc(linebuf[i], fp);
                fputc(' ', fp);
                for (i=linelen; i>pos; i--) fputc('\b', fp);
                fputc('\b', fp);
            }
        }
        else if ((key >= 32) && (key <= 127) && (linelen + 1 < linemaxsize)) {
            /* new character to insert at current position */
            insert_at_pos(linebuf, linebufsize, key, pos);
            terminate_history(linebuf, linebufsize);
            for (i=pos; i<=linelen; i++) fputc(linebuf[i], fp);
            for (i=linelen; i>pos; i--) fputc('\b', fp);
            pos++;
            linelen++;
        }
        /* other characters are ignored */
    }
}

static char *Shell_getline(char *linebuf, uint32_t linebufsize, uint32_t linemaxsize, FILE *fp, int32_t *cmdlen)
{
    _mqx_uint flags, flags_orig;
    int32_t i;
    int32_t rc;

    // disable echo on terminal
    ioctl(fileno(fp), IO_IOCTL_SERIAL_GET_FLAGS, &flags);
    flags_orig = flags;
    flags &= ~(NIO_TTY_FLAGS_ECHO /*| IO_SERIAL_TRANSLATION*/); // keep translation to use \n instead of \r\n in printf()
    ioctl(fileno(fp), IO_IOCTL_SERIAL_SET_FLAGS, &flags);

    // check if first time we are here
    if (*cmdlen == -1) {
        // clear command line buffer before first use by Shell_getline_internal()
        memset(linebuf, 0, linebufsize);
    } else {
        // unsplit previous command that has been split into several argv (\0 instead of spaces)
        for (i=0; i<*cmdlen; i++) if (linebuf[i] == '\0') linebuf[i] = ' ';
    }

    // edit command line
    rc = Shell_getline_internal(linebuf, linebufsize, linemaxsize, fp);

    // remember length of command line before it is split into several argv
    *cmdlen = strlen(linebuf);

    // restore initial terminal settings
    ioctl(fileno(fp), IO_IOCTL_SERIAL_SET_FLAGS, &flags_orig);

    return ((rc < 0) ? NULL : linebuf);
}

static int shell_strtolower(char *arg) {
    char *p = arg;

    if (p) {
        while (*p) {
            *p = tolower((unsigned char)*p);
            p++;
        }
    }

    return 0;
}
/*FUNCTION*-------------------------------------------------------------
 *
 *  Function Name :  Shell
 *  Returned Value:  none
 *  Comments  :  The RTCS shell
 *
 *END*-----------------------------------------------------------------*/

int32_t Shell
(
        const SHELL_COMMAND_STRUCT   shell_commands[],
        char            *start_file
)
{ /* Body */
    SHELL_CONTEXT_PTR    shell_ptr;
    // int32_t               return_code;
    uint32_t              i;
    int32_t cmdlen = -1;
    register KERNEL_DATA_STRUCT_PTR  kernel_data;
    register TD_STRUCT_PTR           active_ptr;
    FILE * this_stdin;
    _GET_KERNEL_DATA(kernel_data);

    shell_ptr = _mem_alloc_zero( sizeof( SHELL_CONTEXT ));
    if (shell_ptr == NULL)  {
        return SHELL_EXIT_ERROR;
    }
    _mem_set_type(shell_ptr, MEM_TYPE_SHELL_CONTEXT);

    active_ptr = kernel_data->ACTIVE_PTR;
    /* only for Shell via "telnetio:" */
    if(active_ptr->STDOUT_STREAM)
    {
        shell_ptr->STDOUT = active_ptr->STDOUT_STREAM;
    }
    else
    {
        shell_ptr->STDOUT = stdout;
    }
    shell_ptr->STDERR = shell_ptr->STDOUT;

    fprintf(shell_ptr->STDOUT, "\nShell (build: %s)\n", __DATE__);
    fprintf(shell_ptr->STDOUT, "Copyright (c) 2013-2014 Freescale Semiconductor;\n");

    shell_ptr->COMMAND_LIST_PTR=(SHELL_COMMAND_PTR)shell_commands;

    if (start_file != NULL)  {
        if (start_file[0] == '\'') {
            strncpy(  shell_ptr->CMD_LINE, &start_file[1],sizeof(shell_ptr->CMD_LINE  ));
        } else {
            shell_ptr->COMMAND_FP = fopen(start_file, "r");
        }
    }

    /* only for Shell via "telnetio:" */
    if(active_ptr->STDIN_STREAM)
    {
        this_stdin = active_ptr->STDIN_STREAM;
        shell_ptr->STDIN = active_ptr->STDIN_STREAM;
    }
    else
    {
        this_stdin = stdin;
        shell_ptr->STDIN = stdin;
    }

    if (!shell_ptr->COMMAND_FP)  {
        shell_ptr->COMMAND_FP = this_stdin;
    }

#if SHELLCFG_USES_MFS
    // initialize current fs ptr, fs name and path
    Shell_set_current_filesystem((void *)shell_ptr, -1);
#endif //SHELLCFG_USES_MFS

    if (shell_ptr->CMD_LINE){
        fprintf(shell_ptr->STDOUT, "shell> %s\n",shell_ptr->CMD_LINE);
    } else {
        fprintf(shell_ptr->STDOUT, "shell> ");
    }
    fflush(shell_ptr->STDOUT);

    while (!shell_ptr->EXIT) {

        if ((!shell_ptr->EXIT) && (shell_ptr->CMD_LINE[0] != '\0'))  {

            if (shell_ptr->COMMAND_FP != this_stdin)  {
                fprintf(shell_ptr->STDOUT, "%s\n", shell_ptr->CMD_LINE);
            }

            if (shell_ptr->CMD_LINE[0] != '#') {
#if 0
                if (strcmp(shell_ptr->CMD_LINE, "!") == 0)  {
                    strncpy(shell_ptr->CMD_LINE,shell_ptr->HISTORY,sizeof(shell_ptr->CMD_LINE));
                } else if (strcmp(shell_ptr->CMD_LINE, "\340H") == 0)  {
                    strncpy(shell_ptr->CMD_LINE,shell_ptr->HISTORY,sizeof(shell_ptr->CMD_LINE));
                } else  {
                    strncpy(shell_ptr->HISTORY,shell_ptr->CMD_LINE,sizeof(shell_ptr->HISTORY));
                }
#endif
                shell_ptr->ARGC = Shell_parse_command_line(shell_ptr->CMD_LINE, shell_ptr->ARGV );

                if (shell_ptr->ARGC > 0) {

                    shell_strtolower(shell_ptr->ARGV[0]);
                    for (i=0;shell_commands[i].COMMAND != NULL;i++)  {
                        if (strcmp(shell_ptr->ARGV[0], shell_commands[i].COMMAND) == 0)  {
                            /* return_code = */ (*shell_commands[i].SHELL_FUNC)(shell_ptr->ARGC, shell_ptr->ARGV);
                            break;
                        }
                    }

                    if (shell_commands[i].COMMAND == NULL)  {
                        fprintf(shell_ptr->STDOUT, "Invalid command.  Type 'help' for a list of commands.\n");
                    }
                }
            }
        }

        if (!shell_ptr->EXIT)
        {
            fprintf(shell_ptr->STDOUT, "shell> ");
            fflush(shell_ptr->STDOUT);

            do
            {
                char *ret = NULL;

                if (start_file != NULL) {
                    ret = fgets(shell_ptr->CMD_LINE, sizeof(shell_ptr->CMD_LINE  ), shell_ptr->COMMAND_FP);
                } else {
                    ret = Shell_getline(shell_ptr->CMD_LINE, sizeof(shell_ptr->CMD_LINE), SHELL_MAX_CMDLEN, shell_ptr->COMMAND_FP, &cmdlen);
                }

                if (!ret)
                {
                    if (shell_ptr->COMMAND_FP != this_stdin) {
                        fclose(shell_ptr->COMMAND_FP);
                        shell_ptr->COMMAND_FP = this_stdin;
                        // shell_ptr->HISTORY[0]=0;
                        shell_ptr->CMD_LINE[0]=0;
                        fprintf(shell_ptr->STDOUT, "\n");
                    } else  {
                        shell_ptr->EXIT = true;
                        break;
                    }
                }
            } while ((shell_ptr->CMD_LINE[0] == '\0') && (shell_ptr->COMMAND_FP != this_stdin)) ;
        }
    }

    fprintf(shell_ptr->STDOUT, "Terminating shell.\n");
    _mem_free(shell_ptr);
    return SHELL_EXIT_SUCCESS;
} /* Endbody */


/* EOF */
