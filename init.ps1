$ErrorActionPreference = 'Stop'

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  throw "cmake not found"
}
if (-not (Get-Command cl -ErrorAction SilentlyContinue) -and -not (Get-Command g++ -ErrorAction SilentlyContinue)) {
  throw "C++ compiler not found"
}

cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure

Write-Host "init.ps1 complete"
