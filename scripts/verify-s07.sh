#!/usr/bin/env bash
# Verify script for S07: GLSL Autocomplete + Polish
# Checks autocomplete source, cross-platform CMake, build, runtime.

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

echo "=== S07 Verification: GLSL Autocomplete + Polish ==="
echo ""

# --- Source file checks ---
echo "[1/8] Source files exist"
check "autocomplete.h exists"    test -f src/editor/autocomplete.h
check "autocomplete.cpp exists"  test -f src/editor/autocomplete.cpp

# --- Autocomplete item count ---
echo ""
echo "[2/8] Autocomplete item count >= 170"
ITEM_COUNT=$(grep -c 'items.push_back' src/editor/autocomplete.cpp 2>/dev/null || echo 0)
TOTAL=$((TOTAL + 1))
if [ "$ITEM_COUNT" -ge 170 ]; then
    echo "  PASS: Autocomplete has $ITEM_COUNT items (>= 170)"
    PASS=$((PASS + 1))
else
    echo "  FAIL: Autocomplete has $ITEM_COUNT items (expected >= 170)"
    FAIL=$((FAIL + 1))
fi

# --- CMake cross-platform checks ---
echo ""
echo "[3/8] CMake uses FindOpenGL"
check "find_package(OpenGL) in CMakeLists.txt"  grep -q "find_package(OpenGL" CMakeLists.txt
check "OpenGL::GL in CMakeLists.txt"            grep -q "OpenGL::GL" CMakeLists.txt

# Check no bare opengl32 in target_link_libraries
TOTAL=$((TOTAL + 1))
if grep "target_link_libraries(magnetar" CMakeLists.txt | grep -q "opengl32"; then
    echo "  FAIL: Hardcoded opengl32 still in target_link_libraries"
    FAIL=$((FAIL + 1))
else
    echo "  PASS: No hardcoded opengl32 in target_link_libraries"
    PASS=$((PASS + 1))
fi

# --- Build ---
echo ""
echo "[4/8] CMake configure"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release > /tmp/s07-cmake.log 2>&1
if [ $? -eq 0 ]; then
    echo "  PASS: CMake configure"
    PASS=$((PASS + 1))
else
    echo "  FAIL: CMake configure"
    cat /tmp/s07-cmake.log
    FAIL=$((FAIL + 1))
fi
TOTAL=$((TOTAL + 1))

echo ""
echo "[5/8] Build"
cmake --build build --config Release > /tmp/s07-build.log 2>&1
if [ $? -eq 0 ]; then
    echo "  PASS: Build succeeded"
    PASS=$((PASS + 1))
else
    echo "  FAIL: Build failed"
    tail -40 /tmp/s07-build.log
    FAIL=$((FAIL + 1))
fi
TOTAL=$((TOTAL + 1))

# --- Executable exists ---
echo ""
echo "[6/8] Executable exists"
check "magnetar.exe exists" test -f build/magnetar.exe

# --- Runtime test ---
echo ""
echo "[7/8] Runtime test (timeout 3s)"
timeout 3 ./build/magnetar.exe > /tmp/s07-stdout.log 2>/tmp/s07-runtime.log
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
echo "[8/8] Runtime log pattern checks"
check_output "TabManager initialized"           "\[TabManager\]"        /tmp/s07-runtime.log
check_output "Renderer init"                    "\[Renderer\]"          /tmp/s07-runtime.log
check_output "ShadertoyFormat wrapping log"     "\[ShadertoyFormat\]"   /tmp/s07-runtime.log

echo ""
echo "=== Results: $PASS passed, $FAIL failed, $TOTAL total ==="

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
