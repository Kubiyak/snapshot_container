/***********************************************************************************************************************
 * snapshot_container:
 * A temporal sequentially accessible container type.
 * Released under the terms of the MIT license:
 * https://opensource.org/licenses/MIT
 **********************************************************************************************************************/
#include "snapshot_slice.h"
#include <vector>
#include <exception>

class snapshot_container
{
    
    template <typename T, typename StorageCreator>
    struct _kernel
    {   
        static size_t npos = size_t(-1);
        using typename storage_base<T>::shared_base_t;
        typedef StorageCreator storage_creator_t;
        
        _kernel(storage_creator_t&& storage_creator):
            m_last_index_lookup(0),
            m_last_indices_pos(0),
            m_storage_creator(storage_creator)
        {            
            m_slices.push_back(_Slice(m_storage_creator(), 0));
            m_indices.push_back(0);
        }
        
        template <typename IterType>
        _kernel(IterType begin, IterType end, storage_creator_t&& storage_creator):
            m_last_index_lookup(0),
            m_last_indices_pos(0),
            m_storage_creator(storage_creator)
        {
            m_slices.push_back(_Slice(m_storage_creator(begin, end), 0));
            m_indices.push_back(m_slices[0].size());
        }
        
        std::tuple<_Slice, size_t> _cow_ops(user_requested_index, slice_index)
        {
            auto max_index = m_indices[slice_index];
            auto& slice_to_split = m_slices[slice_index];
            // If the slice is less than 1024 items just copy it entirely.
            if (slice_to_split.size() <= 1024)
            {
                auto new_slice = slice_to_split.copy(slice_to_split.start_index);
                m_slices[slice_index] = new_slice;
                return std::tuple<_Slice, size_t>(new_slice, slice_index);
            }
            else
            {
                // split slice in 2 for now but consider more strategies for this
                auto slice_len = slice_to_split.size();
                auto min_slice_index = m_indices[slice_index] - slice_len;
                if (user_requested_index <= (min_slice_index + slice_len/2))
                {
                    auto new_slice = slice_to_split.copy(slice_to_split.start_index, slice_to_split.start_index + (slice_len)/2);
                    slice_to_split.start_index = slice_to_split.start_index + (slice_len/2);
                    m_slices.insert(m_slices.begin() + slice_index, new_slice);
                    m_indices.insert(m_indices.begin() + slice_index, m_indices[slice_index] - slice_len/2);
                    return std::tuple<_Slice, size_t>(new_slice, slice_index);
                }
                else
                {
                    auto new_slice = slice_to_split.copy(slice_to_split.start_index + (slice_len/2), slice_to_split.end_index);
                    slice_to_split.end_index = slice_to_split.start_index + (slice_len/2);
                    m_indices.insert(m_indices.begin() + slice_index + 1, m_indices[slice_index]);
                    m_indices[slice_index] -= slice_len/2;
                    m_slices.insert(m_slices.begin() + slice_index + 1, new_slice);
                    return std::tuple<_Slice, size_t>(new_slice, slice_index + 1);                                                                  
                }
            }
        }
                      
        size_t _locate_slice_index(size_t index, bool check_refcount)
        {
            auto slice_index = 0;
            m_last_slice = nullptr;
                                           
            auto num_slices = m_slices.size();
            // Todo: switch to binary search if there are more than 8 slices
            for(; slice_index < num_slices; ++slice_index)
            {
                if (m_indices[slice_index] > index)
                    break;                
            }
            
            if (slice_index == num_slices || m_slices[slice_index].m_end_index < index)
            {
                throw std::out_of_range("Out of bounds index into container");
            }

            auto referenced_slice = nullptr;
            if ((check_refcount && m_slices[slice_index].m_data.use_count > 1) || not m_slices[slice_index].is_modifiable())
            {
                // The slice on which a reference is to be returned is referenced
                // in a snapshot and must be copied.
                [referenced_slice, slice_index] = _cow_ops(index, slice_index);          
            }
            else
            {
                referenced_slice = m_slices[slice_index];
            }                       
            return slice_index;
        }
        
        template <typename IterType>
        void insert(size_t index, IterType begin, IterType end)
        {
            // The semantics of insert are that the elements are inserted before index.
            // There are multiple cases to consider depending on the size of the slice
            // to insert into, whether the slice is referenced in a snapshot etc.
            if (m_last_slice && m_min_abs_index < index && index <= m_max_abs_index)
            {
                if (m_last_slice.m_data.use_count > 1 || not m_slices[slice_index].is_modifiable())
                {
                    auto [new_slice, slice_index] = _cow_ops(index, slice_index);
                    auto max_elems = m_indices[slice_index];
                    new_slice.insert(index - (max_elems - new_slice.size()), begin, end);
                    // fix up sizes
                    
                }
            }
            else
            {
                
            }
        }
        
        // This struct is the basis of a snapshot        
        T& operator [] (size_t index)
        {
            if (m_last_slice && index >= m_min_abs_index && index < m_max_abs_index)
            {
                // Note the user is expected to pass a valid index or this will segv
                return (*m_last_slice)[index - m_min_abs_index];
            }
            
            auto slice_index = _locate_index(index, true);
            m_last_slice = &m_slices[slice_index];             
            m_max_abs_index = m_indices[slice_index];
            m_min_abs_index = m_max_abs_index - m_slice->size();
            return (*m_last_slice)
        }
             
        // These are helpers to speed up index lookups
        _Slice* m_last_slice;
        // These represent the range of the container's indices in m_last_slice
        size_t m_min_abs_index;
        size_t m_max_abs_index
        
        std::vector<_Slice<T>> m_slices;  
        std::vector<size_t> m_indices; // Support for indexing when multiple slices exist
    };
    
    template <typename StorageCreator>
    class RWContainer
    {
        typedef StorageCreator storage_creator_t;
        
    public:
        RWContainer(storage_creator_t&& storage_creator = storage_creator_t()):
            m_storage_creator(storage_creator),
            m_kernel(new _kernel)
        {            
            m_kernel->m_slices.push_back(_Slice(m_storage_creator(), 0));            
        }
        
        template <typename IterType>
        RWContainer(IterType begin, IterType end, storage_creator_t&& storage_creator = storage_creator_t()):
            m_storage_creator(storage_creator),
            m_kernel(new _kernel)
        {
            m_kernel->m_slices.push_back(_Slice(m_storage_creator(begin, end), 0));
        }
        
    private:
        std::shared_ptr<_kernel> m_kernel;
        storage_creator_t m_storage_creator;
    };
};
