# test_proxy.ps1
# PowerShell test script for tiny-https-proxy

$port = 10800
$hostAddress = "127.0.0.1"
$proxyPath = ".\build\proxy.exe"

if (-not (Test-Path $proxyPath)) {
    Write-Host "Error: Proxy executable not found at $proxyPath. Please build it first." -ForegroundColor Red
    exit 1
}

Write-Host "Starting proxy server on $($hostAddress):$port..."
$proc = Start-Process -FilePath $proxyPath -ArgumentList "--listen", $hostAddress, $port -NoNewWindow -PassThru

$success = $false
try {
    # Give it a second to start listening
    Start-Sleep -Seconds 1

    Write-Host "Testing HTTP request through proxy..."
    $httpCode = curl.exe -s -o NUL -w "%{http_code}" -x "http://$($hostAddress):$port" "http://example.com"
    if ($httpCode -eq "200") {
        Write-Host "HTTP Test: SUCCESS (Status Code: $httpCode)" -ForegroundColor Green
    } else {
        Write-Host "HTTP Test: FAILED (Status Code: $httpCode)" -ForegroundColor Red
        return
    }

    Write-Host "Testing HTTPS request (CONNECT tunnel) through proxy..."
    $httpsCode = curl.exe -s -o NUL -w "%{http_code}" -x "http://$($hostAddress):$port" "https://example.com"
    if ($httpsCode -eq "200") {
        Write-Host "HTTPS Test: SUCCESS (Status Code: $httpsCode)" -ForegroundColor Green
        $success = $true
    } else {
        Write-Host "HTTPS Test: FAILED (Status Code: $httpsCode)" -ForegroundColor Red
    }
}
finally {
    Write-Host "Stopping proxy server..."
    if ($proc -and -not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
}

if ($success) {
    Write-Host "All tests passed successfully!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "Some proxy tests failed." -ForegroundColor Red
    exit 1
}
