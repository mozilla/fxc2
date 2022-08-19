x86 :
	i686-w64-mingw32-clang++ -static fxc2.cpp -ofxc2.exe
x64 :
	x86_64-w64-mingw32-clang++ -static fxc2.cpp -ofxc2.exe
arm :
	armv7-w64-mingw32-clang++ -static fxc2.cpp -ofxc2.exe
arm64 :
	aarch64-w64-mingw32-clang++ -static fxc2.cpp -ofxc2.exe 