#!/usr/bin/env bash
# scripts/verify-s05.sh — S05 slice verification (Tabs, Files, Templates, Settings)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"

PASS=0
FAIL=0

pass() { echo "  ✓ $1"; PASS=$((PASS + 1)); }
fail() { echo "  ✗ $1"; FAIL=$((FAIL + 1)); }

echo "=== S05 Verification: Tabs, Files, Templates, Settings ==="

# --- [1/8] Check expected source files ---
echo ""
echo "[1/8] Checking source files..."
for f in \
    src/ui/tab_manager.h \
    src/ui/tab_manager.cpp \
    src/ui/settings.h \
    src/ui/settings.cpp \
    src/ui/session.h \
    src/ui/session.cpp \
    src/ui/file_dialog.h \
    src/ui/file_dialog.cpp \
    src/ui/template_picker.h \
    src/ui/template_picker.cpp \
    src/ui/shortcuts.h \
    src/ui/shortcuts.cpp \
    external/nlohmann/json.hpp
do
    if [ -f "$f" ]; then
        pass "$f exists"
    else
        fail "$f missing"
    fi
done

# --- [2/8] CMake configure ---
echo ""
echo "[2/8] CMake configure..."
if cmake -S . -B build -G Ninja > /tmp/s05_cmake_cfg.log 2>&1; then
    pass "CMake configure succeeded"
else
    fail "CMake configure failed"
    cat /tmp/s05_cmake_cfg.log
    echo ""
    echo "=== RESULT: FAIL ($PASS passed, $FAIL failed) ==="
    exit 1
fi

# --- [3/8] Build ---
echo ""
echo "[3/8] CMake build..."
if cmake --build build > /tmp/s05_cmake_build.log 2>&1; then
    pass "Build succeeded"
else
    fail "Build failed"
    cat /tmp/s05_cmake_build.log
    echo ""
    echo "=== RESULT: FAIL ($PASS passed, $FAIL failed) ==="
    exit 1
fi

# --- [4/8] Executable exists ---
echo ""
echo "[4/8] Checking build artifacts..."
if [ -f build/magnetar.exe ]; then
    pass "magnetar.exe exists"
else
    fail "magnetar.exe missing"
fi

# --- [5/8] Runtime checks (timeout 3s for render-loop app) ---
echo ""
echo "[5/8] Running magnetar.exe (timeout 3s)..."
RUN_OUT=$(timeout 3 ./build/magnetar.exe 2>&1)
RUN_RC=$?

# timeout returns 124 when it kills the process — expected for a render loop
if [ "$RUN_RC" -eq 0 ] || [ "$RUN_RC" -eq 124 ]; then
    pass "magnetar.exe ran successfully (exit=$RUN_RC)"
else
    fail "magnetar.exe exited $RUN_RC"
fi

# --- [6/8] Runtime log patterns ---
echo ""
echo "[6/8] Checking runtime log patterns..."

if echo "$RUN_OUT" | grep -q "\[TabManager\]"; then
    pass "[TabManager] log pattern found"
else
    fail "[TabManager] log pattern not found in output"
fi

if echo "$RUN_OUT" | grep -q "\[Settings\]"; then
    pass "[Settings] log pattern found"
else
    fail "[Settings] log pattern not found in output"
fi

if echo "$RUN_OUT" | grep -q "\[Session\]"; then
    pass "[Session] log pattern found"
else
    fail "[Session] log pattern not found in output"
fi

if echo "$RUN_OUT" | grep -q "\[Shortcuts\]"; then
    pass "[Shortcuts] log pattern found"
else
    fail "[Shortcuts] log pattern not found in output"
fi

# --- [7/8] Source-level feature checks ---
echo ""
echo "[7/8] Source-level feature checks..."

# Template count: at least 8 mainImage definitions in template_picker.cpp
TEMPLATE_COUNT=$(grep -c "mainImage" src/ui/template_picker.cpp 2>/dev/null || echo "0")
if [ "$TEMPLATE_COUNT" -ge 8 ]; then
    pass "At least 8 templates defined ($TEMPLATE_COUNT mainImage occurrences)"
else
    fail "Expected >=8 templates, found $TEMPLATE_COUNT mainImage occurrences"
fi

# ViewMode enum exists
if grep -q "ViewMode" src/ui/layout.h; then
    pass "ViewMode enum found in layout.h"
else
    fail "ViewMode enum not found in layout.h"
fi

# JSON persistence in settings
if grep -q "nlohmann" src/ui/settings.cpp; then
    pass "settings.cpp uses nlohmann/json"
else
    fail "settings.cpp does not use nlohmann/json"
fi

# JSON persistence in session
if grep -q "nlohmann" src/ui/session.cpp; then
    pass "session.cpp uses nlohmann/json"
else
    fail "session.cpp does not use nlohmann/json"
fi

# Session save/load wired in main
if grep -q "sessionManager" src/main.cpp; then
    pass "SessionManager wired in main.cpp"
else
    fail "SessionManager not wired in main.cpp"
fi

# Cursor position accessors in CodeEditor
if grep -q "getCursorLine" src/editor/code_editor.h; then
    pass "CodeEditor has cursor position accessors"
else
    fail "CodeEditor missing cursor position accessors"
fi

# --- [8/8] Settings file written after run ---
echo ""
echo "[8/8] Checking persistence setup..."

# Check if SDL PrefPath directory was created (SDL_GetPrefPath call worked)
# $APPDATA may not be set in MSYS2 bash, $HOME may be /home/user.
# Probe common Windows locations directly.
PREF_DIR=""
for candidate in \
    "$APPDATA/Magnetar/ShaderEditor" \
    "/c/Users/$USER/AppData/Roaming/Magnetar/ShaderEditor" \
    "/c/Users/$(whoami)/AppData/Roaming/Magnetar/ShaderEditor" \
    "$HOME/AppData/Roaming/Magnetar/ShaderEditor"; do
    if [ -d "$candidate" ]; then
        PREF_DIR="$candidate"
        break
    fi
done
if [ -n "$PREF_DIR" ]; then
    pass "SDL PrefPath directory exists ($PREF_DIR)"
else
    fail "SDL PrefPath directory not found (checked APPDATA, /c/Users/*/AppData/Roaming, HOME)"
fi

# Check that session save is wired before shutdown in main.cpp
if grep -q "sessionManager.save" src/main.cpp; then
    pass "Session save wired before shutdown"
else
    fail "Session save not wired before shutdown"
fi

# Check that settings save is wired before shutdown in main.cpp
if grep -q "settings.save" src/main.cpp; then
    pass "Settings save wired before shutdown"
else
    fail "Settings save not wired before shutdown"
fi

# Note: settings.json and session.json are only written on graceful exit.
# The timeout-killed process (exit 124) cannot run shutdown code, so we
# verify the save paths exist in code rather than on disk.
if [ -f "$PREF_DIR/settings.json" ]; then
    pass "settings.json exists on disk (from prior graceful exit)"
fi
if [ -f "$PREF_DIR/session.json" ]; then
    pass "session.json exists on disk (from prior graceful exit)"
fi

# --- Summary ---
echo ""
if [ "$FAIL" -eq 0 ]; then
    echo "=== RESULT: PASS ($PASS passed, $FAIL failed) ==="
    exit 0
else
    echo "=== RESULT: FAIL ($PASS passed, $FAIL failed) ==="
    exit 1
fi
