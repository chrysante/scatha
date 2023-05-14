premake5 xcode4

xcodebuild -project "$1/scatha-c.xcodeproj" -configuration Release
xcodebuild -project "$1/svm.xcodeproj" -configuration Release

cp "$1/build/bin/Release/scatha-c" "/usr/local/bin"
cp "$1/build/bin/Release/libscatha.dylib" "/usr/local/bin"
cp "$1/build/bin/Release/svm" "/usr/local/bin"
