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

    // TODO: Add sections
    REQUIRE(ik->m_slices[0].size() == test_values.size());
    REQUIRE(ik->m_slices[0].m_storage.use_count() == 2);        
    REQUIRE(ik->m_slices[0] == ik2->m_slices[0]);    
    REQUIRE(std::equal(test_values.begin(), test_values.end(), ik->m_slices[0].begin()));
    REQUIRE(std::equal(ik2->m_slices[0].begin(), ik2->m_slices[0].end(), test_values.begin()));
    
    REQUIRE(ik->slice_index(100) == _iterator_kernel<int, deque_storage_creator<int>>::slice_point(0, 100));
    
    auto cow_point = ik2->slice_index(500);    
    auto cow_ops_point = ik2->insert_cow_ops(cow_point);
        
    REQUIRE(ik2->m_slices.size() == 2);
    REQUIRE(ik->m_slices.size() == 1);
    REQUIRE(ik2->m_slices[0].m_end_index == 500);
    REQUIRE(ik2->m_slices[1].m_start_index == 500);
    REQUIRE(ik2->m_slices[0].m_storage.use_count() == 1);
    REQUIRE(ik2->m_slices[1].m_storage.use_count() == 2);
    
    REQUIRE((cow_ops_point.slice() == 0 && cow_ops_point.index() == 500));
    // TODO: Add iterator check over entire iterator_kernel
    
    ik2.reset();
    REQUIRE(ik->m_slices[0].m_storage.use_count() == 1);
    
    auto ik3 = _iterator_kernel<int, deque_storage_creator<int>>::create(ik);
    cow_point = ik3->slice_index(10000);
    cow_ops_point = ik3->insert_cow_ops(cow_point);
    REQUIRE(ik3->container_index(cow_ops_point) == 10000);
    REQUIRE(ik3->m_slices[0].m_end_index == 10000);
    REQUIRE(ik3->m_slices[1].m_start_index == 0);
    
    REQUIRE(ik3->m_slices[0].size() == 10000);
    REQUIRE(ik3->m_slices[1].size() == test_values.size() - 10000);
    REQUIRE(std::equal(test_values.begin(), test_values.begin() + 10000, ik3->m_slices[0].begin()));
    REQUIRE(std::equal(test_values.begin() + 10000, test_values.end(), ik3->m_slices[1].begin()));
    
    REQUIRE(cow_ops_point.slice() == 1);
    REQUIRE(cow_ops_point.index() == 0);
}


TEST_CASE("iterator_kernel insert tests", "[iterator_kernel]") {
    
    deque_storage_creator<int> storage_creator;
    std::vector<int> test_values(128);
    std::iota(test_values.begin(), test_values.end(), 0);    
    auto ik = _iterator_kernel<int, deque_storage_creator<int>>::create(storage_creator, test_values.begin(), test_values.end());
    auto ik2 = _iterator_kernel<int, deque_storage_creator<int>>::create(ik);    
    auto ik3 = _iterator_kernel<int, deque_storage_creator<int>>::create(ik);
    
    auto insert_pos = ik2->slice_index(53);
    ik2->insert(insert_pos, 1024);
    REQUIRE((*ik2)[53] == 1024);
    REQUIRE((*ik3)[53] == 53);

    ik2.reset();
    ik3.reset();
    REQUIRE(std::equal(test_values.begin(), test_values.end(), ik->m_slices[0].begin()));

    ik2 = _iterator_kernel<int, deque_storage_creator<int>>::create(ik); 
    std::vector<int> new_values{1001, 1002, 1003, 1004, 1005};
    
    auto impl = virtual_iter::std_fwd_iter_impl<std::vector<int>, 48>();
    virtual_iter::fwd_iter<int, 48> itr (impl, new_values.begin());
    virtual_iter::fwd_iter<int, 48> end_itr (impl, new_values.end());
    
    
    insert_pos = ik2->slice_index(72);
    auto insert_iter = ik2->insert(insert_pos, itr, end_itr);
    REQUIRE((*ik2)[72] == 1001);
    REQUIRE(std::equal(test_values.begin(), test_values.end(), ik->m_slices[0].begin()));   
}


TEST_CASE("iterator_kernel remove tests", "[iterator_kernel]") {
 
    deque_storage_creator<int> storage_creator;
    std::vector<int> test_values(2048);
    std::iota(test_values.begin(), test_values.end(), 0);    
    auto ik = _iterator_kernel<int, deque_storage_creator<int>>::create(storage_creator, test_values.begin(), test_values.end());
    auto ik2 = _iterator_kernel<int, deque_storage_creator<int>>::create(ik);    
    auto ik3 = _iterator_kernel<int, deque_storage_creator<int>>::create(ik);

    auto remove_pos = ik3->slice_index(79);
    ik3->remove(remove_pos);
    REQUIRE(std::equal(test_values.begin(), test_values.end(), ik->m_slices[0].begin()));
    REQUIRE(ik3->m_slices[0][79] == 80);
    REQUIRE(ik3->size() == 2047);
    REQUIRE(ik2->size() == 2048);
    
    std::vector<int> new_values {10000,10001,10002,10003};
    auto impl = virtual_iter::std_fwd_iter_impl<std::vector<int>, 48>();
    virtual_iter::fwd_iter<int, 48> itr (impl, new_values.begin());
    virtual_iter::fwd_iter<int, 48> end_itr (impl, new_values.end());
        
    auto insert_pos = ik2->slice_index(79);
    ik2->insert(insert_pos, itr, end_itr);
    REQUIRE(ik2->size() == 2052);
    REQUIRE(ik2->m_slices.size() == 2);
    remove_pos = ik2->slice_index(75);
    auto end_remove_pos = ik2->slice_index(100);
    ik2->remove(remove_pos, end_remove_pos);
    REQUIRE(ik2->size() == 2052 - 25);
}

