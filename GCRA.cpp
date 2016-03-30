//===-- GCRA.cpp - Regsiter Allocator --------------------------------===//
//
//
//
//===-----------------------------------------------------------------===//
//
// This is very simple register allocator.
// Currently it only creates inference graph
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
#include "llvm/CodeGen/CalcSpillWeights.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/CodeGen/LiveVariables.h"
#include "llvm/CodeGen/LiveInterval.h"
#include "llvm/CodeGen/LiveIntervalAnalysis.h"
#include "llvm/CodeGen/LiveRangeEdit.h"
#include "llvm/CodeGen/LiveRegMatrix.h"
#include "llvm/CodeGen/LiveStackAnalysis.h"
#include "llvm/PassAnalysisSupport.h"
#include "Spiller.h"
#include <cstdlib>
#include <ctime>
#include <algorithm>

using namespace llvm;
using namespace std;

STATISTIC(NumOfSpilledRegs, "Number of Registers need to be spilled out to memory");
STATISTIC(SpillCost, "Total spill cost for register allocation");
namespace {
	class Cuckoo {
		public:
			map<unsigned,int> habitat;
			int noOfColor;
	};

	class GCRA : public MachineFunctionPass {

	public:
		static char id; // Pass Identification
		typedef set<unsigned> RegSet;
		RegSet VReg2Alloc;
		RegSet PRegAvailable;
		
		// inference graph 
		// [Reg] => [01101010] Map of Reg to a map 
		// the inner map is Reg to  1  if inference is there that means 1 other wise no entry means no Inference 
		map<unsigned, map<unsigned,int> *> InfeGraph; 
		typedef map<unsigned, map<unsigned,int> *>::iterator infe_it;
		typedef map<unsigned, int>::iterator infdata_it;
        map<unsigned, int> PReg2Color;
        map<int, vector<unsigned>> Color2PReg;
		map<unsigned, vector<unsigned> *> PRegsForVReg;
		vector< Cuckoo *> cuckooHabitats;
		vector< pair<float,unsigned> > spillCosts;


		// print the inference graph
		void printInfeGraph() {
			for( auto &Reg : InfeGraph) {
				dbgs() << "["<<Reg.first<<"] : " ;
				for( auto &Reg2 : InfeGraph ) {
					infdata_it it = Reg.second->find(Reg2.first);
					if(it != Reg.second->end())
						dbgs() << "(" << Reg2.first << ") : " << "1 " ;
					else
						dbgs() << "(" << Reg2.first << ") : " << "0 " ;
				}
				dbgs() << "\n";
			}
		}

		void initializeColor(unsigned NodeValue, 
				int vRegCount, 
				Cuckoo *CuckooObj, 
				const TargetRegisterInfo &TRI) {

				map<unsigned, int> * InferenceMap = InfeGraph[NodeValue];
				if(InferenceMap->size() != 0) {
					list<int> adjColors;

					for( auto adjacentNode : *InferenceMap) {
						if(!TRI.isPhysicalRegister(adjacentNode.first)) {
							adjColors.push_back(CuckooObj->habitat[adjacentNode.first]);
							//dbgs() << "Node : " << adjacentNode.first << " Color : " << CuckooObj->habitat[adjacentNode.first] << "\n";
						}
						else {
							adjColors.push_back(PReg2Color[adjacentNode.first]);
							//dbgs() << "Node : " << adjacentNode.first << " Color : " << PReg2Color[adjacentNode.first] << "\n";	
						}
					}

					for (int i = 1; i <= vRegCount ; i++) {
						if (find(adjColors.begin(), adjColors.end(), i) == adjColors.end()) {
							CuckooObj->habitat[NodeValue] = i;
							//dbgs() << "Color assigned : " << i <<"\n";
							if (CuckooObj->noOfColor < i) {
								CuckooObj->noOfColor = i;
							}
							break;
						}
					}
				} else {
					CuckooObj->habitat[NodeValue] = 1;
				}		
		}

