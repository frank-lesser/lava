
// -todo: make sizeBytes() static function
// -todo: make swapIdxPair function
// -todo: copy in CncrBlkLst from simdb

// todo: take out version from CncrLst
// todo: transition CncrLst to be without any stack variables - just use the functions and pass them addresses?
// todo: make default constructor
// todo: make constructor that takes a capacity
// todo: make allocation function
// todo: fill out BlkMeta struct
// todo: make put()
// todo: make operator[]
// todo: make operator()
// todo: make del()

#pragma once

#if !defined(__FLAT_LOCKFREE_MAP_HEADER_GUARD_HPP__)

#include <cstdint>
#include <atomic>

struct flf_map
{
  class     CncrLst
  {
    // Copied from simdb 2017.10.25
    // Internally this is an array of indices that makes a linked list
    // Externally indices can be gotten atomically and given back atomically
    // | This is used to get free indices one at a time, and give back in-use indices one at a time
    // Uses the first 8 bytes that would normally store sizeBytes as the 64 bits of memory for the Head structure
    // Aligns the head on a 64 bytes boundary with the rest of the memory on a separate 64 byte boudary. This puts them on separate cache lines which should eliminate false sharing between cores when atomicallyaccessing the Head union (which will happen quite a bit) 
  public:
    //using     u32  =  uint32_t;
    //using     u64  =  uint64_t;
    //using    au64  =  std::atomic<u64>;
    using ListVec  =  lava_vec<u32>;

    //union Head
    //{
    //  struct { u32 ver; u32 idx; };                           // ver is version, idx is index
    //  u64 asInt;
    //};
    union Head
    {
      u8  dblCachePad[128];
      u32         idx;                                         // idx is index
    };


    static const u32        LIST_END = 0xFFFFFFFF;
    static const u32 NXT_VER_SPECIAL = 0xFFFFFFFF;

  private:
    //ListVec     s_lv;
    //au64*        s_h;

  public:
    //static u64   sizeBytes(u32 size) { return ListVec::sizeBytes(size) + 128; }         // an extra 128 bytes so that Head can be placed
    static u64   sizeBytes(u32 size) { return sizeof(u32) + 128; }         // an extra 128 bytes so that Head can be placed
    static u32  incVersion(u32    v) { return v==NXT_VER_SPECIAL?  1  :  v+1; }

    CncrLst(){}
    CncrLst(void* addr, void* headAddr, u32 size, bool owner=true)             // this constructor is for when the memory is owned an needs to be initialized
    {                                                                          // separate out initialization and let it be done explicitly in the simdb constructor?    
      u64    addrRem  =  (u64)headAddr % 64;
      u64  alignAddr  =  (u64)headAddr + (64-addrRem);
      assert( alignAddr % 64 == 0 );
      au64*       hd  =  (au64*)alignAddr;
      u32*   lstAddr  =  (u32*)((u64)alignAddr+64);
      //new (&s_lv) ListVec(listAddr, size, owner);
      
      // give the list initial values here
      //u32* lstAddr = 
      //if(owner){
      for(u32 i=0; i<(size-1); ++i){ lstAddr[i] = i+1; }
      lstAddr[size-1] = LIST_END;

      //((Head*)s_h)->idx = 0;
      //((Head*)s_h)->ver = 0;
      //}
    }

