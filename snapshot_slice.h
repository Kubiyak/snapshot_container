/***********************************************************************************************************************
 * snapshot_container:
 * A temporal sequentially accessible container type.
 * Copyright 2019 Kuberan Naganathan
 * Released under the terms of the MIT license:
 * https://opensource.org/licenses/MIT
 **********************************************************************************************************************/
#pragma once

#include <deque>
#include <functional>
#include <iterator>
#include <memory>
#include <sys/types.h>
#include "snapshot_storage.h"
#include "virtual_std_iter.h"

#pragma once

namespace snapshot_container
{

    // A slice maintains a valid index range over a storage element.
    template <typename T>
    class _slice
    {
    public:

        // TODO: Generalize slices to work for non-random access storage types also.
        typedef storage_base<T, 48, virtual_iter::rand_iter<T, 48>> storage_base_t;
        typedef typename storage_base_t::shared_base_t shared_base_t;
        typedef typename storage_base_t::fwd_iter_type fwd_iter_type;
        typedef typename storage_base_t::rand_iter_type rand_iter_type;        
        typedef typename storage_base_t::storage_iter_type storage_iter_type;
        
        _slice(const shared_base_t &storage, size_t start_index, size_t end_index = storage_base_t::npos):
            m_start_index(start_index),
            m_end_index(end_index),
            m_storage(storage)
        {
            if (m_end_index == storage_base_t::npos)
                m_end_index = m_storage->size();
        }

        // Note that slices work like shared_ptrs on copy construction and assignment.  Call copy to obtain
        // a deep copy.
        _slice(const _slice<T>& rhs):
            m_storage(rhs.m_storage),
            m_start_index(rhs.m_start_index),
            m_end_index(rhs.m_end_index)
        {
            
        }
        
        _slice& operator = (const _slice<T>& rhs)
        {
            if (this == &rhs)
                return *this;
            
            m_start_index = rhs.m_start_index;
            m_end_index = rhs.m_end_index;
            m_storage = rhs.m_storage;            
            return *this;
        }

                       
        // Append must only be called on extensible slices
        void append(const T& t)
        {
            m_storage->append(t);
            m_end_index += 1;
        }

        // Must only be called on extensible slices
        void append(const fwd_iter_type& start_pos, const fwd_iter_type& end_pos)
        {
            m_storage->append(start_pos, end_pos);
            m_end_index = m_storage->size();
        }
        
        void append(const rand_iter_type& start_pos, const rand_iter_type& end_pos)
        {
            m_storage->append(start_pos, end_pos);
            m_end_index = m_storage->size();
        }

        bool is_modifiable () const
        {
            return (m_start_index == 0 && m_end_index == m_storage->size() && m_storage.use_count() == 1);
        }
                
        _slice<T> copy(size_t start_index, size_t end_index = storage_base_t::npos) const
        {
            // This is how to obtain an empty copy (i.e. an empty slice based on the same impl
            // as this slice.
            if (start_index > end_index)
                start_index = end_index;

            if (end_index == storage_base_t::npos)
                end_index = size();
            
            shared_base_t storage_copy = m_storage->copy(m_start_index + start_index, m_start_index + end_index);
            return _slice<T>(storage_copy, 0, end_index - start_index);
        }

        // Insert must not be called on slices not co-terminus with the end of the storage element.
        void insert(size_t index, const T& t)
        {
            m_storage->insert(m_start_index + index, t);
            m_end_index += 1;
        }

        // Insert must not be called on slices not co-terminus with the end of the storage elememt.
        void insert(size_t index, const fwd_iter_type& startPos, const fwd_iter_type& endPos)
        {
            m_storage->insert(m_start_index + index, startPos, endPos);
            m_end_index = m_storage->size();
        }

        void insert(size_t index, const rand_iter_type& startPos, const rand_iter_type& endPos)
        {
            m_storage->insert(m_start_index + index, startPos, endPos);
            m_end_index = m_storage->size();
        }        
        
        // Remove can only be called on slices extending to the end of the storage element.
        virtual void remove(size_t index)
        {
            m_storage->remove(m_start_index + index);
            m_end_index -= 1;
        }
        
        virtual void remove(size_t start_index, size_t end_index)
        {
            m_storage->remove(m_start_index + start_index, m_start_index + end_index);
            m_end_index -= (end_index - start_index);
        }

        size_t size() const
        {
            return m_end_index - m_start_index;
        }

        shared_base_t& operator->() {return m_storage;}

        storage_iter_type begin() {return m_storage->iterator(m_start_index);}
        storage_iter_type end() {return m_storage->iterator(m_end_index);}
        const storage_iter_type begin() const {return m_storage->iterator(m_start_index);}
        const storage_iter_type end() const {return m_storage->iterator(m_end_index);}

        size_t storage_size() const {return m_storage->size();}

        const T& operator [] (size_t index) const
        {
            return m_storage->operator[](index + m_start_index);
        }

        // The higher level abstraction will need to track ref counts to slices and only permit this operation
        // directly when this ref count is 1.  When it is a higher value, the higher level container will need to
        // effect a copy operation and only call this on a newly created slice after the copy.
        T& operator [] (size_t index)
        {
            return m_storage->operator[](index + m_start_index);
        }
        
        bool operator == (const _slice<T>& rhs) const
        {
            if (this == &rhs)
                return true;
            
            return (m_start_index == rhs.m_start_index && m_end_index == rhs.m_end_index && m_storage == rhs.m_storage);
        }
        
        size_t m_start_index;
        size_t m_end_index;
        shared_base_t m_storage;
    };
}
