#pragma once
/*
 * The MIT License
 *
 * Copyright 2020 Kuberan Naganathan
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
#include "snapshot_iterator.h"


namespace snapshot_container
{   
    // TODO: Support custom allocators. This is going to take quite some effort so will likely occur
    // on a separate branch or in a separate project itself.
    template <typename T, typename StorageCreator=deque_storage_creator<T>, typename ConfigTraits=_iterator_kernel_config_traits>
    class snapshot;
    
    template <typename T, typename StorageCreator=deque_storage_creator<T>, typename ConfigTraits=_iterator_kernel_config_traits>
    class container
    {
    public:        
        typedef StorageCreator storage_creator_t;
        typedef _iterator_kernel<T, StorageCreator, ConfigTraits> kernel_t;
        typedef _iterator<T, StorageCreator> iterator_type;
        typedef std::shared_ptr<kernel_t> shared_kernel_t;
        typedef size_t size_type;
        typedef ssize_t difference_type;
        typedef T* pointer;
        typedef T& reference;
        typedef T value_type;
        typedef container<T, StorageCreator, ConfigTraits> container_t;
        
        container():
            m_kernel(kernel_t::create(storage_creator_t()))
        {
        }
   
        container(const storage_creator_t& creator):
            m_kernel(kernel_t::create(storage_creator_t()))
        {            
        }
        
        template<typename IterType>
        container(const IterType& start_pos, const IterType& end_pos):
            m_kernel(kernel_t::create(storage_creator_t(), start_pos, end_pos))
        {            
        }
        
        template <typename IterType>
        container(const IterType& start_pos, const IterType& end_pos, const storage_creator_t& creator):
            m_kernel(kernel_t::create(creator, start_pos, end_pos))
        {            
        }
        
        container(const container_t& rhs)
        {
            m_kernel->deep_copy(rhs.m_kernel);
        }
        
        container(container_t && rhs) = default;
        
        container_t& operator=(const container_t& rhs)
        {
            m_kernel->deep_copy(rhs.m_kernel);
            return *this;
        }
        
        container_t& operator=(container_t && rhs) = default;     
        ~container() {}
        
        
        iterator_type begin() {return iterator_type(m_kernel, 0);}
        iterator_type end() {return iterator_type(m_kernel, size());}
        const iterator_type begin() const {return iterator_type(m_kernel, 0);}
        const iterator_type end() const {return iterator_type(m_kernel, size());}
        iterator_type iterator(size_t index) {return iterator_type(m_kernel, index);}
        const iterator_type iterator(size_t index) const {return iterator_type(m_kernel, index);}
        
        // It is unsafe to keep pointers or references to elements in container beyond
        // immediate ops. Non-updating actions can invalidate direct references and pointers to elements.
        // These are provided for convenience only. Use the iterator interface instead in order to refer back to a
        // position in the container when any read/write actions intercede obtaining the iterator and re-using it.        
        reference operator[](size_t index) {return (*m_kernel)[index];}
        const reference operator[](size_t index) const {return (*m_kernel)[index];}        
        size_type size() const {return m_kernel->size();}
        
        void clear()
        {
            m_kernel->clear();
        }
                        
        iterator_type insert(const iterator_type& insert_pos, const T& value)
        {
            m_kernel->insert(insert_pos.pos(), value);
            return insert_pos;
        }
        
        template<typename IterType>
        iterator_type insert(const iterator_type& insert_pos, const IterType& start_pos, const IterType& end_pos)
        {
            auto insert_point = m_kernel->insert(insert_pos.pos(), start_pos, end_pos);
            return iterator_type(m_kernel, insert_point);
        }
        
        iterator_type erase(const iterator_type& remove_pos)
        {
            auto result = m_kernel->remove(remove_pos.pos());
            return iterator_type(m_kernel, result);
        }
        
        iterator_type erase(const iterator_type& start_pos, const iterator_type& end_pos)
        {
            auto result = m_kernel->remove(start_pos.pos(), end_pos.pos());
            return iterator_type(m_kernel, result);
        }
                
        template<typename IterType>
        iterator_type append(const iterator_type& start_pos, const iterator_type& end_pos)
        {
            auto result = m_kernel->append(start_pos, end_pos);
            return iterator_type(m_kernel, result);
        }
        
        void push_back(const T& value)
        {
            m_kernel->push_back(value);
        }
                
        void swap(container_t& other) noexcept
        {
            // This is safer than std::swap(m_kernel, other.m_kernel) as
            // doing it this way will not suddenly switch which of this or other that
            // iterators reference.
            // TODO: ensure std::swap works correctly on these types.
            m_kernel->swap(other->m_kernel);
        }
        
        bool empty() const noexcept
        {
            return m_kernel->size() == 0;
        }
        
        // TODO: Improve std::vector compat. Support for emplace, emplace_back etc. Will require some thought.
        // TODO: Reverse iterators
        
    private:
        shared_kernel_t m_kernel;
    };    
}



