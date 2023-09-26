Try
{
    # Try something that could cause an error
    rm *.o
}
Catch
{
    # Catch any error
    Write-Host "Files not found : *.o"
}

Try
{
    rm ./build/index.js
}
Catch
{
    Write-Host "Files not found : ./build/index.js"
}

Try
{
    rm ./build/index.wasm
}
Catch
{
    Write-Host "Files not found : ./build/index.wasm"
}

Write-Host "Clean complete."