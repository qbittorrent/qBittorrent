#!/usr/bin/env bash

set -o pipefail
set -o nounset

# Assumption: file names don't contain `:` (for the `cut` invocation).
# Safe to assume, as such a character in a filename would cause trouble on Windows, a platform we support

# any regression turn this non-zero
regressions=0

echo -e "*** Detect files not encoded in UTF-8 ***\n"

comm -13 <(sort .github/workflows/file_health_exclusions_nonutf8.txt) <(find . -path ./.git -prune -false -o -path ./build -prune -false -o -type f -exec file --mime {} \; | grep -v -e "charset=us-ascii" -e "charset=utf-8" | cut -d ":" -f 1 | sort) | tee >(echo -e "\n--> Result: found" $(wc -l </dev/stdin) "regression(s)") | xargs -I my_input -0 bash -c 'echo "my_input"; test "$(echo -n "my_input" | wc -l)" -eq 0'; regressions=$((regressions+$?))

echo -e "*** Detect files encoded in UTF-8 with BOM ***\n"

comm -13 <(sort .github/workflows/file_health_exclusions_bom.txt) <(grep -rIl $'\xEF\xBB\xBF' --exclude-dir={.git,build} | sort) | tee >(echo -e "\n--> Result: found" $(wc -l </dev/stdin) "regression(s)") | xargs -I my_input -0 bash -c 'echo "my_input"; test "$(echo -n "my_input" | wc -l)" -eq 0'; regressions=$((regressions+$?))

echo -e "*** Detect usage of CR ***\n"

comm -13 <(sort .github/workflows/file_health_exclusions_cr.txt) <(grep -rIlU $'\x0D' --exclude-dir={.git,build} | sort) | tee >(echo -e "\n--> Result: found" $(wc -l </dev/stdin) "regression(s)") | xargs -I my_input -0 bash -c 'echo "my_input"; test "$(echo -n "my_input" | wc -l)" -eq 0'; regressions=$((regressions+$?))

echo -e "*** Detect trailing whitespace in lines ***\n"

comm -13 <(sort .github/workflows/file_health_exclusions_tw.txt) <(grep -rIl "[[:blank:]]$" --exclude-dir={.git,build} | sort) | tee >(echo -e "\n--> Result: found" $(wc -l </dev/stdin) "regression(s)") | xargs -I my_input -0 bash -c 'echo "my_input"; test "$(echo -n "my_input" | wc -l)" -eq 0'; regressions=$((regressions+$?))

echo -e "*** Detect too many trailing newlines ***\n"

comm -13 <(sort .github/workflows/file_health_exclusions_extra_lf.txt) <(find . -path ./build -prune -false -o -path ./.git -prune -false -o -type f -print0 | xargs -0 -L1 bash -c 'test "$(tail -q -c2 "$0" | hexdump -C | grep "0a 0a")" && echo "$0"' | sort) | tee >(echo -e "\n--> Result: found" $(wc -l </dev/stdin) "regression(s)") | xargs -I my_input -0 bash -c 'echo "my_input"; test "$(echo -n "my_input" | wc -l)" -eq 0'; regressions=$((regressions+$?))

echo -e "*** Detect no trailing newline ***\n"

comm -13 <(sort .github/workflows/file_health_exclusions_no_lf.txt) <(find . -type f -not -path "./.git/*" -not -path "./build/*" -not -path "./compile_commands.json" -not -path "*.svg" -exec file --mime {} \; | grep -e "charset=us-ascii" -e "charset=utf-8" |  cut -d ":" -f 1  | xargs -L1 bash -c 'test "$(tail -q -c1 "$0" | hexdump -C | grep "0a")" || echo "$0"' | sort) | tee >(echo -e "\n--> Result: found" $(wc -l </dev/stdin) "regression(s)") | xargs -I my_input -0 bash -c 'echo "my_input"; test "$(echo -n "my_input" | wc -l)" -eq 0'; regressions=$((regressions+$?))

if [ "$regressions" -ne 0 ]; then
    regressions=1
    echo "File health regressions found. Please fix them (or add them as exclusions)."
else
    echo "All OK, no file health regressions found."
fi

exit $regressions;
