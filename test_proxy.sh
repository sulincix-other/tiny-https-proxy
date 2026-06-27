#!/bin/bash
# test_proxy.sh
# Bash test script for tiny-https-proxy

PORT=10800
HOST_ADDRESS="127.0.0.1"

if [ -f "./build/proxy.exe" ]; then
    PROXY_PATH="./build/proxy.exe"
elif [ -f "./build/proxy" ]; then
    PROXY_PATH="./build/proxy"
else
    echo "Error: Proxy executable not found. Please build it first." >&2
    exit 1
fi

echo "Starting proxy server on $HOST_ADDRESS:$PORT..."
$PROXY_PATH --listen "$HOST_ADDRESS" "$PORT" &
PROXY_PID=$!

cleanup() {
    echo "Stopping proxy server..."
    kill $PROXY_PID 2>/dev/null
    wait $PROXY_PID 2>/dev/null
}
trap cleanup EXIT

# Give it a second to start listening
sleep 1

SUCCESS=true

echo "Testing HTTP request through proxy..."
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -x "http://$HOST_ADDRESS:$PORT" "http://example.com")
if [ "$HTTP_CODE" = "200" ]; then
    echo -e "\033[0;32mHTTP Test: SUCCESS (Status Code: $HTTP_CODE)\033[0m"
else
    echo -e "\033[0;31mHTTP Test: FAILED (Status Code: $HTTP_CODE)\033[0m"
    SUCCESS=false
fi

echo "Testing HTTPS request (CONNECT tunnel) through proxy..."
HTTPS_CODE=$(curl -s -o /dev/null -w "%{http_code}" -x "http://$HOST_ADDRESS:$PORT" "https://example.com")
if [ "$HTTPS_CODE" = "200" ]; then
    echo -e "\033[0;32mHTTPS Test: SUCCESS (Status Code: $HTTPS_CODE)\033[0m"
else
    echo -e "\033[0;31mHTTPS Test: FAILED (Status Code: $HTTPS_CODE)\033[0m"
    SUCCESS=false
fi

if [ "$SUCCESS" = true ]; then
    echo -e "\033[0;32mAll tests passed successfully!\033[0m"
    exit 0
else
    echo -e "\033[0;31mSome proxy tests failed.\033[0m" >&2
    exit 1
fi
