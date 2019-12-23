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
#include <algorithm>

using snapshot_container::_iterator_kernel;
using snapshot_container::deque_storage_creator;


TEST_CASE("Basic iterator_kernel test", "[iterator_kernel]") {
    deque_storage_creator<int> storage_creator;
    std::vector<int> test_values(16384);
    std::iota(test_values.begin(), test_values.end(), 0);    
    auto ik = _iterator_kernel<int, deque_storage_creator<int>>::create(storage_creator, test_values.begin(), test_values.end());
    auto ik2 = _iterator_kernel<int, deque_storage_creator<int>>::create(ik);
    
    REQUIRE(ik->m_slices[0].size() == test_values.size());
    REQUIRE(ik->m_slices[0].m_storage.use_count() == 2);        
    REQUIRE(ik->m_slices[0] == ik2->m_slices[0]);    
    REQUIRE(std::equal(test_values.begin(), test_values.end(), ik->m_slices[0].begin()));
    REQUIRE(std::equal(ik2->m_slices[0].begin(), ik2->m_slices[0].end(), test_values.begin()));
    
    REQUIRE(ik->slice_index(100) == _iterator_kernel<int, deque_storage_creator<int>>::slice_point(0, 100));
    
    auto cow_point = ik2->slice_index(500);
    
    auto cow_ops_point = ik2->cow_ops(cow_point);
    
    
    REQUIRE(ik2->m_slices.size() == 2);
    REQUIRE(ik->m_slices.size() == 1);
    REQUIRE(ik2->m_slices[0].m_end_index ==  test_values.size() / 2);
    REQUIRE(ik2->m_slices[0].m_storage.use_count() == 1);
    REQUIRE(ik2->m_slices[1].m_storage.use_count() == 2);
    REQUIRE(ik2->m_slices[1].m_start_index == test_values.size() / 2);
    REQUIRE((cow_ops_point.slice() == 0 && cow_ops_point.index() == 500));
    
    ik2.reset();
    REQUIRE(ik->m_slices[0].m_storage.use_count() == 1);
    
}



