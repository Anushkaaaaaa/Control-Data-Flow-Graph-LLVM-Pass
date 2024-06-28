#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/Pass.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Use.h>
#include <llvm/Analysis/CFG.h>
#include <list>
#include <map>
#include <set>

using namespace llvm;

namespace {
struct DFGPass : public ModulePass {
public:
    // Basic Block nodes
    typedef std::pair<BasicBlock*, StringRef> node;
    // Edges between basic blocks
    typedef std::pair<node, node> edge;
    // Collection of nodes
    typedef std::set<node> node_set;
    // Collection of edges
    typedef std::list<edge> edge_list;
    static char ID;
    edge_list dataFlowEdges;  // Data flow edges
    edge_list controlFlowEdges; // Control flow edges
    std::map<Function*, node_set> functionNodes; // Map of function to its basic block nodes
    int num;

    DFGPass() : ModulePass(ID) { num = 0; }

    // Get name of basic block
    StringRef getBBName(BasicBlock* BB) {
        if (!BB) return "undefined";
        if (BB->getName().empty()) {
            return "BB_" + Twine(num++).str();
        } else {
            return BB->getName();
        }
    }

    bool runOnModule(Module &M) override {
        std::error_code error;
        enum sys::fs::OpenFlags F_None;
        StringRef fileName("CDFG_BB.dot");
        raw_fd_ostream file(fileName, error, F_None);

        errs() << "Hello\n";
        dataFlowEdges.clear();
        controlFlowEdges.clear();
        functionNodes.clear();

        // Process each function in the module
        for (Function &F : M) {
            if (F.isDeclaration()) continue; // Skip function declarations

            node_set &basicBlockNodes = functionNodes[&F];

            for (BasicBlock &BB : F) {
                basicBlockNodes.insert(node(&BB, getBBName(&BB)));

                // Collect data flow edges
                for (Instruction &I : BB) {
                    for (Value *Op : I.operands()) {
                        if (Instruction *OpInst = dyn_cast<Instruction>(Op)) {
                            if (OpInst->getParent() != &BB) {
                                dataFlowEdges.push_back(edge(node(OpInst->getParent(), getBBName(OpInst->getParent())), node(&BB, getBBName(&BB))));
                            }
                        }
                    }
                }
            }

            // Collect control flow edges
            for (BasicBlock &BB : F) {
                Instruction* terminator = BB.getTerminator();
                for (BasicBlock* succ : successors(&BB)) {
                    controlFlowEdges.push_back(edge(node(&BB, getBBName(&BB)), node(succ, getBBName(succ))));
                }
            }
        }

        file << "digraph \"CDFG for Module\" {\n";
        // Dump nodes for each function in a subgraph
        for (auto &FN : functionNodes) {
            Function *F = FN.first;
            node_set &basicBlockNodes = FN.second;
            file << "subgraph cluster_" << F->getName().str() << " {\n";
            file << "label = \"" << F->getName().str() << "\";\n";
            for (node BBNode : basicBlockNodes) {
                file << "\tNode" << BBNode.first << "[shape=record, label=\"" << BBNode.second << "\"];\n";
            }
            file << "}\n";
        }

        // Dump control flow edges
        file << "edge [color=black]" << "\n";
        for (edge_list::iterator EI = controlFlowEdges.begin(), EE = controlFlowEdges.end(); EI != EE; ++EI) {
            file << "\tNode" << EI->first.first << " -> Node" << EI->second.first << "\n";
        }

        // Dump data flow edges
        file << "edge [color=red]" << "\n";
        for (edge_list::iterator EI = dataFlowEdges.begin(), EE = dataFlowEdges.end(); EI != EE; ++EI) {
            file << "\tNode" << EI->first.first << " -> Node" << EI->second.first << "\n";
        }

        errs() << "Write Done\n";
        file << "}\n";
        file.close();
        return false;
    }
};
}

char DFGPass::ID = 0;
static RegisterPass<DFGPass> X("DFGPass", "CDFG Pass Analyze",
    false, false
);

