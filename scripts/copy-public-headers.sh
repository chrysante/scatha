rm -r "include/scatha"


copyHeaders() {
  while IFS="" read -r file || [ -n "$file" ]
  do
    if [ -z "$file" ]; then
        continue
    fi
    export dir=${file%/*}
    mkdir -p "include/$2/$dir"
    cp "$1/$file" "include/$2/$file"
  done < "$1/public-headers"
}

copyHeaders "lib" "scatha"
copyHeaders "svm" "svm"
