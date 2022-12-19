cd external/gmp/
./gmp-install-complete.sh

cd ../mpfr/
./mpfr-install-complete.sh

cd ../../

premake5 xcode4
