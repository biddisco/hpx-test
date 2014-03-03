//  Copyright (c) 2012 Bryce Lelbach
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(TEST_COMPONENT_HPP)
#define TEST_COMPONENT_HPP

#include <hpx/hpx_fwd.hpp>
#include <hpx/include/actions.hpp>
#include <hpx/include/lcos.hpp>
#include <hpx/include/components.hpp>
#include <hpx/include/serialization.hpp>

namespace server
{

class HPX_COMPONENT_EXPORT test_component
  : public hpx::components::managed_component_base<test_component>
{
  public:
    test_component();

    void invoke();
    void invokeFrom(const uint32_t &loc);
    HPX_DEFINE_COMPONENT_ACTION(test_component, invoke, invoke_action);
    HPX_DEFINE_COMPONENT_ACTION(test_component, invokeFrom, invokeFrom_action);

  private:
    uint32_t rank;
};

}

HPX_REGISTER_ACTION_DECLARATION(
    server::test_component::invoke_action, test_component_invoke_action);

HPX_REGISTER_ACTION_DECLARATION(
    server::test_component::invokeFrom_action, test_component_invokeFrom_action);

#endif 

