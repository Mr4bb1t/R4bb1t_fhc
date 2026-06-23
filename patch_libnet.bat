@echo off
:: ─────────────────────────────────────────────────────────────────────────────
:: patch_libnet_final.bat — R4BB1T FHC
:: Caminho correto para arduino-esp32 3.x:
::   tools\esp32-libs\<versao>\lib\libnet80211.a
:: ─────────────────────────────────────────────────────────────────────────────
setlocal enabledelayedexpansion

echo === R4BB1T FHC - patch_libnet_final.bat ===
echo.

set ARDUINO15=%LOCALAPPDATA%\Arduino15
if not exist "%ARDUINO15%" set ARDUINO15=%APPDATA%\Arduino15

:: ── Localizar libnet80211.a ──────────────────────────────────────────────────
echo Procurando libnet80211.a...
set LIBPATH=

:: Busca 1: Arduino IDE — esp32-libs
for /f "delims=" %%F in ('dir /s /b "%ARDUINO15%\packages\esp32\tools\esp32-libs\libnet80211.a" 2^>nul') do (
    set LIBPATH=%%F
    goto :found_lib
)

:: Busca 2: Arduino IDE — fallback
for /f "delims=" %%F in ('dir /s /b "%ARDUINO15%\packages\esp32\libnet80211.a" 2^>nul') do (
    set LIBPATH=%%F
    goto :found_lib
)

:: Busca 3: PlatformIO — framework-arduinoespressif32
set PIO=%USERPROFILE%\.platformio\packages\framework-arduinoespressif32
if exist "%PIO%\tools\sdk\esp32\lib\libnet80211.a" (
    set LIBPATH=%PIO%\tools\sdk\esp32\lib\libnet80211.a
    goto :found_lib
)

:: Busca 4: PlatformIO — caminho alternativo
for /f "delims=" %%F in ('dir /s /b "%USERPROFILE%\.platformio\packages\*\tools\sdk\esp32\lib\libnet80211.a" 2^>nul') do (
    set LIBPATH=%%F
    goto :found_lib
)

echo [ERRO] libnet80211.a nao encontrada.
echo Nao encontrei no Arduino IDE nem no PlatformIO.
echo Abrindo Explorer para localizar manualmente...
explorer "%USERPROFILE%\.platformio\packages"
echo.
echo Quando achar, execute:
echo   patch_libnet.bat "C:\caminho\completo\libnet80211.a"
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

:: ── Localizar xtensa-esp-elf-objcopy ─────────────────────────────────────────
echo.
echo Procurando objcopy...
set OBJCOPY=

:: Busca 1: PlatformIO — toolchain-xtensa-esp32
if exist "%USERPROFILE%\.platformio\packages\toolchain-xtensa-esp32\bin\xtensa-esp32-elf-objcopy.exe" (
    set OBJCOPY=%USERPROFILE%\.platformio\packages\toolchain-xtensa-esp32\bin\xtensa-esp32-elf-objcopy.exe
    goto :found_obj
)

:: Busca 2: Arduino IDE — IDF 5.x / arduino-esp32 3.x (esp-x32)
for /f "delims=" %%F in ('dir /s /b "%ARDUINO15%\packages\esp32\tools\esp-x32\xtensa-esp-elf-objcopy.exe" 2^>nul') do (
    set OBJCOPY=%%F
    goto :found_obj
)

:: Busca 3: Arduino IDE — nome alternativo
for /f "delims=" %%F in ('dir /s /b "%ARDUINO15%\packages\esp32\tools\*\bin\xtensa-esp-elf-objcopy.exe" 2^>nul') do (
    set OBJCOPY=%%F
    goto :found_obj
)

:: Busca 4: Arduino IDE — fallback antigo xtensa-esp32-elf
for /f "delims=" %%F in ('dir /s /b "%ARDUINO15%\packages\esp32\tools\xtensa-esp32-elf-gcc\xtensa-esp32-elf-objcopy.exe" 2^>nul') do (
    set OBJCOPY=%%F
    goto :found_obj
)

:: Busca 5: Geral
for /f "delims=" %%F in ('dir /s /b "%ARDUINO15%\packages\esp32\tools\*objcopy.exe" 2^>nul') do (
    set OBJCOPY=%%F
    goto :found_obj
)

:: Busca 6: PlatformIO — qualquer toolchain
for /f "delims=" %%F in ('dir /s /b "%USERPROFILE%\.platformio\packages\toolchain-*\bin\xtensa-esp32-elf-objcopy.exe" 2^>nul') do (
    set OBJCOPY=%%F
    goto :found_obj
)

echo [ERRO] objcopy nao encontrado.
pause
exit /b 1

:found_obj
echo Encontrado: %OBJCOPY%
echo.

:: ── Aplicar weaken-symbol nos dois símbolos ──────────────────────────────────
echo.
echo Aplicando patches...
echo.

:: 1) ieee80211_raw_frame_sanity_check — bypass de management frames
echo [1/2] Weakening: ieee80211_raw_frame_sanity_check
"%OBJCOPY%" --weaken-symbol=ieee80211_raw_frame_sanity_check "%LIBPATH%" "%LIBPATH%"
if %errorlevel% neq 0 (
    echo.
    echo [ERRO] Falha ao enfraquecer ieee80211_raw_frame_sanity_check!
    pause
    exit /b 1
)
echo       OK

:: 2) ieee80211_is_tx_allowed — bypass de verificacao de permissao TX (control frames)
echo [2/2] Weakening: ieee80211_is_tx_allowed
"%OBJCOPY%" --weaken-symbol=ieee80211_is_tx_allowed "%LIBPATH%" "%LIBPATH%"
if %errorlevel% neq 0 (
    echo.
    echo [ERRO] Falha ao enfraquecer ieee80211_is_tx_allowed!
    pause
    exit /b 1
)
echo       OK

echo.
echo ============================================================
echo  SUCESSO! Ambos os patches aplicados.
echo  Lib : %LIBPATH%
echo  Bkp : %BACKUP%
echo.
echo  Símbolos enfraquecidos:
echo    - ieee80211_raw_frame_sanity_check (management frames)
echo    - ieee80211_is_tx_allowed (control frames / TX permission)
echo.
echo  Compile e grave normalmente (Arduino IDE ou PlatformIO).
echo  Para reverter: copy "%BACKUP%" "%LIBPATH%"
echo ============================================================
pause