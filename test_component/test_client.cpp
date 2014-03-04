//  Copyright (c) 2012 Bryce Lelbach
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "test_component.hpp"
#include <hpx/hpx_init.hpp>
#include <hpx/runtime/agas/interface.hpp>
#include <hpx/lcos/barrier.hpp>
#include <hpx/include/iostreams.hpp>
//
#include <boost/lexical_cast.hpp>

static hpx::lcos::barrier unique_barrier;

// Create a new barrier and register its gid with the given symbolic name.
inline hpx::lcos::barrier
create_barrier(std::size_t num_localities, char const* symname)
{
    hpx::lcos::barrier b;    
    std::cout << "Creating barrier based on N localities " << num_localities << std::endl;
    b.create(hpx::find_here(), num_localities);
    hpx::agas::register_name_sync(symname, b.get_gid());
    return b;
}

// Find a registered barrier object from its symbolic name.
inline hpx::lcos::barrier
find_barrier(char const* symname)
{
    hpx::id_type id;
    for (std::size_t i = 0; i != HPX_MAX_NETWORK_RETRIES; ++i)
    {
        hpx::error_code ec;
        id = hpx::agas::resolve_name_sync(symname, ec);
        if (!ec) break;

        boost::thread::sleep( boost::get_system_time() +
            boost::posix_time::milliseconds(HPX_NETWORK_RETRIES_SLEEP));
    }
    return hpx::lcos::barrier(id);
}

void create_barrier_startup()
{
  unique_barrier = create_barrier(hpx::find_all_localities().size(), "/my_startup_barrier");
}

int hpx_main(boost::program_options::variables_map&)
{
  hpx::naming::id_type here = hpx::find_here();
  uint32_t          hpxrank = hpx::naming::get_locality_id_from_id(here);
  std::cout << "hpx_main running on locality " << hpxrank << std::endl;
  //
  std::cout << "registering component with global name " << "/component/" 
    << boost::lexical_cast<std::string>(hpxrank).c_str() << std::endl;
  //
  auto object = hpx::components::new_<server::test_component>(here).get();
  hpx::agas::register_name_sync(
    "/component/" + boost::lexical_cast<std::string>(hpxrank), 
    object);

  boost::shared_ptr<server::test_component> local_component = hpx::get_ptr_sync<server::test_component>(object);
  local_component->localTest(hpxrank);

  // wait until all processes have created their components
  hpx::lcos::barrier b = find_barrier("/my_startup_barrier");
  if (hpxrank==0) {
    hpx::cout << "Waiting at barrier before invoking actions" << hpx::endl << hpx::flush; 
  }
  b.wait();


  // for each locality, find the handle of the component assigned to it
  // and store it for later use.
  std::vector<hpx::id_type> localities = hpx::find_all_localities();
  std::vector<hpx::id_type> objects;
  for (hpx::naming::id_type &l : localities) {
    objects.push_back(
      hpx::agas::resolve_name_sync("/component/" + boost::lexical_cast<std::string>(hpx::naming::get_locality_id_from_id(l)))
      );
  }

  // for each object, invoke the test action
  for (hpx::naming::id_type &o : objects) {
    hpx::apply<server::test_component::invokeFrom_action>(o, hpxrank);
  }

  if (hpxrank==0) {
    hpx::cout << "Waiting at barrier before exit" << hpx::endl << hpx::flush; 
  }
  b.wait();

  // we don't need the barrier any more
  hpx::agas::unregister_name_sync("/my_startup_barrier");

  return hpx::finalize(); // Initiate shutdown of the runtime system.
}

int main(int argc, char* argv[])
{
  // Configure application-specific options.
  boost::program_options::options_description desc_commandline(
    "usage: " HPX_APPLICATION_STRING " [options]");

  hpx::register_startup_function(&create_barrier_startup);

  return hpx::init(desc_commandline, argc, argv); 
}

