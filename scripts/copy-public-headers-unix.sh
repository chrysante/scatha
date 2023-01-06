rm -r "include/scatha"

while IFS="" read -r file || [ -n "$file" ]
do
    #echo "Processing $file"
    export dir=${file%/*}
    mkdir -p "include/scatha/$dir"
    cp "lib/$file" "include/scatha/$file"
    
    #echo "\tin directory $dir"
done < lib/public-headers
