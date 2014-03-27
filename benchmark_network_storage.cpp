//  Copyright (c) 2014 John Biddiscombe
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/include/util.hpp>
#include <hpx/include/lcos.hpp>
#include <hpx/lcos/future.hpp>
#include <hpx/lcos/future_wait.hpp>
#include <hpx/lcos/when_any.hpp>
#include <hpx/runtime/actions/plain_action.hpp>
#include <hpx/include/iostreams.hpp>
#include <hpx/util/serialize_buffer.hpp>
#include <hpx/runtime/naming/locality.hpp>
//
#include <boost/assert.hpp>
#include <boost/shared_ptr.hpp>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <chrono>
#include <thread>
#include <memory>
#include <cstdio>
#include <random>
#include <atomic>
#include <array>

//
// This is a test program which reads and writes chunks of memory to storage
// distributed across localities.
//
// The principal problem can be sumarized as follows:
//
// Put data into remote memory
//    Pass user data pointer into serializ_buffer (0 copy)
//    Copy data into parcelport for transmission. (1 copy)
//    Transmit data into remote parcelport buffer
//    Copy data from parcelport into serialize_buffer (2 copy)
//    Copy data from serialize buffer into remote host storage (3 copy)
//
// Get data from remote memory
//    Allocate temporary buffer and copy from host storage into it (1 copy)
//    Wrap storage in serialie_buffer and give to parcelport for return 
//    Copy data from serializ_buffer into parcelport for tranmission (2 copy)
//    Receive data into local parcelport buffer
//    Copy from parcelport buffer into serialize_buffer (3 copy)
//    Copy from serialize buffer into user memory (4 copy)
//
// It is the final copy in each case I am trying to remove by customizing the allocator   
// to pass from the parcelport directly into user memory.
//
// To make each process run a main function and participate in the test, 
// use a command line of the kind (no mpiexec assumed)
// benchmark_network_storage.exe -l%1 -%%x --hpx:run-hpx-main --hpx:threads=4 (+or more)
// where %l is num_ranks, and %%x is rank of current process

//----------------------------------------------------------------------------
// These are used to track how many requests are pending for each locality
// Many requests for read/write of memory may be issued at a time, resulting
// in thousands of futures that need to be waited on. To reduce this number
// a background thread can be spawned to check for ready futures and remove 
// them from the waiting list. The vars are used for this bookkeeping task.
//
std::vector<std::vector<hpx::unique_future<int>>> ActiveFutures;
hpx::lcos::local::spinlock                        FuturesMutex;
std::atomic<bool>                                 FuturesActive;
std::array<std::atomic<int>,64>                   FuturesWaiting;

//----------------------------------------------------------------------------
// Used at start and end of each loop for synchonization
hpx::lcos::barrier unique_barrier;

//----------------------------------------------------------------------------
//
// Each locality allocates a buffer of memory which is used to host transfers
//
char *local_storage = NULL;
//
const int         iterations = 10;
const int local_storage_size = (256*1024*1024);
const int      transfer_size = (16*1024*1024);

//----------------------------------------------------------------------------
#define DEBUG_LEVEL 0
#define DEBUG_OUTPUT(level,x) \
  if (DEBUG_LEVEL>=level) {   \
    x                         \
  }                           
//
#define TEST_FAIL    0
#define TEST_SUCCESS 1

//----------------------------------------------------------------------------
void allocate_local_storage() {
  local_storage = new char[local_storage_size];
}

//----------------------------------------------------------------------------
void delete_local_storage() {
  delete[] local_storage;
}

//----------------------------------------------------------------------------
// This routine simply copies from the source buffer into the local memory
// at the address offset.
//
// The function does not need to be asynchronous as it completes immediately, 
// but we return a future as this test needs to mimic "asynchronous" storage
hpx::unique_future<int> copy_to_local_storage(char *src, uint32_t offset, int length) {
  char *dest = &local_storage[offset];
  std::copy(src, src+length, dest);
//  memcpy(dest, src, length);
  return hpx::make_ready_future<int>(TEST_SUCCESS);
}

