#ifndef PDGEDGE_H_
#define PDGEDGE_H_
#include "PDGNode.hh"
#include "PDGEnums.hh"

namespace pdg
{
  class Node;
  class Edge
  {
  private:
    EdgeType _edge_type;
    Node *_source;
    Node *_dst;

  public:
    Edge() = delete;
    Edge(Node *source, Node *dst, EdgeType edge_type)
    {
      _source = source;
      _dst = dst;
      _edge_type = edge_type;
    }
    Edge(const Edge &e) // copy constructor
    {
      _source = e.getSrcNode();
      _dst = e.getDstNode();
      _edge_type = e.getEdgeType();
    }

    EdgeType getEdgeType() const { return _edge_type; }
    Node *getSrcNode() const { return _source; }
    Node *getDstNode() const { return _dst; }
    bool operator<(const Edge &e) const
    {
      return (_source == e.getSrcNode() && _dst == e.getDstNode() && _edge_type == e.getEdgeType());
    }

    // 
    std::string edgeTypeToString() const
    {
      switch(_edge_type)
      {
        case EdgeType::IND_CALL:
          return "IND_CALL";
        case EdgeType::CONTROLDEP_CALLINV:
          return "CONTROLDEP_CALLINV";
        case EdgeType::CONTROLDEP_CALLRET:
          return "CONTROLDEP_CALLRET";
        case EdgeType::CONTROLDEP_ENTRY: 
          return "CONTROLDEP_ENTRY";
        case EdgeType::CONTROLDEP_BR:
          return "CONTROLDEP_BR";
        case EdgeType::CONTROLDEP_IND_BR:
          return "CONTROLDEP_IND_BR";
        case EdgeType::CONTROLDEP_INVOKE_NORMAL:
          return "CONTROLDEP_INVOKE_NORMAL";
        case EdgeType::CONTROLDEP_INVOKE_EXCEPT:
          return "CONTROLDEP_INVOKE_EXCEPT";
        case EdgeType::DATA_DEF_USE:
          return "DATA_DEF_USE";
        case EdgeType::DATA_RAW:
          return "DATA_RAW";
        case EdgeType::DATA_READ: 
          return "DATA_READ";
        case EdgeType::DATA_ALIAS:  
          return "DATA_ALIAS";
        case EdgeType::DATA_RET:  
          return "DATA_RET";
        case EdgeType::PARAMETER_IN:
          return "PARAMETER_IN";
        case EdgeType::PARAMETER_OUT:
          return "PARAMETER_OUT";
        case EdgeType::PARAMETER_FIELD:
          return "PARAMETER_FIELD";
        case EdgeType::GLOBAL_DEP:
          return "GLOBAL_DEP";
        case EdgeType::VAL_DEP:
          return "VAL_DEP";
        case EdgeType::CLS_MTH:
          return "CLS_MTH";
        case EdgeType::CLS_INH:
          return "CLS_INH"; 
        case EdgeType::CLS_CHILD: 
          return "CLS_CHILD"; 
        case EdgeType::ANNO_VAR:  
          return "ANNO_VAR";  
        case EdgeType::ANNO_GLOBAL: 
          return "ANNO_GLOBAL"; 
        case EdgeType::ANNO_OTHER:  
          return "ANNO_OTHER";  
        case EdgeType::TYPE_OTHEREDGE:  
          return "TYPE_OTHEREDGE";  
        default:
          return "UNKNOWN"; 
      }
    }
  };

} // namespace Edge

#endif