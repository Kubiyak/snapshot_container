#include "snapshot_slice.h"
#include <memory>
#include <tuple>
#include <algorithm>

namespace snapshot_container {

    template<typename T, typename StorageCreator>
    class _iterator_kernel : public std::enable_shared_from_this<_iterator_kernel<T,StorageCreator>> {
    public:
        // Implementation details for the iterator type for snapshot_container. Must be created via 
        // shared ptr. Both the container type and iterators for the container will keep a shared ptr to iterator_kernel.
        static constexpr size_t npos = 0xFFFFFFFFFFFFFFFF;
        typedef StorageCreator storage_creator_t;
        typedef _slice<T> slice_t;
        
        struct slice_point
        {            
            slice_point(size_t slice, size_t index):
            m_slice(slice),
            m_index(index)
            {                
            }
            
            bool operator == (const slice_point& rhs) const
            {
                return (m_slice == rhs.m_slice && m_index == rhs.m_index);
            }
            
            size_t m_slice;
            size_t m_index;
            bool valid() const { return m_slice != npos;}
            size_t slice() const { return m_slice; }
            size_t index() const { return m_index; }
        };
        
        _iterator_kernel(const storage_creator_t& storage_creator) :
        m_storage_creator(std::forward(storage_creator))
        {
            m_slices.push_back(slice_t(m_storage_creator(), 0));
            m_indices.push_back(0);
        }
        
        template <typename IteratorType>
        _iterator_kernel(const storage_creator_t& storage_creator, IteratorType begin_pos, IteratorType end_pos):
            m_storage_creator(storage_creator)
        {
            m_slices.push_back(slice_t(m_storage_creator(begin_pos, end_pos), 0));
            m_indices.push_back(m_slices[0].size());
        }

        _iterator_kernel(const _iterator_kernel& rhs) = default;
        _iterator_kernel& operator= (const _iterator_kernel& rhs) = default;
        
        // Perform copy on write actions on the specified slice.
        // The slice can be returned as is, completely copied, or split at cow_point
        // depending on the size of slice and references to the slice from other kernels.
        // Returns newly created slice index in m_slices and index within that slice corresponding to original index represented by cow_point
        slice_point cow_ops(const slice_point& cow_point)
        {
            if (cow_point.slice() >= m_slices.size())
            {
                throw std::logic_error("Invalid cow point in call to cow_ops");
            }
        
            auto& slice = m_slices[cow_point.slice()];
            if (slice.is_modifiable() && slice.size() < 32)
            {
                return cow_point;
            }
            
            if (slice.size() < 32)
            {
                // copy the  slice if it has a relatively small number of elements.
                auto new_slice = slice.copy(0);
                m_slices[cow_point.slice()] = new_slice;
                return cow_point;
            }
                       
            if (slice.size()/2 > cow_point.index())
            {
                if (cow_point.index() == 0 && cow_point.slice() != 0 && 
                    m_slices[cow_point.slice() - 1].is_modifiable())
                {
                    return slice_point(cow_point.slice() - 1, m_slices[cow_point.slice() - 1].size());
                }
                
                auto items_to_copy = cow_point.index();
                auto new_slice = slice_t(m_storage_creator(slice.begin(), slice.begin() + items_to_copy), 0);
                m_indices.insert(m_indices.begin() + cow_point.slice() + 1, m_indices[cow_point.slice()]);
                m_indices[cow_point.slice()] = m_indices[cow_point.slice()] - m_slices[cow_point.slice()].size() + items_to_copy;
                m_slices.insert(m_slices.begin() + cow_point.slice(), new_slice);
                
                m_slices[cow_point.slice() + 1].m_start_index += cow_point.index();
                return slice_point(cow_point.slice(), cow_point.index());
            }                        
            else
            {
                if (cow_point.index() == slice.size() && cow_point.slice() != m_slices.size() - 1 && 
                    m_slices[cow_point.slice() + 1].is_modifiable())
                { 
                    // This assumes efficient insertion at the beginning of a collection.
                    // todo: base this on underlying features of storage
                    return slice_point(cow_point.slice() + 1, 0);
                }
                
                auto items_to_copy = slice.size() - cow_point.index();
                auto new_slice = slice_t(m_storage_creator(slice.end() - items_to_copy, slice.end()), 0);
                m_indices.insert(m_indices.begin() + cow_point.slice(), m_indices[cow_point.slice()] - items_to_copy);
                slice.m_end_index -= items_to_copy;
                m_slices.insert(m_slices.begin() + cow_point.slice() + 1, new_slice);
                return slice_point(cow_point.slice() + 1, 0);
            }  
        }
        
