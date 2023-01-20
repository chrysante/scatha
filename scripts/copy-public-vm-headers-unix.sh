rm -r "include/svm"

while IFS="" read -r file || [ -n "$file" ]
do
    if [ -z "$file" ]; then
        continue
    fi
    export dir=${file%/*}
    mkdir -p "include/svm/$dir"
    cp "svm/$file" "include/svm/$file"
done < svm/public-headers
