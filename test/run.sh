cd ../build && make
cd ../test
clang -emit-llvm -S -Xclang -disable-O0-optnone -fno-discard-value-names main.c -o main.ll

# generate llvm asm
opt -S --mem2reg main.ll -o main.ll
opt -S -load ../build/skeleton/libSkeletonPass.so -sr -enable-new-pm=0 main.ll -o main2.ll 
# generate binary file
opt --mem2reg main.ll -o main
opt -load ../build/skeleton/libSkeletonPass.so -sr -enable-new-pm=0 main.ll -o main2
chmod 761 main
chmod 761 main2