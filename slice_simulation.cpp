/*
 * The MIT License
 *
 * Copyright 2019 Kuberan Naganathan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "snapshot_storage.h"
#include "snapshot_iterator.h"
#include <memory>
#include <map>
#include <vector>
#include <iostream>
#include <random>
#include <ctime>


using snapshot_container::_iterator_kernel;
using snapshot_container::deque_storage_creator;
using snapshot_container::iterator;
typedef _iterator_kernel<int, deque_storage_creator<int>> ikernel;


auto test_ik_creator(size_t num_slices, size_t num_values_per_slice)
{
    deque_storage_creator<int> storage_creator;
    std::vector<int> test_values(num_values_per_slice * num_slices);
    std::iota(test_values.begin(), test_values.end(), 0);    
    auto ik = ikernel::create(storage_creator);
    
    
    for (auto i = 0; i < num_slices; ++i)
    {
        ik->append(test_values.begin() + i*num_values_per_slice, test_values.begin() + (i+1) * num_values_per_slice);        
    }
    return ik;
}


struct slice_stats
{
    slice_stats(const std::shared_ptr<ikernel>& ik):
    m_min(ik->num_slices()),
    m_max(ik->num_slices())
    {
        
    }
    
    void record(const std::shared_ptr<ikernel>& ik)
    {
        auto current_num_slices = ik->num_slices();
        if (current_num_slices > m_max)
            m_max = current_num_slices;
        else if (current_num_slices < m_min)
            m_min = current_num_slices;
        
        size_t fragmentation = size_t(ik->fragmentation_index() * 100.0);
        m_fragmentation_histogram[fragmentation] += 1;
    }
    
    void display_stats()
    {
        std::cout << "********************" << std::endl;
        std::cout << "Min slices: " << m_min << std::endl;
        std::cout << "Max slices: " << m_max << std::endl;
        std::cout << "Fragmentation histogram:" << std::endl;
        for (auto itr =  m_fragmentation_histogram.begin(); itr != m_fragmentation_histogram.end(); ++itr)
        {
            std::cout << itr->first << ": " << itr->second << std::endl;
        }        
        std::cout << "********************" << std::endl;
    }
        
    size_t m_min;
    size_t m_max;
    std::map<size_t, size_t> m_fragmentation_histogram;
};


struct IKSimRunner
{            
    void insert_action(std::shared_ptr<ikernel>& ik, 
                       std::default_random_engine& generator,
                       std::uniform_int_distribution<size_t>& distrib)
    {
        auto ik_size = ik->size();
        auto num_items_to_insert = ik_size > 1000 ? ik_size / 100 : 10;
        std::vector<int> items_to_insert(num_items_to_insert, 0xdeadbeef);
        auto impl = virtual_iter::std_fwd_iter_impl<std::vector<int>, 48>();
        virtual_iter::fwd_iter<int, 48> itr (impl, items_to_insert.begin());
        virtual_iter::fwd_iter<int, 48> end_itr (impl, items_to_insert.end());
        auto insert_index = distrib(generator) % ik_size;
        if (std::distance(itr, end_itr) != items_to_insert.size())
        {
            std::cerr << "Detected problem w/ forward iterator: " << end_itr - itr << std::endl;
            std::terminate();
        }
        else
        {
            std::cerr << "Inserting " << end_itr - itr << " items at " << insert_index << " of " << ik_size << std::endl;
        }
        
        
        auto insert_slice_point = ik->slice_index(insert_index);
        ik->insert(insert_slice_point, itr, end_itr);
    }
    
    void remove_action(std::shared_ptr<ikernel>& ik, 
                       std::default_random_engine& generator,
                       std::uniform_int_distribution<size_t>& distrib)
    {
        auto ik_size = ik->size();
        // This removes on average less than the insertions so the container's size should increase
        auto max_items_to_remove = ik_size > 1000 ? ik_size / 200 : 5;
        auto remove_start = distrib(generator) % ik_size;
        auto remove_end = remove_start + max_items_to_remove < ik_size ? max_items_to_remove + remove_start : ik_size;
        ik->remove(ik->slice_index(remove_start), ik->slice_index(remove_end));
    }
    
    void iter_action(std::shared_ptr<ikernel>& ik, 
                     std::default_random_engine& generator,
                     std::uniform_int_distribution<size_t>& distrib)
    {
        auto ik_size = ik->size();        
        auto max_iteration_length = ik_size > 1000 ? ik_size / 10 : 100;
        auto iter_start = distrib(generator) % ik_size;
        auto iter_end = iter_start + max_iteration_length < ik_size ? max_iteration_length : ik_size - iter_start;
        
        iterator<int, deque_storage_creator<int>> current_pos(ik, iter_start);
        iterator<int, deque_storage_creator<int>> end_pos(ik, iter_end);
        for(; current_pos < end_pos; ++current_pos)
            *current_pos;        
    }
    
    typedef void (IKSimRunner::*action_functions)(std::shared_ptr<ikernel>&, std::default_random_engine&, 
                  std::uniform_int_distribution<size_t>&);
    
    // TODO: Add more plausible action types
    slice_stats run(size_t slice_size=2048, size_t num_slices=1, size_t num_iterations=1000000);
};


slice_stats IKSimRunner::run(size_t slice_size, size_t num_slices, size_t num_iterations)
{
    if (slice_size < 500)
        slice_size = 500;

    auto ik = test_ik_creator(num_slices, slice_size);
    auto ik2 = ikernel::create(ik); // this is a snapshot. This turns on the copy on write logic.
    auto stats = slice_stats(ik);

    std::default_random_engine generator;
    generator.seed(std::time(nullptr));        
    std::uniform_int_distribution<size_t> distribution(0,2);
    std::uniform_int_distribution<size_t> action_distribution(0, 4294967295);

    IKSimRunner::action_functions action_table[] = {&IKSimRunner::insert_action, &IKSimRunner::remove_action, &IKSimRunner::iter_action};

    for(auto i = 0; i < num_iterations; ++i)
    {
        auto action = distribution(generator);
        (this->*action_table[action])(ik, generator, action_distribution);
        stats.record(ik);
    
        if (i + 1 % 10000 == 0)
        {
            stats.display_stats();
        }
    }

    return stats;
}


int main()
{
    IKSimRunner runner;
    auto results = runner.run(2048, 2, 1000);    
    results.display_stats();    
    return 0;
}
