/***********************************************************************************************************************
 * snapshot_container:
 * A temporal sequentially accessible container type.
 * Copyright 2019 Kuberan Naganathan
 * Released under the terms of the MIT license:
 * https://opensource.org/licenses/MIT
 **********************************************************************************************************************/

#pragma once

#include "virtual_iter.h"

namespace virtual_iter
{
    // Implementation of fwd_iter around some c++ std container types.
    template <typename Container, size_t IterMemSize, typename IterType=fwd_iter<typename Container::value_type, IterMemSize> >
    class std_fwd_iter_impl_base: virtual public _fwd_iter_impl_base<typename Container::value_type, IterMemSize, IterType>
    {
    public:
        typedef typename Container::value_type value_type;
        typedef _fwd_iter_impl_base<value_type, IterMemSize, IterType> impl_base_t;
        typedef std::shared_ptr<impl_base_t> shared_base_t;
        typedef IterType iterator_type;
        using difference_type = typename impl_base_t::difference_type;
        
        struct _IterStore
        {
            typename Container::const_iterator m_itr;

            template <typename IteratorType>
            _IterStore(IteratorType& itr):
                    m_itr (itr)
            {
            }
        };
        
        const iterator_type&
        plusplus(const iterator_type& obj) const override
        {
            auto iterStore = reinterpret_cast<_IterStore*>(impl_base_t::mem (obj));

            ++iterStore->m_itr;
            return obj;
        }

        iterator_type& plusplus(iterator_type& obj) const override
        {
            auto iterStore = reinterpret_cast<_IterStore*>(impl_base_t::mem (obj));
            ++iterStore->m_itr;
            return obj;
        }

        void destroy(iterator_type& obj) const override
        {
            _IterStore* iterStore =
                    reinterpret_cast<_IterStore*>(impl_base_t::mem (obj));
            iterStore->~_IterStore();
        }

        bool equals(const iterator_type& lhs, const iterator_type& rhs) const override
        {
            auto lhs_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (
                    lhs));
            auto rhs_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (
                    rhs));