		void initializeCuckoo(const TargetRegisterInfo &TRI) {
			//map<unsigned,int> * habitat = new map<unsigned,int>();
			Cuckoo *CuckooObj = new Cuckoo();
			int vRegCount = VReg2Alloc.size();
			if (vRegCount != 0) {
				for (auto VReg : VReg2Alloc) {
					CuckooObj->habitat[VReg] = vRegCount;
				}

				
				int intialRanNode = rand() % vRegCount;
				infdata_it iterator = CuckooObj->habitat.begin();
				advance(iterator, intialRanNode);
				unsigned NodeValue = iterator->first;
				//dbgs() << "Random node choosen to be colored : " << NodeValue << "\n";
				initializeColor(NodeValue,vRegCount,CuckooObj,TRI);

				vector< pair<int,unsigned>> worList;
				int tempDeg = 0;
				for (auto infeData : InfeGraph) {
					if(!TRI.isPhysicalRegister(infeData.first)) {
						if(infeData.first != NodeValue) {
							tempDeg  = infeData.second->size();
							worList.push_back(pair<int,unsigned>(tempDeg,infeData.first));
						}
					}
				}
				sort(worList.begin(),worList.end());
				while(!worList.empty()) {

					unsigned NodeToColor = worList.back().second;
					//dbgs() << "NodeToColor : " << NodeToColor << " Degree : " << worList.back().first << "\n";
					worList.pop_back();
					initializeColor(NodeToColor,vRegCount,CuckooObj,TRI);
				}
				cuckooHabitats.push_back(CuckooObj); 
			}
		}

		void spillVReg(unsigned VReg,
                             SmallVectorImpl<unsigned> &NewIntervals,
                             MachineFunction &MF, LiveIntervals &LIS,
                             VirtRegMap &VRM, Spiller &VRegSpiller) {

			VReg2Alloc.erase(VReg);
			LiveRangeEdit LRE(&LIS.getInterval(VReg), NewIntervals, MF, LIS, &VRM);
			VRegSpiller.spill(LRE);

			const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
			//(void)TRI;
			dbgs() << "VREG " << PrintReg(VReg, &TRI) << " -> SPILLED (Cost: "
			               << LRE.getParent().weight << ", New vregs: ";

			// Copy any newly inserted live intervals into the list of regs to
			  // allocate.
			for (LiveRangeEdit::iterator I = LRE.begin(), E = LRE.end();
			    I != E; ++I) {
			    const LiveInterval &LI = LIS.getInterval(*I);
			    assert(!LI.empty() && "Empty spill range.");
			    dbgs() << PrintReg(LI.reg, &TRI) << " ";
			    VReg2Alloc.insert(LI.reg);
			}

			dbgs() << ")\n";
		}

		void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.setPreservesCFG();
        	AU.addRequired<AAResultsWrapperPass>();
  			AU.addPreserved<AAResultsWrapperPass>();
  			AU.addRequired<LiveIntervals>();
  			AU.addPreserved<LiveIntervals>();
  			AU.addPreserved<SlotIndexes>();
  			AU.addRequired<LiveStacks>();
  			AU.addPreserved<LiveStacks>();
  			AU.addRequired<MachineBlockFrequencyInfo>();
  			AU.addPreserved<MachineBlockFrequencyInfo>();
  			AU.addRequiredID(MachineDominatorsID);
  			AU.addPreservedID(MachineDominatorsID);
  			AU.addRequired<MachineLoopInfo>();
  			AU.addPreserved<MachineLoopInfo>();
  			AU.addRequired<VirtRegMap>();
  			AU.addPreserved<VirtRegMap>();
  			AU.addRequired<LiveRegMatrix>();
  			AU.addPreserved<LiveRegMatrix>();
		 	MachineFunctionPass::getAnalysisUsage(AU);
		}

		GCRA() : MachineFunctionPass(id) {
			
  			initializeLiveIntervalsPass(*PassRegistry::getPassRegistry());
  			initializeSlotIndexesPass(*PassRegistry::getPassRegistry());
  			initializeRegisterCoalescerPass(*PassRegistry::getPassRegistry());
  			initializeMachineSchedulerPass(*PassRegistry::getPassRegistry());
  			initializeLiveStacksPass(*PassRegistry::getPassRegistry());
			initializeMachineDominatorTreePass(*PassRegistry::getPassRegistry());
  			initializeMachineLoopInfoPass(*PassRegistry::getPassRegistry());
  			initializeVirtRegMapPass(*PassRegistry::getPassRegistry());
  			initializeLiveRegMatrixPass(*PassRegistry::getPassRegistry());
		}

