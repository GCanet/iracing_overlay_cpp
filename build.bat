@echo off
REM iRacing Overlay - Build Script
REM Compila el proyecto con CMake

echo ========================================
echo   iRacing Overlay - Build Script
echo ========================================
echo.

REM Verificar si existe CMake
where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake no encontrado
    echo Descarga CMake de: https://cmake.org/download/
    pause
    exit /b 1
)

REM Crear directorio build
if not exist build mkdir build
cd build

REM Configurar con CMake
echo Configurando proyecto...
cmake .. -G "Visual Studio 16 2019" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Configuracion fallida
    pause
    exit /b 1
)

REM Compilar
echo.
echo Compilando...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Compilacion fallida
    pause
    exit /b 1
)

echo.
echo ========================================
echo   Compilacion exitosa!
echo ========================================
echo.
echo Ejecutable en: build\bin\Release\iRacingOverlay.exe
echo.

cd ..
pause
