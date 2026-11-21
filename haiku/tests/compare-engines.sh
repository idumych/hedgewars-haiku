#!/bin/sh
# compare-engines.sh C_ENGINE PASCAL_ENGINE: runs upstream's Lua tests on both
# engines and fails if their output differs in any way (ticks, positions, results).
# On Haiku, from the top of the tree; the engines must sit next to lib/.
set -u
DATA=$(pwd)/share/hedgewars/Data
USER_DIR=${TMPDIR:-/tmp}/hw-compare-user
mkdir -p "$USER_DIR"
run() {
	for t in tests/lua/*.lua tests/lua/luaAPI/*.lua; do
		echo "== $t"
		timeout 300 "$1" --prefix "$DATA" --user-prefix "$USER_DIR" --nosound --nomusic \
			--stats-only --lua-test "$t" 2>&1 | grep "^Lua:"
	done
}
run "$1" > "$USER_DIR/c.txt"
run "$2" > "$USER_DIR/pascal.txt"
for f in c pascal; do	# an engine that didn't start prints no test lines at all
	grep -q "^Lua: .*loaded" "$USER_DIR/$f.txt" || { echo "compare-engines: no test output from the $f engine"; exit 1; }
done
if diff "$USER_DIR/pascal.txt" "$USER_DIR/c.txt"; then
	echo "compare-engines: identical ($(grep -c "^Lua:" "$USER_DIR/c.txt") lines from $(grep -c "^==" "$USER_DIR/c.txt") tests)"
else
	echo "compare-engines: the engines differ (above: < Pascal, > C)"
	exit 1
fi