//----------------------------------------------------------------------------
// This routine simply copies from local memory at the address offset
// into a provided buffer 
// 
// The function does not need to be asynchronous as it completes immediately, 
// but we return a future as this test needs to mimic "asynchronous" storage
hpx::unique_future<int> copy_from_local_storage(char *dest, uint32_t offset, int length) {
  char *src = &local_storage[offset];
  std::copy(src, src+length, dest);
//  memcpy(dest, src, length);
  return hpx::make_ready_future<int>(TEST_SUCCESS);
}

//----------------------------------------------------------------------------
// A custom allocator which takes a pointer in its constructor and then returns 
// this pointer in reponse to any allocate request. It is here to try to fool
// the hpx serialization into copying directly into a user provided buffer
// without copying from a result into another bufffer.
// 
template <typename T>
class AllocatorPointer: public std::allocator<T>
{
public:
  typedef T              value_type;
  typedef T*             pointer;
  typedef const T*       const_pointer;
  typedef T&             reference;
  typedef const T&       const_reference;
  typedef std::size_t    size_type;
  typedef std::ptrdiff_t difference_type;

  pointer the_pointer;

  AllocatorPointer() throw() { }

  pointer address (reference value) const { return &value; }
  const_pointer address (const_reference value) const { return &value; }

  pointer allocate(size_type n, const void *hint=0) {
    return static_cast<T*>(the_pointer);
  }

  void deallocate(pointer p, size_type n) {}

  AllocatorPointer(pointer a_pointer) throw(): std::allocator<T>() {
    this->the_pointer = a_pointer;
  }
  AllocatorPointer(const AllocatorPointer &a) throw(): std::allocator<T>(a) {
    this->the_pointer = a.the_pointer;
  }
};

//----------------------------------------------------------------------------
// A simple Buffer for sending data, it does not need any special allocator
// user data may be sent to another locality using zero copy by wrapping
// it in one of these buffers
typedef hpx::util::serialize_buffer<char, std::allocator<char>> TransferBuffer;
//
// When receiving data, we receive a hpx::serialize_buffer, we try to minimize
// copying of data by providing a receive buffer with a fixed data pointer
// so that data is placed directly into it. 
// It doesn't produce any speedup, but is here to provide a basis for experimentation
typedef AllocatorPointer<char>                              PointerAllocator;
typedef hpx::util::serialize_buffer<char, PointerAllocator> SerializeToPointer;

//----------------------------------------------------------------------------
// The TransferBufferReceive provides a constructor which copied from the 
// input buffer into the final memory location
class TransferBufferReceive : public SerializeToPointer
{
public:
  TransferBufferReceive(const TransferBuffer &a, PointerAllocator::pointer a_pointer) throw(): SerializeToPointer(AllocatorPointer<char>(a_pointer))
  {
    // copy from the source into our memory
    memcpy(a_pointer, a.data(), a.size());
  }
};

//----------------------------------------------------------------------------
//
// Two actions which are called from remoote or local localities
//
// Copy to storage, just invokes the local copy function and returns the future from it
//
// Copy from storage allocates a buffer for the return memory and then wraps it into
// a serialize buffer which is returned and passed into the local process.
// This unfortunately means memory is copied from the storage, into a buffer
// and not zero copied as we would like.
// I have not been successful in removing this copy.
namespace Storage {
  //----------------------------------------------------------------------------
  // A PUT into memory on this locality from a requester sending a TransferBuffer
  hpx::unique_future<int> CopyToStorage(TransferBuffer const& srcbuffer, uint32_t address, int length)
  {
    boost::shared_array<char>    src = srcbuffer.data_array();
    return std::move(
      copy_to_local_storage(src.get(), address, length)
    );
  }

