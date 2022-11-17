#include "ObjectAllocator.h"
#include <string.h>
#include <cstring>
  
/*****************************************************************************/
/*!
  \brief 
    Creates the ObjectManager per the specified values
    Throws an exception if the construction fails. 
    (Memory allocation problem)

  \param ObjectSize
    Size of the objects that are in the blocks

  \param config
    the configuration of the blocks
*/
/*****************************************************************************/
ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config)
{
  clientConfig = config;
  stats.ObjectSize_ = ObjectSize;
  stats.PageSize_ = calculate_page_size();
  PageList_ = nullptr;
  FreeList_ = nullptr;
  if (config.UseCPPMemManager_ == false)
  {
    allocate_new_page();
  }
}

/*****************************************************************************/
/*!
  \brief
    Destroys the ObjectManager (never throws)
*/
/*****************************************************************************/
ObjectAllocator::~ObjectAllocator()
{
  while (PageList_)
  {
    GenericObject *toDelete = PageList_;
    PageList_ = PageList_->Next;
    delete[] toDelete;
  }
}

/*****************************************************************************/
/*!
  \brief
    Take an object from the free list and give it to the client (simulates new)
    Throws an exception if the object can't be allocated. 
    (Memory allocation problem)

  \param label
    Label that could be stored in a header

  \return 
    a pointer to the data allocated
*/
/*****************************************************************************/
void *ObjectAllocator::Allocate(const char *label)
{
  if (clientConfig.UseCPPMemManager_ == false)
  {
    if (!FreeList_)
    {
      allocate_new_page();
    }

    return take_off_freelist(label);
  }
  
  else
  {
    stats.Allocations_++;
    stats.MostObjects_++;
    try
    {
      return new char[stats.ObjectSize_];
    }
    catch (std::bad_alloc)
    {
      throw OAException(OAException::E_NO_MEMORY, "No Memory");
    }
  }
}

  
/*****************************************************************************/
/*!
  \brief
    Returns an object to the free list for the client (simulates delete)
    Throws an exception if the the object can't be freed. (Invalid object)

  \param Object 
    Indicates which object to free
*/
/*****************************************************************************/
void ObjectAllocator::Free(void *Object)
{
  if (clientConfig.UseCPPMemManager_ == false)
  {
    put_on_freelist(Object);
  }
  else
  {
    stats.Deallocations_++;
    delete[] reinterpret_cast<char *>(Object);
  }
}

/*****************************************************************************/
/*!
  \brief
    Calls the callback fn for each block still in use

  \param fn
    function to dump the data in use

  \return 
    amount of objects in use
*/
/*****************************************************************************/
unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const
{
  GenericObject *pageListCopy = (PageList_);

  int numObjects = 0;
  
  while (pageListCopy)
  {
    char *freeListCopy = create_offset(reinterpret_cast<char *>(pageListCopy));

    for (unsigned i = 0; i < clientConfig.ObjectsPerPage_; i++)
    {
      if (clientConfig.HBlockInfo_.size_)
      {
        if (check_leak_in_header(freeListCopy))
        {
          fn(freeListCopy + clientConfig.HBlockInfo_.size_, stats.ObjectSize_);
          numObjects++;
        }
      }
      else
      {

      }
      freeListCopy += calculate_block_size();
    }

    pageListCopy = pageListCopy->Next;
  }
  

  return numObjects;
}

/*****************************************************************************/
/*!
  \brief
    Calls the callback fn for each block that is potentially corrupted

  \param fn
    dumps the objects with corrupted memory

  \return
    amount of objects corrupted
*/
/*****************************************************************************/
unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const
{
  GenericObject *pageListCopy = (PageList_);

  int numObjects = 0;

  while (pageListCopy)
  {
    char *freeListCopy = create_offset(reinterpret_cast<char *>(pageListCopy))
                   + (clientConfig.HBlockInfo_.size_ + clientConfig.PadBytes_);

    for (unsigned i = 0; i < clientConfig.ObjectsPerPage_; i++)
    {
      if (clientConfig.HBlockInfo_.size_)
      {
        if (check_corruption(freeListCopy))
        {
          fn(freeListCopy + clientConfig.HBlockInfo_.size_, stats.ObjectSize_);
          numObjects++;
        }
      }
      else
      {

      }
      freeListCopy += calculate_block_size();
    }

    pageListCopy = pageListCopy->Next;
  }


  return numObjects;
}

  // Frees all empty pages (extra credit)
