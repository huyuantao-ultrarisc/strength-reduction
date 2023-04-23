#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
using namespace llvm;

#include <tuple>
#include <map>
#include <optional>
#include <iostream>
using namespace std;

namespace
{
   struct SkeletonPass : public FunctionPass
   {
      struct InductionInfo
      {
         // 依赖的 祖先（注意不一定是直接parent，而是追溯到最上面的parent）
         Value *par;
         // 要转变后的induction variable每步增量
         optional<int> step;
         // induction variable 初始值
         optional<int> init;
         // 是否是Phi node
         bool is_phi = false;
         // 是否是induction_var;
         bool induction_var = true;
         // 相对 par 的
         int parent_factor = 1;
         // 相对 par 的位移
         int parent_dif = 0;
      };
      // induction variable 表
      map<Value *, InductionInfo> ind_tab;
      void print_ind_tab()
      {
         for (auto [val, info] : ind_tab)
         {
            errs() << "-------------------------\n";
            errs() << *val << "\n";
            errs() << *info.par << "\n";
            errs() << "+: " << info.parent_dif << "\n";
            errs() << "*:" << info.parent_factor << "\n";
            errs() << "init: " << *info.init << "\n";
            errs() << "step:" << *info.step << "\n";
            errs() << "\n------------------------\n";
         }
      }
      void getIndVarTab(Loop *loop)
      {
         ind_tab.clear();
         auto loop_header = loop->getHeader();
         // get blocks name
         set<StringRef> loop_block_names;
         for (auto &blk : loop->getBlocks())
            loop_block_names.insert(blk->getName());

         // get init phi node
         for (auto &ins : *loop_header)
         {
            if (auto *phi_node = dyn_cast<PHINode>(&ins))
            {
               // to test if it can be a induction variable
               bool flag = false;
               InductionInfo ind_info;
               // 第一轮
               ind_info.is_phi = true, ind_info.par = nullptr;

               for (int i = 0; i < phi_node->getNumIncomingValues(); i++)
               {
                  auto blk = phi_node->getIncomingBlock(i);
                  auto val = phi_node->getIncomingValue(i);
                  auto *const_val = dyn_cast<ConstantInt>(val);
                  // 从其他BB中来，且是常量一般都是初始值
                  if (const_val && !loop_block_names.count(blk->getName()))
                  {
                     ind_info.init = const_val->getSExtValue();
                  }
                  else if (loop_block_names.count(blk->getName()))
                     ind_info.par = val;
               }

               if (ind_info.init.has_value())
               {
                  ind_tab[&ins] = ind_info;
               }
            }
         }
         // 遍历所有指令，找到bianry operator,如果一个operand 是const另一个在ind_var_tab中，那么将其添加到ind_var_tab中。
         bool changed = true;
         // 不满足条件的 Value (不应该加入ind_tab)
         set<Value *> rm_values;

         // 找出所有的潜在ind_var_tab
         while (changed)
         {
            changed = false;
            for (auto blk : loop->getBlocks())
            {
               for (auto &ins : *blk)
               {
                  if (auto *op = dyn_cast<BinaryOperator>(&ins))
                  {
                     Value *opds[2];
                     opds[0] = op->getOperand(0), opds[1] = op->getOperand(1);
                     // idx 是常数操作数的下标, idx^1 即另一个
                     auto idx = dyn_cast<ConstantInt>(opds[0]) ? 0 : 1;
                     // 一个bin op里面没有常操作数，加入到 rm_values，依赖他的将会被删除。
                     if (!dyn_cast<ConstantInt>(opds[idx]))
                     {
                        if (!rm_values.count(&ins))
                        {
                           changed = true;
                           rm_values.insert(&ins);
                        }
                        continue;
                     }

                     int const_val = dyn_cast<ConstantInt>(opds[idx])->getSExtValue();
                     // 满足一个是常操作数，另一个是可疑ind_var
                     if (ind_tab.count(opds[idx ^ 1]))
                     {
                        auto &info = ind_tab[opds[idx ^ 1]];
                        if (rm_values.count(opds[idx ^ 1]))
                        {
                           rm_values.insert(opds[idx ^ 1]);
                           rm_values.insert(&ins);
                           changed = true;
                           continue;
                        }

                        InductionInfo new_ind_info = info;
                        new_ind_info.is_phi = false;

                        if (info.is_phi)
                           new_ind_info.par = opds[idx ^ 1];

                        if (op->getOpcode() == Instruction::Add)
                           new_ind_info.parent_dif += const_val;
                        else if (op->getOpcode() == Instruction::Sub)
                           new_ind_info.parent_dif -= const_val;
                        else if (op->getOpcode() == Instruction::Mul)
                           new_ind_info.parent_dif *= const_val, new_ind_info.parent_factor *= const_val;
                        else
                        {
                           rm_values.insert(&ins);
                           continue;
                        }
                        if (!ind_tab.count(&ins))
                           changed = true;

                        ind_tab[&ins] = new_ind_info;
                     }
                  }
               }
            }
         }
         // 消除不满足条件的可疑 ind var
         changed = true;
         while (changed)
         {
            changed = false;
            for (auto it = ind_tab.begin(); it != ind_tab.end();)
            {
               if (rm_values.count(it->first) || rm_values.count(it->second.par))
               {
                  changed = true;
                  it = ind_tab.erase(it);
                  continue;
               }
               if (!ind_tab.count(it->second.par))
                  it = ind_tab.erase(it);
               else if (it->second.is_phi)
               {
                  auto res = ind_tab.find(it->second.par);
                  if (res->second.parent_factor != 1)
                     changed = true, it = ind_tab.erase(it);
                  else
                     it++;
               }
               else
                  it++;
            }
         }
         /*
            计算 ind_var 的step 和init
            如果ind var是phi结点，那么他的init 已经算出来了，step就是它par的 diff
            如果其他的话就根据 par来做偏移。
         */
         changed = true;
         while (changed)
         {
            changed = false;
            for (auto &[ind_var, info] : ind_tab)
            {
               auto &par_info = ind_tab[info.par];
               if (info.is_phi && (!info.step || !info.init))
               {
                  changed = true;
                  info.step = par_info.parent_dif;
               }
               else
               {
                  if (par_info.step && par_info.init && (!info.step || !info.init))
                  {
                     changed = true;
                     info.init = *par_info.init * info.parent_factor + info.parent_dif;
                     info.step = *par_info.step * info.parent_factor;
                  }
               }
            }
         }
         print_ind_tab();
      }

