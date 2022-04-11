#!/bin/bash
#
mkdir -p objs_w64_clang
# Fix up some JM includes that use wrong case 
[ -e objs_w64_clang/Winsock2.h ] || echo "#include <WinSock2.h>" > objs_w64_clang/Winsock2.h
[ -e objs_w64_clang/windows.h ] || echo "#include <Windows.h>" > objs_w64_clang/windows.h
toolchain_windows.sh "$PWD" make CC=clang-cl CXX=clang-cl LD=lld-link OBJSDIR=objs_w64_clang -f Makefile.w64 $*
