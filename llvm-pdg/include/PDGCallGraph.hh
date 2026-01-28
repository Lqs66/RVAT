#ifndef PDGCALLGRAPH_H_
#define PDGCALLGRAPH_H_
#include <iostream>
#include <fstream>
#include <algorithm>

#include "LLVMEssentials.hh"
#include "Graph.hh"
#include "PDGUtils.hh"

// SVF headers
#include "SVF-LLVM/LLVMUtil.h"
#include "Graphs/SVFG.h"
#include "WPA/Andersen.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "SVF-LLVM/LLVMModule.h"
#include "MemoryModel/PointerAnalysis.h"


namespace pdg
{
  typedef std::set<llvm::Function *> CandidateSet;                        //used to store candidates of indirect callsite
  typedef std::map<const llvm::CallBase *, CandidateSet> IndirectCallCandsMap;  //used to store indirect callsites and its candidates
  
  /**
   * used for building PDG
  */
  class PDGCallGraph : public GenericGraph
  {
  public:
    using PathVecs = std::vector<std::vector<llvm::Function *>>;
    PDGCallGraph() = default;
    PDGCallGraph(const PDGCallGraph &) = delete;
    PDGCallGraph(PDGCallGraph &&) = delete;
    PDGCallGraph &operator=(const PDGCallGraph &) = delete;
    PDGCallGraph &operator=(PDGCallGraph &&) = delete;
    static PDGCallGraph &getInstance()
    {
      static PDGCallGraph g{};
      return g;
    }
    void build(llvm::Module &M) override;

    std::set<llvm::Function *> getIndirectCallCandidates(llvm::CallBase &ci);

    bool isFuncSignatureMatch(llvm::CallBase &ci, llvm::Function &f);
    bool isTypeEqual(llvm::Type &t1, llvm::Type &t2);
    bool canReach(Node &src, Node &sink);
    void dump();
    void printPaths(Node &src, Node &sink);
    PathVecs computePaths(Node &src, Node &sink); // compute all pathes
    void computePathsHelper(PathVecs &path_vecs, Node &src, Node &sink, std::vector<llvm::Function *> cur_path, std::unordered_set<llvm::Function *> visited_funcs, bool &found_path);

    void createInCallCandsMapBySVF(llvm::Module &M);
    void createIncallCandsMapByTargetsMD(llvm::Module &M);

    // void setIncallsMap(std::map<int, std::set<std::string>>&& incallsMap){
    //   this->incallsMap = std::move(incallsMap);
    // }

    // 
    Node* getFuncNodeByName(std::string funcName);
    std::set<Node*> getSubCallGraph(std::string &start_node_name, std::set<std::string> &slice_funcs);
    bool hasTargetsMD(llvm::Module &M);
  private:
    // 
    IndirectCallCandsMap _indrect_call_cands_map; //used to store indirect callsites and its candidates
    // std::map<int, std::set<std::string>> incallsMap; // user defined indirect callsites
    std::map<std::string, Node*> _funcNodeMap;
  };
} // namespace pdg

#endif