@echo off

rem convert path to backslash format
set ROOTDIR=%1
set ROOTDIR=%ROOTDIR:/=\%
set ROOTDIR=%ROOTDIR:"=%
set OUTPUTDIR=%2
set OUTPUTDIR=%OUTPUTDIR:/=\%
set OUTPUTDIR=%OUTPUTDIR:"=%
set TOOL=%3

rem process one of label bellow
goto label_%TOOL%

:label_armgcc
IF NOT EXIST "%OUTPUTDIR%" mkdir "%OUTPUTDIR%"
IF NOT EXIST "%OUTPUTDIR%\." mkdir "%OUTPUTDIR%\."
copy "%ROOTDIR%\source\include\mfs_cnfg.h" "%OUTPUTDIR%\mfs_cnfg.h" /Y
copy "%ROOTDIR%\source\include\mfs_rev.h" "%OUTPUTDIR%\mfs_rev.h" /Y
copy "%ROOTDIR%\source\include\sh_mfs.h" "%OUTPUTDIR%\sh_mfs.h" /Y
copy "%ROOTDIR%\source\include\mfs.h" "%OUTPUTDIR%\mfs.h" /Y
copy "%ROOTDIR%\source\include\part_mgr.h" "%OUTPUTDIR%\part_mgr.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_esdhc\esdhc.h" "%OUTPUTDIR%\esdhc.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_sdcard\sdcard_esdhc\sdcard_esdhc.h" "%OUTPUTDIR%\sdcard_esdhc.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_sdcard\sdcard.h" "%OUTPUTDIR%\sdcard.h" /Y
copy "%ROOTDIR%\source\mfs_usb\mfs_usb.h" "%OUTPUTDIR%\mfs_usb.h" /Y
copy "%ROOTDIR%\..\..\..\rtos\mqx\nshell\source\include\sh_mfs.h" "%OUTPUTDIR%\sh_mfs.h" /Y
goto end_script


:label_iar
IF NOT EXIST "%OUTPUTDIR%" mkdir "%OUTPUTDIR%"
IF NOT EXIST "%OUTPUTDIR%\." mkdir "%OUTPUTDIR%\."
copy "%ROOTDIR%\source\include\mfs_cnfg.h" "%OUTPUTDIR%\mfs_cnfg.h" /Y
copy "%ROOTDIR%\source\include\mfs_rev.h" "%OUTPUTDIR%\mfs_rev.h" /Y
copy "%ROOTDIR%\source\include\sh_mfs.h" "%OUTPUTDIR%\sh_mfs.h" /Y
copy "%ROOTDIR%\source\include\mfs.h" "%OUTPUTDIR%\mfs.h" /Y
copy "%ROOTDIR%\source\include\part_mgr.h" "%OUTPUTDIR%\part_mgr.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_esdhc\esdhc.h" "%OUTPUTDIR%\esdhc.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_sdcard\sdcard_esdhc\sdcard_esdhc.h" "%OUTPUTDIR%\sdcard_esdhc.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_sdcard\sdcard.h" "%OUTPUTDIR%\sdcard.h" /Y
copy "%ROOTDIR%\source\mfs_usb\mfs_usb.h" "%OUTPUTDIR%\mfs_usb.h" /Y
copy "%ROOTDIR%\..\..\..\rtos\mqx\nshell\source\include\sh_mfs.h" "%OUTPUTDIR%\sh_mfs.h" /Y
goto end_script


