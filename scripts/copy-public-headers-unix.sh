rm -r "include/scatha"

while IFS="" read -r file || [ -n "$file" ]
do
    export dir=${file%/*}
    mkdir -p "include/scatha/$dir"
    cp "lib/$file" "include/scatha/$file"
done < lib/public-headers
