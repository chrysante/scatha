if [ $# -eq 0 ]; then
    echo "Usage: ./format-all.sh base/directory"
    exit 0
fi

for dir in "$@"; do
    if [ ! -d "${dir}" ]; then
        echo "$dir is not a directory";
    else 
        "$dir/scripts/format.sh" "$dir/lib"
        "$dir/scripts/format.sh" "$dir/test"
        "$dir/scripts/format.sh" "$dir/scatha-c"
        "$dir/scripts/format.sh" "$dir/playground"
        "$dir/scripts/format.sh" "$dir/svm"
        "$dir/scripts/format.sh" "$dir/include/svm"
    fi
done
