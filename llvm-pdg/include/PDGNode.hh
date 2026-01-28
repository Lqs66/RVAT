#ifndef NODE_H_
#define NODE_H_
#include "LLVMEssentials.hh"
#include "PDGEdge.hh"
#include "PDGEnums.hh"
#include <set>
#include <iterator>
#include <filesystem>

namespace pdg
{
  template <typename NodeTy>
  class EdgeIterator;

  class Edge;
  class Node
  {
  public:
    using EdgeSet = std::set<Edge *>;
    using iterator = EdgeIterator<Node>;
    using const_iterator = EdgeIterator<Node>;

    Node() = delete;

    Node(GraphNodeType node_type)
    {
      _val = nullptr;
      _node_type = node_type;
      _is_visited = false;
      _func = nullptr;
      _node_di_type = nullptr;
    }
    
    Node(llvm::Value &v, GraphNodeType node_type)
    {
      _val = &v;
      if (auto inst = llvm::dyn_cast<llvm::Instruction>(&v)) // if v is an instruction, get the function that contains the instruction
        _func = inst->getFunction();
      else
        _func = nullptr;
      _node_type = node_type;
      _is_visited = false;
      _node_di_type = nullptr;
    }
    
    /**
     * 
     */
    Node(llvm::Value &v, GraphNodeType node_type, const llvm::DISubprogram *dis)
    {
      _val = &v;
      if (auto inst = llvm::dyn_cast<llvm::Instruction>(&v)) // if v is an instruction, get the function that contains the instruction
      {
        _func = inst->getFunction();
        _func_name = _func->getName().str();
      }
      else
        _func = nullptr;
      _node_type = node_type;
      _is_visited = false;
      _node_di_type = nullptr;
      if (dis)
      {
        std::string file_name = dis->getFilename().str();
        std::string directory = dis->getDirectory().str();
        std::string full_path = directory + "/" + file_name;
        std::filesystem::path file_path = std::filesystem::path(full_path).lexically_normal();
        _file_dir = file_path.string();
      }
    }

    /**
     * 
     */
    Node(llvm::Value &v, GraphNodeType node_type, const llvm::DebugLoc &loc, std::string opcode_name)
    {
      _val = &v;
      if (auto inst = llvm::dyn_cast<llvm::Instruction>(&v))
      {
        _func = inst->getFunction();
        _func_name = _func->getName().str();
      }
      else
        _func = nullptr;
      _node_type = node_type;
      _is_visited = false;
      _node_di_type = nullptr;
      if (loc)
      {
        _line_num = loc.getLine();
        _col_num = loc.getCol();
        if (auto *scope = llvm::dyn_cast<llvm::DIScope>(loc->getScope()))
        {
          std::string file_name = scope->getFilename().str();
          std::string directory = scope->getDirectory().str();
          std::string full_path = directory + "/" + file_name;
          std::filesystem::path file_path = std::filesystem::path(full_path).lexically_normal();
          _file_dir = file_path.string();
        }
      }
      if (opcode_name != "")
      {
        _opcode_name = opcode_name;
      }
    }

    void addInEdge(Edge &e) { _in_edge_set.insert(&e); }
    void addOutEdge(Edge &e) { _out_edge_set.insert(&e); }
    EdgeSet &getInEdgeSet() { return _in_edge_set; }
    EdgeSet &getOutEdgeSet() { return _out_edge_set; }
    void setNodeType(GraphNodeType node_type) { _node_type = node_type; }
    GraphNodeType getNodeType() const { return _node_type; }
    bool isVisited() { return _is_visited; }
    llvm::Function *getFunc() const { return _func; }
    void setFunc(llvm::Function &f) { _func = &f; }
    llvm::Value *getValue() { return _val; }
    llvm::DIType *getDIType() const { return _node_di_type; }
    void setDIType(llvm::DIType &di_type) { _node_di_type = &di_type; }
    void addNeighbor(Node &neighbor, EdgeType edge_type);
    EdgeSet::iterator begin() { return _out_edge_set.begin(); }
    EdgeSet::iterator end() { return _out_edge_set.end(); }
    EdgeSet::const_iterator begin() const { return _out_edge_set.begin(); }
    EdgeSet::const_iterator end() const { return _out_edge_set.end(); }
    std::set<Node *> getInNeighbors();
    std::set<Node *> getInNeighborsWithDepType(EdgeType edge_type);
    std::set<Node *> getOutNeighbors();
    std::set<Node *> getOutNeighborsWithDepType(EdgeType edge_type);
    bool hasInNeighborWithEdgeType(Node &n, EdgeType edge_type);
    bool hasOutNeighborWithEdgeType(Node &n, EdgeType edge_type);
    virtual ~Node() = default;

     //
    int getLineNum() { return _line_num; }
    int getColNum() { return _col_num; }
    std::string getFuncName() { return _func_name; }
    std::string getFileDir() { return _file_dir; }
    std::string getOpcodeName() { return _opcode_name; }

  protected:
    llvm::Value *_val; // corresponding llvm value
    llvm::Function *_func; // if _val is an instruction, _func is the function that contains the instruction
    bool _is_visited = 0;
    EdgeSet _in_edge_set;
    EdgeSet _out_edge_set;
    GraphNodeType _node_type;
    llvm::DIType *_node_di_type;
    //
    int _line_num = -1;
    int _col_num = -1;
    std::string _func_name = "";
    std::string _file_dir = "";
    std::string _opcode_name = "";
  };

  // used to iterate through all neighbors (used in dot pdg printer)
  template <typename NodeTy>
  class EdgeIterator : public std::iterator<std::input_iterator_tag, NodeTy>
  {
    typename Node::EdgeSet::iterator _edge_iter;
    typedef EdgeIterator<NodeTy> this_type;

  public:
    EdgeIterator(NodeTy *N) : _edge_iter(N->begin()) {}
    EdgeIterator(NodeTy *N, bool) : _edge_iter(N->end()) {}

    this_type &operator++()
    {
      _edge_iter++;
      return *this;
    }

    this_type operator++(int)
    {
      this_type old = *this;
      _edge_iter++;
      return old;
    }

    Node *operator*()
    {
      return (*_edge_iter)->getDstNode();
    }

    Node *operator->()
    {
      return operator*();
    }

    bool operator!=(const this_type &r) const
    {
      return _edge_iter != r._edge_iter;
    }

    bool operator==(const this_type &r) const
    {
      return !(operator!=(r));
    }

    EdgeType getEdgeType()
    {
      return (*_edge_iter)->getEdgeType();
    }
  };

} // namespace pdg
#endif