## How to build

1. clone repo : `git clone https://github.com/huyuantao-ultrarisc/strength-reduction.git`
2. `cmake -DLLVM_DIR=/usr/lib/llvm-14/cmake -B build`
3. `cmake --build build --parallel 8`
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

为了实现上述优化，首先我们需要找到该循环的归纳变量(induction variable)，即存在一个正的或负的常数 $c$ 使得每次 $x$ 赋值时他的值总是增加 $c$，那么 $x$ 就称为“归纳变量(induction variable)”。本例中 $i$ 是一个很明显的归纳变量，$j$ 也是归纳变量。

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

1. 计算所有 induction variable （见 skeleton.cpp 下 `void getIndVarTab(Loop *loop)`）
   对于每个induction variable 我们有以下属性

    ```cpp
   struct InductionInfo
   {
       // 依赖的 祖先（注意不一定是直接parent，而是追溯到最上面的parent）
       Value *par = nullptr;
       // 要转变后的induction variable每步增量
       optional<int> step;
       // induction variable 初始值
       optional<int> init;
       // 是否是Phi node
       bool is_phi = false;
       // 相对 par 的
       int parent_factor = 1;
       // 相对 par 的位移
       int parent_dif = 0;
   };
    ```

   每个induction varaible 都有一个par属性，它代表依赖的induction variable，如果是phi结点，那么依赖的induction variable 就会是 phi 节点的某个非常数的 incoming 部分。我们的目的是计算出 step和 init，然后就可以在循环的header 插入每个induction variable 的 phi 结点，然后再在循环body的尾变添加step 指令。比如 (伪IR) 我们分析出

    ```
   loop_header:
     tmp = phi [INIT,entry] [tmp2,body]
     ...
   body:
     tmp2 = tmp + STEP
     ...
    ```

   起初 loop_header 中出现的 phi 结点都加入到 ind_tab 中作为可疑的induction variable。然后遍历整个循环的所有指令，如果一个 BinaryOperator 满足以下条件我们就将它加入到ind_tab 来作为可疑的induction variable

   * 其中一个操作数是常数，另一个操作数是 induction variable（称其为依赖的归纳变量）。
   * 操作符为 +, - , *

   遇到这样的 BinaryOperator 我们首先将依赖归纳变量的par作为自己的par（如果依赖的的归纳变量是phi结点，那么我们直接将这个 phi 结点作为自己的par），并且计算相对这个 par 的乘积和偏移。（减法需要注意依赖的归纳变量的位置，如果归纳变量在右边例如 `j=5-i` 我们要看作 `j = -1*i + 5` 。j 的属性 parent_diff 和parent_factor 与 i 相比较先取相反数然后 parent_diff 再加上5，详见代码实现）

   如果这条指令没有加入到 ind_tab 中，我们需要将其加入到 rm_values 中，计算完 ind_tab 后我们需要检查是否有归纳变量依赖 rm_values 中的值，如果依赖我们将其从 ind_tab 中删除。这样我们跑一个不动点算法将所有不符合条件的归纳变量删除。

2. 计算 init 和 step 

   找到所有的潜在归纳变量且计算了它相对 par 的倍数和 diff，我们就可以计算 它的 init 和 step 了，phi 结点的 init 我们都计算出来了，他已经再 phi 中显示声明了。phi 的 step 我们却还没有计算，phi 的 par 是 phi 的其中一个 incoming，而这个 incoming 的 par 是 phi。而这个 incoming 的 parent_diff 就是 phi 的 step。注意如果 这个incoming 的 parent_factor 不是 1 那么这个 phi 结点就不是归纳变量，需要使用一个不动点算法将不满足条件的归纳变量移除。

   计算完 phi 结点的 init 和 step 那其他依赖 phi 结点的归纳变量就可以轻松计算出来了，对于一个归纳变量 x 计算公式为 

   
   $$
   init = x.\mathrm{parent\_diff} + x.par.\mathrm{init}\times x.\mathrm{parent\_factor} \\
   step = x.\mathrm{parent\_factor}\times x.par.\mathrm{step}
   $$

 3. 修改 IR 

    下面就是更新 IR 即可，见 代码中的 `runOnFunction`

4. 继续修改

   * 这个 pass 是一个非常简单的强度削减的 pass，它只能优化特殊情况，下面是可以做的工作

     * 这个 pass 只兼容 int 类型，可以尝试将其拓展到浮点类型

     * 这个 pass 归纳变量的初始值必须是常数，可以稍微修改一下让他支持初始值非常数。比如下列循环

       ```cpp
       for(int i = x; i<=y;i++)
       {
           sum += i*3-4;
       }
       ```

       因为归纳变量 `i` 的初始值不是常数，不会将 `i` 加入到 `ind_tab`，导致 `i*3-4` 也不会加入到 ind_tab，这就错失了一次优化时机。

     * 分析数组的 induction variable，可以让每次数组位置访问转变为一个指针的加减。