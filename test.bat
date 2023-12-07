@echo off
setlocal

set local=%~dp0
echo �ش������� Lyra�� Root������ �־��ּ���
echo ���� ��ġ�� ���� %~dp0
echo ���� ��θ� ��Ȯ�� �����ּ���.
echo.
set /p path=������ ���� ��ġ�� �Է��ϼ���:


if not exist %path% (mkdir %path% )

echo %path%
pushd %path%

:REDO
set /p str=������ ���� �̸��� �Է��ϼ���:
IF "%str%" == "" goto REDO

:CREDO
set /p uclass=Class�� ��Ÿ�����ʹ� �����Դϱ�?:(ex_ UCLASS/USTRUCT)
IF "%uclass%"=="" goto CREDO


:PREDO
set /p bparent=����� �θ� �ֽ��ϱ�?(Yes:1/No:2):
IF "%bparent%" == "" goto PREDO

set /a bpar=%bparent%

IF %bpar%==2 goto NOPARENT

set /p parent=����� �θ� �Է����ּ���:
set fullparent=^:public %parent%
set parentclass=%parent:~0,1%



:PARENT
set FObject=FObjectInitializer
set Initializer=ObejctInitializer
set Get=^:^:Get()



echo #pragma once>>%str%.h
echo #include "%str%.generated.h">>%str%.h
echo.>>%str%.h
echo %uclass%^(^)>>%str%.h
echo class %parentclass%%str%%fullparent%>>%str%.h
echo {>>%str%.h
echo    GENERATED_BODY()>>%str%.h
echo public:>>%str%.h
echo    %parentclass%%str%^(const %FObject%^&%Initializer%^=%FObject%%Get%)^;>>%str%.h
echo }^;>>%str%.h



echo #include "%str%.h">>%str%.cpp
echo #include UE_INLINE_GENERATED_CPP_BY_NAME(%str%)>>%str%.cpp

set /p generated=Generated ProjectFile�� �����Ͻðڽ��ϱ�?(Yes=1/No=2)
set /a answer=%generated%
IF %answer%==2 (goto REDO)

pushd %local%



endlocal





if not exist "%~dp0Engine\Build\BatchFiles\GenerateProjectFiles.bat" goto Error_BatchFileInWrongLocation
call "%~dp0Engine\Build\BatchFiles\GenerateProjectFiles.bat" %* pause
exit /B %ERRORLEVEL%

:Error_BatchFileInWrongLocation
echo GenerateProjectFiles ERROR: The batch file does not appear to be located in the root UE directory.  This script must be run from within that directory.
pause
exit /B 1



:NOPARENT

set /p parentclass= Ŭ������ �������ּ���(U/F/A):
set fullparent=
goto PARENT