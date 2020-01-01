#include "snapshot_slice.h"
#include <memory>
#include <tuple>
#include <algorithm>
#include <iostream>


namespace snapshot_container {

    template<typename T, typename C>
    class iterator;
    
    struct _iterator_kernel_config_traits
    {        
        // These affect how new slices are created and compacted.
        // Below lwm slices are created when convenient
        // above lwm slices compaction begins to occur when iterating with a non-const iterator
        // above hwm new slice creation is limited in favor of copying slices.
        static constexpr size_t num_slices_lwm = 16;
        static constexpr size_t num_slices_hwm = 64;
        struct cow_ops
        {            
            // Min size at which a slice will be split to effect a cow op
            static constexpr size_t min_split_size = 2048;
            
            // Max size block that will be merged directly into previous slice if possible during
            // an iteration cow action
            static constexpr size_t max_merge_size = 1024;
            
            // 1/copy_fraction_denominator is the amount of slice copied beyond the current index
            // when an iteration action triggers a cow copy.
            // This fraction is also the max amount of a slice shifted to effect an insert op without
            // creation of a new slice.
            static constexpr size_t copy_fraction_denominator = 8;
            
            // Max size of a slice that will be copied completely to effect an insertion which preserves copy on write
            // properties.
            static constexpr size_t max_insertion_copy_size = 32;
            
            // cow actions at an offset less than this number from the beginning of a slice or more than this many items
            // from the end of the slice will result in a copy of this many items to effect certain cow ops.
            static constexpr size_t slice_edge_offset = 4;
        };
    };
    
    
    template<typename T, typename StorageCreator,typename ConfigTraits=_iterator_kernel_config_traits>
    class _iterator_kernel : public std::enable_shared_from_this<_iterator_kernel<T,StorageCreator>> {
    public:
        
        friend iterator<T, StorageCreator>;
        
        // Implementation details for the iterator type for snapshot_container. Must be created via 
        // shared ptr. Both the container type and iterators for the container will keep a shared ptr to iterator_kernel.
        static constexpr size_t npos = 0xFFFFFFFFFFFFFFFF;
        typedef StorageCreator storage_creator_t;
        typedef _slice<T> slice_t;
        typedef typename slice_t::fwd_iter_type fwd_iter_type;
        typedef ConfigTraits config_traits;
        
        struct slice_point
        {            
            slice_point(size_t slice, size_t index):
            m_slice(slice),
            m_index(index)
            {                
            }
            
            slice_point():
            m_slice(npos),
            m_index(npos)
            {}
            
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
        m_storage_creator(storage_creator)
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
        
        bool _is_prev_slice_modifiable(size_t slice) const
        {
            if (slice > 0 && m_slices[slice - 1].is_modifiable())
                return true;
            
            return false;
        }
        
