//  Copyright (c) 2014 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/util/lightweight_test.hpp>
#include <hpx/util/serialize_buffer.hpp>
#include <hpx/include/iostreams.hpp>
#include <hpx/runtime/naming/locality.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

///////////////////////////////////////////////////////////////////////////////
typedef hpx::util::serialize_buffer<char> buffer_plain_type;

buffer_plain_type bounce_plain(buffer_plain_type const& receive_buffer)
{
    return receive_buffer;
}
HPX_PLAIN_ACTION(bounce_plain);

HPX_REGISTER_BASE_LCO_WITH_VALUE_DECLARATION(
    buffer_plain_type, serialization_buffer_char);
HPX_REGISTER_BASE_LCO_WITH_VALUE(
    buffer_plain_type, serialization_buffer_char);

///////////////////////////////////////////////////////////////////////////////
typedef hpx::util::serialize_buffer<char, std::allocator<char> >
    buffer_allocator_type;

buffer_allocator_type bounce_allocator(buffer_allocator_type const& receive_buffer)
{
    return receive_buffer;
}
HPX_PLAIN_ACTION(bounce_allocator);

HPX_REGISTER_BASE_LCO_WITH_VALUE_DECLARATION(
    buffer_allocator_type, serialization_buffer_char_allocator);
HPX_REGISTER_BASE_LCO_WITH_VALUE(
    buffer_allocator_type, serialization_buffer_char_allocator);

///////////////////////////////////////////////////////////////////////////////
template <typename Buffer, typename Action>
void test(hpx::id_type dest, char* send_buffer, std::size_t size)
{
    typedef Buffer buffer_type;
    buffer_type recv_buffer;

    std::vector<hpx::unique_future<buffer_type> > recv_buffers;
    recv_buffers.resize(10);

    Action act;
    for(std::size_t j = 0; j != 10; ++j)
    {
        recv_buffers[j] = hpx::async(act, dest,
            buffer_type(send_buffer, size, buffer_type::reference));
    }
    hpx::wait_all(recv_buffers);

    BOOST_FOREACH(hpx::unique_future<buffer_type>& f, recv_buffers)
    {
        buffer_type b = f.get();
        HPX_TEST_EQ(b.size(), size);
        HPX_TEST(0 == memcmp(b.data(), send_buffer, size));
    }
}

template <typename Allocator>
void test_stateful_allocator(hpx::id_type dest, char* send_buffer,
    std::size_t size, Allocator const& alloc)
{
    typedef buffer_allocator_type buffer_type;
    buffer_type recv_buffer;

    std::vector<hpx::unique_future<buffer_type> > recv_buffers;
    recv_buffers.resize(10);

    bounce_allocator_action act;
    for(std::size_t j = 0; j != 10; ++j)
    {
        recv_buffers[j] = hpx::async(act, dest,
            buffer_type(send_buffer, size, buffer_type::reference, alloc));
    }
    hpx::wait_all(recv_buffers);

    BOOST_FOREACH(hpx::unique_future<buffer_type>& f, recv_buffers)
    {
        buffer_type b = f.get();
        HPX_TEST_EQ(b.size(), size);
        HPX_TEST(0 == memcmp(b.data(), send_buffer, size));
    }
}

///////////////////////////////////////////////////////////////////////////////
int hpx_main(int argc, char* argv[])
{
    std::size_t const max_size = 1 << 10;
    boost::scoped_ptr<char> send_buffer(new char[max_size]);
    hpx::id_type    here = hpx::find_here();
    uint64_t        rank = hpx::naming::get_locality_id_from_id(here);
    std::string     name = hpx::get_locality_name();
    std::size_t  current = hpx::get_worker_thread_num();

    std::cout << "Hello world from " << name.c_str() << " rank " << rank << std::endl;
    char const* msg = "hello world from OS-thread %1% on locality %2% mpi rank %3% hostname %4%";
    hpx::cout << (boost::format(msg) % current % hpx::get_locality_id() % rank % name.c_str())
              << hpx::endl;

    char c;
    if (rank==0) {
      std::cout << "Attach debugger " << std::endl;
      std::cin >> c;
    }

    BOOST_FOREACH(hpx::id_type loc, hpx::find_all_localities())
    {
        for (std::size_t size = 1; size <= max_size; size *= 2)
        {
          hpx::cout << rank << " " << name.c_str() << " Sending size (plain) " << size << " to " << (loc.get_msb()>>32) << hpx::endl << hpx::flush;
            test<buffer_plain_type, bounce_plain_action>(
                loc, send_buffer.get(), size);
          hpx::cout << rank << " " << name.c_str() << " Sending size (alloc) " << size << " to " << (loc.get_msb()>>32) << hpx::endl << hpx::flush;
            test<buffer_allocator_type, bounce_allocator_action>(
                loc, send_buffer.get(), size);
          hpx::cout << rank << " " << name.c_str() << " Sending size (state) " << size << " to " << (loc.get_msb()>>32) << hpx::endl << hpx::flush;
            test_stateful_allocator(
                loc, send_buffer.get(), size, std::allocator<char>());
        }
    }

    return hpx::finalize();
}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    HPX_TEST_EQ(hpx::init(argc, argv), 0);
    return 0;
}
