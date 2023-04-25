cd ../build && make
cd ../test
clang -emit-llvm -S -Xclang -disable-O0-optnone -fno-discard-value-names main.c -o main.ll

# generate llvm asm
opt -S --mem2reg main.ll -o main.ll
opt -S -load ../build/skeleton/libSkeletonPass.so -sr -enable-new-pm=0 main.ll -o main2.ll 
# generate binary file
# llc -O0 main.ll --filetype=obj
# clang main.o -o main
# rm main.o

# llc -O0 main2.ll --filetype=obj
# clang main2.o -o main2
# rm main2.o