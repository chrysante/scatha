premake5 xcode4

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

xcodebuild -project "$PROJ_DIR/scatha-c.xcodeproj" -configuration Release
xcodebuild -project "$PROJ_DIR/svm.xcodeproj" -configuration Release

cp "$1/build/bin/Release/scatha-c" "/usr/local/bin"
cp "$1/build/bin/Release/libscatha.dylib" "/usr/local/bin"
cp "$1/build/bin/Release/svm" "/usr/local/bin"
