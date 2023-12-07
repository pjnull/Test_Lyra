@echo off
setlocal

set local=%~dp0
echo 해당파일은 Lyra의 Root폴더에 넣어주세요
echo 현재 위치한 파일 %~dp0
echo 생성 경로를 정확히 적어주세요.
echo.
set /p path=생성할 파일 위치를 입력하세요:


if not exist %path% (mkdir %path% )

echo %path%
pushd %path%

:REDO
set /p str=생성할 파일 이름을 입력하세요:
IF "%str%" == "" goto REDO

:CREDO
set /p uclass=Class의 메타데이터는 무엇입니까?:(ex_ UCLASS/USTRUCT)
IF "%uclass%"=="" goto CREDO


:PREDO
set /p bparent=상속할 부모가 있습니까?(Yes:1/No:2):
IF "%bparent%" == "" goto PREDO

set /a bpar=%bparent%

IF %bpar%==2 goto NOPARENT

set /p parent=상속할 부모를 입력해주세요:
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

set /p generated=Generated ProjectFile을 실행하시겠습니까?(Yes=1/No=2)
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

set /p parentclass= 클래스를 지정해주세요(U/F/A):
set fullparent=
goto PARENT