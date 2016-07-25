make clean distclean
mkdir finlib
rm finlib/*.a

./configure --disable-shared --enable-unicode-properties --disable-cpp --enable-utf8 --host=arm-apple-darwin CFLAGS="-arch arm64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS8.3.sdk" CXXFLAGS="-arch arm64 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS8.3.sdk" LDFLAGS="-L." CC="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc" CXX="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++"

make

mv .libs/libpcre.a finlib/libpcre_arm64.a

./configure --disable-shared --enable-unicode-properties --disable-cpp --enable-utf8 --host=arm-apple-darwin CFLAGS="-arch armv7s -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS8.3.sdk" CXXFLAGS="-arch armv7s -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS8.3.sdk" LDFLAGS="-L." CC="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc" CXX="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++"

make

mv .libs/libpcre.a finlib/libpcre_armv7s.a

./configure --disable-shared --enable-unicode-properties --disable-cpp --enable-utf8 --host=arm-apple-darwin CFLAGS="-arch armv7 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS8.3.sdk" CXXFLAGS="-arch armv7 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS8.3.sdk" LDFLAGS="-L." CC="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc" CXX="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++"

make

mv .libs/libpcre.a finlib/libpcre_armv7.a

./configure --disable-shared --enable-unicode-properties --disable-cpp --enable-utf8

make

mv .libs/libpcre.a finlib/libpcre_x86_64.a

cd finlib

lipo -create libpcre_arm64.a libpcre_armv7s.a libpcre_armv7.a libpcre_x86_64.a -output libpcre-8.36.a

rm finlib/libpcre_*