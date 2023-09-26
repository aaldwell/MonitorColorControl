Try
{
    ./clean.ps1
}
Catch
{
    # Catch any error
    Write-Host "Clean step failed."
}

Try
{
    make
}
Catch
{
    Write-Host "Build step failed."
}

Try
{
    python -m http.server -d ./build
}
Catch
{
    Write-Host "failed to run http server"
}