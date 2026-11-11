
mkdir dist
@REM copy .\build\Desktop_Qt_6_7_3_MSVC2019_64bit-Release\singboxUser.exe .\dist
copy .\build\Release\caddemo.exe .\dist
@REM copy ..\build\Desktop_Qt_5_15_2_MSVC2019_32bit-Release\EQSoft.exe .\dist

@REM windeployqt.exe ..\vs\Release\EQSoft.exe
@REM windeployqt.exe dist\EQSoft.exe
@REM C:\Qt\6.7.3\msvc2019_64\bin\windeployqt.exe dist\singboxUser.exe
C:\Qt\6.7.3\msvc2019_64\bin\windeployqt.exe dist\caddemo.exe
@REM C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe dist\proxyManager.exe
@REM C:\Qt5\5.15.2\msvc2019_64\bin\windeployqt.exe dist\EQSoft.exe

@echo 获取系统年月日 其中~0,4 表示从第 0 位开始截取 4 个字符
set nowdate=%Date:~0,4%%Date:~5,2%%Date:~8,2%
@REM echo "nowdate: " %nowdate%

rem 提取时分秒
set time=%datetime%
@REM echo Time: %time:~0,2%%time:~3,2%%time:~6,2%
@REM set nowtime=%time:~0,2%%time:~3,2%%time:~6,2%
set hour=%time:~0,2%
set width=2
set "paddedHour=%hour%"
setlocal enabledelayedexpansion
for /l %%i in (1,1,%width%) do (
   if "!paddedHour!" equ "!paddedHour:~0,%%i!" (
       set "paddedHour=0!paddedHour!"
   )
)

echo "paddedHour: " %paddedHour%
set nowtime=%time:~0,2%%time:~3,2%%time:~6,2%

@REM set filename=caddemo_%nowdate%_%nowtime%.zip

@REM echo 打包成tar.gz格式
@REM set filename=caddemo_%nowdate%.tar.gz
@REM echo "filename: "%filename%
@REM tar -czvf %filename%  -C ..\vs\Release\ *

echo 打包成zip格式
set filename=caddemo_%nowdate%.zip
echo "filename: "%filename%
7z a -tzip %filename% .\dist\*

cd ..
@REM  pause