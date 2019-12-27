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
#include <tuple>


using snapshot_container::_iterator_kernel;
using snapshot_container::deque_storage_creator;
using snapshot_container::iterator;
using config_traits = snapshot_container::_iterator_kernel_config_traits;

template class deque_storage_creator<int>;
template class _iterator_kernel<int, deque_storage_creator<int>>;
template class iterator<int, deque_storage_creator<int>>;


auto test_ik_creator(size_t num_slices, size_t num_values_per_slice)
{
    deque_storage_creator<int> storage_creator;
    std::vector<int> test_values(num_values_per_slice * num_slices);
    std::iota(test_values.begin(), test_values.end(), 0);    
    auto ik = _iterator_kernel<int, deque_storage_creator<int>>::create(storage_creator);
    
    
    for (auto i = 0; i < num_slices; ++i)
    {
        ik->append(test_values.begin() + i*num_values_per_slice, test_values.begin() + (i+1) * num_values_per_slice);        
    }
    return std::make_tuple(ik, test_values);
}


TEST_CASE("Basic iterator_kernel test", "[iterator_kernel]") {
    
    auto [ik, test_values] = test_ik_creator(1, 16384);   
    auto ik2 = _iterator_kernel<int, deque_storage_creator<int>>::create(ik);

    // TODO: Add sections
    REQUIRE(ik->m_slices[0].size() == test_values.size());
    REQUIRE(ik->m_slices[0].m_storage.use_count() == 2);        
    REQUIRE(ik->m_slices[0] == ik2->m_slices[0]);    
    REQUIRE(std::equal(test_values.begin(), test_values.end(), ik->m_slices[0].begin()));
    REQUIRE(std::equal(ik2->m_slices[0].begin(), ik2->m_slices[0].end(), test_values.begin()));
    
    REQUIRE(ik->slice_index(100) == _iterator_kernel<int, deque_storage_creator<int>>::slice_point(0, 100));
    
    auto cow_point = ik2->slice_index(500);    
    auto cow_ops_point = ik2->_insert_cow_ops(cow_point);
        
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
    cow_ops_point = ik3->_insert_cow_ops(cow_point);
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
    
    auto [ik, test_values] = test_ik_creator(1, 128);
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
    auto [ik, test_values] = test_ik_creator(1, 2048);
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


TEST_CASE("basic iterator tests", "[iterator_kernel]") {
 
    auto [ik, test_values] = test_ik_creator(1, 2048);   
    auto end_itr = iterator<int, deque_storage_creator<int>>(ik, ik->end());
    auto itr = iterator<int, deque_storage_creator<int>>(ik, ik->end());
    REQUIRE(std::equal(itr, end_itr, test_values.begin()));

    itr = iterator<int, deque_storage_creator<int>>(ik, ik->begin());
    auto itr2 = itr + 5;
    
    REQUIRE(*itr2 == test_values[5]);
    REQUIRE(itr2 - 5  == itr);
    REQUIRE(itr + 5 == itr2);

    SECTION("increment and decrement checks") {        
        auto itr3 = itr2;
        REQUIRE(++itr3 == itr2 + 1);
        REQUIRE(--itr3 == itr2);
        REQUIRE(itr3++ == itr2);
        REQUIRE(itr3 == itr2 + 1);
        REQUIRE(itr3-- == itr2 + 1);
        REQUIRE(itr3 == itr2);
    }
}


TEST_CASE("complex merge and insert tests", "[iterator kernel]") {
    
    SECTION("merge into previous on insert") {
        // create a max merge size iterator kernel to check merge behavior   
        size_t max_merge_size = config_traits::cow_ops::max_merge_size;
        
        for (auto node_size : {max_merge_size / 2, max_merge_size, 2 * max_merge_size})
        {
            auto insert_pos = node_size / config_traits::cow_ops::copy_fraction_denominator + 1;

            auto [ik, test_values2] = test_ik_creator(1, node_size);
            auto insert_point = ik->slice_index(insert_pos);
            auto insert_index = ik->container_index(insert_point);
            ik->insert(insert_point, 0xdeadbeef);

            REQUIRE(ik->m_cum_slice_lengths[0] == insert_pos + 1);

            std::vector<int> new_values { 10001, 10002, 10003, 10004 };
            auto impl = virtual_iter::std_fwd_iter_impl<std::vector<int>, 48>();
            auto vitr = virtual_iter::fwd_iter<int, 48>(impl, new_values.begin());
            auto vend_itr = virtual_iter::fwd_iter<int, 48>(impl, new_values.end());

            insert_point = ik->slice_index(insert_index + 1);            
            auto slice_size_before_insert = ik->m_slices[0].size();
            ik->insert(insert_point, vitr, vend_itr);
            REQUIRE(ik->m_cum_slice_lengths[0] == slice_size_before_insert + config_traits::cow_ops::slice_edge_offset + (vend_itr - vitr));
            REQUIRE(ik->size() == node_size + 5);
        }
    }

    
    SECTION("insert split past half point of node", "[iterator kernel]") {
        size_t max_merge_size = config_traits::cow_ops::max_merge_size;
        for (auto node_size : {/*max_merge_size / 2, max_merge_size, */2 * max_merge_size})
        {
            auto insert_pos = node_size - (node_size / config_traits::cow_ops::copy_fraction_denominator + 1);
            auto [ik, test_values2] = test_ik_creator(1, node_size);
            auto ik2 = _iterator_kernel<int, deque_storage_creator<int>>::create(ik);
            auto insert_point = ik->slice_index(insert_pos);
            auto insert_index = ik->container_index(insert_point);
            ik->insert(insert_point, 0xdeadbeef);
            REQUIRE(ik->m_cum_slice_lengths[0] == insert_pos);
            auto itr = iterator<int, deque_storage_creator<int>>(ik, ik->slice_index(insert_index));            

            // force non-const iterator access which will force a copy on write
            // !Serious bug. Non insert/remove modification invalidates iterator.
            // seems iterator needs to track more than slice_index to recover from this situation.
            // number of slices can change on non-insert/remove ops but this cannot invalidate iterators.
            
            *(itr - 1) += 1;
            *(itr - 1) -= 1;
            
            auto result = (*(itr - 1) == insert_index - 1);
            REQUIRE(result);
            REQUIRE(*itr == 0xdeadbeef);
        }
    }
    
    
    SECTION("merge into previous on iteration max_merge_size") {
        size_t max_merge_size = config_traits::cow_ops::max_merge_size;
        auto [ik, test_values2] = test_ik_creator(1, max_merge_size);
        auto insert_point = ik->slice_index((max_merge_size / config_traits::cow_ops::copy_fraction_denominator) + 1);
        auto insert_index = ik->container_index(insert_point);
        ik->insert(insert_point, 1024);
        
        auto iter_point = insert_index + 1;
        auto itr = iterator<int, deque_storage_creator<int>>(ik, ik->slice_index(iter_point));
        auto value = *itr;
        REQUIRE(value == insert_index);        
        REQUIRE(*(itr - 1) == 1024);
    }
    
    
    SECTION("merge into previous on iteration > max_merge_size") {
        // TODO: Figure out how to do a looping construct in catch tests
        size_t slice_size = 2 * config_traits::cow_ops::max_merge_size;
        auto [ik, test_values2] = test_ik_creator(1, slice_size);       
        auto insert_point = ik->slice_index(( slice_size / config_traits::cow_ops::copy_fraction_denominator) + 1);
        auto insert_index = ik->container_index(insert_point);
        ik->insert(insert_point, 5019); // some value distinct from what is already there
        auto iter_point = insert_index + 1;
        auto itr = iterator<int, deque_storage_creator<int>>(ik, ik->slice_index(iter_point));        
        auto value = *itr;
        REQUIRE(value == insert_index);
        REQUIRE(*(itr - 1) == 5019);
    }    
}