            return lhs_store->m_itr == rhs_store->m_itr;
        }

        ssize_t distance(const iterator_type& lhs, const iterator_type& rhs) const override
        {
            auto lhs_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (lhs));
            auto rhs_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (rhs));

            return lhs_store->m_itr - rhs_store->m_itr;
        }
     
        const value_type* pointer(const iterator_type& arg) const override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (arg));
            return (iter_store->m_itr).operator-> ();
        }

        const value_type& reference(const iterator_type& arg) const override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (arg));
            return (iter_store->m_itr).operator* ();
        }


        // This function should be specialized for the specific type of iterator.  This currently
        // assumes a random access iterator supporting  a constant time distance (subtraction) op.
        size_t copy(value_type* result_ptr, size_t max_items, void* iter, void* end_iter) const override
        {
            auto lhs_iter = reinterpret_cast<_IterStore*>(iter);
            auto rhs_iter = reinterpret_cast<_IterStore*>(end_iter);
            ssize_t distance_to_end = rhs_iter->m_itr - lhs_iter->m_itr;

            if (distance_to_end <= 0)
                return 0;

            if (distance_to_end < max_items)
                max_items = (size_t) distance_to_end;

            size_t copy_count = 0;
            while (copy_count < max_items)
            {
                *result_ptr++ = *lhs_iter->m_itr++;
                ++copy_count;
            }
            return copy_count;
        }

        void visit(void* iter, void* end_iter, std::function<bool(const value_type&)>& f) const override
        {
            auto lhs_iter = reinterpret_cast<_IterStore*>(iter);
            auto rhs_iter = reinterpret_cast<_IterStore*>(end_iter);

            typename Container::difference_type diff = rhs_iter->m_itr - lhs_iter->m_itr;

            for (size_t i = 0; i < diff; ++i)
            {
                if (!f (*lhs_iter->m_itr))
                    return;

                ++lhs_iter->m_itr;
            }
        }
    };


   // Implementation of fwd_iter around some c++ std container types.
    template <typename Container, size_t IterMemSize, typename IterType=fwd_iter<typename Container::value_type, IterMemSize> >
    class std_fwd_iter_impl: public std_fwd_iter_impl_base<Container, IterMemSize, IterType>
    {
        public:
                                 
        typedef typename Container::value_type value_type;
        typedef _fwd_iter_impl_base<value_type, IterMemSize, IterType> impl_base_t;
        typedef std::shared_ptr<impl_base_t> shared_base_t;
        typedef IterType iterator_type;
        using difference_type = typename impl_base_t::difference_type;
                           
        using _IterStore = typename std_fwd_iter_impl_base<Container, IterMemSize, IterType>::_IterStore;
        
        template <typename IteratorType>
        shared_base_t create_fwd_iter_impl(IteratorType& iter)
        {
            return shared_base_t (new std_fwd_iter_impl<Container, IterMemSize>);
        }          
        
        template <typename IteratorType>
        void instantiate(iterator_type& arg, IteratorType& itr)
        {
            static_assert (sizeof (_IterStore) <= IterMemSize, "container_fwd_iter_impl: IterMemSize too small.");
            void* buffer = impl_base_t::mem (arg);
            _IterStore* store = new (buffer) _IterStore (itr);
        }

        void instantiate(iterator_type& lhs,
                         const iterator_type& rhs) const override
        {
            auto rhsStore = reinterpret_cast<_IterStore*>(impl_base_t::mem (rhs));

            void* buffer = impl_base_t::mem (lhs);
            _IterStore* lhsStore = new (buffer) _IterStore (rhsStore->m_itr);
        }        
        
        iterator_type plus(const iterator_type& lhs, ssize_t offset) const override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (lhs));
            auto new_iter = iter_store->m_itr + offset;
            return iterator_type (std_fwd_iter_impl(), new_iter);
        }

        iterator_type minus(const iterator_type& lhs, ssize_t offset) const override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (lhs));            
            return iterator_type (std_fwd_iter_impl(),
                                  iter_store->m_itr - offset);
        }            

    };
    
    
    template <typename Container, size_t IterMemSize, typename IterType=rand_iter<typename Container::value_type, IterMemSize>>
    class std_rand_iter_impl : public std_fwd_iter_impl_base<Container, IterMemSize, IterType>,
                               public _rand_iter_impl_base<typename Container::value_type, IterMemSize, IterType>
                                                             
    {
    public:
        typedef std_fwd_iter_impl<Container, IterMemSize, IterType> fwd_impl_base_t;
        typedef _rand_iter_impl_base<typename Container::value_type, IterMemSize, IterType> impl_base_t;
        typedef std::shared_ptr<impl_base_t> shared_base_t;
        typedef IterType iterator_type;
        using difference_type = typename fwd_impl_base_t::difference_type;
        using _IterStore = typename fwd_impl_base_t::_IterStore;

        template <typename IteratorType>
        void instantiate(iterator_type& arg, IteratorType& itr)
        {
            static_assert (sizeof (_IterStore) <= IterMemSize, "container_rand_iter_impl: IterMemSize too small.");
            void* buffer = impl_base_t::mem (arg);
            _IterStore* store = new (buffer) _IterStore (itr);
        }

        void instantiate(iterator_type& lhs,
                         const iterator_type& rhs) const override
        {
            auto rhsStore = reinterpret_cast<_IterStore*>(impl_base_t::mem (rhs));

            void* buffer = impl_base_t::mem (lhs);
            _IterStore* lhsStore = new (buffer) _IterStore (rhsStore->m_itr);
        }            

        template <typename IteratorType>
        shared_base_t create_rand_iter_impl(IteratorType& iter)
        {
            return shared_base_t (new std_rand_iter_impl<Container, IterMemSize>());
        }            

        const iterator_type& minusminus(const iterator_type& obj) const override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (obj));
            --iter_store->m_itr;
            return obj;                
        }

        iterator_type& minusminus(iterator_type& obj) override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (obj));
            --iter_store->m_itr;
            return obj;                
        }            

        iterator_type& pluseq(iterator_type& obj, difference_type incr) override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (obj));
            iter_store->m_itr += incr;
            return obj;
        }

        const iterator_type& pluseq(const iterator_type& obj, difference_type incr) const override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (obj));
            iter_store->m_itr += incr;
            return obj;
        }

        iterator_type& minuseq(iterator_type& obj, difference_type decr) override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (obj));
            iter_store->m_itr -= decr;
            return obj;
        }

        const iterator_type& minuseq(const iterator_type& obj, difference_type decr) const override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (obj));
            iter_store->m_itr -= decr;
            return obj;
        }             

        iterator_type plus(const iterator_type& lhs, difference_type offset) const override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (lhs));
            auto new_iter = iter_store->m_itr + offset;
            return iterator_type (std_rand_iter_impl(), new_iter);
        }

        iterator_type minus(const iterator_type& lhs, difference_type offset) const override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (lhs));            
            return iterator_type (std_rand_iter_impl(), iter_store->m_itr - offset);
        }
    };
    
    
    // Some convenient using statements for common container types.  std::deque's iterator type requires
    // the most storage on GCC.  Todo: Refactor this logic a bit to work correctly on more platforms.
    // The goal is to successfully store all these iterator objects overlayed on a fixed size struct.
    // The reason for that will be apparent in the snapshotvector implementation.
    template <typename T>
    using vector_fwd_iter_impl = std_fwd_iter_impl<std::vector<T>, 48>;

    template <typename T>
    using deque_fwd_iter_impl = std_fwd_iter_impl<std::deque<T>, 48>;

    template <typename T>
    using set_fwd_iter_impl = std_fwd_iter_impl<std::set<T>, 48>;
    
    template <typename T>
    using deque_rand_iter_impl = std_rand_iter_impl<std::deque<T>, 48>;
    
    template <typename T>
    using vector_rand_iter_impl = std_rand_iter_impl<std::vector<T>, 48>;
}