:label_kds
IF NOT EXIST "%OUTPUTDIR%" mkdir "%OUTPUTDIR%"
IF NOT EXIST "%OUTPUTDIR%\." mkdir "%OUTPUTDIR%\."
copy "%ROOTDIR%\source\include\mfs_cnfg.h" "%OUTPUTDIR%\mfs_cnfg.h" /Y
copy "%ROOTDIR%\source\include\mfs_rev.h" "%OUTPUTDIR%\mfs_rev.h" /Y
copy "%ROOTDIR%\source\include\sh_mfs.h" "%OUTPUTDIR%\sh_mfs.h" /Y
copy "%ROOTDIR%\source\include\mfs.h" "%OUTPUTDIR%\mfs.h" /Y
copy "%ROOTDIR%\source\include\part_mgr.h" "%OUTPUTDIR%\part_mgr.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_esdhc\esdhc.h" "%OUTPUTDIR%\esdhc.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_sdcard\sdcard_esdhc\sdcard_esdhc.h" "%OUTPUTDIR%\sdcard_esdhc.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_sdcard\sdcard.h" "%OUTPUTDIR%\sdcard.h" /Y
copy "%ROOTDIR%\source\mfs_usb\mfs_usb.h" "%OUTPUTDIR%\mfs_usb.h" /Y
copy "%ROOTDIR%\..\..\..\rtos\mqx\nshell\source\include\sh_mfs.h" "%OUTPUTDIR%\sh_mfs.h" /Y
goto end_script


:label_atl
IF NOT EXIST "%OUTPUTDIR%" mkdir "%OUTPUTDIR%"
IF NOT EXIST "%OUTPUTDIR%\." mkdir "%OUTPUTDIR%\."
copy "%ROOTDIR%\source\include\mfs_cnfg.h" "%OUTPUTDIR%\mfs_cnfg.h" /Y
copy "%ROOTDIR%\source\include\mfs_rev.h" "%OUTPUTDIR%\mfs_rev.h" /Y
copy "%ROOTDIR%\source\include\sh_mfs.h" "%OUTPUTDIR%\sh_mfs.h" /Y
copy "%ROOTDIR%\source\include\mfs.h" "%OUTPUTDIR%\mfs.h" /Y
copy "%ROOTDIR%\source\include\part_mgr.h" "%OUTPUTDIR%\part_mgr.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_esdhc\esdhc.h" "%OUTPUTDIR%\esdhc.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_sdcard\sdcard_esdhc\sdcard_esdhc.h" "%OUTPUTDIR%\sdcard_esdhc.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_sdcard\sdcard.h" "%OUTPUTDIR%\sdcard.h" /Y
copy "%ROOTDIR%\source\mfs_usb\mfs_usb.h" "%OUTPUTDIR%\mfs_usb.h" /Y
copy "%ROOTDIR%\..\..\..\rtos\mqx\nshell\source\include\sh_mfs.h" "%OUTPUTDIR%\sh_mfs.h" /Y
goto end_script


:label_mdk
IF NOT EXIST "%OUTPUTDIR%" mkdir "%OUTPUTDIR%"
IF NOT EXIST "%OUTPUTDIR%\." mkdir "%OUTPUTDIR%\."
copy "%ROOTDIR%\source\include\mfs_cnfg.h" "%OUTPUTDIR%\mfs_cnfg.h" /Y
copy "%ROOTDIR%\source\include\mfs_rev.h" "%OUTPUTDIR%\mfs_rev.h" /Y
copy "%ROOTDIR%\source\include\sh_mfs.h" "%OUTPUTDIR%\sh_mfs.h" /Y
copy "%ROOTDIR%\source\include\mfs.h" "%OUTPUTDIR%\mfs.h" /Y
copy "%ROOTDIR%\source\include\part_mgr.h" "%OUTPUTDIR%\part_mgr.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_esdhc\esdhc.h" "%OUTPUTDIR%\esdhc.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_sdcard\sdcard_esdhc\sdcard_esdhc.h" "%OUTPUTDIR%\sdcard_esdhc.h" /Y
copy "%ROOTDIR%\source\mfs_sdcard\nio_sdcard\sdcard.h" "%OUTPUTDIR%\sdcard.h" /Y
copy "%ROOTDIR%\source\mfs_usb\mfs_usb.h" "%OUTPUTDIR%\mfs_usb.h" /Y
copy "%ROOTDIR%\..\..\..\rtos\mqx\nshell\source\include\sh_mfs.h" "%OUTPUTDIR%\sh_mfs.h" /Y
goto end_script



:end_script

