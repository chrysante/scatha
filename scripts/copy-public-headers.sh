rm -r "include/scatha"

while IFS="" read -r file || [ -n "$file" ]
do
    if [ -z "$file" ]; then
        continue
    fi
    if [ "$OS" == 'Windows_NT' ]; then
#Windows
        export udir=${file%/*}
        export dir=$(cygpath -w "$udir")
        file=$(cygpath -w "$file")
        export target_dir="include\scatha\\$dir"
        target_dir=${target_dir//}
        mkdir -p "$target_dir"
        export source_file="lib\\$file"
        source_file=${source_file//}
        export target_file="include\scatha\\$file"
        target_file=${target_file//}
        cp "$source_file" "$target_file"
    else 
#Posix
        export dir=${file%/*}
        mkdir -p "include/scatha/$dir"
        cp "lib/$file" "include/scatha/$file"
    fi    
done < lib/public-headers
