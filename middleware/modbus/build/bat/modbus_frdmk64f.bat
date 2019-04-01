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

:label_kds
IF NOT EXIST "%OUTPUTDIR%" mkdir "%OUTPUTDIR%"
IF NOT EXIST "%OUTPUTDIR%\." mkdir "%OUTPUTDIR%\."
copy "%ROOTDIR%\source\include\modbus.h" "%OUTPUTDIR%\modbus.h" /Y
copy "%ROOTDIR%\source\include\modbus-private.h" "%OUTPUTDIR%\modbus-private.h" /Y
copy "%ROOTDIR%\source\include\modbus-rtu.h" "%OUTPUTDIR%\modbus-rtu.h" /Y
copy "%ROOTDIR%\source\include\modbus-rtu-private.h" "%OUTPUTDIR%\modbus-rtu-private.h" /Y
copy "%ROOTDIR%\source\include\modbus-tcp.h" "%OUTPUTDIR%\modbus-tcp.h" /Y
copy "%ROOTDIR%\source\include\modbus-tcp-private.h" "%OUTPUTDIR%\modbus-tcp-private.h" /Y
copy "%ROOTDIR%\source\include\modbus-version.h" "%OUTPUTDIR%\modbus-version.h" /Y
goto end_script


:end_script