      static char ID;
      SkeletonPass() : FunctionPass(ID) {}

      void getAnalysisUsage(AnalysisUsage &AU) const
      {
         AU.setPreservesCFG();
         AU.addRequired<LoopInfoWrapperPass>();
         AU.addRequired<TargetLibraryInfoWrapperPass>();
      }

      virtual bool runOnFunction(Function &F)
      {

         Module *module = F.getParent();

         // 先做个死代码消除
         legacy::FunctionPassManager FPM(module);
         FPM.add(createDeadCodeEliminationPass());
         FPM.doInitialization();
         bool changed = FPM.run(F);
         FPM.doFinalization();

         // 获取循环信息 ： GetLoopInformations
         LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
         for (auto *L : LI)
         {
            getIndVarTab(L);
         }

         return true;
      }
   };
}

char SkeletonPass::ID = 0;
static RegisterPass<SkeletonPass> X("sr", "Stregth Reduction Pass",
                                    false /* Only looks at CFG */,
                                    true /* Not Analysis Pass */);

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerSkeletonPass(const PassManagerBuilder &,
                                 legacy::PassManagerBase &PM)
{
   PM.add(new SkeletonPass());
}
static RegisterStandardPasses
    RegisterMyPass(PassManagerBuilder::EP_LoopOptimizerEnd,
                   registerSkeletonPass);