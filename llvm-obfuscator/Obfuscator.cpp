//===- Obfuscator.cpp - out-of-tree LLVM obfuscation pass plugin ----------===//
//
// An OLLVM-style obfuscator implemented as a modern (new PassManager) out-of-tree
// plugin. No LLVM fork required: build against a stock LLVM and load into clang
// (-fpass-plugin=) or opt (-load-pass-plugin + -passes=).
//
// Implements two classic middle-end transforms from the research report:
//
//   * substitution (MBA)   : rewrites integer add/sub/and/or/xor into
//                            equivalent Mixed Boolean-Arithmetic expressions.
//   * bogus-control-flow   : guards each basic block body behind a runtime-seeded
//                            opaque predicate with a bogus (but harmless) edge,
//                            inflating the CFG.
//
// Both are correctness-preserving: substitution uses exact integer identities,
// and the bogus false-edge falls through to the real body so either path is safe.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <vector>

using namespace llvm;

namespace {

//===----------------------------------------------------------------------===//
// Substitution (MBA) pass
//===----------------------------------------------------------------------===//
struct SubstitutionPass : PassInfoMixin<SubstitutionPass> {
    static bool isEligible(BinaryOperator *BO) {
        return BO->getType()->isIntegerTy();
    }

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
        bool changed = false;
        // Snapshot instructions first: we insert new ones while iterating.
        std::vector<BinaryOperator *> work;
        for (Instruction &I : instructions(F))
            if (auto *BO = dyn_cast<BinaryOperator>(&I))
                if (isEligible(BO))
                    work.push_back(BO);

        for (BinaryOperator *BO : work) {
            IRBuilder<> B(BO);
            Value *x = BO->getOperand(0);
            Value *y = BO->getOperand(1);
            Value *rep = nullptr;

            switch (BO->getOpcode()) {
            case Instruction::Add: {
                // x + y == (x ^ y) + 2*(x & y)
                Value *x_xor_y = B.CreateXor(x, y);
                Value *x_and_y = B.CreateAnd(x, y);
                Value *two = ConstantInt::get(BO->getType(), 2);
                rep = B.CreateAdd(x_xor_y, B.CreateMul(two, x_and_y));
                break;
            }
            case Instruction::Sub: {
                // x - y == (x ^ ~y) + 2*(x & ~y) + 1    (== add(x, -y))
                Value *ny = B.CreateNot(y);
                Value *x_xor = B.CreateXor(x, ny);
                Value *x_and = B.CreateAnd(x, ny);
                Value *two = ConstantInt::get(BO->getType(), 2);
                Value *one = ConstantInt::get(BO->getType(), 1);
                Value *t = B.CreateAdd(x_xor, B.CreateMul(two, x_and));
                rep = B.CreateAdd(t, one);
                break;
            }
            case Instruction::Xor: {
                // x ^ y == (x | y) - (x & y)
                rep = B.CreateSub(B.CreateOr(x, y), B.CreateAnd(x, y));
                break;
            }
            case Instruction::And: {
                // x & y == (x + y) - (x | y)
                rep = B.CreateSub(B.CreateAdd(x, y), B.CreateOr(x, y));
                break;
            }
            case Instruction::Or: {
                // x | y == (x & y) + (x ^ y)
                rep = B.CreateAdd(B.CreateAnd(x, y), B.CreateXor(x, y));
                break;
            }
            default:
                break;
            }

            if (rep) {
                BO->replaceAllUsesWith(rep);
                BO->eraseFromParent();
                changed = true;
            }
        }
        return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};

//===----------------------------------------------------------------------===//
// Bogus Control Flow pass
//===----------------------------------------------------------------------===//
struct BogusControlFlowPass : PassInfoMixin<BogusControlFlowPass> {
    // A module-level volatile global the optimiser cannot prove constant, so the
    // opaque predicate built from it is not folded away.
    static GlobalVariable *getOpaqueSeed(Module *M) {
        const char *Name = "xobf_opaque_seed";
        if (auto *G = M->getGlobalVariable(Name, true))
            return G;
        auto *Ty = Type::getInt32Ty(M->getContext());
        auto *G = new GlobalVariable(*M, Ty, /*isConstant=*/false,
                                     GlobalValue::InternalLinkage,
                                     ConstantInt::get(Ty, 0x1337), Name);
        return G;
    }

