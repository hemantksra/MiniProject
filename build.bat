@echo off
gcc *.c -o editor.exe -lncursesw
if %ERRORLEVEL% equ 0 (
    echo Build successful. Run editor.exe to start.
) else (
    echo Build failed.
)
