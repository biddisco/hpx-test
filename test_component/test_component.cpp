//  Copyright (c) 2012 Bryce Lelbach
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "test_component.hpp"
#include <hpx/include/iostreams.hpp>

namespace server
{

  test_component::test_component() {
    uint32_t hpxrank = hpx::naming::get_locality_id_from_id(hpx::find_here());
    this->rank = hpxrank;
  }

  void test_component::invoke()
  {
      uint32_t hpxrank = hpx::naming::get_locality_id_from_id(hpx::find_here());
      char const*  msg = "Action invoke created on %1% called from rank %2% %3%";
      std::cout << (boost::format(msg) % this->rank % hpxrank % hpx::get_locality_name()) << std::endl;
  }

  void test_component::invokeFrom(const uint32_t &loc)
  {
      char const*  msg = "Action invokeFrom created on %1% called from rank %2% %3%";
      std::cout << (boost::format(msg) % this->rank % loc % hpx::get_locality_name()) << std::endl;
  }

}

HPX_REGISTER_COMPONENT_MODULE();

typedef hpx::components::managed_component<server::test_component> test_component_type;

HPX_REGISTER_MINIMAL_COMPONENT_FACTORY(test_component_type, test_component);

HPX_REGISTER_ACTION(server::test_component::invoke_action, test_component_invoke_action);
HPX_REGISTER_ACTION(server::test_component::invokeFrom_action, test_component_invokeFrom_action);

