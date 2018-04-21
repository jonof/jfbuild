# Generator for version-auto.c on Windows

try {
    $gitdescribe = & git.exe describe --always 2>$Null
    if ($LastExitCode -ne 0) {
        $gitdescribe = "git error";
    }
} catch {
    $gitdescribe = "git error"
}

Write-Host ("const char *build_version = ""{0}"";" -f $gitdescribe)
Write-Host "const char *build_date = __DATE__;"
Write-Host "const char *build_time = __TIME__;"
