#ifndef _GRAPH_H_
#define _GRAPH_H_
#include "LLVMEssentials.hh"
#include "PDGNode.hh"
#include "PDGEdge.hh"
#include "Tree.hh"
#include "CallWrapper.hh"
#include "FunctionWrapper.hh"
#include "PDGEnums.hh"
#include "PDGCommandLineOptions.hh"


#include <unordered_map>
#include <map>
#include <set>

namespace pdg
{
  class Node;
  class Edge;

  class GenericGraph
  {
  public:
    typedef std::unordered_map<llvm::Value *, Node *> ValueNodeMap;
    typedef std::set<Edge *> EdgeSet;
    typedef std::set<Node *> NodeSet;
    ValueNodeMap::iterator val_node_map_begin() { return _val_node_map.begin(); }
    ValueNodeMap::iterator val_node_map_end() { return _val_node_map.end(); }
    GenericGraph() = default;
    NodeSet::iterator begin() { return _node_set.begin(); }
    NodeSet::iterator end() { return _node_set.end(); }
    NodeSet::iterator begin() const { return _node_set.begin(); }
    NodeSet::iterator end() const { return _node_set.end(); }
    virtual void build(llvm::Module &M) = 0;
    void addEdge(Edge &e) { _edge_set.insert(&e); }
    void addNode(Node &n) { _node_set.insert(&n); }
    Node *getNode(llvm::Value &v);
    bool hasNode(llvm::Value &v);
    int numEdge() { return _edge_set.size(); }
    int numNode() { return _val_node_map.size(); }
    int NodeSetSize() { return _node_set.size(); }
    void setIsBuild() { _is_build = true; }
    bool isBuild() { return _is_build; }
    bool canReach(pdg::Node &src, pdg::Node &dst);
    bool canReach(pdg::Node &src, pdg::Node &dst, std::set<EdgeType> exclude_edge_types);
    ValueNodeMap &getValueNodeMap() { return _val_node_map; }
    void dumpGraph();

  protected:
    ValueNodeMap _val_node_map;  // store the mapping between llvm::Value and pdg::Node
    EdgeSet _edge_set;
    NodeSet _node_set;
    bool _is_build = false;
  };

  class ProgramGraph : public GenericGraph
  {
  public:
    typedef std::unordered_map<llvm::Function *, FunctionWrapper *> FuncWrapperMap;
    typedef std::unordered_map<llvm::CallBase *, std::set<CallWrapper *>> CallWrapperMap;
    typedef std::unordered_map<std::string, Node *> ClassNodeMap;
    typedef std::unordered_map<Node *, llvm::DIType *> NodeDIMap;

    ProgramGraph() = default;
    ProgramGraph(const ProgramGraph &) = delete;
    ProgramGraph(ProgramGraph &&) = delete;
    ProgramGraph &operator=(const ProgramGraph &) = delete;
    ProgramGraph &operator=(ProgramGraph &&) = delete;
    static ProgramGraph &getInstance()
    {
      static ProgramGraph g{};
      return g;
    }

    FuncWrapperMap &getFuncWrapperMap() { return _func_wrapper_map; }
    CallWrapperMap &getCallWrapperMap() { return _call_wrapper_map; }
    NodeDIMap &getNodeDIMap() { return _node_di_type_map; }
    void build(llvm::Module &M) override;
    bool hasFuncWrapper(llvm::Function &F) { return _func_wrapper_map.find(&F) != _func_wrapper_map.end(); }
    bool hasCallWrappers(llvm::CallBase &ci) { return _call_wrapper_map.find(&ci) != _call_wrapper_map.end(); }
    FunctionWrapper *getFuncWrapper(llvm::Function &F) { return _func_wrapper_map[&F]; }
    std::set<CallWrapper *> getCallWrappers(llvm::CallInst &ci) { return _call_wrapper_map[&ci]; }
    void bindDITypeToNodes(llvm::Module &M);
    llvm::DIType *computeNodeDIType(Node &n);
    void addTreeNodesToGraph(Tree &tree);
    void addFormalTreeNodesToGraph(FunctionWrapper &func_w);
    bool isAnnotationCallInst(llvm::Instruction &inst);
    void buildGlobalAnnotationNodes(llvm::Module &M);
    ClassNodeMap &getClassNodeMap() { return _class_node_map; }
    void insertClassNode(std::string cls_name);
    Node* getClassNodeByName(std::string cls_name);

    // 
    std::set<Node*> getNodeByAttrs(int line, std::string file_dir = "");
    std::set<Node*> getNodeByAttrs(std::string opcode, int line, int col = -1, std::string file_dir = "");
    std::set<Node*> getFuncNodeByName(std::string funcName);
    std::set<Node*> forward_slice(Node &start_node);
    std::set<Node*> backward_slice(Node &start_node);
    std::set<Node*> flow_slice(std::set<Node*> &source, std::set<Node*> &sink);
  private:
    FuncWrapperMap _func_wrapper_map;
    CallWrapperMap _call_wrapper_map;   // one call inst has multiple call wrappers, because it may have multiple candidates
    NodeDIMap _node_di_type_map;
    ClassNodeMap _class_node_map;
    //std::set<std::string> _class_name_set;  // could lead to out of memory
  };
} // namespace pdg

#endif