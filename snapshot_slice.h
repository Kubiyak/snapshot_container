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
    class slice
    {
    public:

        typedef typename storage_base<T>::shared_base_t shared_base_t;
        typedef typename storage_base<T>::fwd_iter_type fwd_iter_type;

        slice(shared_base_t &storage, size_t startIndex, size_t endIndex = storage_base<T>::npos):
            m_startIndex(startIndex),
            m_endIndex(endIndex),
            m_storage(storage)
        {
            if (m_endIndex == storage_base<T>::npos)
                m_endIndex = m_storage->size();
        }

        // Note that slices work like shared_ptrs on copy construction and assignment.  Call copy to obtain
        // a deep copy.
        slice(const slice<T>& rhs) = default;
        slice& operator = (const slice<T>& rhs) = default;

        // Append must not be called on slices not overlapping the end of the storage element.
        // The calling code should verify that before calling append.
        void append(const T& t)
        {
            m_storage->append(t);
            m_endIndex += 1;
        }

        // Append must not be called on slices not overlapping the end of the storage element.
        void append(fwd_iter_type& startPos, const fwd_iter_type& endPos)
        {
            m_storage->append(startPos, endPos);
            m_endIndex = m_storage->size();
        }

        slice<T> copy(size_t startIndex, size_t endIndex = storage_base<T>::npos) const
        {
            // This is how to obtain an empty copy (i.e. an empty slice based on the same impl
            // as this slice.
            if (startIndex > endIndex)
                startIndex = endIndex;

            shared_base_t storageCopy = m_storage->copy(startIndex, endIndex);
            return slice<T>(storageCopy, 0, endIndex - startIndex);
        }

        // Insert must not be called on slices not co-terminus with the end of the storage element.
        void insert(size_t index, const T& t)
        {
            m_storage->insert(m_startIndex + index, t);
            m_endIndex += 1;
        }

        // Insert must not be called on slices not co-terminus with the end of the storage elememt.
        void insert(size_t index, fwd_iter_type& startPos, const fwd_iter_type& endPos)
        {
            m_storage->insert(m_startIndex + index, startPos, endPos);
            m_endIndex = m_storage->size();
        }

        // Remove can only be called on slices extending to the end of the storage element.
        virtual void remove(size_t index)
        {
            m_storage->remove(m_startIndex + index);
            m_endIndex -= 1;
        }

        size_t size() const
        {
            return m_endIndex - m_startIndex;
        }

        void update(size_t index, const T& newValue)
        {
            m_storage->update(m_startIndex + index, newValue);
        }

        shared_base_t& operator->() {return m_storage;}

        fwd_iter_type begin() {return m_storage->iterator(m_startIndex);}
        fwd_iter_type end() {return m_storage->iterator(m_endIndex);}
        const fwd_iter_type begin() const {return m_storage->iterator(m_startIndex);}
        const fwd_iter_type end() const {return m_storage->iterator(m_endIndex);}

        size_t storage_size() const {return m_storage->size();}

        const T& operator [] (size_t index) const
        {
            return m_storage->operator[](index + m_startIndex);
        }

    private:
        size_t m_startIndex;
        size_t m_endIndex;
        shared_base_t m_storage;
    };
}
