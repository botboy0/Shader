#!/usr/bin/env bash
# scripts/verify-s02.sh — S02 slice verification (ImGui + Magnetar theme)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"

PASS=0
FAIL=0

pass() { echo "  ✓ $1"; PASS=$((PASS + 1)); }
fail() { echo "  ✗ $1"; FAIL=$((FAIL + 1)); }

echo "=== S02 Verification: ImGui Integration + Magnetar Theme ==="

# --- [1/5] Check expected source files ---
echo ""
echo "[1/5] Checking source files..."
for f in \
    external/imgui/imgui.h \
    external/imgui/backends/imgui_impl_sdl3.h \
    src/ui/imgui_manager.h \
    src/ui/imgui_manager.cpp \
    src/ui/layout.h \
    src/ui/layout.cpp \
    src/ui/theme.h \
    src/ui/theme.cpp \
    src/ui/fonts/oxproto_regular.h \
    src/ui/fonts/space_mono_regular.h
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
if cmake -S . -B build -G Ninja > /tmp/s02_cmake_cfg.log 2>&1; then
    pass "CMake configure succeeded"
else
    fail "CMake configure failed"
    cat /tmp/s02_cmake_cfg.log
    echo ""
    echo "=== RESULT: FAIL ($PASS passed, $FAIL failed) ==="
    exit 1
fi

# --- [3/5] Build ---
echo ""
echo "[3/5] CMake build..."
if cmake --build build > /tmp/s02_cmake_build.log 2>&1; then
    pass "Build succeeded"
else
    fail "Build failed"
    cat /tmp/s02_cmake_build.log
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

# ImGui initialization
if echo "$RUN_OUT" | grep -q "\[ImGui\] ImGui initialized"; then
    pass "[ImGui] initialization logged"
else
    fail "[ImGui] initialization not found in output"
fi

# Font loading
if echo "$RUN_OUT" | grep -q "\[Theme\] Fonts loaded"; then
    pass "[Theme] Fonts loaded logged"
else
    fail "[Theme] Fonts loaded not found in output"
fi

# Theme application
if echo "$RUN_OUT" | grep -q "\[Theme\] Magnetar theme applied"; then
    pass "[Theme] Magnetar theme applied logged"
else
    fail "[Theme] Magnetar theme applied not found in output"
fi

# DockSpace layout
if echo "$RUN_OUT" | grep -q "\[Layout\]"; then
    pass "[Layout] DockSpace logged"
else
    fail "[Layout] DockSpace not found in output"
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
    echo "=== RESULT: PASS ($PASS passed, $FAIL failed) ==="
    exit 0
else
    echo "=== RESULT: FAIL ($PASS passed, $FAIL failed) ==="
    exit 1
fi
