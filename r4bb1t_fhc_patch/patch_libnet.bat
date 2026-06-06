@echo off
:: ─────────────────────────────────────────────────────────────────────────────
:: patch_libnet_final.bat — R4BB1T FHC
:: Caminho correto para arduino-esp32 3.x:
::   tools\esp32-libs\<versao>\lib\libnet80211.a
:: ─────────────────────────────────────────────────────────────────────────────
setlocal enabledelayedexpansion

set SYMBOL=ieee80211_raw_frame_sanity_check
echo === R4BB1T FHC - patch_libnet_final.bat ===
echo.

set ARDUINO15=%LOCALAPPDATA%\Arduino15
if not exist "%ARDUINO15%" set ARDUINO15=%APPDATA%\Arduino15

:: ── Localizar libnet80211.a em tools\esp32-libs ───────────────────────────────
echo Procurando libnet80211.a em esp32-libs...
set LIBPATH=
for /f "delims=" %%F in ('dir /s /b "%ARDUINO15%\packages\esp32\tools\esp32-libs\libnet80211.a" 2^>nul') do (
    set LIBPATH=%%F
    goto :found_lib
)

:: Fallback: busca em qualquer lugar
for /f "delims=" %%F in ('dir /s /b "%ARDUINO15%\packages\esp32\libnet80211.a" 2^>nul') do (
    set LIBPATH=%%F
    goto :found_lib
)

echo [ERRO] libnet80211.a nao encontrada.
echo Abrindo Explorer para localizar manualmente...
explorer "%ARDUINO15%\packages\esp32\tools"
echo.
echo Quando achar, execute:
echo   patch_libnet_final.bat "C:\caminho\completo\libnet80211.a"
pause
exit /b 1

:found_lib
echo Encontrado: %LIBPATH%

:: ── Argumento manual sobrescreve ──────────────────────────────────────────────
if not "%~1"=="" if exist "%~1" (
    set LIBPATH=%~1
    echo Usando caminho manual: %LIBPATH%
)

:: ── Backup ───────────────────────────────────────────────────────────────────
set BACKUP=%LIBPATH%.orig_backup
if not exist "%BACKUP%" (
    copy "%LIBPATH%" "%BACKUP%" > nul
    echo Backup criado: %BACKUP%
) else (
    echo Backup ja existe, pulando.
)

:: ── Localizar xtensa-esp-elf-objcopy (IDF 5.x usa esp-x32/xtensa-esp-elf) ────
echo.
echo Procurando objcopy...
set OBJCOPY=

:: IDF 5.x / arduino-esp32 3.x: toolchain chama-se esp-x32, binario = xtensa-esp-elf-objcopy
for /f "delims=" %%F in ('dir /s /b "%ARDUINO15%\packages\esp32\tools\esp-x32\xtensa-esp-elf-objcopy.exe" 2^>nul') do (
    set OBJCOPY=%%F
    goto :found_obj
)
:: Nome alternativo sem prefixo esp
for /f "delims=" %%F in ('dir /s /b "%ARDUINO15%\packages\esp32\tools\*\bin\xtensa-esp-elf-objcopy.exe" 2^>nul') do (
    set OBJCOPY=%%F
    goto :found_obj
)
:: Fallback antigo xtensa-esp32-elf
for /f "delims=" %%F in ('dir /s /b "%ARDUINO15%\packages\esp32\tools\xtensa-esp32-elf-gcc\xtensa-esp32-elf-objcopy.exe" 2^>nul') do (
    set OBJCOPY=%%F
    goto :found_obj
)
:: Busca geral por qualquer *objcopy dentro de tools
for /f "delims=" %%F in ('dir /s /b "%ARDUINO15%\packages\esp32\tools\*objcopy.exe" 2^>nul') do (
    set OBJCOPY=%%F
    goto :found_obj
)

echo [ERRO] objcopy nao encontrado.
pause
exit /b 1

:found_obj
echo Encontrado: %OBJCOPY%
echo.

:: ── Aplicar weaken-symbol ─────────────────────────────────────────────────────
echo Aplicando --weaken-symbol=%SYMBOL%...
"%OBJCOPY%" --weaken-symbol=%SYMBOL% "%LIBPATH%" "%LIBPATH%"

if %errorlevel% neq 0 (
    echo.
    echo [ERRO] Falha! Tente executar como Administrador.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo  SUCESSO! Patch aplicado.
echo  Lib : %LIBPATH%
echo  Bkp : %BACKUP%
echo.
echo  Compile e grave normalmente no Arduino IDE.
echo  Para reverter: copy "%BACKUP%" "%LIBPATH%"
echo ============================================================
pause