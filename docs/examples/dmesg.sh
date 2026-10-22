#!/bin/sh
# Prints the kernel log as dmesg does, colored by level when the output is a terminal
# Reads /dev/kmsg, which then waits for new records, or the file given as the first argument

if [ -t 1 ]; then
    color=1
else
    color=0
fi

exec awk -v color="$color" '
BEGIN {
    # Levels 0 to 7: emerg, alert, crit, err, warning, notice, info, debug
    split("1;31 1;31 1;31 31 33 1 0 2", styles, " ")
    hex = "0123456789abcdef"
}

# Each \xNN in the text stands for one byte
function unescape(text,    out, at) {
    out = ""

    while((at = index(text, "\\x")) > 0) {
        out = out substr(text, 1, at - 1)
        out = out sprintf("%c", (index(hex, substr(text, at + 2, 1)) - 1) * 16 + index(hex, substr(text, at + 3, 1)) - 1)
        text = substr(text, at + 4)
    }

    return out text
}

{
    semicolon = index($0, ";")

    if(semicolon == 0) {
        next
    }

    count = split(substr($0, 1, semicolon - 1), head, ",")

    if(count < 3) {
        next
    }

    level = head[1] % 8
    usec = head[3]
    stamp = sprintf("[%5d.%06d]", int(usec / 1000000), usec % 1000000)
    text = unescape(substr($0, semicolon + 1))

    if(color) {
        printf "\033[32m%s\033[0m \033[%sm%s\033[0m\n", stamp, styles[level + 1], text
    }
    else {
        printf "%s %s\n", stamp, text
    }

    fflush()
}
' "${1:-/dev/kmsg}"
