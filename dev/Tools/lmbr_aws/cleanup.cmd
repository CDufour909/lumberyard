@ECHO OFF 

REM $Revision: #1 $

SETLOCAL
SET CMD_DIR=%~dp0
SET CMD_DIR=%CMD_DIR:~0,-1%

SET ROOT_DIR=%CMD_DIR%\..\..

SET PYTHON_DIR=%ROOT_DIR%\Tools\Python
IF EXIST "%PYTHON_DIR%" GOTO PYTHON_READY
ECHO Missing: %PYTHON_DIR%
GOTO :EOF
:PYTHON_READY
SET PYTHON=%PYTHON_DIR%\python3.cmd

SET PYTHONPATH=
"%PYTHON%" "%CMD_DIR%"\cleanup.py %*