        size_t container_index(const slice_point& slice_pos) const
        {
            if (m_indices.size() < slice_pos.slice())
                return m_indices[m_indices.size() - 1];
            
            auto slice = m_slices[slice_pos.slice()];
            auto size_upto_slice = m_indices[slice_pos.slice()];
            return (size_upto_slice + slice_pos.index() - slice.size());
        }
        
        slice_point slice_index(size_t container_index) const
        {
            // Maps index within the kernel to the index of m_slices where that index resides and
            // the index within that slice. this is the basis of many iterator ops.
            // Out of bounds index accesses return the end iterator position.
        
            // handle some common cases fast
            if (container_index < m_slices[0].size())
            {
                return (slice_point(0, m_slices[0].size() + container_index - m_indices[0]));
            }
            else if (m_indices.size() > 1 && container_index >= m_indices[m_indices.size() - 2])
            {
                if (container_index < m_indices[m_indices.size() - 1])
                {
                    return slice_point(m_indices.size() - 1, 
                        m_slices[m_indices.size() - 1].size() + container_index - m_indices[m_indices.size() - 1]);
                }
                else
                {
                    return end();
                }
            }
            else if(m_indices.size() > 1)
            {            
                return _slice_index_binary(container_index);
            }
            else
            {
                return end();
            }
        }
        
        slice_point begin() const
        {
            if (m_indices[0] > 0)
                return slice_point(0, 0);
            else
                return end();
        }
                
        slice_point end() const
        {
            return slice_point(m_slices.size() - 1, m_slices[m_slices.size() - 1].size());
        }
            
        static std::shared_ptr<_iterator_kernel<T, StorageCreator>> create(const StorageCreator& creator)
        {
            return std::make_shared<_iterator_kernel<T,StorageCreator>>(creator);
        }
        
        template <typename IterType>
        static std::shared_ptr<_iterator_kernel<T, StorageCreator>> create(const StorageCreator& creator, 
                                                                          IterType begin_pos, IterType end_pos)
        {
            return std::make_shared<_iterator_kernel<T, StorageCreator>>(creator, begin_pos, end_pos);
        }
        
        static std::shared_ptr<_iterator_kernel<T, StorageCreator>> create(const std::shared_ptr<_iterator_kernel<T, StorageCreator>>& rhs)
        {
            if (rhs)
                return std::make_shared<_iterator_kernel<T, StorageCreator>>(*rhs);
            else
                throw std::logic_error("Called create with an empty shared pointer");
        }
                
#ifndef _SNAPSHOTCONTAINER_TEST        
    private:
#endif        
        
        slice_point _slice_index_binary(size_t container_index) const
        {
            auto indices_pos = std::lower_bound(m_indices.begin(), m_indices.end(), container_index);
            if (indices_pos == m_indices.end())
                return end();
            
            auto slice_index = indices_pos - m_indices.begin();
            auto& slice = m_slices[slice_index];
            return slice_point(slice_index, slice.size() + container_index - m_indices[slice_index]);            
        }
        
        std::vector<slice_t> m_slices;
        std::vector<size_t> m_indices;
        storage_creator_t m_storage_creator;
    };
}