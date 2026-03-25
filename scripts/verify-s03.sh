#!/usr/bin/env bash
# scripts/verify-s03.sh — S03 slice verification (Code Editor + Shader Compilation)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"

PASS=0
FAIL=0

pass() { echo "  ✓ $1"; PASS=$((PASS + 1)); }
fail() { echo "  ✗ $1"; FAIL=$((FAIL + 1)); }

echo "=== S03 Verification: Code Editor + Shader Compilation ==="

# --- [1/5] Check expected source files ---
echo ""
echo "[1/5] Checking source files..."
for f in \
    external/ImGuiColorTextEdit/TextEditor.h \
    external/ImGuiColorTextEdit/TextEditor.cpp \
    src/editor/code_editor.h \
    src/editor/code_editor.cpp \
    src/editor/compiler.h \
    src/editor/compiler.cpp
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
if cmake -S . -B build -G Ninja > /tmp/s03_cmake_cfg.log 2>&1; then
    pass "CMake configure succeeded"
else
    fail "CMake configure failed"
    cat /tmp/s03_cmake_cfg.log
    echo ""
    echo "=== RESULT: FAIL ($PASS passed, $FAIL failed) ==="
    exit 1
fi

# --- [3/5] Build ---
echo ""
echo "[3/5] CMake build..."
if cmake --build build > /tmp/s03_cmake_build.log 2>&1; then
    pass "Build succeeded"
else
    fail "Build failed"
    cat /tmp/s03_cmake_build.log
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

# CodeEditor initialization
if echo "$RUN_OUT" | grep -q "\[CodeEditor\]"; then
    pass "[CodeEditor] log pattern found"
else
    fail "[CodeEditor] log pattern not found in output"
fi

# Compiler activity (success or failure)
if echo "$RUN_OUT" | grep -q "\[Compiler\]"; then
    pass "[Compiler] log pattern found"
else
    fail "[Compiler] log pattern not found in output"
fi

# Renderer program swap (initial compile triggers setProgram)
if echo "$RUN_OUT" | grep -q "\[Renderer\] Program swapped"; then
    pass "[Renderer] Program swapped log found"
else
    fail "[Renderer] Program swapped not found in output"
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
    echo "=== RESULT: PASS ($PASS passed, $FAIL failed) ==="
    exit 0
else
    echo "=== RESULT: FAIL ($PASS passed, $FAIL failed) ==="
    exit 1
fi
