/***********************************************************************************************************************
 * snapshot_container:
 * A temporal sequentially accessible container type.
 * Copyright 2019 Kuberan Naganathan
 * Released under the terms of the MIT license:
 * https://opensource.org/licenses/MIT
 **********************************************************************************************************************/
#pragma once

#include "virtual_iter.h"
#include "virtual_std_iter_detail.h"
#include <type_traits>

namespace virtual_iter
{
    template <typename ConstIterType, size_t IterMemSize, typename IterType=fwd_iter<typename ConstIterType::value_type, IterMemSize> >
    class std_fwd_iter_impl_base: virtual public _fwd_iter_impl_base<typename ConstIterType::value_type, IterMemSize, IterType>
    {
    public:        

        // It would be great if std::vector<T>::iterator could somehow be mapped to std::vector<T>::const_iterator
        // but I don't know a convenient way to move between these types. The static assert is defensive but not very
        // user friendly. There are some helper creators to work around this awkwardness as a partial solution.
        static_assert(std::is_const<typename std::remove_pointer<typename ConstIterType::pointer>::type>::value,
                      "virtual_iter::std_fwd_iter_impl must be constructed based on a const_iterator type");

        typedef typename ConstIterType::value_type value_type;
        typedef _fwd_iter_impl_base<value_type, IterMemSize, IterType> impl_base_t;
        typedef std::shared_ptr<impl_base_t> shared_base_t;
        typedef IterType iterator_type;
        using difference_type = typename impl_base_t::difference_type;

        struct _IterStore
        {
            ConstIterType m_itr;

            template <typename IteratorType>
            _IterStore(IteratorType& itr):
                    m_itr (itr)
            {
            }
        };