		static inline float normalizeGCSpillWeight(float UseDefFreq, unsigned Size,
                                         unsigned NumInstr) {
 		 // All intervals have a spill weight that is mostly proportional to the number
 		 // of uses, with uses in loops having a bigger weight.
 		 return NumInstr * normalizeSpillWeight(UseDefFreq, Size, 1);
		}

		bool runOnMachineFunction(MachineFunction &MFn) {
			bool needMoreRound = false;

	        dbgs() << "Function : " << MFn.getName() << "\n";

			MachineRegisterInfo &MRI = MFn.getRegInfo();
			MRI.freezeReservedRegs(MFn);

			// unsigned int virtualRegCount = MFn.getRegInfo().getNumVirtRegs();
			// dbgs() << "Number of virtual registers: " << virtualRegCount << "\n";
			LiveIntervals &LIS = getAnalysis<LiveIntervals>();
			VirtRegMap &VRM = getAnalysis<VirtRegMap>();
			
			MachineBlockFrequencyInfo &MBFI = getAnalysis<MachineBlockFrequencyInfo>();
			calculateSpillWeightsAndHints(LIS, MFn, &VRM, getAnalysis<MachineLoopInfo>(),
	                                			MBFI, normalizeGCSpillWeight);
	  		Spiller* VRegSpiller = createInlineSpiller(*this, MFn, VRM);
	  		srand(time(NULL)); // Initial seed for random number generator 
	  		do {
				needMoreRound = false;
				dbgs() << "Round begins: \n";
				unsigned int virtualRegCount = MFn.getRegInfo().getNumVirtRegs();
				dbgs() << "Number of virtual registers: " << virtualRegCount << "\n";
				// clear data from the previous pass
				InfeGraph.clear();
				PRegsForVReg.clear();
				cuckooHabitats.clear();
				spillCosts.clear();
				VReg2Alloc.clear();
				//LIS.print(dbgs());

				for(unsigned i = 0 , e = virtualRegCount; i != e; ++i) {
					unsigned Reg = TargetRegisterInfo::index2VirtReg(i); // reg ID
					if(MRI.reg_nodbg_empty(Reg)) {
	                    //dbgs() << "Debug Related Register : " << Reg << "\n";
						continue;
	                }
					else {
						VReg2Alloc.insert(Reg);
						InfeGraph[Reg] = new map<unsigned,int>();
						LiveInterval &VRLive = LIS.getInterval(Reg);
						spillCosts.push_back(pair<float,unsigned>(VRLive.weight,Reg));
						//dbgs() << "Virtual Register : " << Reg << " Live Range : ";
						//VRLive.print(dbgs());
						//dbgs() << "\n";

						// This is very expensive 
						for(unsigned j = 0 , ej = virtualRegCount; j != ej; ++j) {
							unsigned RegT = TargetRegisterInfo::index2VirtReg(j); // reg ID 
							
							if(MRI.reg_nodbg_empty(RegT) || RegT == Reg)
								continue;
							else {
								LiveInterval &VR2Live = LIS.getInterval(RegT);
								//dbgs() << " \t checking Inference with : " << RegT << " Live Range : "; 
								//VR2Live.print(dbgs());
								//dbgs() << "\n";
								if(VRLive.overlaps(VR2Live)) {
									//dbgs() << "Inference !\n";
									(*InfeGraph[Reg])[RegT] = 1;
								}
								
							}
						}
						//dbgs() << "\n";
					}
				}
				sort(spillCosts.rbegin(), spillCosts.rend());
				//printInfeGraph();

				const TargetMachine &TM = MFn.getTarget();
				const MCRegisterInfo *MCRI = TM.getMCRegisterInfo();

				//dbgs() << "Number of physical register units: " << MCRI->getNumRegUnits() << "\n";

				const TargetRegisterInfo &TRI = *MFn.getSubtarget().getRegisterInfo();
				vector<unsigned> Worklist(VReg2Alloc.begin(), VReg2Alloc.end());

				while (!Worklist.empty()) {
	    			unsigned VReg = Worklist.back();
	    			Worklist.pop_back();
	    			const TargetRegisterClass *TRC = MRI.getRegClass(VReg);
	    			LiveInterval &VRegLI = LIS.getInterval(VReg);
	    			    // Record any overlaps with regmask operands.

	    			BitVector RegMaskOverlaps;
	    			LIS.checkRegMaskInterference(VRegLI, RegMaskOverlaps);
	    			dbgs() << "Finding availble registers for VReg : " << VReg << " ";
	    			// Compute an initial allowed set for the current vreg.
	    			vector<unsigned> *VRegAllowed = new vector<unsigned>();
	    			PRegsForVReg[VReg] = VRegAllowed;
	    			ArrayRef<MCPhysReg> RawPRegOrder = TRC->getRawAllocationOrder(MFn);
	    			for (unsigned I = 0; I != RawPRegOrder.size(); ++I) {
	      				unsigned PReg = RawPRegOrder[I];
	      				bool Interference = false;
	      				if (MRI.isReserved(PReg)) {
	      					continue;
	      				}
	      				// vregLI crosses a regmask operand that clobbers preg.
	      				if (!RegMaskOverlaps.empty() && !RegMaskOverlaps.test(PReg)) {
							Interference = true;
	        				
	      				} else {
	        				PRegAvailable.insert(PReg);
	        				if(InfeGraph.find(PReg) == InfeGraph.end()) {
	        					//dbgs() << "Memory allocated \n";
	        					InfeGraph[PReg] = new map<unsigned,int>();
	        				}
	        			}
	      				// vregLI overlaps fixed regunit interference.
	      				if(!Interference){
	      					for (MCRegUnitIterator Units(PReg, &TRI); Units.isValid(); ++Units) {
	        					if (VRegLI.overlaps(LIS.getRegUnit(*Units))) {
	          						Interference = true;
	          						break;
	        					}
	      					}
	      				}

	      				if (Interference) {
	      					dbgs() << "Inference between VReg : " << VReg << " PReg : " << PReg << "\n";
	      				    (*InfeGraph[VReg])[PReg] = 1;
	      					(*InfeGraph[PReg])[VReg] = 1;
	        				continue;
	      				}
	      				dbgs() << "Pushing PReg : " << MCRI->getName(PReg) << "\n"; 
	      				VRegAllowed->push_back(PReg);	
	    			}

	  			}

	  			
	  			dbgs() << "Total Physical availble : "<< PRegAvailable.size() << "\n";
	  			for( auto mapEntry : PRegsForVReg) {

	  				dbgs() << "Allowed Physical Registers for VirtualRegister : " << mapEntry.first <<"\n";
	      				for( auto preg : *mapEntry.second ) {
	      					dbgs() << MCRI->getName(preg) << " ";
	       				}
	       				dbgs() << "\n";
	  			}
				
	  			dbgs() << " \n \n-------------- Inference Graph --------------- \n \n";
	  			printInfeGraph();
	  			dbgs() << " \n \n----------- Inference Graph End ------------ \n \n";

	  			Color2PReg.clear();

		            int color = 1;
		            RegSet tempPRegs = PRegAvailable;
		  			for (auto pReg : PRegAvailable) {
		                if (tempPRegs.find(pReg) != tempPRegs.end()) {
			  				PReg2Color[pReg] = color;
			  				//dbgs() << "Pushing PReg : " << pReg << "\n";
			                Color2PReg[color].push_back(pReg);
			                for (auto pRegC : tempPRegs) {
			                	//dbgs() << "pReg : " << pReg << " pRegC : " << pRegC << "\n";
			                    if(TRI.regsOverlap(pReg, pRegC) && pReg != pRegC) {
			                    	//dbgs() << "Pushing pRegC : " << pRegC << "\n";
			                        PReg2Color[pRegC] = color;
			                        Color2PReg[color].push_back(pRegC);
			                    }
			                }
			                for (auto pRegDone : Color2PReg[color]) {
			                	//dbgs() << "Deleting : " << pRegDone << "\n";
			                    tempPRegs.erase(pRegDone);
			                }
			                color++;
		            	}
		  			}
	  			

	            // Print the data structures created 
	            for (auto mapEntry : PReg2Color) {
	                dbgs() << "PReg : " << mapEntry.first << " Color : " << mapEntry.second << "\n";
	            }

	            for (auto mapEntry : Color2PReg) {
	                dbgs() << "Color : " << mapEntry.first << " Regs : " ;
	                for (auto reg : mapEntry.second) {
	                    dbgs() << MCRI->getName(reg) << " " ; 
	                }
	                dbgs() << "\n";
	            }
	            

	            /* Logic for applying cuckoo optimization for Graph Coloring begins from here. */

	            // Initialize 5 cuckoos

	            initializeCuckoo(TRI);
	            //initializeCuckoo(TRI);
	            //initializeCuckoo(TRI);
	            //initializeCuckoo(TRI);
	            //initializeCuckoo(TRI);

	            VRM.clearAllVirt(); // clear previous allocation
	            bool doneColoring = false;
	            for(auto cuckoo : cuckooHabitats) {
	            	if (!doneColoring) {
		            	// dbgs() << "Initial Coloring : \n";
		            	// for(auto pair : cuckoo->habitat) {
		            	// 	dbgs() << "VReg : " << pair.first << " Assigned Color : " << pair.second <<"\n";
		            	// }
		            	// If any initial cuckoo has coloring with out conflicts then use it directly
		            	if ( cuckoo->noOfColor <= Color2PReg.size() ) {
		            		dbgs() << "No Splits are required ! Register can be assigned \n";
		            		for(auto pair : cuckoo->habitat) {
		            			dbgs() << "New pair VReg : " << pair.first << " Color : " << pair.second << "\n";
		            			vector<unsigned> regVec = Color2PReg[pair.second];
		            			bool allocated = false;
		            			dbgs() << "RegVec size : " << regVec.size() << "\n";
		            			for ( auto reg : regVec) {
		            				for (auto allowedReg : *PRegsForVReg[pair.first]) {
		            					if(reg == allowedReg) {
		            						dbgs() << "Assigning VReg : " << pair.first << " to PReg : " << MCRI->getName(reg) << "\n \n";
		            						VRM.assignVirt2Phys(pair.first,reg);
		            						allocated = true;
		            						break;
		            					} 
		            				}
		            				if(allocated)
		            					break;
		            			}
		            		}
		            		doneColoring = true;
		            		break;
		            	}
	            	}
	            	else {
	            		break;
	            	}
	            }
	             if (!doneColoring) {
	            	// first we try to minimize the color required with cuckoo optimization 
	             	// if no success then spill the registers and if any new Live Intervals are added repeate the whole flow.
	             	dbgs() << "Need to spill registers ! \n";
	            	
	             	// for( auto spillCostPair : spillCosts) {
	             	// 	dbgs() << "Spill cost : " << spillCostPair.first << " VReg : " << spillCostPair.second << "\n";
	             	// } 
	             	unsigned VRegToSpill = spillCosts.back().second;
	             	SmallVector<unsigned, 8> NewVRegs;
	      			spillVReg(VRegToSpill, NewVRegs, MFn, LIS, VRM,*VRegSpiller);
	      			NumOfSpilledRegs++;
	      			dbgs() << "Deleting VReg : " << VRegToSpill << "\n";
	      			VReg2Alloc.erase(VRegToSpill); // Delete the spilled register from the VReg2Alloc set 
	      			for( auto newReg : NewVRegs ){
	      				dbgs() << "Inserting New VReg : " << newReg << "\n";
	      				VReg2Alloc.insert(newReg); // Consider any new register for allocation
	      			}

	      			dbgs() << "Need to re-iterate again \n";
	      			needMoreRound = true;
	            } 
            } while(needMoreRound);
			return false;
		}

    virtual const char *getPassName() const {
    return "My Register Allocator";
}


	};
char GCRA::id = 0;

}  
   

FunctionPass *createGCRA() { return new GCRA(); }
  

static RegisterRegAlloc gCRA("gc",
					"graph-coloring register allocator",
					createGCRA);