    u32         nxt()                                                             // moves forward in the list and return the previous index
    {
      Head  curHead, nxtHead;
      curHead.asInt  =  s_h->load();
      do{
        if(curHead.idx==LIST_END){return LIST_END;}

        nxtHead.idx  =  s_lv[curHead.idx];
        nxtHead.ver  =  curHead.ver==NXT_VER_SPECIAL? 1  :  curHead.ver+1;
      }while( !s_h->compare_exchange_strong(curHead.asInt, nxtHead.asInt) );

      return curHead.idx;
    }
    u32        free(u32 idx)                                                    // not thread safe when reading from the list, but it doesn't matter because you shouldn't be reading while freeing anyway, since the CncrHsh will already have the index taken out and the free will only be triggered after the last reader has read from it 
    {
      Head curHead, nxtHead; u32 retIdx;
      curHead.asInt = s_h->load();
      do{
        retIdx = s_lv[idx] = curHead.idx;
        nxtHead.idx  =  idx;
        nxtHead.ver  =  curHead.ver + 1;
      }while( !s_h->compare_exchange_strong(curHead.asInt, nxtHead.asInt) );

      return retIdx;
    }
    u32        free(u32 st, u32 en)                                            // not thread safe when reading from the list, but it doesn't matter because you shouldn't be reading while freeing anyway, since the CncrHsh will already have the index taken out and the free will only be triggered after the last reader has read from it 
    {
      Head curHead, nxtHead; u32 retIdx;
      curHead.asInt = s_h->load();
      do{
        retIdx = s_lv[en] = curHead.idx;
        nxtHead.idx  =  st;
        nxtHead.ver  =  curHead.ver + 1;
      }while( !s_h->compare_exchange_strong(curHead.asInt, nxtHead.asInt) );

      return retIdx;
    }
    auto      count() const -> u32 { return ((Head*)s_h)->ver; }
    auto        idx() const -> u32
    {
      Head h; 
      h.asInt = s_h->load();
      return h.idx;
    }
    auto       list() -> ListVec const* { return &s_lv; }                      // not thread safe
    u32      lnkCnt()                                                          // not thread safe
    {
      u32    cnt = 0;
      u32 curIdx = idx();
      while( curIdx != LIST_END ){
        curIdx = s_lv[curIdx];
        ++cnt;
      }
      return cnt;
    }
    auto       head() -> Head* { return (Head*)s_h; }
  };

  using   u8    =    uint8_t;
  using  u32    =   uint32_t;
  using  u64    =   uint64_t;
  using au32    =   std::atomic<uint32_t>;
  using au64    =   std::atomic<uint64_t>;
  using Key     =   u64;
  using Value   =   u64;
  using Hash    =   std::hash<Key>;
  using LstIdx  =   u32;

  static const u32           EMPTY  =  0x00FFFFFF;              // max value of 2^24 to set all 24 bits of the value index
  static const u32         DELETED  =  0x00FFFFFE;              // one less than the max value above
  static const u32 SPECIAL_VALUE_START  =  DELETED;             // comparing to this is more clear than comparing to DELETED
  static const u32        LIST_END  =  0xFFFFFFFF;
  //static const u32 NXT_VER_SPECIAL  =  0xFFFFFFFF;

  struct       Idx {
    u32 readers :  8;
    u32 val_idx : 24;
  };
  union    IdxPair {
    struct { Idx first; Idx second; };
    u64 asInt;
  };
  struct    Header {
    // first 8 bytes - two 1 bytes characters that should equal 'lm' for lockless map
    u64     typeChar1  :  8;
    u64     typeChar2  :  8;
    u64     sizeBytes  : 48;
    
    // next 8 bytes keep track of the size of the values and number inserted - from that and the sizeBytes, capacity can be inferred
    u64          size :  32;      // this could be only 24 bits since that is the max index
    u64  valSizeBytes :  32;

    // next 4 bytes is the current block list
    u32        curBlk;
  };
  struct   BlkMeta {
  };

  u8*     m_mem = nullptr;  // single pointer is all that ends up on the stack

  void   make_list(void* addr, u32* head, u32 size)               // this constructor is for when the memory is owned an needs to be initialized
  {                                                             // separate out initialization and let it be done explicitly in the simdb constructor?    
    u32*  lstAddr  =  (u32*)addr;
    for(u32 i=0; i<(size-1); ++i){ lstAddr[i] = i+1; }
    lstAddr[size-1] = LIST_END;
  }
  u32          nxt(au32* head, u32* lst)                                      // moves forward in the list and return the previous index
  {
    u32  curHead, nxtHead;
    curHead = head->load();
    do{
      if(curHead==LIST_END){return LIST_END;}
      nxtHead = lst[curHead];
    }while( !head->compare_exchange_strong(curHead, nxtHead) );

    return curHead;
  }
  u32         free(au32* head, u32* lst, u32 idx)                             // not thread safe when reading from the list, but it doesn't matter because you shouldn't be reading while freeing anyway, since the CncrHsh will already have the index taken out and the free will only be triggered after the last reader has read from it 
  {
    u32 curHead, nxtHead, retIdx;
    curHead = head->load();
    do{
      retIdx  = lst[idx] = curHead;
      nxtHead = idx;
    }while( !head->compare_exchange_strong(curHead, nxtHead) );

    return retIdx;
  }
  u32         free(au32* head, u32* lst, u32 st, u32 en)                                            // not thread safe when reading from the list, but it doesn't matter because you shouldn't be reading while freeing anyway, since the CncrHsh will already have the index taken out and the free will only be triggered after the last reader has read from it 
  { // todo: possibly take this out, there might not be an oportunity to free a linked list of indices instead of a single index 
    u32 curHead, nxtHead, retIdx;
    curHead = head->load();
    do{
      retIdx  = lst[en] = curHead;
      nxtHead = st;
    }while( !head->compare_exchange_strong(curHead, nxtHead) );

    return retIdx;
  }
  //u32        count(au32* head) const { return ((Header*)head)->cap; }