  //----------------------------------------------------------------------------
  // A GET from memory on this locality is returned to the requester in the TransferBuffer
  hpx::unique_future<TransferBuffer> CopyFromStorage(uint32_t address, int length)
  {
    // we must allocate a return buffer
    std::allocator<char> allocator;
    boost::shared_array<char> dest(allocator.allocate(length), [](char*){});

    // allow the storage class to asynchronously copy the data into dest buffer
    hpx::unique_future<int> fut = std::move(
      copy_from_local_storage(dest.get(), address, length)
    );

    // when the task completes, return a TransferBuffer
    return std::move(fut.then(hpx::launch::sync,
      [=](hpx::unique_future<int> &&f) -> TransferBuffer {
        int success = f.get();
        if (success != 1) {
          throw std::runtime_error("Fail in Get");
        }
        // return the result buffer in a serializable hpx TransferBuffer
        // tell the return buffer that it now owns the buffer using ::take mode
        return std::move(TransferBuffer(dest.get(), length, TransferBuffer::take));
      }
    ));
  }
} // namespace storage
//----------------------------------------------------------------------------
// normally these are in a header
HPX_DEFINE_PLAIN_ACTION(Storage::CopyToStorage, CopyToStorage_action);
HPX_REGISTER_PLAIN_ACTION_DECLARATION(CopyToStorage_action);
HPX_DEFINE_PLAIN_ACTION(Storage::CopyFromStorage, CopyFromStorage_action);
HPX_REGISTER_PLAIN_ACTION_DECLARATION(CopyFromStorage_action);
// and these in a cpp
HPX_REGISTER_PLAIN_ACTION(CopyToStorage_action);
HPX_REGISTER_PLAIN_ACTION(CopyFromStorage_action);

//----------------------------------------------------------------------------
// the main message sending loop may generate many thousands of send requests
// and each is associated with a future. To reduce the number we must wait on
// this loop runs in a background thread and simply removes any completed futures
// from the main list of active futures.
int RemoveCompletions()
{
  int num_removed = 0;
  //
  while (FuturesActive) {
    int last_removed = num_removed;
    {            
      hpx::lcos::local::spinlock::scoped_lock lk(FuturesMutex);
      for (std::vector<hpx::unique_future<int>> &futvec: ActiveFutures) {
        for (std::vector<hpx::unique_future<int>>::iterator fut = futvec.begin(); fut != futvec.end(); ) {
           if (fut->is_ready()){
              if (fut->get() != TEST_SUCCESS) {
                throw std::runtime_error("Remote put/get failed");
              }
              num_removed++;
              fut = futvec.erase(fut); 
           }
           else {
              ++fut;
           }
        }
      }
    }
    hpx::this_thread::suspend(boost::posix_time::microseconds(1));
  }
  return num_removed;
}

//----------------------------------------------------------------------------
// Take a vector of futures representing pass/fail and reduce to a single pass fail
int reduce(hpx::unique_future<std::vector<hpx::unique_future<int>>> &&futvec)
{
  int res = TEST_SUCCESS;
  std::vector<hpx::unique_future<int>> vfs = futvec.get();  
  for (hpx::unique_future<int>& f: vfs) {
    if (f.get() == TEST_FAIL) return TEST_FAIL;
  }
  return res;
}

//----------------------------------------------------------------------------
// Create a new barrier and register its gid with the given symbolic name.
hpx::lcos::barrier create_barrier(std::size_t num_localities, char const* symname)
{
  hpx::lcos::barrier b;
  DEBUG_OUTPUT(2,
    std::cout << "Creating barrier based on N localities " << num_localities << std::endl;
  );
  b.create(hpx::find_here(), num_localities);
  hpx::agas::register_name_sync(symname, b.get_gid());
  return b;
}

//----------------------------------------------------------------------------
// Find a registered barrier object from its symbolic name.
hpx::lcos::barrier find_barrier(char const* symname)
{
  hpx::id_type id;
  for (std::size_t i = 0; i != HPX_MAX_NETWORK_RETRIES; ++i) {
    hpx::error_code ec;
    id = hpx::agas::resolve_name_sync(symname, ec);
    if (!ec) break;
    boost::thread::sleep( boost::get_system_time() +
                         boost::posix_time::milliseconds(HPX_NETWORK_RETRIES_SLEEP));
  }
  return hpx::lcos::barrier(id);
}

