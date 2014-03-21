//  Copyright (c) 2013 Hartmut Kaiser
//  Copyright (c) 2013 Thomas Heller
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Bidirectional network bandwidth test

#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/include/iostreams.hpp>
#include <hpx/util/serialize_buffer.hpp>
#include <hpx/runtime/naming/locality.hpp>

#include <boost/assert.hpp>
#include <boost/shared_ptr.hpp>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <chrono>
#include <thread>
//
//----------------------------------------------------------------------------
#define MESSAGE_ALIGNMENT 64
#define MAX_ALIGNMENT 512
//
#define LOOP_LARGE  100
#define SKIP_LARGE  0
//
char *send_buffer_orig;
//
#include <memory>
#include <stdio.h>
//----------------------------------------------------------------------------
template <typename T>
class test_allocator: public std::allocator<T>
{
public:
  typedef T        value_type;
  typedef T*       pointer;
  typedef const T* const_pointer;
  typedef T&       reference;
  typedef const T& const_reference;
  typedef std::size_t    size_type;
  typedef std::ptrdiff_t difference_type;

  // we want to make sure anything else uses the std allocator
  template <class U> 
  struct rebind { typedef std::allocator<U> other; };

  pointer address (reference value) const {
    return &value;
  }
  const_pointer address (const_reference value) const {
    return &value;
  }

  pointer allocate(size_type n, const void *hint=0)
  {
    char const*  msg = "Allocator returning %1% bytes ";
    std::cout << (boost::format(msg) % n ) << std::endl ;
    return std::allocator<T>::allocate(n, hint);
  }

  void deallocate(pointer p, size_type n)
  {
    char const*  msg = "Allocator deleting %1% bytes (address %2%) ";
    std::cout << (boost::format(msg) % n % p) << std::endl;
    return std::allocator<T>::deallocate(p, n);
  }

  // initialize elements of allocated storage p with value value
  void construct (pointer p, const T& value) {
    char const*  msg = "Allocator constructing %1% objects";
    std::cout << (boost::format(msg) % value) << hpx::endl << hpx::flush;
    new ((void*)p)T(value);
  }

  void destroy (pointer p) {
    char const*  msg = "Allocator deleting %1% objects (address %2%) ";
    std::cout << (boost::format(msg) % p) << hpx::endl << hpx::flush;
    p->~T();
  }

  test_allocator() throw(): std::allocator<T>() { 
    uint32_t hpxrank = hpx::naming::get_locality_id_from_id(hpx::find_here());
//    char const*  msg = "Allocator constructor on rank %1% ";
//    std::cout << (boost::format(msg) % hpxrank) << hpx::endl << hpx::flush;
  }
  test_allocator(const test_allocator &a) throw(): std::allocator<T>(a) { 
    uint32_t hpxrank = hpx::naming::get_locality_id_from_id(hpx::find_here());
//    char const*  msg = "Allocator copy constructor on rank %1% ";
//    std::cout << (boost::format(msg) % hpxrank) << hpx::endl << hpx::flush;
  }
  ~test_allocator() throw() { }

};

//----------------------------------------------------------------------------
/*
typedef hpx::util::serialize_buffer<char>  buffer_type;
buffer_type message(buffer_type const& receive_buffer)
{
uint32_t hpxrank = hpx::naming::get_locality_id_from_id(hpx::find_here());
char const*  msg = "Rank %1% receiving message of size %2% ";
std::cout << (boost::format(msg) % hpxrank % receive_buffer.size()) << hpx::endl << hpx::flush;
return receive_buffer;
}

//----------------------------------------------------------------------------
HPX_PLAIN_ACTION(message);
HPX_REGISTER_BASE_LCO_WITH_VALUE_DECLARATION(hpx::util::serialize_buffer<char>, serialization_buffer_char);
HPX_REGISTER_BASE_LCO_WITH_VALUE(hpx::util::serialize_buffer<char>, serialization_buffer_char);
*/

