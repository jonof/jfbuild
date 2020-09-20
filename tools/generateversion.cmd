@ECHO OFF
REM Generator for version-auto.c on Windows

git.exe --version >NUL 2>&1
IF %ERRORLEVEL% EQU 0 (
    FOR /F %%G IN ('git.exe describe --always') DO ECHO const char *build_version = "%%G";
) ELSE (
    ECHO const char *build_version = "git error";
)
ECHO const char *build_date = __DATE__;
ECHO const char *build_time = __TIME__;

EXIT /B 0