    // Build an i1 that is always true at runtime: (x*(x+1)) is even -> LSB == 0.
    static Value *makeOpaqueTrue(IRBuilder<> &B, GlobalVariable *Seed) {
        Type *Ty = Type::getInt32Ty(B.getContext());
        LoadInst *x = B.CreateLoad(Ty, Seed, /*isVolatile=*/true);
        Value *xp1 = B.CreateAdd(x, ConstantInt::get(Ty, 1));
        Value *prod = B.CreateMul(x, xp1);
        Value *lsb = B.CreateAnd(prod, ConstantInt::get(Ty, 1));
        return B.CreateICmpEQ(lsb, ConstantInt::get(Ty, 0));  // always true
    }

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
        if (F.isDeclaration())
            return PreservedAnalyses::all();
        Module *M = F.getParent();
        GlobalVariable *Seed = getOpaqueSeed(M);

        // Snapshot eligible blocks (skip entry and EH pads for safety).
        std::vector<BasicBlock *> blocks;
        for (BasicBlock &BB : F) {
            if (&BB == &F.getEntryBlock())
                continue;
            if (BB.isEHPad() || BB.isLandingPad())
                continue;
            if (isa<PHINode>(BB.front()))
                continue;  // keep PHIs simple: skip blocks starting with PHIs
            blocks.push_back(&BB);
        }

        bool changed = false;
        for (BasicBlock *BB : blocks) {
            // Split so 'head' ends with an unconditional branch to 'body'.
            Instruction *splitPt = BB->getFirstNonPHI();
            if (!splitPt)
                continue;
            BasicBlock *body = BB->splitBasicBlock(splitPt, BB->getName() + ".body");
            BasicBlock *head = BB;

            // Junk block: harmless dead ops, then branch to the real body, so
            // taking this edge is still semantically correct.
            BasicBlock *junk =
                BasicBlock::Create(F.getContext(), head->getName() + ".junk", &F);
            {
                IRBuilder<> JB(junk);
                Type *Ty = Type::getInt32Ty(F.getContext());
                Value *j = JB.CreateLoad(Ty, Seed, true);
                j = JB.CreateMul(j, ConstantInt::get(Ty, 7));
                j = JB.CreateAdd(j, ConstantInt::get(Ty, 13));
                (void)j;
                JB.CreateBr(body);
            }

            // Replace head's unconditional branch with: opaque_true ? body : junk.
            head->getTerminator()->eraseFromParent();
            IRBuilder<> HB(head);
            Value *cond = makeOpaqueTrue(HB, Seed);
            HB.CreateCondBr(cond, body, junk);
            changed = true;
        }
        return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};

}  // namespace

//===----------------------------------------------------------------------===//
// Plugin registration
//===----------------------------------------------------------------------===//
llvm::PassPluginLibraryInfo getObfuscatorPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "Obfuscator", LLVM_VERSION_STRING,
            [](PassBuilder &PB) {
                // Allow: opt -passes="substitution,bogus-cf"
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager &FPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "substitution") {
                            FPM.addPass(SubstitutionPass());
                            return true;
                        }
                        if (Name == "bogus-cf") {
                            FPM.addPass(BogusControlFlowPass());
                            return true;
                        }
                        return false;
                    });
                // Auto-run when loaded via clang -fpass-plugin (optimizer-last).
                PB.registerOptimizerLastEPCallback(
                    [](ModulePassManager &MPM, OptimizationLevel) {
                        FunctionPassManager FPM;
                        FPM.addPass(SubstitutionPass());
                        FPM.addPass(BogusControlFlowPass());
                        MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
                    });
            }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getObfuscatorPluginInfo();
}
