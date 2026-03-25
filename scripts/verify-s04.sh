#!/usr/bin/env bash
# scripts/verify-s04.sh — S04 slice verification (Shadertoy Compatibility + Uniform System)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"

PASS=0
FAIL=0

pass() { echo "  ✓ $1"; PASS=$((PASS + 1)); }
fail() { echo "  ✗ $1"; FAIL=$((FAIL + 1)); }

echo "=== S04 Verification: Shadertoy Compatibility + Uniform System ==="

# --- [1/5] Check expected source files ---
echo ""
echo "[1/5] Checking source files..."
for f in \
    src/shader/shader_format.h \
    src/shader/shadertoy_format.h \
    src/shader/shadertoy_format.cpp \
    src/shader/uniform_bridge.h \
    src/shader/uniform_bridge.cpp
do
    if [ -f "$f" ]; then
        pass "$f exists"
    else
        fail "$f missing"
    fi
done

# --- [2/5] CMake configure ---
echo ""
echo "[2/5] CMake configure..."
if cmake -S . -B build -G Ninja > /tmp/s04_cmake_cfg.log 2>&1; then
    pass "CMake configure succeeded"
else
    fail "CMake configure failed"
    cat /tmp/s04_cmake_cfg.log
    echo ""
    echo "=== RESULT: FAIL ($PASS passed, $FAIL failed) ==="
    exit 1
fi

# --- [3/5] Build ---
echo ""
echo "[3/5] CMake build..."
if cmake --build build > /tmp/s04_cmake_build.log 2>&1; then
    pass "Build succeeded"
else
    fail "Build failed"
    cat /tmp/s04_cmake_build.log
    echo ""
    echo "=== RESULT: FAIL ($PASS passed, $FAIL failed) ==="
    exit 1
fi

# --- [4/5] Executable exists ---
echo ""
echo "[4/5] Checking build artifacts..."
if [ -f build/magnetar.exe ]; then
    pass "magnetar.exe exists"
else
    fail "magnetar.exe missing"
fi

# --- [5/5] Runtime checks (timeout 3s for render-loop app) ---
echo ""
echo "[5/5] Running magnetar.exe (timeout 3s)..."
RUN_OUT=$(timeout 3 ./build/magnetar.exe 2>&1)
RUN_RC=$?

echo "$RUN_OUT"

# timeout returns 124 when it kills the process — expected for a render loop
if [ "$RUN_RC" -eq 0 ] || [ "$RUN_RC" -eq 124 ]; then
    pass "magnetar.exe ran successfully (exit=$RUN_RC)"
else
    fail "magnetar.exe exited $RUN_RC"
fi

# ShadertoyFormat wrapping log
if echo "$RUN_OUT" | grep -q "\[ShadertoyFormat\]"; then
    pass "[ShadertoyFormat] log pattern found"
else
    fail "[ShadertoyFormat] log pattern not found in output"
fi

# UniformBridge activity log
if echo "$RUN_OUT" | grep -q "\[UniformBridge\]"; then
    pass "[UniformBridge] log pattern found"
else
    fail "[UniformBridge] log pattern not found in output"
fi

# Uniform locations include iMouse (renderer caches all 6 Shadertoy uniforms)
if echo "$RUN_OUT" | grep -q "iMouse"; then
    pass "Uniform locations include iMouse"
else
    fail "iMouse not found in uniform locations log"
fi

# Uniform locations include iDate
if echo "$RUN_OUT" | grep -q "iDate"; then
    pass "Uniform locations include iDate"
else
    fail "iDate not found in uniform locations log"
fi

# Uniform locations include iFrame
if echo "$RUN_OUT" | grep -q "iFrame"; then
    pass "Uniform locations include iFrame"
else
    fail "iFrame not found in uniform locations log"
fi

# Uniform locations include iTimeDelta
if echo "$RUN_OUT" | grep -q "iTimeDelta"; then
    pass "Uniform locations include iTimeDelta"
else
    fail "iTimeDelta not found in uniform locations log"
fi

# Default shader uses mainImage (Shadertoy style, not raw void main())
if grep -q "mainImage" src/shader/shadertoy_format.cpp; then
    pass "Default shader source contains mainImage"
else
    fail "Default shader source does not contain mainImage"
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
    echo "=== RESULT: PASS ($PASS passed, $FAIL failed) ==="
    exit 0
else
    echo "=== RESULT: FAIL ($PASS passed, $FAIL failed) ==="
    exit 1
fi
