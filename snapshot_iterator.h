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
        typedef typename slice_t::fwd_iter_type fwd_iter_type;
        
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
            m_cum_slice_lengths.push_back(0);
        }
        
        template <typename IteratorType>
        _iterator_kernel(const storage_creator_t& storage_creator, IteratorType begin_pos, IteratorType end_pos):
            m_storage_creator(storage_creator)
        {
            m_slices.push_back(slice_t(m_storage_creator(begin_pos, end_pos), 0));
            m_cum_slice_lengths.push_back(m_slices[0].size());
        }

        _iterator_kernel(const _iterator_kernel& rhs) = default;
        _iterator_kernel& operator= (const _iterator_kernel& rhs) = default;
        
        bool is_prev_slice_modifiable(size_t slice)
        {
            if (slice > 0 && m_slices[slice - 1].is_modifiable())
                return true;
            
            return false;
        }
        
        // Copy a range of elements if necessary to guarantee iteration w/ modification of iterated elements
        // will preserve cow semantics. This is optimized for foward iteration. A separate function optimized
        // for reverse iteration will be required when modifiable reverse iterators are supported.
        slice_point iteration_cow_ops(const slice_point& iter_point)
        {            
            auto& slice = m_slices[iter_point.slice()];

            // TODO: Revisit this logic and the next if. This needs refinement
            if (slice.is_modifiable() && slice.size() < 2048)
            {
                return iter_point;
            }
                
            if (slice.size() < 1024)
            {
                if (is_prev_slice_modifiable(iter_point.slice()))
                {
                    auto& prev_slice = m_slices[iter_point.slice() - 1];
                    auto prev_slice_size = prev_slice.size();
                    prev_slice.append(slice.begin(), slice.end());
                    m_cum_slice_lengths[iter_point.slice() -1] = m_cum_slice_lengths[iter_point.slice()];
                    m_cum_slice_lengths.erase(m_cum_slice_lengths.begin() + iter_point.slice());
                    m_slices.erase(m_slices.begin() + iter_point.slice());
                    return slice_point(iter_point.slice() - 1, prev_slice_size + iter_point.index());
                }
                else
                {
                    auto new_slice = slice.copy(0);
                    m_slices[iter_point.slice()] = new_slice;
                    return iter_point;
                }
            }
                            
            auto items_to_copy = slice.size() / 8 + 1;
            if (items_to_copy + iter_point.index() >= slice.size())
                items_to_copy = slice.size() - iter_point.index();
            
            // if the previous slice is modifiable and this access is immediately adjacent to it
            // copy elems into the previous slice instead of creating a new one. This will help decrease
            // fragmentation in some common iteration patterns.
            if (is_prev_slice_modifiable(iter_point.slice()) && iter_point.index() <= slice.size() / 8)
            {                
                auto& prev_slice = m_slices[iter_point.slice() - 1];
                auto prev_slice_size = prev_slice.size();
                prev_slice.append(slice.begin(), slice.begin() + (iter_point.index() + items_to_copy));
                m_cum_slice_lengths[iter_point.slice() - 1] += items_to_copy;                
                if (m_cum_slice_lengths[iter_point.slice() - 1] == m_cum_slice_lengths[iter_point.slice()])
                {
                    // no elems left in element cow_point.slice so remove it
                    m_cum_slice_lengths.erase(m_cum_slice_lengths.begin() + iter_point.slice());
                    m_slices.erase(m_slices.begin() + iter_point.slice());                                    
                }
                else
                {
                    m_slices[iter_point.slice()].m_start_index += iter_point.slice() + items_to_copy;
                }
                return slice_point(iter_point.slice() - 1, prev_slice_size + iter_point.index());
            }
            else
            {
                // insert a new slice
                auto new_slice = slice.copy(iter_point.index(), iter_point.index() + items_to_copy);
                if (slice.size() - items_to_copy == 0)
                {
                    m_slices[iter_point.slice()] = new_slice;
                    return slice_point(iter_point.slice(), 0);
                }
                else
                {
                    m_cum_slice_lengths.insert(m_cum_slice_lengths.begin() + iter_point.slice(), m_cum_slice_lengths[iter_point.slice()] - new_slice.size());
                    m_slices.insert(m_slices.begin() + iter_point.slice(), new_slice);
                    return slice_point(iter_point.slice() - 1, 0);
                }                               
            }            
        }
        
        // cow op related to insertion. The returned slice_point is reasonably optimal for an insertion op.
        // some care is taken to ensure that an empty split does not occur as this would break an invariant
        // of the internal data structures.
        slice_point insert_cow_ops(const slice_point& insert_point)
        {
            if (insert_point.slice() >= m_slices.size())
            {
                throw std::logic_error("Invalid cow point in call to cow_ops");
            }
        
            auto& slice = m_slices[insert_point.slice()];
            if (slice.is_modifiable() && (insert_point.index() <= slice.size()/4 || insert_point.index() + slice.size()/4 >= slice.size()))
            {
                // insert point reasonably near one end of the slice so inserts will be reasonably fast here.
                return insert_point;
            }
            else if (slice.size() < 32)
            {
                auto new_slice = slice.copy(0);
                m_slices[insert_point.slice()] = new_slice;
                return insert_point;
            }
            
            // avoid some corner cases where split point is near the beginning or end of the slice.
            auto copy_index = insert_point.index();
            if (copy_index < 4)
                copy_index = 4;
            else if (copy_index + 4 >= slice.size())
                copy_index = slice.size() - 4;
            
            // This idiom copies on averate 1/4 of the elements in slice to create an insertion point
            // respecting cow semantics
            if (slice.size()/2 > copy_index)
            {                
                auto items_to_copy = copy_index;
                auto new_slice = slice_t(m_storage_creator(slice.begin(), slice.begin() + items_to_copy), 0);
                m_cum_slice_lengths.insert(m_cum_slice_lengths.begin() + insert_point.slice() + 1, m_cum_slice_lengths[insert_point.slice()]);
                m_cum_slice_lengths[insert_point.slice()] = m_cum_slice_lengths[insert_point.slice()] - m_slices[insert_point.slice()].size() + items_to_copy;
                m_slices.insert(m_slices.begin() + insert_point.slice(), new_slice);
                
                m_slices[insert_point.slice() + 1].m_start_index += copy_index;
                return slice_point(insert_point.slice(), insert_point.index());
            }                        
            else
            {                
                auto items_to_copy = slice.size() - copy_index;
                auto new_slice = slice_t(m_storage_creator(slice.end() - items_to_copy, slice.end()), 0);
                m_cum_slice_lengths.insert(m_cum_slice_lengths.begin() + insert_point.slice(), m_cum_slice_lengths[insert_point.slice()] - items_to_copy);
                slice.m_end_index -= items_to_copy;
                m_slices.insert(m_slices.begin() + insert_point.slice() + 1, new_slice);
                return slice_point(insert_point.slice() + 1, insert_point.index() - copy_index);
            }  
        }
        
        size_t container_index(const slice_point& slice_pos) const
        {
            if (m_cum_slice_lengths.size() < slice_pos.slice())
                return m_cum_slice_lengths[m_cum_slice_lengths.size() - 1];
            
            auto slice = m_slices[slice_pos.slice()];
            auto size_upto_slice = m_cum_slice_lengths[slice_pos.slice()];
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
                return (slice_point(0, m_slices[0].size() + container_index - m_cum_slice_lengths[0]));
            }
            else if (m_cum_slice_lengths.size() > 1 && container_index >= m_cum_slice_lengths[m_cum_slice_lengths.size() - 2])
            {
                if (container_index < m_cum_slice_lengths[m_cum_slice_lengths.size() - 1])
                {
                    return slice_point(m_cum_slice_lengths.size() - 1, 
                        m_slices[m_cum_slice_lengths.size() - 1].size() + container_index - m_cum_slice_lengths[m_cum_slice_lengths.size() - 1]);
                }
                else
                {
                    return end();
                }
            }
            else if(m_cum_slice_lengths.size() > 1)
            {            
                return _slice_index_binary(container_index);
            }
            else
            {
                return end();
            }
        }
            
        slice_point insert(const slice_point& insert_before, const T& value)
        {
            // insert value into the container respecting snapshots
            slice_point insert_pos = insert_cow_ops(insert_before);
            m_slices[insert_pos.slice()].insert(insert_pos.index(), value);
            _update_slice_lengths(insert_pos.slice(), 1);
            return insert_pos;                
        }
        
        slice_point insert(const slice_point& insert_before, const fwd_iter_type& start_pos, const fwd_iter_type& end_pos)
        {
            slice_point insert_pos = insert_cow_ops(insert_before);
            auto elems_before_insert = m_slices[insert_pos.slice()].size();
            m_slices[insert_pos.slice()].insert(insert_pos.index(), start_pos, end_pos);
            
            auto inserted_elems = m_slices[insert_pos.slice()].size() - elems_before_insert;            
            _update_slice_lengths(insert_pos.slice(), inserted_elems);                        
            return insert_pos;
        }
        
        
        void _update_slice_lengths(size_t begin_index, ssize_t adjustment)
        {
            for(auto itr = m_cum_slice_lengths.begin() + begin_index; itr != m_cum_slice_lengths.end(); ++itr)
                *itr += adjustment;
        }
        
        
        slice_point _drop_slice(size_t slice)
        {
            // Note that this erase occurs irrespective of the ref counts on the slice.
            m_cum_slice_lengths.erase(m_cum_slice_lengths.begin() + slice);
            m_slices.erase(m_slices.begin() + slice);
            if (m_cum_slice_lengths.size() == 0) {
                // push on an empty slice as there must always be at least one slice in the deck
                m_cum_slice_lengths.push_back(0);
                m_slices.push_back(slice_t(m_storage_creator(), 0));
            }
            return slice_point(slice, 0);            
        }
                
        slice_point remove(const slice_point& remove_pos)
        {
            // Remove element at the specified slice point
            // Returns iterator to element after deletion.
            if (remove_pos.slice() >= m_cum_slice_lengths.size())
                throw std::logic_error("Invalid slice_point to remove");
            
            auto& slice = m_slices[remove_pos.slice()];
            // TODO: Revisit this condition. 
            if (remove_pos.index() >= slice.size())
                throw std::logic_error("Invalid slice_point index to remove");

            _update_slice_lengths(remove_pos.slice(), -1);
                       
            if (slice.size() == 1) 
            {                
                return _drop_slice(remove_pos.slice());
            }
            else if(slice.m_storage.use_count() == 1)
            {
                slice.remove(remove_pos.index());
                return remove_pos;
            }
            else
            {
                // TODO: Improve on this logic by minimizing copying.
                auto new_slice = slice.copy(0);
                m_slices[remove_pos.slice()] = new_slice;
                new_slice.remove(remove_pos.index());
                return remove_pos;
            }
        }
                
        slice_point _remove_within_slice(const slice_point& start_pos, const slice_point& end_pos)
        {
            auto& slice = m_slices[start_pos.slice()];
            
            _update_slice_lengths(start_pos.slice(), end_pos.index() - start_pos.index());            
            if (start_pos.index() == 0 && end_pos.index() == slice.size())
            {
                return _drop_slice(start_pos.slice());
            }
                                 
            if (slice.m_storage.use_count() > 1)
            {            
                // make a copy. TODO: improve efficiency here.
                auto new_slice = slice.copy(0);
                m_slices[start_pos.slice()] = new_slice;
            }
            
            slice.remove(start_pos.index(), end_pos.index());
            return start_pos;
        }
        
        slice_point remove(const slice_point& start_pos, const slice_point& end_pos)
        {
            if (start_pos.slice() >= m_slices.size() || end_pos.slice() >= m_slices.size())
                throw std::logic_error("Invalid slice_point values passed to remove");
        
            auto& start_pos_slice = m_slices[start_pos.slice()];
            auto& end_pos_slice = m_slices[end_pos.slice()];
            
            if ( start_pos.index() > start_pos_slice.size() || end_pos.index() > end_pos_slice.size())
                throw std::logic_error("Invalid slice_point indices passed to remove");
            
            if (start_pos.slice() > end_pos.slice() || (start_pos.slice() == end_pos.slice() && start_pos.index() >= end_pos.index()))
                return end_pos;
            
            // remove is all within the same slice
            if (start_pos.slice() == end_pos.slice())
                return _remove_within_slice(start_pos, end_pos);
        }
        
        
        // convenience function mostly useful for testing. The higher level abstractions will mostly call
        // cow_ops directly to obtain the slice_point which is useful for caching current position of last
        // index op.
        T& operator[](size_t container_index)
        {
            // This variant can update the underlying container thus it is necessary to do cow_ops
            slice_point slice_pos = slice_index(container_index);
            slice_pos = iteration_cow_ops(slice_pos);
            return m_slices[slice_pos.slice()][slice_pos.index()];
        }
        
        const T& operator[](size_t container_index) const
        {
            auto slice_pos = slice_index(container_index);
            return m_slices[slice_pos.slice()][slice_pos.index()];
        }
               
        slice_point begin() const
        {
            if (m_cum_slice_lengths[0] > 0)
                return slice_point(0, 0);
            else
                return end();
        }
                
        slice_point end() const
        {
            return slice_point(m_slices.size() - 1, m_slices[m_slices.size() - 1].size());
        }
            
        slice_point next(const slice_point& current) const
        {
            // move slice_point to the next value
            if (current.slice() >= m_slices.size())
                return end();
            
            auto current_slice = m_slices[current.slice()];
            if (current.index() >= current_slice.size())
            {
                if (current.slice() + 1 == m_slices.size())
                    return end();
                else
                    return slice_point(current.slice() + 1, 0);
            }
            else
            {
                return slice_point(current.slice(), current.index() + 1);
            }            
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
            auto indices_pos = std::lower_bound(m_cum_slice_lengths.begin(), m_cum_slice_lengths.end(), container_index);
            if (indices_pos == m_cum_slice_lengths.end())
                return end();
            
            auto slice_index = indices_pos - m_cum_slice_lengths.begin();
            auto& slice = m_slices[slice_index];
            return slice_point(slice_index, slice.size() + container_index - m_cum_slice_lengths[slice_index]);            
        }
        
        std::vector<slice_t> m_slices;
        std::vector<size_t> m_cum_slice_lengths;
        storage_creator_t m_storage_creator;
    };
}