//----------------------------------------------------------------------------
typedef hpx::util::serialize_buffer<char, test_allocator<char>> buffer_test_allocator;
buffer_test_allocator allocator_message(buffer_test_allocator const& receive_buffer)
{
  uint32_t hpxrank = hpx::naming::get_locality_id_from_id(hpx::find_here());
  char const*  msg = "Rank %1% receiving (allocator) message of size %2% ";
  std::cout << (boost::format(msg) % hpxrank % receive_buffer.size()) << hpx::endl << hpx::flush;
  return receive_buffer;
}
HPX_PLAIN_ACTION(allocator_message);
//HPX_REGISTER_BASE_LCO_WITH_VALUE_DECLARATION(buffer_test_allocator, serialization_buffer_char_allocator);
//HPX_REGISTER_BASE_LCO_WITH_VALUE(buffer_test_allocator, serialization_buffer_char_allocator);

//----------------------------------------------------------------------------
double receive(
  hpx::naming::id_type dest,
  char * send_buffer,
  std::size_t size,
  std::size_t window_size)
{
  hpx::id_type   here = hpx::find_here();  
  uint32_t    hpxrank = hpx::naming::get_locality_id_from_id(here);
  int skip = SKIP_LARGE;

  hpx::util::high_resolution_timer t;

  std::vector<hpx::unique_future<buffer_test_allocator> > recv_buffers;
  recv_buffers.reserve(window_size);

  allocator_message_action msg;
  for(std::size_t j = 0; j < window_size; ++j)
  {
    recv_buffers.push_back(std::move(hpx::async(msg, dest, 
      buffer_test_allocator(send_buffer, size, buffer_test_allocator::reference))));
      //      buffer_type(send_buffer, size, buffer_type::reference));
  }
  hpx::wait_all(recv_buffers);
  for(std::size_t j = 0; j < window_size; ++j)
  {
    buffer_test_allocator message = recv_buffers[j].get();     
    std::cout << "Rank " << hpxrank << " received " << message.data() << hpx::endl << hpx::flush;
  }

//  std::this_thread::sleep_for(std::chrono::seconds(10));

  double elapsed = t.elapsed();
  return (elapsed * 1e6) / (2.0 * 1.0 * window_size);
}
//----------------------------------------------------------------------------

int hpx_main(int argc, char* argv[])
{
  hpx::id_type    here = hpx::find_here();
  uint64_t        rank = hpx::naming::get_locality_id_from_id(here);
  std::string     name = hpx::get_locality_name();
  uint64_t        size = hpx::get_num_localities().get();
  std::size_t  current = hpx::get_worker_thread_num();
  std::vector<hpx::id_type>    remotes = hpx::find_remote_localities();
  std::vector<hpx::id_type> localities = hpx::find_all_localities();

  char const* msg = "hello world from OS-thread %1% on locality %2% mpi rank %3% hostname %4%";
  std::cout << (boost::format(msg) % current % hpx::get_locality_id() % rank % name.c_str())
    << std::endl;

  std::cout << hpx::flush;

  uint64_t transfersize = 0x01000000;

  // alloc buffer to send
  char* send_buffer = new char[transfersize];

  boost::shared_array<char> char_data(new char[1024]());
  char const *fmt = "Hello char array generated on rank %1% ";
  std::string temp = boost::str(boost::format(fmt) % rank );
  strcpy(&char_data[0], temp.c_str());

  hpx::util::high_resolution_timer timer;
                                                            
  for (hpx::id_type& loc: localities) {
    char const* msg = "Rank %1% sending %2% bytes to %3%";
    std::cout << (boost::format(msg) % rank % transfersize % hpx::naming::get_locality_id_from_id(loc))
      << hpx::endl;
    double latency = receive(loc, char_data.get(), 1024, 1);
    std::cout << std::left << std::setw(10) << transfersize << latency << hpx::endl << hpx::flush;
  }

  std::cout << "Total time: " << timer.elapsed_nanoseconds() << "\n" << hpx::flush;
  delete[] send_buffer_orig;

//  if (hpxrank!=0) {
    return hpx::finalize();
 // }
 // else {
 //   return 0;
 // }
}
//----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
/*
  int flag = 0;
  MPI_Initialized(&flag);
  if (flag == 0)
  {
    int provided, rank, size;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    //
    if (rank == 0) {
      if (provided != MPI_THREAD_MULTIPLE) {
        std::cout << "MPI_THREAD_MULTIPLE not set, you may need to recompile your "
          << "MPI distribution with threads enabled" << std::endl;
      }
      else {
        std::cout << "MPI_THREAD_MULTIPLE is OK (DSM override)" << std::endl;
      }
    }
  }
*/
  return hpx::init(argc, argv);
}