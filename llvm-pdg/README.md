# PDG Document

**Notice**: C++ PDG requires LLVM 13.0.0.

## Introduction

This project is a key component of our PtrSplit and Program-mandering works. It aims at building a modular inter-procedural program dependence graph (PDG) for practical use. Our program dependence graph is field senstive, context-insensitive and flow-insensitive. For more details, welcome to read our CCS'17 paper about PtrSplit: \[[http://www.cse.psu.edu/~gxt29/papers/ptrsplit.pdf\]](http://www.cse.psu.edu/~gxt29/papers/ptrsplit.pdf%5D) If you find this tool useful, please cite the PtrSplit paper in your publication. Here's the bibtex entry:

@inproceedings{LiuTJ17Ptrsplit,

author = {Shen Liu and Gang Tan and Trent Jaeger},

title = {{PtrSplit}: Supporting General Pointers in Automatic Program Partitioning},

booktitle = {24th ACM Conference on Computer and Communications Security ({CCS})},

pages = {2359--2371},

year = {2017}

}

Currently, the implmentation in master branch is based on LLVM 10.0.1. 

The current implementation only supports building PDGs for C programs.

For detailed description of PDG node and edge, please refer to **PDG_Specification.md**. 


## Getting Started
```
mkdir build
cd build
cmake ..
make
opt -load libpdg.so -dot-pdg < test.bc
```

### Available Passes

**\-pdg:** generate the program dependence graph (inter-procedural)

**\-cdg:** generate the control dependence graph (intra-procedural)

**\-ddg:** generate the data dependence graph (intra-procedural)

**\-dot-\*:** for visualization. (dot)

For those large software, generating a visualizable PDG is not easy. Graphviz often fails to generate the .dot file for a program with more than 1000 lines of C code. Fortunately, we rarely need such a large .dot file but only do kinds of analyses on the PDG, which is always in memory.

## LLVM IR compilation
For simple C programs(e.g., test.c), do

> clang -emit-llvm -S -g test.c -o test.bc

Now you have a binary format LLVM bitcode file which can be directly used as the input for PDG generation.

For those large C software (e.g., wget), you can refer to this great article for help:

