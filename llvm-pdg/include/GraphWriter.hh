#ifndef GRAPHWRITER_H_
#define GRAPHWRITER_H_
#include "llvm/ADT/GraphTraits.h"
#include "llvm/Analysis/DOTGraphTraitsPass.h"
#include "GraphTraits.hh"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/GraphWriter.h"

namespace llvm
{
  template <>
  struct DOTGraphTraits<pdg::Node *> : public DefaultDOTGraphTraits
  {
    DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}
  };

  template <>
  struct DOTGraphTraits<pdg::ProgramDependencyGraph *> : public DefaultDOTGraphTraits
  {
    DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

    // Return graph name;
    static std::string getGraphName(pdg::ProgramDependencyGraph *)
    {
      return "Program Dependency Graph";
    }

    static std::string getNodeAttributes(pdg::Node *node, pdg::ProgramDependencyGraph *G)
    {
      std::vector <std::string> nodeAttrs;
      std::string nodeAttr = "";
      if (node->getOpcodeName() != "")
      {
        nodeAttrs.push_back("opcode=\"" + node->getOpcodeName() + "\"");
      }
      if (node->getLineNum() != -1)
      {
        nodeAttrs.push_back("line=\"" + std::to_string(node->getLineNum()) + "\"");
      }
      if (node->getColNum() != -1)
      {
        nodeAttrs.push_back("col=\"" + std::to_string(node->getColNum()) + "\"");
      }
      if (node->getFuncName() != "")
      {
        nodeAttrs.push_back("func=\"" + node->getFuncName() + "\"");
      }
      if (node->getFileDir() != "")
      {
        nodeAttrs.push_back("file=\"" + node->getFileDir() + "\"");
      }
      for (int i = 0; i < nodeAttrs.size(); i++)
      {
        nodeAttr += nodeAttrs[i];
        if (i != nodeAttrs.size() - 1)
        {
          nodeAttr += ", ";
        }
      }
      return nodeAttr;
    }

    std::string getCDGNodeLabel(pdg::Node *node)
    {
      pdg::GraphNodeType node_type = node->getNodeType();
      Function *func = node->getFunc();
      Value *node_val = node->getValue();
      std::string str;
      raw_string_ostream OS(str);
      return "";
    }

    std::string getDDGNodeLabel(pdg::Node *node)
    {
      pdg::GraphNodeType node_type = node->getNodeType();
      Function *func = node->getFunc();
      Value *node_val = node->getValue();
      std::string str;
      raw_string_ostream OS(str);

      switch (node_type)
      {
      case pdg::GraphNodeType::FUNC_ENTRY:
        return "<<ENTRY>> " + func->getName().str();
      case pdg::GraphNodeType::INST_OTHER:
      {
        if (Instruction *i = dyn_cast<Instruction>(node_val))
        {
          OS << *i;
          return OS.str(); // print the instruction literal
        }
      }
      case pdg::GraphNodeType::INST_FUNCALL:
      {
        if (Instruction *i = dyn_cast<Instruction>(node_val))
        {
          OS << *i;
          return OS.str(); // print the instruction literal
        }
      }
      case pdg::GraphNodeType::INST_RET:
      {
        if (Instruction *i = dyn_cast<Instruction>(node_val))
        {
          OS << *i;
          return OS.str(); // print the instruction literal
        }
      }
      case pdg::GraphNodeType::INST_BR:
      {
        if (Instruction *i = dyn_cast<Instruction>(node_val))
        {
          OS << *i;
          return OS.str(); // print the instruction literal
        }
      }
      case pdg::GraphNodeType::ANNO_VAR:
      {
        OS << "Local Anno: " << node_val->getName().str() << " type: " << *node_val->getType();
        return OS.str();
      }
      case pdg::GraphNodeType::ANNO_GLOBAL:
      {
        OS << "Global Anno: " << node_val->getName().str() << " type: " << *node_val->getType();
        return OS.str();
      }
      case pdg::GraphNodeType::VAR_STATICALLOCGLOBALSCOPE:
      {
        OS << "global var: " << node_val->getName().str() << " type: " << *node_val->getType();
        return OS.str();
      }
      case pdg::GraphNodeType::VAR_STATICALLOCMODULESCOPE:
      {
        OS << "static global var: " << node_val->getName().str() << " type: " << *node_val->getType();
        return OS.str();
      }
      case pdg::GraphNodeType::VAR_STATICALLOCFUNCTIONSCOPE:
      {
        OS << "static func var: " << node_val->getName().str() << " type: " << *node_val->getType();
        return OS.str();
      }
      default:
        break;
      }

      return "style=invis";
    }

    std::string getNodeLabel(pdg::Node *node, pdg::ProgramDependencyGraph *G)
    {
      if (pdg::DOTONLYDDG)
        return getDDGNodeLabel(node);
      if (pdg::DOTONLYCDG)
        return getCDGNodeLabel(node);

      pdg::GraphNodeType node_type = node->getNodeType();
      Function *func = node->getFunc();
      Value *node_val = node->getValue();
      std::string str;
      raw_string_ostream OS(str);

      switch (node_type)
      {
      case pdg::GraphNodeType::FUNC_ENTRY:
        return "<<ENTRY>> " + func->getName().str();
      case pdg::GraphNodeType::PARAM_FORMALIN:
      {
        pdg::pdgutils::printTreeNodesLabel(node, OS, "FORMAL_IN");
        return OS.str();
      }
      case pdg::GraphNodeType::PARAM_FORMALOUT:
      {
        pdg::pdgutils::printTreeNodesLabel(node, OS, "FORMAL_OUT");
        return OS.str();
      }
      case pdg::GraphNodeType::PARAM_ACTUALIN:
      {
        pdg::pdgutils::printTreeNodesLabel(node, OS, "ACTUAL_IN");
        return OS.str();
      }
      case pdg::GraphNodeType::PARAM_ACTUALOUT:
      {
        pdg::pdgutils::printTreeNodesLabel(node, OS, "ACTUAL_OUT");
        return OS.str();
      }
      case pdg::GraphNodeType::INST_OTHER:
      {
        if (Instruction *i = dyn_cast<Instruction>(node_val))
        {
          OS << *i;
          return OS.str(); // print the instruction literal
        }
      }
      case pdg::GraphNodeType::INST_FUNCALL:
      {
        if (Instruction *i = dyn_cast<Instruction>(node_val))
        {
          OS << *i;
          return OS.str(); // print the instruction literal
        }
      }
      case pdg::GraphNodeType::INST_RET:
      {
        if (Instruction *i = dyn_cast<Instruction>(node_val))
        {
          OS << *i;
          return OS.str(); // print the instruction literal
        }
      }
      case pdg::GraphNodeType::INST_BR:
      {
        if (Instruction *i = dyn_cast<Instruction>(node_val))
        {
          OS << *i;
          return OS.str(); // print the instruction literal
        }
      }
      case pdg::GraphNodeType::ANNO_VAR:
      {
        OS << "Local Anno: " << node_val->getName().str() << " type: " << *node_val->getType();
        return OS.str();
      }
      case pdg::GraphNodeType::ANNO_GLOBAL:
      {
        OS << "Global Anno: " << node_val->getName().str() << " type: " << *node_val->getType();
        return OS.str();
      }
      case pdg::GraphNodeType::VAR_STATICALLOCGLOBALSCOPE:
      {
        OS << "global var: " << node_val->getName().str() << " type: " << *node_val->getType();
        return OS.str();
      }
      case pdg::GraphNodeType::VAR_STATICALLOCMODULESCOPE:
      {
        OS << "static global var: " << node_val->getName().str() << " type: " << *node_val->getType();
        return OS.str();
      }
      case pdg::GraphNodeType::VAR_STATICALLOCFUNCTIONSCOPE:
      {
        OS << "static func var: " << node_val->getName().str() << " type: " << *node_val->getType();
        return OS.str();
      }
      default:
        break;
      }
      return "";
    }

    std::string getDDGEdgeAttributes(pdg::Node::iterator edge_iter)
    {
      pdg::EdgeType edge_type = edge_iter.getEdgeType();
      switch (edge_type)
      {
      case pdg::EdgeType::DATA_DEF_USE:
        return "style=dotted,label = \"{D_DEF_USE}\" ";
      case pdg::EdgeType::DATA_ALIAS:
        return "style=dotted,label = \"{D_ALIAS}\" ";
      case pdg::EdgeType::DATA_RAW:
        return "style=dotted,label = \"{D_RAW}\" ";
      case pdg::EdgeType::DATA_RET:
        return "style=dashed, color=\"red\", label =\"{D_RET}\"";
      case pdg::EdgeType::ANNO_GLOBAL:
        return "style=dashed, color=\"green\", label =\"{ANNO_GLOB}\"";
      case pdg::EdgeType::ANNO_VAR:
        return "style=dashed, color=\"green\", label =\"{ANNO_VAR}\"";
      default:
        break;
      }
      return "style=invis";
    }

    std::string getEdgeAttributes(pdg::Node *Node, pdg::Node::iterator edge_iter, pdg::ProgramDependencyGraph *PDG)
    {
      if (pdg::DOTONLYDDG)
        return getDDGEdgeAttributes(edge_iter);
      pdg::EdgeType edge_type = edge_iter.getEdgeType();
      switch (edge_type)
      {
      case pdg::EdgeType::CONTROLDEP_ENTRY:
        return "label = \"{CONTROLDEP_ENTRY}\"";
      case pdg::EdgeType::CONTROLDEP_BR:
        return "label = \"{CONTROLDEP_BR}\"";
      case pdg::EdgeType::CONTROLDEP_CALLINV:
        return "label = \"{CONTROLDEP_CALLINV}\"";
      case pdg::EdgeType::CONTROLDEP_CALLRET:
        return "label = \"{CONTROLDEP_CALLRET}\"";
      case pdg::EdgeType::CONTROLDEP_IND_BR:
        return "label = \"{CONTROLDEP_IND_BR}\"";
      case pdg::EdgeType::CONTROLDEP_INVOKE_NORMAL:
        return "label = \"{CONTROLDEP_INK_NOR}\"";
      case pdg::EdgeType::CONTROLDEP_INVOKE_EXCEPT:
        return "label = \"{CONTROLDEP_INK_EX}\"";
      case pdg::EdgeType::DATA_DEF_USE:
        return "style=dotted,label = \"{D_DEF_USE}\" ";
      case pdg::EdgeType::DATA_ALIAS:
        return "style=dotted,label = \"{D_ALIAS}\" ";
      case pdg::EdgeType::PARAMETER_IN:
        return "style=dashed, color=\"blue\", label=\"{P_IN}\"";
      case pdg::EdgeType::PARAMETER_OUT:
        return "style=dashed, color=\"blue\", label=\"{P_OUT}\"";
      case pdg::EdgeType::PARAMETER_FIELD:
        return "style=dashed, color=\"blue\", label=\"{P_F}\"";
      case pdg::EdgeType::DATA_RAW:
        return "style=dotted,label = \"{D_RAW}\" ";
      case pdg::EdgeType::DATA_RET:
        return "style=dashed, color=\"red\", label =\"{D_RET}\"";
      case pdg::EdgeType::ANNO_GLOBAL:
        return "style=dashed, color=\"green\", label =\"{ANNO_GLOB}\"";
      case pdg::EdgeType::ANNO_VAR:
        return "style=dashed, color=\"green\", label =\"{ANNO_VAR}\"";
      case pdg::EdgeType::CLS_MTH:
        return "style=dashed, color=\"red\", label =\"{CLS_MTH}\"";
      case pdg::EdgeType::CLS_INH:
        return "style=dashed, color=\"red\", label =\"{CLS_INH}\"";
      default:
        break;
      }
      return "";
    }
  };
} // namespace llvm


#endif