        // Copy a range of elements if necessary to guarantee iteration w/ modification of iterated elements
        // will preserve cow semantics. This is optimized for foward iteration. A separate function optimized
        // for reverse iteration will be required when modifiable reverse iterators are supported.
        slice_point _iteration_cow_ops(const slice_point& iter_point)
        {            
            auto& slice = m_slices[iter_point.slice()];
            
            // edge case issues
            if (iter_point.index() == slice.size())
            {
                if (iter_point.slice() < m_slices.size() - 1)
                    return _iteration_cow_ops(slice_point(iter_point.slice() + 1, 0));
                else
                    return iter_point;
            }
            
            if (slice.is_modifiable() && m_slices.size() <= config_traits::num_slices_lwm)
                return iter_point;
            
            if (_is_prev_slice_modifiable(iter_point.slice())) 
            {
                if (slice.size() <= config_traits::cow_ops::max_merge_size)
                {
                    auto& prev_slice = m_slices[iter_point.slice() - 1];
                    auto prev_slice_size = prev_slice.size();
                    prev_slice.append(slice.begin(), slice.end());
                    m_cum_slice_lengths[iter_point.slice() -1] = m_cum_slice_lengths[iter_point.slice()];
                    m_cum_slice_lengths.erase(m_cum_slice_lengths.begin() + iter_point.slice());
                    m_slices.erase(m_slices.begin() + iter_point.slice());
                    return slice_point(iter_point.slice() - 1, prev_slice_size + iter_point.index());                
                }
                else if (iter_point.index() <= slice.size() / config_traits::cow_ops::copy_fraction_denominator)
                {
                    auto items_to_copy = slice.size() / config_traits::cow_ops::copy_fraction_denominator + 1;
                    if (items_to_copy + iter_point.index() >= slice.size())
                        items_to_copy = slice.size() - iter_point.index();                

                    auto& prev_slice = m_slices[iter_point.slice() - 1];
                    auto prev_slice_size = prev_slice.size();
                    prev_slice.append(slice.begin(), slice.begin() + (iter_point.index() + items_to_copy));
                    m_cum_slice_lengths[iter_point.slice() - 1] += items_to_copy + iter_point.index();                
                    if (m_cum_slice_lengths[iter_point.slice() - 1] == m_cum_slice_lengths[iter_point.slice()])
                    {
                        // no elems left in element cow_point.slice so remove it
                        m_cum_slice_lengths.erase(m_cum_slice_lengths.begin() + iter_point.slice());
                        m_slices.erase(m_slices.begin() + iter_point.slice());                                    
                    }
                    else
                    {
                        m_slices[iter_point.slice()].m_start_index += iter_point.index() + items_to_copy;
                    }
                    return slice_point(iter_point.slice() - 1, prev_slice_size + iter_point.index());                    
                }
            }

            // previous slice is not modifiable or current slice cannot be merged into it.
            if (slice.is_modifiable())
                return iter_point;
           
            // slice is not modifiable. If num slices is above hwm or slice is small enough, just copy it
            if (m_slices.size() > config_traits::num_slices_hwm || slice.size() <= config_traits::cow_ops::max_insertion_copy_size)            
            {
                // make a copy of the slice
                auto new_slice = slice.copy(0);
                m_slices[iter_point.slice()] = new_slice;
                return iter_point;
            }
        
            // Copy out a range of elements into a new slice to allow for writable iteration over the copied slice
            if (iter_point.index() < slice.size()/2)
            {
                // TODO: Improve this logic to copy less.
                auto extra_items_to_copy = slice.size() / config_traits::cow_ops::copy_fraction_denominator;                
                auto new_slice = slice.copy(0, iter_point.index() + extra_items_to_copy);
                auto cum_slice_length = iter_point.slice() == 0 ? new_slice.size() : m_cum_slice_lengths[iter_point.slice() - 1] + new_slice.size();
                m_cum_slice_lengths.insert(m_cum_slice_lengths.begin() + iter_point.slice(), cum_slice_length);
                slice.m_start_index += iter_point.index() + extra_items_to_copy;
                m_slices.insert(m_slices.begin() + iter_point.slice(), new_slice);                
                return slice_point(iter_point.slice(), iter_point.index());
            }
            else
            {
                // copy to end of slice
                auto items_to_copy = slice.size() - iter_point.index();                
                if (items_to_copy < config_traits::cow_ops::slice_edge_offset)
                    items_to_copy = config_traits::cow_ops::slice_edge_offset;
                
                auto slice_size = slice.size();
                auto new_slice = slice.copy(slice_size - items_to_copy);
                auto cum_slice_length = m_cum_slice_lengths[iter_point.slice()] - items_to_copy;
                m_cum_slice_lengths.insert(m_cum_slice_lengths.begin() + iter_point.slice(), cum_slice_length);
                slice.m_end_index-= items_to_copy;
                m_slices.insert(m_slices.begin() + iter_point.slice() + 1, new_slice);
                return slice_point(iter_point.slice() + 1, iter_point.index() - (slice_size - items_to_copy));                
            }                                   
        }
        