unsigned ObjectAllocator::FreeEmptyPages(void)
{
  return 0;
}

  // Returns true if FreeEmptyPages and alignments are implemented
bool ObjectAllocator::ImplementedExtraCredit(void)
{
  return false;
}

  // true=enable, false=disable
void ObjectAllocator::SetDebugState(bool State)
{
  clientConfig.DebugOn_ = State;
}

  // returns a pointer to the internal free list
const void *ObjectAllocator::GetFreeList(void) const
{
  return FreeList_;
}

  // returns a pointer to the internal page list
const void *ObjectAllocator::GetPageList(void) const
{
  return PageList_;
}

  // returns the configuration parameters
OAConfig ObjectAllocator::GetConfig(void) const
{
  return clientConfig;
}

// returns the statistics for the allocator
OAStats ObjectAllocator::GetStats(void) const
{
  return stats;
}

  // allocates another page of objects
void ObjectAllocator::allocate_new_page(void)
{
  allocate_empty_page();
  segment_page();
}

/*****************************************************************************/
/*
  HELPER FUNCTIONS
*/
/*****************************************************************************/
void ObjectAllocator::allocate_empty_page(void)
{
  // checks to see if the user allocated more pages than allowed in the config
  if (stats.PagesInUse_ >= clientConfig.MaxPages_)
  {
    throw OAException(OAException::E_NO_PAGES, "Out of pages");
  }

  GenericObject *newPage;
  try // makes sure there is enough memory
  {
    newPage = reinterpret_cast<GenericObject *>(new char[stats.PageSize_]);
    if (clientConfig.DebugOn_)
    {
      std::memset(newPage, UNALLOCATED_PATTERN, stats.PageSize_);
    }
  }
  catch (std::bad_alloc &)
  {
    throw OAException(OAException::E_NO_MEMORY, "out of memory");
  }

  // creates the first page
  if (!PageList_)
  {
    PageList_ = newPage;
    PageList_->Next = nullptr;
  }
  else // links the new page to the previous ones
  {
    newPage->Next = PageList_;
    PageList_ = newPage;
  }
}

void ObjectAllocator::segment_page(void)
{
  blockIter = 0;

  // segments the page into its blocks
  for (unsigned i = 0; i < clientConfig.ObjectsPerPage_; i++)
  {
    add_to_page();
  }
  stats.PagesInUse_++;
}

void ObjectAllocator::add_to_page(void)
{ 
  char* block = make_block(false);
                         
  // creates the next block to link to
  if(blockIter != 0)
  {
    char* prevBlock = make_block(true);

    FreeList_ = reinterpret_cast<GenericObject *>(block);
    FreeList_->Next = reinterpret_cast<GenericObject *>(prevBlock);
  }
  else // creates the first block
  {
    if (clientConfig.DebugOn_)
    {
      std::memset(block - clientConfig.PadBytes_, PAD_PATTERN, clientConfig.PadBytes_);
    }
    FreeList_ = reinterpret_cast<GenericObject *>(block);
    reinterpret_cast<GenericObject *>(block)->Next = nullptr;
  }
  stats.FreeObjects_++;
  blockIter++;
  configure_header(block, false, false);
}

char * ObjectAllocator::make_block(bool makePrev)
{
  // creates a new block
  char *newBlock = create_offset(reinterpret_cast<char *>(PageList_)) + (calculate_block_size() * (blockIter - makePrev));
  newBlock += clientConfig.HBlockInfo_.size_ + clientConfig.PadBytes_;

  if (clientConfig.DebugOn_)
  {
    std::memset(newBlock - clientConfig.PadBytes_, PAD_PATTERN, clientConfig.PadBytes_);
    std::memset(newBlock + stats.ObjectSize_, PAD_PATTERN, clientConfig.PadBytes_);
  }

  return newBlock;
}

