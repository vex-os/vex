#!/bin/sh
# Prints the device tree as it is at open, then follows its changes
# Reads /dev/devctl, or the file given as the first argument

# Sets f_<key> for each key=value argument, so keys can't clobber shell variables
fields() {
    f_id= f_parent= f_name= f_driver= f_kind=
    for field; do
        eval "f_${field%%=*}=\${field#*=}"
    done
}

# Two spaces for each ancestor of node $1, the root (0) excluded
indent() {
    i=
    eval "p=\${parent_$1:-0}"
    while [ "$p" != 0 ]; do
        i="$i  "
        eval "p=\${parent_$p:-0}"
    done
    printf '%s' "$i"
}

name_of() { eval "printf '%s' \"\${name_$1:-#$1}\""; }

snapshot_begin() { live=; }
snapshot_end()   { live=1; echo "--- listening"; }

add() {
    fields "$@"
    eval "parent_$f_id=$f_parent name_$f_id=$f_name"

    if [ "$live" ]; then
        echo "+ $f_name"
    else
        echo "$(indent "$f_id")$f_name${f_driver:+ ($f_driver)}"
    fi
}

remove()           { fields "$@"; echo "- $(name_of "$f_id")"; unset "parent_$f_id" "name_$f_id"; }
bind()             { fields "$@"; [ "$live" ] && echo "$(name_of "$f_id"): bound to $f_driver"; }
unbind()           { fields "$@"; echo "$(name_of "$f_id"): unbound from $f_driver"; }
interface()        { fields "$@"; echo "$(name_of "$f_id"): $f_kind $f_name"; }
interface_remove() { fields "$@"; echo "$(name_of "$f_id"): lost $f_name"; }

while read -r event args; do
    case $event in
        snapshot_begin|snapshot_end|add|remove|bind|unbind|interface|interface_remove)
            eval "$event $args" ;;
    esac
done < "${1:-/dev/devctl}"
