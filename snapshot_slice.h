/***********************************************************************************************************************
 * snapshot_container:
 * A temporal sequentially accessible container type.
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

        typedef typename storage_base<T>::shared_base_t shared_base_t;
        typedef typename storage_base<T>::fwd_iter_type fwd_iter_type;
                
        _slice(const shared_base_t &storage, size_t start_index, size_t end_index = storage_base<T>::npos):
            m_start_index(start_index),
            m_end_index(end_index),
            m_storage(storage)
        {
            if (m_end_index == storage_base<T>::npos)
                m_end_index = m_storage->size();
        }

        // Note that slices work like shared_ptrs on copy construction and assignment.  Call copy to obtain
        // a deep copy.
        _slice(const _slice<T>& rhs) = default;
        _slice& operator = (const _slice<T>& rhs) = default;

        // Append must only be called on extensible slices
        void append(const T& t)
        {
            m_storage->append(t);
            m_end_index += 1;
        }

        // Must only be called on extensible slices
        void append(fwd_iter_type& start_pos, const fwd_iter_type& end_pos)
        {
            m_storage->append(start_pos, end_pos);
            m_end_index = m_storage->size();
        }

        bool is_modifiable () const
        {
            return (m_start_index == 0 && m_end_index == m_storage->size() && m_storage.use_count() == 1);
        }
                
        _slice<T> copy(size_t start_index, size_t end_index = storage_base<T>::npos) const
        {
            // This is how to obtain an empty copy (i.e. an empty slice based on the same impl
            // as this slice.
            if (start_index > end_index)
                start_index = end_index;

            shared_base_t storage_copy = m_storage->copy(start_index, end_index);
            return _slice<T>(storage_copy, 0, end_index - start_index);
        }

        // Insert must not be called on slices not co-terminus with the end of the storage element.
        void insert(size_t index, const T& t)
        {
            m_storage->insert(m_start_index + index, t);
            m_end_index += 1;
        }

        // Insert must not be called on slices not co-terminus with the end of the storage elememt.
        void insert(size_t index, fwd_iter_type& startPos, const fwd_iter_type& endPos)
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

        size_t size() const
        {
            return m_end_index - m_start_index;
        }

        shared_base_t& operator->() {return m_storage;}

        fwd_iter_type begin() {return m_storage->iterator(m_start_index);}
        fwd_iter_type end() {return m_storage->iterator(m_end_index);}
        const fwd_iter_type begin() const {return m_storage->iterator(m_start_index);}
        const fwd_iter_type end() const {return m_storage->iterator(m_end_index);}

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
        
        size_t m_start_index;
        size_t m_end_index;
        shared_base_t m_storage;
    };
}