//----------------------------------------------------------------------------
// Main test loop which randomly sends packets of data from one locality to another
// looping over the entire buffer address space and timing the total transmit/receive time
// to see how well we're doing.
int hpx_main(int argc, char* argv[])
{
  hpx::id_type                    here = hpx::find_here();
  uint64_t                        rank = hpx::naming::get_locality_id_from_id(here);
  std::string                     name = hpx::get_locality_name();
  uint64_t                      nranks = hpx::get_num_localities().get();
  std::size_t                  current = hpx::get_worker_thread_num();
  std::vector<hpx::id_type>    remotes = hpx::find_remote_localities();
  std::vector<hpx::id_type> localities = hpx::find_all_localities();
  //
  char const* msg = "hello world from OS-thread %1% on locality %2% rank %3% hostname %4%";
  std::cout << (boost::format(msg) % current % hpx::get_locality_id() % rank % name.c_str()) << std::endl;
  //
  allocate_local_storage();
  //
  uint64_t num_transfer_slots = local_storage_size/transfer_size;
  //
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> random_rank(0, nranks-1);
  std::uniform_int_distribution<> random_slot(0, num_transfer_slots-1);
  // 
  CopyToStorage_action actWrite;
  CopyFromStorage_action actRead;
  //
  ActiveFutures.resize(nranks);
  for (int i=0; i<nranks; i++) {
    FuturesWaiting[i] = 0;
  }

  // create a barrier we will use at the start and end of each run to synchronize
  create_barrier(nranks, "/DSM_barrier");
  unique_barrier = find_barrier("/DSM_barrier");

  ////////////////////////////////
  // Test speed of write/put
  ////////////////////////////////
  //
  unique_barrier.wait();
  hpx::util::high_resolution_timer timerWrite;
  //
  for (int i=0; i<iterations; i++) {
    //
    // start a thread which will clear any completed futures from our list.
    //
    FuturesActive = true;
    hpx::unique_future<int> cleaner = hpx::async(RemoveCompletions);
    //
    // Start main message sending loop
    //
    for (int i=0; i<num_transfer_slots; i++) {
      // pick a random locality to send to
      int send_rank = random_rank(gen);
      // get the pointer to the current packet send buffer
      char *buffer = &local_storage[i*transfer_size];
      // Get the HPX locality from the dest rank
      hpx::id_type locality = hpx::naming::get_id_from_locality_id(send_rank);
      // pick a random slot to write our data into
      int memory_slot = random_slot(gen);
      uint32_t memory_offset = memory_slot*transfer_size;

      // Execute a PUT on whatever locality we chose
      // Create a serializable memory buffer ready for sending. Do not copy any data.
      // protect this with a mutex to ensure the background thread removing completed futures doesn't collide
      {
        ++FuturesWaiting[send_rank];
        hpx::lcos::local::spinlock::scoped_lock lk(FuturesMutex);
        ActiveFutures[send_rank].push_back(
          std::move(
            hpx::async(actWrite, locality, TransferBuffer(static_cast<char*>(buffer), transfer_size, TransferBuffer::reference), memory_offset, transfer_size).then(hpx::launch::sync,
              [=](hpx::unique_future<int> &&fut) -> int {
                int result = fut.get();
                --FuturesWaiting[send_rank]; 
                return result;
              }
            )
          )
        );
      }
    }
    // tell the cleaning thread it's time to stop
    FuturesActive = false;
    // wait for cleanup thread to terminate before we reduce any remaining futures
    int removed = cleaner.get();
    DEBUG_OUTPUT(2, 
      std::cout << "Cleaning thread removed " << removed << std::endl;
    );
    //
    std::vector<hpx::unique_future<int>> final_list;
    for (int i=0; i<nranks; i++) {
      // move the contents of intermediate vector into final list
      std::move(ActiveFutures[i].begin(), ActiveFutures[i].end(), std::back_inserter(final_list));
      ActiveFutures[i].clear();
    }

    hpx::unique_future<int> result = when_all(final_list).then(hpx::launch::sync,reduce);
    result.get();
  }
  unique_barrier.wait();
  //
  double writeMB   = nranks*local_storage_size*iterations/(1024.0*1024.0);
  double writeTime = timerWrite.elapsed();
  double writeBW   = writeMB/writeTime;
  if (rank==0) {
    std::cout << "Total time         : " << writeTime << "\n";
    std::cout << "Memory Transferred : " << writeMB   << "MB \n";
    std::cout << "Aggregate BW Write : " << writeBW   << "MB/s" << std::endl;
  }

  ////////////////////////////////
  // Test speed of read/get
  ////////////////////////////////
  // this is mostly the same as the put loop, except that the received future is not
  // an int, but a transfer buffer which we have to copy out of.
  //
  hpx::util::high_resolution_timer timerRead;
  //
  for (int i=0; i<iterations; i++) {
    //
    // start a thread which will clear any completed futures from our list.
    //
    FuturesActive = true;
    hpx::unique_future<int> cleaner = hpx::async(RemoveCompletions);
    //
    // Start main message sending loop
    //
    for (int i=0; i<num_transfer_slots; i++) {
      // pick a random locality to send to
      int send_rank = random_rank(gen);
      // get the pointer to the current packet send buffer
      char *buffer = &local_storage[i*transfer_size];
      // Get the HPX locality from the dest rank
      hpx::id_type locality = hpx::naming::get_id_from_locality_id(send_rank);
      // pick a random slot to write our data into
      int memory_slot = random_slot(gen);
      uint32_t memory_offset = memory_slot*transfer_size;

      // Execute a PUT on whatever locality we chose
      // Create a serializable memory buffer ready for sending. Do not copy any data.
      // protect this with a mutex to ensure the background thread removing completed futures doesn't collide
      {
        ++FuturesWaiting[send_rank];
        hpx::lcos::local::spinlock::scoped_lock lk(FuturesMutex);
        ActiveFutures[send_rank].push_back(
          std::move(
            hpx::async(actRead, locality, memory_offset, transfer_size).then(hpx::launch::sync,
              [=](hpx::unique_future<TransferBuffer> &&fut) -> int {
                // Retrieve the serialized data buffer that was returned from the action
                // try to minimize copies by receiving into our custom buffer.
                TransferBufferReceive buffer(fut.get(), static_cast<char*>(buffer));
                --FuturesWaiting[send_rank];
                return TEST_SUCCESS;
              }
            )
          )
        );
      }
    }
    // tell the cleaning thread it's time to stop
    FuturesActive = false;
    // wait for cleanup thread to terminate before we reduce any remaining futures
    int removed = cleaner.get();
    DEBUG_OUTPUT(2, 
      std::cout << "Cleaning thread removed " << removed << std::endl;
    );
    //
    std::vector<hpx::unique_future<int>> final_list;
    for (int i=0; i<nranks; i++) {
      // move the contents of intermediate vector into final list
      std::move(ActiveFutures[i].begin(), ActiveFutures[i].end(), std::back_inserter(final_list));
      ActiveFutures[i].clear();
    }

    hpx::unique_future<int> result = when_all(final_list).then(hpx::launch::sync,reduce);
    result.get();
  }
  unique_barrier.wait();
  //
  double readMB   = nranks*local_storage_size*iterations/(1024.0*1024.0);
  double readTime = timerRead.elapsed();
  double readBW   = readMB/readTime;
  if (rank==0) {
    std::cout << "Total time         : " << readTime << "\n";
    std::cout << "Memory Transferred : " << readMB   << "MB \n";
    std::cout << "Aggregate BW Read  : " << readBW   << "MB/s" << std::endl;
  }

  //
  delete_local_storage();
  return hpx::finalize();
}

//----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  return hpx::init(argc, argv);
}