#!/usr/bin/env zsh

set -o nounset

# Assumption: file names don't contain `:` (for the `cut` invocation).
# Safe to assume, as such a character in a filename would cause trouble on Windows, a platform we support

# any regression turn this non-zero
regressions=0

# exclusions (these are just grep extended regular expressions to match against paths relative to the root of the repository)
exclusions_nonutf8='(.*(7z|gif|ic(ns|o)|png|qm|zip))'
exclusions_bom='src/base/unicodestrings.h'
exclusions_tw='(*.ts)|src/webui/www/private/scripts/lib/mootools-1.2-more.js'
exclusions_trailing_newline='configure'
exclusions_no_lf='(*.ts)|(.*svg)|compile_commands.json|src/webui/www/private/scripts/lib/mootools-1.2-(core-yc.js|more.js)'

echo -e "\n*** Detect files not encoded in UTF-8 ***\n"

find . -path ./build -prune -false -o -path ./.git -prune -false -o -type f -exec file --mime {} \; | sort \
    | grep -v -e "charset=us-ascii" -e "charset=utf-8" | cut -d ":" -f 1 \
    | grep -E -v -e "${exclusions_nonutf8}" \
    | tee >(echo -e "--> Files not encoded in UTF-8: found" "$(wc -l < /dev/stdin)" "regression(s)\n") \
    | xargs -I my_input -0 bash -c 'echo "my_input"; test "$(echo -n "my_input" | wc -l)" -eq 0'
regressions=$((regressions+$?))

echo -e "\n*** Detect files encoded in UTF-8 with BOM ***\n"

grep --exclude-dir={.git,build} -rIl $'\xEF\xBB\xBF' | sort \
    | grep -E -v -e "${exclusions_bom}" \
    | tee >(echo -e "--> Files encoded in UTF-8 with BOM: found" "$(wc -l < /dev/stdin)" "regression(s)\n") \
    | xargs -I my_input -0 bash -c 'echo "my_input"; test "$(echo -n "my_input" | wc -l)" -eq 0'
regressions=$((regressions+$?))

echo -e "\n*** Detect usage of CR byte ***\n"

grep --exclude-dir={.git,build} -rIlU $'\x0D' | sort \
    | tee >(echo -e "--> Usage of CR byte: found" "$(wc -l < /dev/stdin)" "regression(s)\n") \
    | xargs -I my_input -0 bash -c 'echo "my_input"; test "$(echo -n "my_input" | wc -l)" -eq 0'
regressions=$((regressions+$?))

echo -e "\n*** Detect trailing whitespace in lines ***\n"

grep --exclude-dir={.git,build} -rIl "[[:blank:]]$" | sort \
    | grep -E -v -e "${exclusions_tw}" \
    | tee >(echo -e "--> Trailing whitespace in lines: found" "$(wc -l < /dev/stdin)" "regression(s)\n") \
    | xargs -I my_input -0 bash -c 'echo "my_input"; test "$(echo -n "my_input" | wc -l)" -eq 0';
regressions=$((regressions+$?))

echo -e "\n*** Detect too many trailing newlines ***\n"

find . -path ./build -prune -false -o -path ./.git -prune -false -o -type f -exec file --mime {} \; | sort \
    | grep -e "charset=us-ascii" -e "charset=utf-8" | cut -d ":" -f 1 \
    | grep -E -v -e "${exclusions_trailing_newline}" \
    | xargs -L1 -I my_input bash -c 'test "$(tail -q -c2 "my_input" | hexdump -C | grep "0a 0a")" && echo "my_input"' \
    | tee >(echo -e "--> Too many trailing newlines: found" "$(wc -l < /dev/stdin)" "regression(s)\n") \
    | xargs -I my_input -0 bash -c 'echo "my_input"; test "$(echo -n "my_input" | wc -l)" -eq 0'
regressions=$((regressions+$?))

echo -e "\n*** Detect no trailing newline ***\n"

find . -path ./build -prune -false -o -path ./.git -prune -false -o -type f -exec file --mime {} \; | sort \
    | grep -e "charset=us-ascii" -e "charset=utf-8" |  cut -d ":" -f 1 \
    | grep -E -v -e "${exclusions_no_lf}" \
    | xargs -L1 -I my_input bash -c 'test "$(tail -q -c1 "my_input" | hexdump -C | grep "0a")" || echo "my_input"' \
    | tee >(echo -e "--> No trailing newline: found" "$(wc -l < /dev/stdin)" "regression(s)\n") \
    | xargs -I my_input -0 bash -c 'echo "my_input"; test "$(echo -n "my_input" | wc -l)" -eq 0'
regressions=$((regressions+$?))

echo -e "\n*** Detect translation closing tag in new line ***\n"

grep --exclude-dir={.git,build} -nri "^</translation>" | sort \
    | cut -d ":" -f 1,2 \
    | tee >(echo -e "--> Translation closing tag in new line: found" "$(wc -l < /dev/stdin)" "regression(s)\n") \
    | xargs -I my_input -0 bash -c 'echo "my_input"; test "$(echo -n "my_input" | wc -l)" -eq 0'
regressions=$((regressions+$?))

if [ "$regressions" -ne 0 ]; then
    regressions=1
    echo "\nFile health regressions found. Please fix them (or add them as exclusions)."
else
    echo "All OK, no file health regressions found."
fi

exit $regressions;