[Compiling Autotooled projects to LLVM Bitcode](http://gbalats.github.io/2015/12/10/compiling-autotooled-projects-to-LLVM-bitcode.html)

(We successfully compiled SPECCPU 2006 INT/thttpd/wget/telnet/openssh/curl/nginx/sqlite, thanks to the author!)

## User Guide

We can use the current PDG as a required pass through following steps:

### Compile PDG

1. download PDG repo: git clone https://github.com/ARISTODE/program-dependence-graph.git
2. cd program-dependence-graph
3. make

### Use PDG as a required Pass
Using cmake, add 
```
include_directories(program_dependence_graph/include)
add_subdirectory(program_dependence_graph)
```

Then, add 
```
AU.addRequired<ProgramDependencyGraph>();
```
in your pass's **getAnalysisUsage** method (legacy pass manager).

### Useful APIs

**Query the reachability of two nodes:**

```
ProgramGraph *g = getAnalysis<ProgramDependencyGraph>()->getPDG();

Value* src;
Value* dst;

pdg::Node* src_node = g->getNode(*src);
pdg::Node* dst_node = g->getNode(*dst);

if (g->canReach(src_node, dst_node)) 
{
  // do something...
}

```


**Traverse the PDG with path constrains**
This method is useful to traverse the graph through certain edge types. In the example, we put the edge types we want to exclude in the set **exclude_edges**. Then, pass that as an argument to the **canReach** function.

```
ProgramGraph *g = getAnalysis<ProgramDependencyGraph>()->getPDG();

Value* src;
Value* dst;

pdg::Node* src_node = g->getNode(*src);
pdg::Node* dst_node = g->getNode(*dst);

std::set<pdg::EdgeType> exclude_edges;

if (g->canReach(src_node, dst_node, exclude_edges)) 
{
  // do something...
}
```

## C++ PDG Features
### List of Support Features
- Represent C++ class
- Represent the define relation between a class and its class method
- Template class / function
- Class hierarchy
- Control dependencies for exceptions
- Lambda functions

### Unsupported Features
- Smart Pointers

### Node Types and Edge Types
#### Node Types
```
INST_FUNCALL,
INST_RET,
INST_BR,
INST_OTHER,         // In the future, we may break out INST_INDBR and INST_SWITCH to capture the other types of branching instructions in LLVM

VAR_STATICALLOCGLOBALSCOPE,    // C: global variable          LLVM: global with common linkage
VAR_STATICALLOCMODULESCOPE,    // C: static global variable   LLVM: global with internal linkage 
VAR_STATICALLOCFUNCTIONSCOPE,  // C: static function variable LLVM: global with internal linkage, variable name is prefixed by function name  
VAR_OTHER           // In the future we may also call out VAR_STACK and VAR_HEAP

FUNCTIONENTRY,

PARAM_FORMALIN,   
PARAM_FORMALOUT, 
PARAM_ACTUALIN,  
PARAM_ACTUALOUT

ANNOTATION_VAR,   
ANNOTATION_GLOBAL, // PL propose splitting global annotation into variables and functions so we would have ANNOTATION_GLOBAL_FUNC and ANNOTATION_GLOBAL_VAR
ANNOTATION_OTHER, 

CLASS // represent a class defined in the source code
```

### Edge Types
```
 CONTROLDEP_CALLINV,
    CONTROLDEP_CALLRET,
    CONTROLDEP_EXCEPTIONRET,
    CONTROLDEP_ENTRY,
    CONTROLDEP_BR,
	CONTROLDEP_INVOKE_NORMAL,
	CONTROLDEP_INVOKE_EXCEPT, // used for invoke instruction abnormal jump
    CONTROLDEP_OTHER,

    DATADEP_DEFUSE,
    DATADEP_RAW,
    DATADEP_RET,
    DATADEP_ALIAS,

    PARAMETER_IN,
    PARAMETER_OUT,
    PARAMETER_FIELD,

    ANNO_GLOBAL, // PL propose splitting global annotation edges into variables and functions so we would have ANNO_GLOBAL_FUNC and ANNO_GLOBAL_VAR
    ANNO_VAR,
    ANNO_OTHER,
```

## Test Examples for Supported Features
### Steps for Visualizing PDG for Testing
```bash
cd test/c++_tests/
./build.sh # this script automatically generates llvm bc files for testing
cd bitcode # this folder contains all the test bc files
opt -load ../../build/libpdg.so -pdg -debug < test.bc # replace test.bc with the example name. e.g., testClassMethod.bc
```

### C++ Class
**Description**: PDG represents Class. Each class is 
represented as a class node in PDG. The node has node type **CLASS**.

**How**: PDG relies on debugging information to construct class node. 

*Steps*:
1. scans through all the debug instruction in the functions
2. extract debugging information which is attached to class definition 
3. extract class name from the debugging information, and create a class node with the class name

**Example**: testClassMethod.cpp

### Class Method
**Description**: PDG connects class node with corresponding class function entry nodes with edge **CLS_MTH**. 

**How**: In C++ program, class methods receive a default first pointer parameter named "this", which points to the corresponding class instance. PDG relies on this default named parameter to determine whether a function belongs to a class. Then it connects the class node with the function entry node of the identified class methods.

**Example**: testClassMethod.cpp

### Class Hierarchy
**Description**: PDG connects CLASS nodes that have inheritance relations. The parent class node connects child class nodes with **CLS_CHILD** edge. The child nodes connect the parent node with edge **CLS_INH**.

**How**: PDG identify class inheritance relations using type information. In LLVM, parent class types are appended to the beginning of child class types. PDG utilize this pattern to identify all the parent class types for a child class type. 

**Example**: 
1. testInheritanceRelations.cpp
2. testInheritedFields.cpp
3. testVirtualInheritance.cpp


### Template Class / Function
**Description**: Template class/function with different types are represented distinctly on LLVM IR. Thus, parameter trees will be constructed for each type.

**How**: PDG doesn't need any change to handle template class/function. Because definitions are created for each template class instance/function. And PDG considers these definitions when computing data/control dependencies.

**Example**: testTemplate.cpp

#### Exception Handling
**Description**: PDG provides basic querying for control dependency for exceptions. Currently, PDG only supports control dependencies for exceptions. However, the data dependencies (passing exception structures) are not represented.

**How**: To represent control dependencies in the presence of exceptions, two parts need to be handled:
1. Invoke instruction (invoke)
2. throw instruction (throw)

On PDG, we made the following changes:
1. For invoke instruction, all the catch blocks are considered control dependent on the correspond invoke instruction. The instructions in the catch blocks are connected with the invoke instruction with **CONTROLDEP_INVOKE_EXCEPT** edge.
2. The throw instruction (\_\_cxa\_throw) is connected with the invoke instruction in the caller with **CONTROLDEP_EXCEPTIONRET** edge.

**Example**:testException.cpp

#### Lambda Function
**Description**: PDG allows querying data/control dependencies with lambda functions.

**How**: LLVM provides function definition for each lambda function. When a lambda function is invoked, it behaves just like a normal function call. If lambda function captures local variables, the definition will treat these local variables as default parameters. For these reasons, PDG can build parameter trees for the lambda function's call site and definition, and connect them in the same way as normal functions.

**Example**: testLambda.cpp
