//===-- GCRA.cpp - Regsiter Allocator --------------------------------===//
//
//
//
//===-----------------------------------------------------------------===//
//
// This is very simple register allocator based on George and Appel.
//
//===-----------------------------------------------------------------===//

#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineBlockFrequencyInfo.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachinePostDominators.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetMachine.h"
#define DEBUG_TYPE "gcra"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Compiler.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/CodeGen/LiveVariables.h"
#include "llvm/PassAnalysisSupport.h"

#include "RDfact.h"

using namespace llvm;
using namespace std;

typedef map<const MachineBasicBlock *, set<unsigned>*> BBtoRegMap;
typedef map<const MachineInstr *, set<unsigned>*> InstrToRegMap;
typedef map<const MachineBasicBlock *, set<RDfact *>*> BBtoRDfactMap;
typedef map<const MachineInstr *, set<RDfact *>*> InstrToRDfactMap;

typedef map<const unsigned, set<MachineInstr *>*> RegToInstrsMap;
typedef map<const unsigned, set<unsigned>*> RegToRegsMap;

namespace {
	class GCRA : public MachineFunctionPass {

	public:
		static char id; // Pass Identification

		void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.setPreservesAll();
		 	MachineFunctionPass::getAnalysisUsage(AU);
		}

		GCRA() : MachineFunctionPass(id) {
			initializeLiveVariablesPass(*PassRegistry::getPassRegistry());
		}

		bool runOnMachineFunction(MachineFunction &MFn) {
			unsigned int virtualRegCount = MFn.getRegInfo().getNumVirtRegs();
			dbgs() << "Number of virtual registers: " << virtualRegCount << "\n";

			const TargetMachine *TM = &MFn.getTarget();
			const MCRegisterInfo *MCRI = TM->getMCRegisterInfo();

			dbgs() << "Number of physical registers: " << MCRI->getNumRegs() << "\n";

			for (MachineFunction::iterator b = MFn.begin(), e = MFn.end(); b != e; ++b)
      			for (MachineBasicBlock::iterator MI = b->begin(), e = b->end(); MI != e; ++MI) {
      				MI->print(dbgs());
     			}
			return false;
		}

	};

  char GCRA::id = 0;
  
}

class LiveRange {
public:
  RegToInstrsMap range;

  // TODO: Physical registers don't obey the single assignment rule, violating the assumptions we made below!
  // We pretend that we don't need to worry about them. Therefore we don't support inline assemblies.
  // We assume caller-save general-purpose except for EAX.
  LiveRange(MachineFunction &Fn, InstrToRegMap &insLiveBeforeMap, InstrToRDfactMap &insRDbeforeMap)
  {
    // 1. Build initial live ranges
    // For each CFG node D that defines variable x, the initial live range for D consists of: 
    // ( <x>, <{D} union {N | x in N.live-before and D in N.reaching-defs-before}> ) 
    // x in N.live-before means that x is used in N or after,
    // D in N.reaching-defs-before means that the use of x in N really is defined by D
    // Due to LLVM's single assignment nature, I don't think it's necessary to compute reaching defs.

    // 2. Convert initial live ranges to final live ranges (collapse overlapping initial live ranges for the same variable):
    // LLVM IR has true SSA due to phi nodes, but phi nodes have been eliminated in the lowered representation.
    // So it's possible that x is defined twice in two branches.

    // We combine the two steps into one.
    for (MachineFunction::iterator b = Fn.begin(), e = Fn.end(); b != e; ++b)
      for (MachineBasicBlock::iterator D = b->begin(), e = b->end(); D != e; ++D) {
        int n = D->getNumOperands();
        for (int j = 0; j < n; j++) {
          MachineOperand op = D->getOperand(j);  
          if (op.isReg() && op.getReg() && op.isDef()) {
            unsigned x = op.getReg();
            if (TargetRegisterInfo::isPhysicalRegister(x)) // This may be due to calling conventions
              continue;

            set<MachineInstr *> *s = range[x];
            if (!s) {
              s = new set<MachineInstr *>();
              range[x] = s;
            }
            s->insert(D);
            for (MachineFunction::iterator b = Fn.begin(), e = Fn.end(); b != e; ++b)
              for (MachineBasicBlock::iterator N = b->begin(), e = b->end(); N != e; ++N)
                if (insLiveBeforeMap[N]->count(x))
                  s->insert(N);
            }
          } // end iterating operands
      } // end iterating instructions and blocks
  }

  void debug(map<MachineInstr *, unsigned> &InstrToNumMap)
  {
    errs() << "\n\nLIVE RANGES\n";
    map<const unsigned, set<MachineInstr *>*>::iterator p, e;
    for (p = range.begin(), e = range.end(); p != e; ++p) {
      errs() << p->first << ": {";
      set<MachineInstr *> *s = p->second;
      set<MachineInstr *>::iterator i = s->begin(), e = s->end();
      for(; i != e; ++i)
        errs() << " %" << InstrToNumMap[*i];
      errs() << " }\n";
    }
  }
};
  
FunctionPass *createGCRA() { return new GCRA(); }
  

RegisterRegAlloc register_gcra("gc",
					"graph-coloring register allocator",
					createGCRA);
