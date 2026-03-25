#!/usr/bin/env bash
# Verify script for S06: Multipass Rendering
# Checks source files, build, runtime, and backward compatibility.

export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"

PASS=0
FAIL=0
TOTAL=0

check() {
    TOTAL=$((TOTAL + 1))
    local desc="$1"
    shift
    if "$@" > /dev/null 2>&1; then
        echo "  PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc"
        FAIL=$((FAIL + 1))
    fi
}

check_output() {
    TOTAL=$((TOTAL + 1))
    local desc="$1"
    local pattern="$2"
    local file="$3"
    if grep -q "$pattern" "$file" 2>/dev/null; then
        echo "  PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc (pattern '$pattern' not found)"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== S06 Verification: Multipass Rendering ==="
echo ""

# --- Source file checks ---
echo "[1/7] Source files exist"
check "multipass_pipeline.h exists"    test -f src/shader/multipass_pipeline.h
check "multipass_pipeline.cpp exists"  test -f src/shader/multipass_pipeline.cpp
check "multipass_parser.h exists"      test -f src/shader/multipass_parser.h
check "multipass_parser.cpp exists"    test -f src/shader/multipass_parser.cpp

# --- Content checks ---
echo ""
echo "[2/7] Source content checks"
check "iChannel in shadertoy_format.cpp"  grep -q "iChannel" src/shader/shadertoy_format.cpp
check "MultipassPipeline in tab_manager.h" grep -q "MultipassPipeline" src/ui/tab_manager.h
check "Feedback Blur in template_picker.cpp" grep -q "Feedback Blur" src/ui/template_picker.cpp
check "Magnetar Logo in template_picker.cpp" grep -q "Magnetar Logo" src/ui/template_picker.cpp
check "applyToProgram in uniform_bridge.h" grep -q "applyToProgram" src/shader/uniform_bridge.h
check "multipass_pipeline.h included in main.cpp" grep -q "multipass_pipeline.h" src/main.cpp

# --- Build ---
echo ""
echo "[3/7] CMake configure"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release > /tmp/s06-cmake.log 2>&1
if [ $? -eq 0 ]; then
    echo "  PASS: CMake configure"
    PASS=$((PASS + 1))
else
    echo "  FAIL: CMake configure"
    cat /tmp/s06-cmake.log
    FAIL=$((FAIL + 1))
fi
TOTAL=$((TOTAL + 1))

echo ""
echo "[4/7] Build"
cmake --build build --config Release > /tmp/s06-build.log 2>&1
if [ $? -eq 0 ]; then
    echo "  PASS: Build succeeded"
    PASS=$((PASS + 1))
else
    echo "  FAIL: Build failed"
    tail -40 /tmp/s06-build.log
    FAIL=$((FAIL + 1))
fi
TOTAL=$((TOTAL + 1))

# --- Executable exists ---
echo ""
echo "[5/7] Executable exists"
check "magnetar.exe exists" test -f build/magnetar.exe

# --- Runtime test ---
echo ""
echo "[6/7] Runtime test (timeout 3s)"
timeout 3 ./build/magnetar.exe > /tmp/s06-stdout.log 2>/tmp/s06-runtime.log
EXIT_CODE=$?
TOTAL=$((TOTAL + 1))
if [ $EXIT_CODE -eq 124 ]; then
    echo "  PASS: magnetar.exe ran and was killed by timeout (exit 124)"
    PASS=$((PASS + 1))
else
    echo "  FAIL: magnetar.exe exited with code $EXIT_CODE (expected 124)"
    FAIL=$((FAIL + 1))
fi

# --- Runtime log checks ---
echo ""
echo "[7/7] Runtime log pattern checks"
check_output "ShadertoyFormat wrapping log"     "\[ShadertoyFormat\] Wrapping source" /tmp/s06-runtime.log
check_output "Header line count is 14"          "14 header lines"                     /tmp/s06-runtime.log
check_output "TabManager initialized"           "\[TabManager\] Initialized"          /tmp/s06-runtime.log
check_output "Renderer init"                    "\[Renderer\]"                        /tmp/s06-runtime.log
check_output "UniformBridge applied"            "\[UniformBridge\]"                   /tmp/s06-runtime.log

echo ""
echo "=== Results: $PASS passed, $FAIL failed, $TOTAL total ==="

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