        // cow op related to insertion. The returned slice_point is reasonably optimal for an insertion op.
        // some care is taken to ensure that an empty split does not occur as this would break an invariant
        // of the internal data structures.
        slice_point _insert_cow_ops(const slice_point& insert_point)
        {
            if (insert_point.slice() >= m_slices.size())
            {
                throw std::logic_error("Invalid cow point in call to cow_ops");
            }
                        
            size_t copy_fraction = config_traits::cow_ops::copy_fraction_denominator;        
            auto& slice = m_slices[insert_point.slice()];            
            if (slice.is_modifiable()) 
            {                
                if (m_slices.size() > config_traits::num_slices_hwm || 
                    insert_point.index() <= slice.size()/copy_fraction || insert_point.index() + slice.size()/copy_fraction >= slice.size())
                {
                    // insert point reasonably near one end of the slice so inserts will be reasonably fast here.
                    // also insert directly into slice if the number of slices is above hwm.
                    return insert_point;
                }
            }
            
            if (m_slices.size() > config_traits::num_slices_hwm || slice.size() <= config_traits::cow_ops::max_insertion_copy_size)
            {
                auto new_slice = slice.copy(0);
                m_slices[insert_point.slice()] = new_slice;
                return insert_point;
            }
            
            // avoid some corner cases where split point is near the beginning or end of the slice.
            auto copy_index = insert_point.index();
            if (copy_index < config_traits::cow_ops::slice_edge_offset)
                copy_index = config_traits::cow_ops::slice_edge_offset;
            else if (copy_index + config_traits::cow_ops::slice_edge_offset >= slice.size())
                copy_index = slice.size() - config_traits::cow_ops::slice_edge_offset;
            
                        
            // This idiom copies on average 1/4 of the elements in slice to create an insertion point
            // respecting cow semantics
            if (slice.size()/2 > copy_index)
            {                
                if (_is_prev_slice_modifiable(insert_point.slice()))
                {
                    auto& prev_slice = m_slices[insert_point.slice() - 1];
                    auto prev_slice_size = prev_slice.size();
                    prev_slice.append(slice.begin(), slice.begin() + copy_index);
                    m_cum_slice_lengths[insert_point.slice() - 1] += copy_index;                    
                    slice.m_start_index += copy_index;
                    return slice_point(insert_point.slice() - 1, prev_slice_size + insert_point.index());
                }
                
                auto items_to_copy = copy_index;
                auto new_slice = slice_t(m_storage_creator(slice.begin(), slice.begin() + items_to_copy), 0);
                m_cum_slice_lengths.insert(m_cum_slice_lengths.begin() + insert_point.slice() + 1, m_cum_slice_lengths[insert_point.slice()]);
                m_cum_slice_lengths[insert_point.slice()] = m_cum_slice_lengths[insert_point.slice()] - m_slices[insert_point.slice()].size() + items_to_copy;
                m_slices.insert(m_slices.begin() + insert_point.slice(), new_slice);
                
                m_slices[insert_point.slice() + 1].m_start_index += items_to_copy;
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
            _incr_update_count();
            slice_point insert_pos = _insert_cow_ops(insert_before);
            m_slices[insert_pos.slice()].insert(insert_pos.index(), value);
            _update_slice_lengths(insert_pos.slice(), 1);
            return insert_pos;                
        }
        
        slice_point insert(const slice_point& insert_before, const fwd_iter_type& start_pos, const fwd_iter_type& end_pos)
        {
            _incr_update_count();            
            slice_point insert_pos = _insert_cow_ops(insert_before);
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
            auto drop_slice_size = m_slices[slice].size();            
            m_cum_slice_lengths.erase(m_cum_slice_lengths.begin() + slice);
            m_slices.erase(m_slices.begin() + slice);
            if (m_cum_slice_lengths.size() == 0) 
            {
                // push on an empty slice as there must always be at least one slice in the deck
                m_cum_slice_lengths.push_back(0);
                m_slices.push_back(slice_t(m_storage_creator(), 0));
            }
            else
            {
                _update_slice_lengths(slice, -1 * drop_slice_size);
            }
                        
            return slice_point(slice, 0);            
        }
                
        slice_point remove(const slice_point& remove_pos)
        {
            _incr_update_count();
            
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
                        
            if (start_pos.index() == 0 && end_pos.index() == slice.size())
            {
                return _drop_slice(start_pos.slice());
            }
            
            auto num_elems_to_remove = end_pos.index() - start_pos.index();
            auto original_size = slice.size();
            _update_slice_lengths(start_pos.slice(), -1 * num_elems_to_remove);
            
            if (slice.m_storage.use_count() > 1)
            {            
                if (start_pos.index() == 0)
                {
                    auto new_slice = slice.copy(end_pos.index(), slice.size());
                    m_slices[start_pos.slice()] = new_slice;
                }
                else if(end_pos.index() == slice.size())
                {
                    auto new_slice = slice.copy(0, start_pos.index());
                    m_slices[start_pos.slice()] = new_slice;
                }
                else
                {
                    // an inner range of the slice is to be removed.
                    auto new_slice = slice.copy(0, start_pos.index());
                    new_slice.append(slice.begin() + end_pos.index(), slice.end());
                    m_slices[start_pos.slice()] = new_slice;
                }
            }
            else
            {            
                slice.remove(start_pos.index(), end_pos.index());
            }
            
            if (original_size == start_pos.index() + num_elems_to_remove)
                return slice_point(start_pos.slice() + 1, 0);
            else
                return start_pos;
        }
        
        slice_point remove(const slice_point& start_pos, const slice_point& end_pos)
        {
            _incr_update_count();
            if (start_pos.slice() >= m_slices.size() || end_pos.slice() >= m_slices.size())
            {                
                throw std::logic_error("Invalid slice_point values passed to remove");
            }
                
            auto& start_pos_slice = m_slices[start_pos.slice()];
            auto& end_pos_slice = m_slices[end_pos.slice()];
            
            if ( start_pos.index() > start_pos_slice.size() || end_pos.index() > end_pos_slice.size())
            {
                std::cerr << "start index: " << start_pos.index() << " max: " << start_pos_slice.size() << std::endl;
                std::cerr << "end index: " << end_pos.index() << " max: " << end_pos_slice.size() << std::endl;
                throw std::logic_error("Invalid slice_point indices passed to remove");
            }
                
            if (start_pos.slice() > end_pos.slice() || (start_pos.slice() == end_pos.slice() && start_pos.index() >= end_pos.index()))
                return end_pos;
            
            // remove is all within the same slice
            if (start_pos.slice() == end_pos.slice())
                return _remove_within_slice(start_pos, end_pos);
        
            // range to remove spans slices.
            auto end_slice = end_pos.slice();
            auto current_slice = start_pos.slice();
            auto current_slice_index = start_pos.index();
            while (current_slice < end_slice)
            {
                if (current_slice_index == 0)
                {
                    _drop_slice(current_slice);
                    end_slice -= 1;
                }
                else
                {
                    _remove_within_slice(slice_point(current_slice, current_slice_index), 
                                                     slice_point(current_slice, m_slices[current_slice].size()));
                    current_slice_index = 0;
                    current_slice += 1;
                }                
            }
        
            if (end_pos.index() != 0)
                _remove_within_slice(slice_point(end_slice, 0), slice_point(end_slice, end_pos.index()));
         
            if (start_pos.slice() == m_slices.size() - 1)
                return start_pos;
            else
                return slice_point(start_pos.slice() + 1, 0);
        }
                
        // convenience function mostly useful for testing. The higher level abstractions will mostly call
        // cow_ops directly to obtain the slice_point which is useful for caching current position of last
        // index op.
        T& operator[](size_t container_index)
        {
            _incr_update_count();
            // This variant can update the underlying container thus it is necessary to do cow_ops
            slice_point slice_pos = slice_index(container_index);
            slice_pos = _iteration_cow_ops(slice_pos);
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
            
        slice_point next(const slice_point& current, size_t incr = 1) const
        {
            // move slice_point to the next value
            if (current.slice() >= m_slices.size())
                return end();
            
            if(incr == 1 && current.index() < m_slices[current.slice()].size())
            {
                if(current.index() + 1 == m_slices[current.slice()].size())
                {
                    if(current.slice() < m_slices.size() - 1)
                        return slice_point(current.slice() + 1, 0);
                    else
                        return end();
                }
                return slice_point(current.slice(), current.index() + 1);
            }
            
            auto index = container_index(current);
            if (index + incr < size())
                return slice_index(index + incr);
            else
                return end();            
        }
        
        slice_point prev(const slice_point& current, size_t decr = 1) const
        {
            if (current.slice() >= m_slices.size())
                return end();

            if(decr == 1)
            {
                if(current.index() == 0 && current.slice() == 0)
                    return end(); // TODO: Needs revisiting if reverse iterators will be supported
            
                if(current.index() == 0)
                    return slice_point(current.slice() - 1, m_slices[current.slice() - 1].size() - 1);
                else
                    return slice_point(current.slice(), current.index() - 1);
            }
        
            auto index = container_index(current);
            // TODO: Need rend() for reverse iterators. One possibility is that slice_point(npos, npos) can be rend()
            if (index > size())
                return end();
            
            if (index - decr >= 0)
                return slice_index(index - decr);
            return end();
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
        
        size_t size() const
        {
            return *(m_cum_slice_lengths.end() - 1);
        }
        
        size_t num_slices() const
        {
            return m_slices.size();
        }
        
        double fragmentation_index() const
        {
            // Returns num_slices * (1.0 - (num elements)/(storage size))
            // A value close to 0 indicates low fragmentation and a
            // value close to num_slices indicates a high degree of fragmentation
            double num_elements = 0.0;
            double storage_size = 0.0;
            for (auto& slice : m_slices)
            {
                num_elements += slice.size();
                storage_size += slice.storage_size();
            }
            return num_slices() * (1.0 - (num_elements/storage_size));
        }
        
        template <typename IterType>
        slice_point append(const IterType& start_pos, const IterType& end_pos)
        {
            // Append creates a new slice with the requisite elements and add this to the end
            // of the container.
            // returns the position of first element appended. Returns end() if start_pos == end_pos
            if (start_pos == end_pos)
                return end();
            
            auto pre_append_size = size();
            if (pre_append_size == 0)
            {
                m_slices.clear();
                m_cum_slice_lengths.clear();
            }
            
            auto new_slice = slice_t(m_storage_creator(start_pos, end_pos), 0);
            m_slices.push_back(new_slice);
            m_cum_slice_lengths.push_back(pre_append_size + new_slice.size());
            return slice_index(pre_append_size);
        }
        
        bool integrity_check() const
        {
            // Check for referential integrity. Returns true if check passes and false otherwise
            // 1. size integrity
            // 2. slice length integrity
            
            if (m_cum_slice_lengths.size() != m_slices.size())
            {
                std::cerr << "Slices and cum slice lengths don't have same size" << std::endl;
                return false;
            }
            
            auto expected_total_size = size();
            size_t actual_total_size = 0;
            for(auto& slice: m_slices)
            {
                actual_total_size += slice.size();
            }
            if (expected_total_size != actual_total_size)
            {
                    std::cerr << "Expected size: " << expected_total_size << " Actual size: " << 
                        actual_total_size << std::endl;
                    return false;
            }
            
            auto size_upto_here = 0;
            for (size_t i = 0; i < m_cum_slice_lengths.size(); ++i)
            {
                if (m_cum_slice_lengths[i] - size_upto_here != m_slices[i].size())
                {
                    std::cerr << "Referential integrity break at slice index " << i << std::endl;
                    std::cerr << "Total slices: " << m_slices.size() << std::endl;
                    std::cerr << "Cum size: " << m_cum_slice_lengths[i] << std::endl;
                    std::cerr << "Prev cum size: " << size_upto_here << std::endl;
                    std::cerr << "Expected size: " << m_cum_slice_lengths[i] - size_upto_here << std::endl;
                    std::cerr << "Actual size: " << m_slices[i].size() << std::endl;
                    return false;
                }                
                size_upto_here = m_cum_slice_lengths[i];
            }
                        
            return true;
        }
        
#ifndef _SNAPSHOTCONTAINER_TEST        
    private:
#endif        
    
        size_t get_update_count()
        {
            return m_update_count;
        }
        
        void _incr_update_count() { ++m_update_count; }
               
        slice_point _slice_index_binary(size_t container_index) const
        {
            auto indices_pos = std::lower_bound(m_cum_slice_lengths.begin(), m_cum_slice_lengths.end(), container_index);
            if (indices_pos == m_cum_slice_lengths.end())
                return end();
            
            auto slice_index = indices_pos - m_cum_slice_lengths.begin();
            if (container_index == m_cum_slice_lengths[slice_index])
            {
                slice_index += 1;
                if (slice_index == m_cum_slice_lengths.size())
                    return end();
            }
            auto& slice = m_slices[slice_index];
            return slice_point(slice_index, slice.size() + container_index - m_cum_slice_lengths[slice_index]);            
        }
        
        std::vector<slice_t> m_slices;
        std::vector<size_t> m_cum_slice_lengths;
        storage_creator_t m_storage_creator;
        size_t m_update_count = 0; // indicator to iterators that state changed
    };
    
    template<typename T, typename StorageCreator>
    class iterator {
    public:
        static constexpr size_t npos = 0xFFFFFFFFFFFFFFFF;
        using iterator_kernel_t = _iterator_kernel<T, StorageCreator>;
        typedef typename iterator_kernel_t::slice_t slice_t;
        typedef typename iterator_kernel_t::slice_point slice_point;
        typedef std::random_access_iterator_tag iterator_category;
        typedef T value_type;
        typedef ssize_t difference_type;
        typedef T* pointer;
        typedef T& reference;

        iterator() = default;

        // TODO: Only the higher container type should be able to use this constructor directly.
        iterator(const std::shared_ptr<iterator_kernel_t>& kernel, const slice_point& iter_pos) :
        m_kernel(kernel),
        m_iter_pos(iter_pos),
        m_current_slice(nullptr),
        m_update_count(npos),
        m_container_index(0)
        {
            m_container_index = m_kernel->container_index(m_iter_pos);
        }

        iterator(const std::shared_ptr<iterator_kernel_t>& kernel, size_t container_index):
            m_kernel(kernel),
            m_iter_pos(),
            m_current_slice(nullptr),
            m_update_count(npos),
            m_container_index(container_index)
        {
            if (m_container_index != npos)
                m_iter_pos = m_kernel->slice_index(container_index);
        }
                
        iterator(const iterator& rhs) = default;
        iterator& operator=(const iterator& rhs) = default;
        
        iterator& operator+=(ssize_t incr)
        {
            return _prefix_plusplus_impl(incr);
        }
        
        const iterator& operator+=(ssize_t incr) const
        {
            return _prefix_plusplus_impl(incr);
        }
        
        iterator& operator -=(ssize_t decr)
        {
            return _prefix_minusminus_impl(decr);
        }
        
        const iterator& operator-=(ssize_t decr) const
        {
            return _prefix_minusminus_impl(decr);
        }
                
        const iterator& operator++() const 
        {
            return _prefix_plusplus_impl();
        }

        iterator& operator++() 
        {
            return _prefix_plusplus_impl();
        }

        const iterator operator++(int) const
        {
            iterator pos(*this);
            _prefix_plusplus_impl();
            return pos;
        }
        
        iterator operator++(int)
        {
            iterator pos(*this);
            _prefix_plusplus_impl();
            return pos;
        }
        
        iterator& operator--()
        {
            return _prefix_minusminus_impl();
        }
        
        const iterator& operator--() const
        {
            return _prefix_minusminus_impl();
        }
        
        iterator operator--(int)
        {
            iterator pos(*this);
            _prefix_minusminus_impl();
            return pos;
        }        
        
        const iterator operator--(int) const
        {
            iterator pos(*this);
            _prefix_minusminus_impl();
            return pos;
        }          
        
        T& operator * ()
        {
            return _dereference_impl();
        }
        
        const T& operator * () const
        {
            return _dereference_impl();
        }
        
        T* operator->()
        {
            return &(_dereference_impl());
        }
        
        const T* operator->() const
        {
            return &(_dereference_impl());
        }
        
        bool operator < (const iterator& rhs) const
        {
            if (not (m_kernel && m_kernel == rhs.m_kernel))
                return false;
        
            // npos is the max size_t value although it is interpreted as the smallest container index value
            if (m_container_index == npos)
                return true;
            if (rhs.m_container_index == npos)
                return false;
            return m_container_index < rhs.m_container_index;            
        }
        
        bool operator > (const iterator& rhs) const
        {
            if (not (m_kernel && m_kernel == rhs.m_kernel))
                return false;

            if (m_container_index == npos)
                return false;
            if (rhs.m_container_index == npos)
                return true;
            
            return m_container_index > rhs.m_container_index;            
        }
        
        bool operator <= (const iterator& rhs) const
        {
            if (not (m_kernel && m_kernel == rhs.m_kernel))
                return false;            
            
            return m_container_index <= rhs.m_container_index;
        }
        
        bool operator >= (const iterator& rhs) const
        {
            if (not (m_kernel && m_kernel == rhs.m_kernel))
                return false;            

            return m_container_index >= rhs.m_container_index;
        }
        
        bool operator == (const iterator& rhs) const
        {
            if (not (m_kernel && m_kernel == rhs.m_kernel))
                return false;

            return m_container_index == rhs.m_container_index;
        }
        
        bool operator != (const iterator& rhs) const
        {
            return !(*this == rhs);
        }
        
        difference_type operator-(const iterator& rhs) const 
        {
            if (not m_kernel)
                return 0x7FFFFFFFFFFFFFFF;
                        
            if (m_kernel && rhs.m_kernel == m_kernel)
                return rhs.m_container_index - m_container_index;
            else
                throw std::logic_error("Invalid iterator subtraction");
        }

        iterator operator-(difference_type offset)
        {
            if (not m_kernel)
                return iterator();
                
            if (offset > m_container_index)
                return iterator(m_kernel, npos);
                        
            return iterator(m_kernel, m_container_index - offset);
        }        
        
        const iterator operator-(difference_type offset) const 
        {
            return const_cast<iterator*>(this)->operator-(offset);
        }

        iterator operator+(difference_type offset)
        {        
            if (not m_kernel)
                return iterator();
        
            if (m_container_index + offset > m_kernel->size())
                return iterator(m_kernel, m_kernel->size());
            return iterator(m_kernel, m_container_index + offset);
        }

        const iterator operator+(difference_type offset) const
        {        
            return const_cast<iterator*>(this)->operator+(offset);
        }
                
    protected:
                       
        iterator& _prefix_plusplus_impl(ssize_t incr=1)
        {
            if (not m_kernel)
                return *this;

            if (incr == 1 && m_kernel->get_update_count() == m_update_count && m_current_slice) 
            {
                // the state of the underlying container has not changed since the last modification to the iterator
                // thus cached state can be used to determine next iter position.
                if (m_current_slice->size() > m_iter_pos.index() + 1) 
                {
                    m_iter_pos = slice_point(m_iter_pos.slice(), m_iter_pos.index() + 1);
                    m_container_index += 1;
                    return *this;
                }
                // move to next slice or to end pos            
                if (m_iter_pos.slice() < m_kernel->m_slices.size() - 1)
                {
                    m_iter_pos = slice_point(m_iter_pos.slice() + 1, 0);                    
                    m_current_slice = &m_kernel->m_slices[m_iter_pos.slice()];
                }
                else
                {
                    m_iter_pos = m_kernel->end();
                }
                m_container_index += 1;
                return *this;    
            }

            m_current_slice = nullptr;                       
            m_update_count = m_kernel->get_update_count();
            m_iter_pos = m_kernel->slice_index(m_container_index + incr);
            if (m_iter_pos.slice() < m_kernel->m_slices.size())
                m_current_slice = &m_kernel->m_slices[m_iter_pos.slice()];
            
            if (m_current_slice)
                m_container_index += incr;
            else
                m_container_index = m_kernel->size();
            
            return *this;
        }

        const iterator& _prefix_plusplus_impl(ssize_t incr=1) const
        {
            return const_cast<const iterator&>(const_cast<iterator*>(this)->_prefix_plusplus_impl(incr));
        }
        
        iterator& _prefix_minusminus_impl(ssize_t decr=1) 
        {
            if (not m_kernel)
                return *this;

            if (decr == 1 && m_kernel->get_update_count() == m_update_count && m_current_slice) 
            {
                if (m_iter_pos.index() > 0) 
                {
                    m_iter_pos = slice_point(m_iter_pos.slice(), m_iter_pos.index() - 1);
                    m_container_index -= 1;
                    return *this;
                }

                if (m_iter_pos.slice() > 0) 
                {
                    m_iter_pos = slice_point(m_iter_pos.slice() - 1, m_kernel->m_slices[m_iter_pos.slice() - 1].size() - 1);
                    m_current_slice = &m_kernel->m_slices[m_iter_pos.slice()];
                } else 
                {
                    *this = iterator();
                }
                m_container_index -= 1;
                return *this;
            }

            m_current_slice = nullptr;
            
            // this is decrementing to before the first element. This is an invalid state but it can
            // occur in reverse iteration loops.
            if (decr > m_container_index)
            {
                m_container_index = npos; // + 1 will return to a valid position
                m_update_count = npos;
                return *this;
            }
                
            m_update_count = m_kernel->get_update_count();
            m_iter_pos = m_kernel->slice_index(m_container_index - decr);
            if (m_iter_pos.slice() < m_kernel->m_slices.size())
                m_current_slice = &m_kernel->m_slices[m_iter_pos.slice()];
            
            m_container_index -= decr;
            return *this;
        }

        const iterator& _prefix_minusminus_impl(ssize_t decr=1) const
        {
            return const_cast<const iterator&>(const_cast<iterator*>(this)->_prefix_minusminus_impl(decr));
        }
        
        T& _dereference_impl()
        {
            if (not m_kernel)
                throw std::logic_error("Invalid iterator dereference (no kernel)");
            
            if (m_update_count == m_kernel->get_update_count() && m_current_slice && m_current_slice->m_storage.use_count() == 1)
                return (*m_current_slice)[m_iter_pos.index()];
                           
            m_current_slice = nullptr;            
            if (m_container_index == npos)
                throw std::logic_error("Invalid iterator dereference (npos)");
            
            m_iter_pos = m_kernel->slice_index(m_container_index);
            auto new_iter_pos = m_kernel->_iteration_cow_ops(m_iter_pos);
            
            if (new_iter_pos == m_kernel->end())
                throw std::logic_error("Invalid iterator dereference (end)");
            
            m_update_count = m_kernel->get_update_count();
            
            m_iter_pos = new_iter_pos;
            m_current_slice = &m_kernel->m_slices[m_iter_pos.slice()];
            return (*m_current_slice)[m_iter_pos.index()];
        }
                
        const T& _dereference_impl() const
        {
            if (!m_kernel)
                throw std::logic_error("Invalid iterator dereference (no kernel)");
            
            // This version doesn't need to call cow_ops
            if (m_update_count == m_kernel->get_update_count() && m_current_slice)
               return (*m_current_slice)[m_iter_pos.index()]; 
         
            m_current_slice = nullptr; 
            if (m_container_index == npos)
                throw std::logic_error("Invalid iterator dereference (npos)");
            m_iter_pos = m_kernel->slice_index(m_container_index);
             
            if (m_iter_pos == m_kernel->end())
                throw std::logic_error("Invalid iterator dereference (end)");
            
            m_current_slice = &m_kernel->m_slices[m_iter_pos.slice()];
            return (*m_current_slice)[m_iter_pos.index()];            
        }
                
        mutable slice_t* m_current_slice;
        mutable size_t m_update_count;
        mutable size_t m_container_index;
        mutable slice_point m_iter_pos;        
        std::shared_ptr<iterator_kernel_t> m_kernel;        
    };
}