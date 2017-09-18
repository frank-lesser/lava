
// -todo: try single file libc
// -todo: make lava thread local allocation

// todo: make FileToString output a test tbl

//#include <cstdio>
#include "../no_rt_util.h"
#include "../LavaFlow.hpp"

//#define LIBC_HPP_IMPL
//#include "../libc.hpp"

extern "C"
{
  const char* FileToStringInNames[]   = {"File Path",    nullptr};
  const char* FileToStringOutNames[]  = {"File String",  nullptr};
  const char* FileToStringInTypes[]   = {"tbl_str",      nullptr};
  const char* FileToStringOutTypes[]  = {"tbl_str",      nullptr};

  uint64_t FileToString(LavaParams* inout_lp, LavaVal* in, LavaOut* out)
  {
    using namespace std;

    printf("\n File To String called \n");

    // output temp string here
    const char* tmp = "Temp out str";
    auto        len = strlen(tmp) + 1;
    void*       mem = inout_lp->mem_alloc(len);
    memcpy_s(mem, len, tmp, len);
    inout_lp->outputs = 1;

    this_thread::sleep_for( chrono::milliseconds(500) );
    return 85;
  }

  LavaNode LavaNodes[] =
  {
    {
      FileToString,                                  // function
      (uint64_t)LavaNode::FLOW,                      // node_type  
      "FileToString",                                // name
      FileToStringInNames,                           // in_names
      FileToStringOutNames,                          // out_names
      FileToStringInTypes,                           // in_types 
      FileToStringOutTypes,                          // out_types 
      0,                                             // version 
      0                                              // id
    },                                             

    {nullptr, (uint64_t)LavaNode::NONE, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0}
  };

  __declspec(dllexport) LavaNode* GetLavaNodes()
  {
    return (LavaNode*)LavaNodes;
  }

}



//
//{nullptr, (uint64_t)LavaNode::NONE, 0,0, nullptr, nullptr,nullptr, 0, 0}

//
//#include "../LavaNode.h"

//
//nullptr, (char**)({"tbl_str"},             // in_types, out_types 

//{ReadFile, "ReadFile",                     // function, name
//1, ReadFileIn, 1, ReadFileOut,             // inputs, in_types, outputs, out_types 
//0, 0},                                     // version, id

//__declspec(dllexport) LavaFlowNode* GetNodes()
//{
//  return (LavaFlowNode*)nodes;
//

//LavaArg* ReadFile(LavaArg* in)
//{
//  TO(in->len,i){
//    printf(" %l \n", in->idx);
//  }
//
//  return nullptr;
//}
//
//const char* ReadFileIn[]  = {"string"};
//const char* ReadFileOut[] = {"string"};
//
//LavaNode nodes[] =
//{
//  {ReadFile, "ReadFile",                     // function, name
//  1, ReadFileIn, 1, ReadFileOut,             // inputs, in_types, outputs, out_types 
//  0, 0},                                     // version, id
//
//  {nullptr, nullptr, 0, nullptr, 0, nullptr, 0, 0}
//};
//
//__declspec(dllexport) LavaNode* GetNodes()
//{
//  return (LavaNode*)nodes;
//}
//

//auto  cs     =  (CS*)in;
//auto& clrs   =  cs->colors();
//auto& smpls  =  cs->samples();
//auto  sz     =  cs->size();
////auto  sz     =  smpls.w;
//auto  iv     =  (IndexedVerts*)IndexedVertsCreate(0, 6, IV_POINTS, sz, sz, 1, 1, 4);
//
//float* clrPtr  =  cs->colors().p;
//auto    w      =  cs->colors().w;
//auto    h      =  cs->colors().h;
////assert(clrPtr != nullptr);
////assert(w < 10000);
////assert(h < 10000);
//TO(sz,i)
//{
//  Vertex& v = iv->verts[i];
//  //TO(3,c) v.colour[c]  =  clrPtr[i*w + 0];
//
//  TO(3,c) v.colour[c]  =  clrs(c,i);
//  ////TO(3,c) v.colour[c]  =  clrs(c,i) * 8.f;
//  v.colour[3]          =  1.f;
//  
//  v.texCoord[0] = v.position[0] = smpls(i,0);
//  v.texCoord[1] = v.position[1] = 1.f - smpls(i,1);
//  v.position[2] = 0.f;
//  
//  iv->indices[i]       =  i;
//}
//
//memset(iv->pixels, 0, 4*sizeof(float));
//
//return iv;