        iterator_type& plusplus(iterator_type& obj) override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (obj));
            ++iter_store->m_itr;
            return obj;
        }

        void destroy(iterator_type& obj) const override
        {
            _IterStore* iter_store =
                    reinterpret_cast<_IterStore*>(impl_base_t::mem (obj));
            iter_store->~_IterStore();
        }

        bool equals(const iterator_type& lhs, const iterator_type& rhs) const override
        {
            auto lhs_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (lhs));
            auto rhs_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (rhs));
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

        void visit(void* iter, void* end_iter, std::function<bool(const value_type&)>& f) override
        {
            auto lhs_iter = reinterpret_cast<_IterStore*>(iter);
            auto rhs_iter = reinterpret_cast<_IterStore*>(end_iter);

            typename iterator_type::difference_type diff = rhs_iter->m_itr - lhs_iter->m_itr;

            for (size_t i = 0; i < diff; ++i)
            {
                if (!f (*lhs_iter->m_itr))
                    return;

                ++lhs_iter->m_itr;
            }
        }
    };


    // Implementation of fwd_iter around standard c++ iterator types.
    template <typename ConstIterType, size_t IterMemSize, typename IterType=fwd_iter<typename ConstIterType::value_type, IterMemSize> >
    class std_fwd_iter_impl: public std_fwd_iter_impl_base<ConstIterType, IterMemSize, IterType>
    {
    public:

        typedef typename ConstIterType::value_type value_type;
        typedef _fwd_iter_impl_base<value_type, IterMemSize, IterType> impl_base_t;
        typedef std::shared_ptr<impl_base_t> shared_base_t;
        typedef IterType iterator_type;
        using difference_type = typename impl_base_t::difference_type;

        using _IterStore = typename std_fwd_iter_impl_base<ConstIterType, IterMemSize, IterType>::_IterStore;
        template <typename WrappedIter>
        shared_base_t create_fwd_iter_impl(WrappedIter& iter)
        {
            return std::make_shared<std_fwd_iter_impl<ConstIterType, IterMemSize, IterType>>();
        }

        template <typename WrappedIter>
        void instantiate(iterator_type& arg, WrappedIter& itr)
        {
            static_assert (sizeof (_IterStore) <= sizeof(WrappedIter), "std_fwd_iter_impl: IterMemSize too small.");
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


    template <typename ConstIterType, size_t IterMemSize, typename IterType=rand_iter<typename ConstIterType::value_type, IterMemSize>>
    class std_rand_iter_impl : public std_fwd_iter_impl_base<ConstIterType, IterMemSize, IterType>,
                               public _rand_iter_impl_base<typename ConstIterType::value_type, IterMemSize, IterType>
    {
    public:
        typedef typename ConstIterType::value_type value_type;
        typedef std_fwd_iter_impl_base<ConstIterType, IterMemSize, IterType> fwd_impl_base_t;
        typedef _rand_iter_impl_base<typename ConstIterType::value_type, IterMemSize, IterType> impl_base_t;
        typedef std::shared_ptr<impl_base_t> shared_base_t;
        typedef IterType iterator_type;
        using difference_type = typename fwd_impl_base_t::difference_type;
        using _IterStore = typename fwd_impl_base_t::_IterStore;

        template <typename IteratorType>
        void instantiate(iterator_type& arg, IteratorType& itr)
        {
            static_assert (sizeof (_IterStore) <= IterMemSize, "std_rand_iter_impl: IterMemSize too small.");
            void* buffer = impl_base_t::mem (arg);
            _IterStore* store = new (buffer) _IterStore (itr);
        }

        void instantiate(iterator_type& lhs,
                         const iterator_type& rhs) const override
        {
            auto rhs_store = reinterpret_cast<_IterStore*>(impl_base_t::mem (rhs));

            void* buffer = impl_base_t::mem (lhs);
            _IterStore* lhs_store = new (buffer) _IterStore (rhs_store->m_itr);
        }            

        template <typename IteratorType>
        shared_base_t create_rand_iter_impl(IteratorType& iter)
        {                       
            return std::make_shared<std_rand_iter_impl<ConstIterType, IterMemSize>>();
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

        iterator_type& minuseq(iterator_type& obj, difference_type decr) override
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


    struct std_iter_impl_creator
    {
        template <typename ContainerType, 
                 size_t MemSize=48, 
                 std::enable_if_t< std::is_same<typename ContainerType::iterator, typename ContainerType::iterator>::value, int > = 0>
        static auto create(const ContainerType& prototype)
        {
            if constexpr (std::is_same<typename ContainerType::iterator::iterator_category, std::random_access_iterator_tag>::value)
            {
                return std_rand_iter_impl<typename ContainerType::const_iterator, MemSize>();
            }
            else
            {
                return std_fwd_iter_impl<typename ContainerType::const_iterator, MemSize>();
            }            
        }
                
        template <typename IteratorType, size_t MemSize=48,
                  std::enable_if_t<
                  std::is_same<typename IteratorType::iterator_category, typename IteratorType::iterator_category>::value, int > = 0>
        static auto create(const IteratorType& prototype)
        {
            if constexpr (std::is_same<typename IteratorType::iterator_category, std::random_access_iterator_tag>::value)
            {
                return std_rand_iter_impl<typename decltype(
                                          virtual_iter_detail::make_const_iterator::create(std::declval<IteratorType>()))::type,
                                          MemSize>();
            }
            else
            {
                return std_fwd_iter_impl<typename decltype(
                                         virtual_iter_detail::make_const_iterator::create(std::declval<IteratorType>()))::type, 
                                        MemSize>();
            }
        } 
    };
   
    
    struct std_fwd_iter_impl_creator
    {
        template <typename ContainerType, size_t MemSize=48,
                  std::enable_if_t< std::is_same<typename ContainerType::iterator, typename ContainerType::iterator>::value, int> = 0>
        static auto create(const ContainerType& prototype)
        {
            return std_fwd_iter_impl<typename ContainerType::const_iterator, MemSize>();
        }

        template <typename IteratorType, size_t MemSize=48, 
                  std::enable_if_t<std::is_same<typename IteratorType::iterator_category, typename IteratorType::iterator_category>::value> = 0>
        static auto create(const IteratorType& prototype)
        {
            return std_fwd_iter_impl<typename decltype(
                                     virtual_iter_detail::make_const_iterator::create(std::declval<IteratorType>()))::type, MemSize>();
        }                        
    };             
}
