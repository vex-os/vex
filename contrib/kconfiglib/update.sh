#!/bin/sh
# Usage: update.sh [ref]
# Downloads Kconfiglib at the given branch, tag or commit (main
# by default), copies the files the build uses and records the
# commit in README.vex

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd "$(dirname "$(realpath "${0}")")" || exit 1


repo_url="https://github.com/sysprog21/Kconfiglib"
ref="${1:-main}"

# Resolve branches and tags to a commit so README.vex always
# points at exactly what was copied; a full hash is used as is
commit="$(git ls-remote "${repo_url}" "${ref}" | head -n1 | cut -f1)"
commit="${commit:-${ref}}"

archive_url="${repo_url}/archive/${commit}.tar.gz"

temp_file="$(mktemp /tmp/kconfiglib-XXXXXX.tar.gz)"
temp_dir="$(mktemp -d /tmp/kconfiglib-XXXXXX)"

cleanup() {
    rm -fv ${temp_file}
    rm -frv ${temp_dir}
}

trap cleanup EXIT

sh ../download.sh ${temp_file} ${archive_url} || exit 1

tar -xzf ${temp_file} -C ${temp_dir} || exit 1

src_dir="$(find ${temp_dir} -maxdepth 1 -mindepth 1 -type d | head -n1)"

for name in kconfiglib.py menuconfig.py oldconfig.py listnewconfig.py defconfig.py savedefconfig.py rawterm.py uicommon.py; do
    cp -v ${src_dir}/${name} ${name} || exit 1
done

cp -v ${src_dir}/LICENSE LICENSE || exit 1

version="$(sed -n 's/^VERSION = (\([0-9]*\), \([0-9]*\), \([0-9]*\))$/\1.\2.\3/p' kconfiglib.py)"

sed -i "s/^    commit .*$/    commit ${commit} (version ${version})/" README.vex || exit 1

exit 0
