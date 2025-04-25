SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

function generate() {
    for entry in $2/*; do
        # Recursively search subdirectories
        if [[ -d $entry ]]; then
            generate $1 $entry
            continue
        fi
        if [[ $entry != *.h ]] && [[ $entry != *.hpp ]]; then
            continue
        fi
        $SCRIPT_DIR/include-guard-generator.pl -r $1 -i ${entry} &
    done
}

if [ $# -lt 2 ]; then
    echo "Usage: ./generate_include_guards.sh base/directory base/directory/to/fix"
    exit 0
fi

# Check all of the arguments first to make sure they're all directories
for dir in "$@"; do
    if [ ! -d "${dir}" ]; then
        echo "${dir} is not a directory";
    fi
done

generate $@
