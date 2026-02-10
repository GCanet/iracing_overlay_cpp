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
    echo Descarga CMake de: https://cmake.org/download/  ^(recomendado version 3.28+ o superior^)
    pause
    exit /b 1
)

REM Crear directorio build si no existe
if not exist build mkdir build
cd build

REM Intentar configuracion automatica (mejor opcion) o fallbacks
echo Configurando proyecto...

cmake .. -A x64
if %ERRORLEVEL% EQU 0 goto BUILD_SUCCESS

echo Auto-detect fallo. Intentando VS 2026...
cmake .. -G "Visual Studio 18 2026" -A x64
if %ERRORLEVEL% EQU 0 goto BUILD_SUCCESS

echo Intentando VS 2022...
cmake .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% EQU 0 goto BUILD_SUCCESS

echo Intentando VS 2019 ^(viejo^)...
cmake .. -G "Visual Studio 16 2019" -A x64
if %ERRORLEVEL% EQU 0 goto BUILD_SUCCESS

echo.
echo ERROR: No se pudo detectar ninguna version de Visual Studio compatible.
echo Asegurese de tener instalado:
echo   - Visual Studio 2022 o 2026 con la carga de trabajo "Desarrollo de escritorio con C++"
echo   - Componente "Herramientas de CMake para Windows"
echo Abra Visual Studio Installer -> Modificar -> Verifique cargas de trabajo y componentes.
pause
exit /b 1

:BUILD_SUCCESS
echo.
echo Configuracion exitosa. Compilando...
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
echo Ejecutable en: %CD%\bin\Release\iRacingOverlay.exe
echo.

cd ..
pause
