## How to build

1. clone repo : `git clone https://github.com/huyuantao-ultrarisc/strength-reduction.git`
2. `mkdir build && cd build`
3. `LLVM_DIR="your llvm cmake path for example /usr/lib/llvm-14/cmake" cmake ..`
4. and the shared object will be built.

## How to test

see `/test`

##  Basic Knowledge

 循环往往耗时比较高，如果我们能将循环中的一些指令替换为代价较低的指令那么收益会很大。比如下面一个例子。

```cpp
int main(){
    int sum = 0;
    for(int i=0;i<100;i++)
    {
        int j=i*3-2;
        sum+=j;
    }
   	// ...
}
```

这个例子中计算 $j$ 用到了一个减法指令和一个乘法指令。乘法指令往往代价较高，其实我们不难发现，因为每次循环 $i$ 总是加1，$j$ 的值每轮循环都会加 $3$，我们其实可以将其替换为。

```cpp
int main(){
    int sum = 0;
    int j=-2;
    for(int i=0;i<100;i++)
    {
        sum+=j;
        j+=3;
    }
   	// ...
}
```

这样一个乘法加上一个减法指令就被替换为了加法指令理论上性能就可以得到不错的提升。

为了实现上述优化，首先我们需要找到该循环的归纳变量(induction variable)，即存在一个正的或负的常数 $c$ 使得每次 $x$ 赋值时他的值总是增加 $c$，那么 $x$ 就称为“归纳变量”。本例中 $i$ 是一个很明显的归纳变量，$j$ 也是归纳变量。

为了实现这个优化，首先我们先使用 mem2reg 将部分 load 和 store 替换为 phi 指令。使用如下命令编译（llvm-14）

```bash
clang -emit-llvm -S -Xclang -disable-O0-optnone -fno-discard-value-names main.c -o main.ll
opt -S --mem2reg main.ll -o main.ll
```

得到的 llvm ir为

```
define dso_local i32 @main() #0 {
entry:
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %sum.0 = phi i32 [ 0, %entry ], [ %add, %for.inc ]
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %for.inc ]
  %cmp = icmp slt i32 %i.0, 100
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %mul = mul nsw i32 %i.0, 3
  %sub = sub nsw i32 %mul, 2
  %add = add nsw i32 %sum.0, %sub
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %inc = add nsw i32 %i.0, 1
  br label %for.cond, !llvm.loop !6

for.end:                                          ; preds = %for.cond
  ret i32 0
}
```

然后我们专心编写我们的 pass 。

## Implementation

### Algorithm
<!-- 
1. 计算出所有的induction variable，首先从 loop_header 中找到phi 结点（这往往是induction varaible），然后找出满足下列条件的变量作为潜在的 induction variable。（使用不动点算法）
   * add/sub/mul 等binary operator 指令中某个operand 是induction variable 且另一个是constant 

2. 给每个 induction variable 三个属性，`(depe, step, init)`，分别代表依赖的 induction variable，相对该induction variable 的乘法因子和加法因子，比如上述中 $i.0$ 的三个属性分别为 $(i.0,1,0)$，$mul$ 的三个属性为 $(i.0,3,0)$，$sub$ 的三个属性为 $(i.0,3,-2)$ 。$inc$ 三个属性为 $(i.0,1,1)$

3.  -->