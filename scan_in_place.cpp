//  Copyright (c) 2015 Daniel Bourgeois
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/include/parallel_scan.hpp>
#include <hpx/include/parallel_transform_scan.hpp>
#include <hpx/util/lightweight_test.hpp>

#include <boost/iterator/counting_iterator.hpp>

#define FILL_VALUE 10
#define ARRAY_SIZE 10000

// n'th value of sum of 1+2+3+...
int check_n_triangle(int n) {
  return n<0 ? 0 : (n)*(n+1)/2;  
}

// n'th value of sum of x+x+x+...
int check_n_const(int n, int x) {
  return n<0 ? 0 : n*x;  
}

// run scan algorithm, validate that output array hold expected answers.
void test_scan_inclusive(std::vector<int> &a, std::vector<int> &b)
{
    using namespace hpx::parallel;
    typedef std::vector<int>::iterator Iter;

    // test 1, fill array with numbers counting from 0, then run scan algorithm
    a.clear();
    std::copy(boost::counting_iterator<int>(0), boost::counting_iterator<int>(ARRAY_SIZE), std::back_inserter(a));
    b.resize(a.size());
    hpx::parallel::inclusive_scan(hpx::parallel::par, a.begin(), a.end(), b.begin(), 0,
      [](int bar, int baz){ return bar+baz; });
    // 
    for (int i=0; i<b.size(); ++i) {
      // counting from zero, 
      int value = b[i];
      int expected_value  = check_n_triangle(i);
      if (!HPX_TEST(value == expected_value)) break;
    }

    // test 2, fill array with numbers counting from 1, then run scan algorithm
    a.clear();
    std::copy(boost::counting_iterator<int>(1), boost::counting_iterator<int>(ARRAY_SIZE), std::back_inserter(a));
    b.resize(a.size());
    hpx::parallel::inclusive_scan(hpx::parallel::par, a.begin(), a.end(), b.begin(), 0,
      [](int bar, int baz){ return bar+baz; });
    // 
    for (int i=0; i<b.size(); ++i) {
      // counting from 1, use i+1 
      int value = b[i];
      int expected_value  = check_n_triangle(i+1);
      if (!HPX_TEST(value == expected_value)) break;
    }

    // test 3, fill array with constant
    a.clear();
    std::fill_n(std::back_inserter(a), ARRAY_SIZE, FILL_VALUE);
    b.resize(a.size());
    hpx::parallel::inclusive_scan(hpx::parallel::par, a.begin(), a.end(), b.begin(), 0,
      [](int bar, int baz){ return bar+baz; });
    // 
    for (int i=0; i<b.size(); ++i) {
      int value = b[i];
      int expected_value  = check_n_const(i+1, FILL_VALUE);
      if (!HPX_TEST(value == expected_value)) break;
    }
}

// run scan algorithm, validate that output array hold expected answers.
void test_scan_exclusive(std::vector<int> &a, std::vector<int> &b)
{
    using namespace hpx::parallel;
    typedef std::vector<int>::iterator Iter;

    // test 1, fill array with numbers counting from 0, then run scan algorithm
    a.clear();
    std::copy(boost::counting_iterator<int>(0), boost::counting_iterator<int>(ARRAY_SIZE), std::back_inserter(a));
    b.resize(a.size());
    hpx::parallel::exclusive_scan(hpx::parallel::par, a.begin(), a.end(), b.begin(), 0,
      [](int bar, int baz){ return bar+baz; });
    // 
    for (int i=0; i<b.size(); ++i) {
      // counting from zero, 
      int value = b[i];
      int expected_value  = check_n_triangle(i-1);
      if (!HPX_TEST(value == expected_value)) break;
    }

    // test 2, fill array with numbers counting from 1, then run scan algorithm
    a.clear();
    std::copy(boost::counting_iterator<int>(1), boost::counting_iterator<int>(ARRAY_SIZE), std::back_inserter(a));
    b.resize(a.size());
    hpx::parallel::exclusive_scan(hpx::parallel::par, a.begin(), a.end(), b.begin(), 0,
      [](int bar, int baz){ return bar+baz; });
    // 
    for (int i=0; i<b.size(); ++i) {
      // counting from 1, use i+1 
      int value = b[i];
      int expected_value  = check_n_triangle(i);
      if (!HPX_TEST(value == expected_value)) break;
    }

    // test 3, fill array with constant
    a.clear();
    std::fill_n(std::back_inserter(a), ARRAY_SIZE, FILL_VALUE);
    b.resize(a.size());
    hpx::parallel::exclusive_scan(hpx::parallel::par, a.begin(), a.end(), b.begin(), 0,
      [](int bar, int baz){ return bar+baz; });
    // 
    for (int i=0; i<b.size(); ++i) {
      // counting from zero, 
      int value = b[i];
      int expected_value  = check_n_const(i, FILL_VALUE);
      if (!HPX_TEST(value == expected_value)) break;
    }
}

int hpx_main(boost::program_options::variables_map& vm)
{
    std::vector<int> a, b;
    // test scan algorithms using separate array for output
    std::cout << "Testing scan inclusive, separate arrays " << std::endl;
    test_scan_inclusive(a,b);
    std::cout << "Testing scan exclusive, separate arrays " << std::endl;
    test_scan_exclusive(a,b);
    // test scan algorithms using in_place array for output
    std::cout << "Testing scan inclusive, in place arrays " << std::endl;
    test_scan_inclusive(a,a);
    std::cout << "Testing scan exclusive, in place arrays " << std::endl;
    test_scan_exclusive(a,a);

    return hpx::finalize();
}

int main(int argc, char* argv[])
{
    // add command line option which controls the random number generator seed
    using namespace boost::program_options;
    options_description desc_commandline(
        "Usage: " HPX_APPLICATION_STRING " [options]");

    // By default this test should run on all available cores
    std::vector<std::string> cfg;
    cfg.push_back("hpx.os_threads=" +
        boost::lexical_cast<std::string>(hpx::threads::hardware_concurrency()));

    // Initialize and run HPX
    HPX_TEST_EQ_MSG(hpx::init(desc_commandline, argc, argv, cfg), 0,
        "HPX main exited with non-zero status");

    return hpx::util::report_errors();
}
