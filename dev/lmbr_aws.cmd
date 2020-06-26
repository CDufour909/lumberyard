@ECHO OFF
REM 
REM All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
REM its licensors.
REM
REM For complete copyright and license terms please see the LICENSE at the root of this
REM distribution (the "License"). All use of this software is governed by the License,
REM or, if provided, by the license below or the license accompanying this file. Do not
REM remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
REM WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
REM
REM Original file Copyright Crytek GMBH or its affiliates, used under license.
REM

SETLOCAL

pushd %~dp0%

REM search for the engine root from the engine.json if possible
IF NOT EXIST engine.json GOTO noSetupConfig

FOR /F "tokens=1,2*" %%A in ('findstr /I /N "ExternalEnginePath" engine.json') do SET ENGINE_ROOT=%%C

REM Clear the trailing comma if any
SET ENGINE_ROOT=%ENGINE_ROOT:,=%

REM Trim the double quotes
SET ENGINE_ROOT=%ENGINE_ROOT:"=%

IF "%ENGINE_ROOT%"=="" GOTO noSetupConfig

IF NOT EXIST "%ENGINE_ROOT%" GOTO noSetupConfig

REM Set the base path to the value
SET BASE_PATH=%ENGINE_ROOT%\
ECHO [WAF] Engine Root: %BASE_PATH%
GOTO engineRootSet

:noSetupConfig
SET BASE_PATH=%~dp0
ECHO [WAF] Engine Root: %BASE_PATH%

:engineRootSet

SET TOOLS_DIR=%BASE_PATH%\Tools

SET PYTHON_DIR=%TOOLS_DIR%\Python
IF EXIST "%PYTHON_DIR%" GOTO PYTHON_DIR_EXISTS

ECHO Could not find Python3 in %TOOLS_DIR%
GOTO :EOF

:PYTHON_DIR_EXISTS

SET PYTHON=%PYTHON_DIR%\python3.cmd
IF EXIST "%PYTHON%" GOTO PYTHON_EXISTS

ECHO Could not find python3.cmd in %PYTHON_DIR%
GOTO :EOF

:PYTHON_EXISTS

SET LMBR_AWS_DIR=%BASE_PATH%\Tools\lmbr_aws

SET PYTHONPATH=%LMBR_AWS_DIR%

call "%PYTHON%" "%LMBR_AWS_DIR%\cli.py" %*

popd

