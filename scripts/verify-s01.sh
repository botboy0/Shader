#!/usr/bin/env bash
# scripts/verify-s01.sh — Final S01 build and verification

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"

PASS=0
FAIL=0

pass() { echo "  ✓ $1"; PASS=$((PASS + 1)); }
fail() { echo "  ✗ $1"; FAIL=$((FAIL + 1)); }

echo "=== S01 Final Verification ==="

# --- Check all expected source files ---
echo ""
echo "[1/5] Checking source files..."
for f in \
    src/main.cpp \
    src/core/app.h \
    src/core/app.cpp \
    src/core/renderer.h \
    src/core/renderer.cpp \
    src/core/frame_control.h \
    src/core/frame_control.cpp \
    src/glad/glad.h \
    src/glad/glad.c \
    CMakeLists.txt
do
    if [ -f "$f" ]; then
        pass "$f exists"
    else
        fail "$f missing"
    fi
done

# --- Configure ---
echo ""
echo "[2/5] CMake configure..."
if cmake -S . -B build -G Ninja > /tmp/cmake_cfg.log 2>&1; then
    pass "CMake configure succeeded"
else
    fail "CMake configure failed"
    cat /tmp/cmake_cfg.log
    echo "RESULT: FAIL ($PASS passed, $FAIL failed)"
    exit 1
fi

# --- Build ---
echo ""
echo "[3/5] CMake build..."
if cmake --build build > /tmp/cmake_build.log 2>&1; then
    pass "Build succeeded"
else
    fail "Build failed"
    cat /tmp/cmake_build.log
    echo "RESULT: FAIL ($PASS passed, $FAIL failed)"
    exit 1
fi

# --- Executable exists ---
echo ""
echo "[4/5] Checking build artifacts..."
if [ -f build/magnetar.exe ]; then
    pass "magnetar.exe exists"
else
    fail "magnetar.exe missing"
fi

# --- Run executable (with timeout since it has a render loop) ---
echo ""
echo "[5/5] Running magnetar.exe (timeout 3s)..."
RUN_OUT=$(timeout 3 ./build/magnetar.exe 2>&1)
RUN_RC=$?

echo "$RUN_OUT"

# timeout returns 124 when it kills the process — that's expected for a render loop app
if [ "$RUN_RC" -eq 0 ] || [ "$RUN_RC" -eq 124 ]; then
    pass "magnetar.exe ran successfully (exit=$RUN_RC)"
else
    fail "magnetar.exe exited $RUN_RC"
fi

if echo "$RUN_OUT" | grep -q "GL Version:"; then
    pass "GL version reported"
else
    fail "GL version not found in output"
fi

if echo "$RUN_OUT" | grep -q "\[Renderer\] Shader compiled successfully"; then
    pass "Shader compiled successfully"
else
    fail "Shader compilation not confirmed"
fi

if echo "$RUN_OUT" | grep -q "\[Renderer\] FBO created"; then
    pass "FBO created"
else
    fail "FBO creation not confirmed"
fi

if echo "$RUN_OUT" | grep -q "\[Renderer\] Fullscreen quad created"; then
    pass "Fullscreen quad created"
else
    fail "Fullscreen quad creation not confirmed"
fi

if echo "$RUN_OUT" | grep -q "\[App\] Entering main loop"; then
    pass "Main loop entered"
else
    fail "Main loop not entered"
fi

if echo "$RUN_OUT" | grep -q "\[FrameControl\] Initialized"; then
    pass "FrameControl initialized"
else
    fail "FrameControl initialization not confirmed"
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
    echo "=== RESULT: PASS ($PASS passed, $FAIL failed) ==="
    exit 0
else
    echo "=== RESULT: FAIL ($PASS passed, $FAIL failed) ==="
    exit 1
fi
