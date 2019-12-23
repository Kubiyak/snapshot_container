/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "snapshot_slice.h"
#include "snapshot_iterator.h"
#include "snapshot_storage.h"
#include <numeric>
#include <vector>
#include <memory>


using snapshot_container::_iterator_kernel;
using snapshot_container::deque_storage_creator;


TEST_CASE("Basic iterator_kernel test", "[iterator_kernel]") {
    deque_storage_creator<int> storage_creator;
    std::vector<int> test_values(16384);
    std::iota(test_values.begin(), test_values.end(), 0);    
    auto ik = _iterator_kernel<int, deque_storage_creator<int>>::create(storage_creator, test_values.begin(), test_values.end());
    auto ik2 = _iterator_kernel<int, deque_storage_creator<int>>::create(ik);
    
    REQUIRE(ik->m_slices[0].m_storage.use_count() == 2);        
    REQUIRE(ik->m_slices[0] == ik2->m_slices[0]);
    
   
}