void ObjectAllocator::configure_header(char * block, bool allocated, bool freed, const char *label)
{
  char *headerLocation = block - (clientConfig.PadBytes_ + clientConfig.HBlockInfo_.size_);

  switch (clientConfig.HBlockInfo_.type_)
  {
  case OAConfig::hbBasic: // sets the memory to follow the basic header pattern
    std::memset(headerLocation, 0, clientConfig.HBlockInfo_.size_);

    if (allocated) 
    {
      int *headerInt = reinterpret_cast<int *>(headerLocation);
      *headerInt = stats.Allocations_;
      headerLocation += sizeof(int);
      *headerLocation = true;
    }
    break;

  case OAConfig::hbExtended: // sets the memory to follow the extended header pattern
    if (!allocated && !freed) // initializes the bytes for first use
    {
      std::memset(headerLocation, 0, clientConfig.HBlockInfo_.size_);
    }
    else if (allocated) // updates the bytes once something is allocated
    {
      std::memset(headerLocation, 0, clientConfig.HBlockInfo_.additional_);
      short *headerShort = reinterpret_cast<short *>(headerLocation + clientConfig.HBlockInfo_.additional_);
      (*headerShort)++;
      int *headerInt = reinterpret_cast<int *>(headerShort + 1);
      *headerInt = stats.Allocations_;
      char *headerFlag = reinterpret_cast<char *>(headerInt + 1);
      *headerFlag = true;
    }
    else // makes sure the use counter does not get reset once something is freed
    {
      std::memset(headerLocation + clientConfig.HBlockInfo_.additional_ + sizeof(short), 0, sizeof(int) + sizeof(char));
    }
    break;

  case OAConfig::hbExternal: // sets the pointer to the external header
    if (!allocated && !freed) // initailizes the bytes for the first time
    {
      std::memset(headerLocation, 0, clientConfig.HBlockInfo_.size_);
    }
    else if (allocated) // allocates the memblockinfo struct
    {
      MemBlockInfo *info;
      try
      {
        info = new MemBlockInfo();
      }
      catch (std::bad_alloc &)
      {
        throw OAException(OAException::E_NO_MEMORY, "could not allocate the memblock info");
      }
      if (label)
      {
        try
        {
          info->label = new char[strlen(label) + 1];
        }
        catch (std::bad_alloc)
        {
          throw OAException(OAException::E_NO_MEMORY, "could not allocate the label");
        }
        strcpy(info->label, label);
      }
      info->alloc_num = stats.Allocations_;
      info->in_use = true;
      *(reinterpret_cast<MemBlockInfo **>(headerLocation)) = info;
    }
    else // deletes the struct once it's done
    {
      MemBlockInfo *toDelete = reinterpret_cast<MemBlockInfo *>(*(reinterpret_cast<MemBlockInfo **>(headerLocation)));
      if (toDelete->label)
      {
        delete[] toDelete->label;
      }
      delete toDelete;
      memset(headerLocation, 0, clientConfig.HBlockInfo_.size_);
    }
    break;

  default:
    break;
  }
}

  // takes Object off the free list
GenericObject* ObjectAllocator::take_off_freelist(const char *label)
{
  stats.FreeObjects_--;
  stats.ObjectsInUse_++;
  stats.MostObjects_++;
  stats.Allocations_++;
  configure_header(reinterpret_cast<char *>(FreeList_), true, false, label);
  GenericObject* block = FreeList_; // stores the block to give to the client
  FreeList_ = FreeList_->Next; 
  if (clientConfig.DebugOn_)
  {
    std::memset(block, ALLOCATED_PATTERN, stats.ObjectSize_);
  }
  
  return block;
}

  // puts object back on freelist
void ObjectAllocator::put_on_freelist(void *Object)
{
  stats.Deallocations_++;
  stats.ObjectsInUse_--;
  stats.FreeObjects_++;

  GenericObject *newFreeNode = reinterpret_cast<GenericObject *>(Object);
  if (clientConfig.DebugOn_)
  {
    check_freelist(newFreeNode);
  }
  configure_header(reinterpret_cast<char *>(newFreeNode), false, true);

  if (clientConfig.DebugOn_)
  {
    std::memset(newFreeNode, FREED_PATTERN, stats.ObjectSize_);
  }

  // Sets the freelist back up if all memory was taken
  if (!FreeList_)
  {
    FreeList_ = newFreeNode;
    FreeList_->Next = nullptr;
  }
  else // attaches the list if it's not null
  {
    newFreeNode->Next = FreeList_;
    FreeList_ = newFreeNode;
  }
}