  u64   slotByteOffset(u64 idx){ return sizeof(Header) + idx*sizeof(IdxPair); }
  Idx*         slotPtr(u64 idx){ return (Idx*)(m_mem + slotByteOffset(idx)); }
  bool      incReaders(void* oldIp, IdxPair*  newIp = nullptr) 
  {
    au64*   atmIncPtr  =  (au64*)(oldIp);

    Idx idxs[2];
    u64 oldVal, newVal;
    do{
      oldVal          = atmIncPtr->load(std::memory_order::memory_order_seq_cst);        // get the value of both Idx structs atomically
      *((u64*)(idxs)) = oldVal;   // default memory order for now
 
      if(idxs[0].val_idx < SPECIAL_VALUE_START &&
         idxs[1].val_idx < SPECIAL_VALUE_START ){
        idxs[0].readers  +=  1;        // increment the reader values if neithe of the indices have special values like EMPTY or DELETED
        idxs[1].readers  +=  1;
      }else{
        return false;
      }

      newVal = *((u64*)(idxs));
    }while( atmIncPtr->compare_exchange_strong(oldVal, newVal) );      // store it back if the pair of indices hasn't changed - this is not an ABA problem because we aren't relying on the values at these indices yet, we are just incrementing the readers so that 1. the data is not deleted at these indices and 2. the indices themselves can't be reused until we decrement the readers

    if(newIp) newIp->asInt = newVal;
    return true; // the readers were successfully incremented, but we need to return the indices that were swapped since we read them atomically
  }
  bool     swapIdxPair(IdxPair* ip, IdxPair* prevIp = nullptr)
  {
    using namespace std;
    
    au64*     aip  =  (au64*)ip;
    IdxPair oldIp;
    oldIp.asInt    =  aip->load();
    
    IdxPair nxtIp;
    nxtIp.first  = oldIp.second;
    nxtIp.second = oldIp.first;

    bool ok = aip->compare_exchange_strong( oldIp.asInt, nxtIp.asInt, std::memory_order_seq_cst);

    if(prevIp){ *prevIp = oldIp; }
    return ok;
  }
  u64          hashKey(Key const& k)
  {
    return Hash()(k);  // instances a hash function object and calls operator()
  }


  static u64 sizeBytes(u64 capacity)
  {
    u64 szPerCap = sizeof(BlkMeta) + sizeof(Idx) + sizeof(Key) + sizeof(Value) + sizeof(LstIdx);
    return sizeof(Header) + capacity*szPerCap;
  }
};

#endif








//
//nxtHead.ver  =  curHead.ver + 1;

//au32* head = 
//nxtHead.ver  =  curHead.ver==NXT_VER_SPECIAL? 1  :  curHead.ver+1;

//nxtIp.asInt    =  aip->load();
//Idx      tmp = nxtIp.first;

//bool incTwoIdxReaders(u64 idx, IdxPair* newIp = nullptr)
//bool incIdxPairReaders(u64 idx, IdxPair* newIp = nullptr)
//
// u8*   firstIdxPtr  =  
// Idx*       idxPtr  =  (Idx*)(m_mem + idx*sizeof(Idx));     // it is crucial and fundamental that sizeof(Idx) needs to be 4 bytes so that two of them can be swapped even if unaligned, but this should be more correct and clear
//au64*   atmIncPtr  =  (au64*)(m_mem + idx*sizeof(Idx));       // it is crucial and fundamental that sizeof(Idx) needs to be 4 bytes so that two of them can be swapped even if unaligned, but this should be more correct and clear

//IdxPair ip;
//ip.asInt = newVal;

