//  Copyright (c) 2012 Bryce Lelbach
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "test_component.hpp"
#include <hpx/hpx_init.hpp>
#include <hpx/runtime/agas/interface.hpp>
#include <boost/lexical_cast.hpp>

int hpx_main(boost::program_options::variables_map&)
{
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
  }

  return hpx::finalize(); // Initiate shutdown of the runtime system.
}

int main(int argc, char* argv[])
{
  // Configure application-specific options.
  boost::program_options::options_description desc_commandline(
    "usage: " HPX_APPLICATION_STRING " [options]");

  return hpx::init(desc_commandline, argc, argv); 
}