void ObjectAllocator::check_freelist(GenericObject *node)
{
  char *freeListCopy = reinterpret_cast<char *>(FreeList_);
  char *pageListCopy = reinterpret_cast<char *>(PageList_);
  char *nodeCopy     = reinterpret_cast<char *>(node);

  if (clientConfig.PadBytes_ != 0)
  {
    if (check_corruption(nodeCopy))
    {
      throw OAException(OAException::E_CORRUPTED_BLOCK, "Overrote padding");
    }
  }
  check_multiple_free(nodeCopy, freeListCopy);
  check_bad_location(nodeCopy, pageListCopy);
}

void ObjectAllocator::check_bad_location(char * toCheck, char * pageList)
{
  bool toThrow = false;
  
  // if one of these checks goes wrong, it will store that, and then
  // check the other pages unless both return false
  while (pageList)
  { 
    if (check_out_of_page(toCheck, pageList) || check_wrong_offset(toCheck, pageList))
    {
      toThrow = true;
    }
    else
    {
      toThrow = false;
      break;
    }

    pageList = reinterpret_cast<char *>
      (reinterpret_cast<GenericObject *>
      (pageList)->Next);
  }
  if (toThrow)
  {
    throw OAException(OAException::E_BAD_BOUNDARY, "Not freed inside a page");
  }
}

void ObjectAllocator::check_multiple_free(char * toCheck, char * freeList)
{
  // checks the whole freelist to see the data is on the freelist
  while (freeList)
  {
    if (freeList == toCheck)
    {
      throw OAException(OAException::E_MULTIPLE_FREE, "Freed multiple times");
    }
    freeList = reinterpret_cast<char *>
      (reinterpret_cast<GenericObject *>
      (freeList)->Next);
  }
}

bool ObjectAllocator::check_out_of_page(char * toCheck, char * pageList)
{
  // checks to see if the data is under or over where the page it
  if (toCheck < pageList || toCheck > (pageList + calculate_page_size()) - sizeof(void *))
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool ObjectAllocator::check_wrong_offset(char * toCheck, char * pageList)
{ 
  // calculates the differece between where the first block starts and the block in question
  long long locationDifference = toCheck - (create_offset(pageList) + clientConfig.HBlockInfo_.size_ + clientConfig.PadBytes_);

  // sees if the difference is divisable by the block size
  if (locationDifference % calculate_block_size())
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool ObjectAllocator::check_leak_in_header(char *node) const
{
  // checks the external struct's in use variable
  if (clientConfig.HBlockInfo_.type_ == OAConfig::hbExternal)
  {
    MemBlockInfo *info = reinterpret_cast<MemBlockInfo *>(node);
    return info->in_use;
  }
  else // checks the header's in use variable
  {
    node += (clientConfig.HBlockInfo_.size_ - 1);
    return *node;
  }
  return false;
}

bool ObjectAllocator::check_corruption(char * toCheck) const
{
  // points to the padding before and after the data
  char *firstPad  = toCheck - clientConfig.PadBytes_;
  char *secondPad = toCheck + stats.ObjectSize_;

  for (unsigned i = 0; i < clientConfig.PadBytes_; i++)
  {
    // tests to see if the padding has its correct pattern
    if (static_cast<unsigned char>(*(firstPad + i)) != PAD_PATTERN 
     || static_cast<unsigned char>(*(secondPad + i)) != PAD_PATTERN)
    {
      return true;
    }
  }
  return false;
}

// calculates where blocks start
char * ObjectAllocator::create_offset(char * page) const
{
  return page + sizeof(void *);
}

// calculates the size of a page
size_t ObjectAllocator::calculate_page_size(void)
{
  return sizeof(void *) + (calculate_block_size() * clientConfig.ObjectsPerPage_);
}

// calculates the size of a block
size_t ObjectAllocator::calculate_block_size() const
{
  size_t headerSize = clientConfig.HBlockInfo_.size_;;
  size_t padding = clientConfig.PadBytes_ * 2;

  return stats.ObjectSize_ + padding + headerSize;
}

