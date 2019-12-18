/**********************************************************************************************************************
* virtual_iter:
* Iterator types for opaque sequence collections.
* Released under the terms of the MIT License:
* https://opensource.org/licenses/MIT
**********************************************************************************************************************/

#pragma once

#include "virtual_iter.h"

namespace virtual_iter
{
    // Implementation of fwd_iter around some c++ std container types.
    template <typename Container, size_t IterMemSize = 32>
    class std_fwd_iter_impl : public _fwd_iter_impl_base<typename Container::value_type, IterMemSize>
    {
    public:
        typedef typename Container::value_type value_type;
        typedef std::shared_ptr<_fwd_iter_impl_base<value_type, IterMemSize>> shared_base_t;
        typedef fwd_iter<value_type, IterMemSize> iterator_type;

        template <typename IterType>
        shared_base_t shared_forward_iterator_impl(IterType& iter) const
        {
            return shared_base_t (new std_fwd_iter_impl<Container, IterMemSize>);
        }

        struct _IterStore
        {
            typename Container::const_iterator m_itr;

            template <typename IterType>
            _IterStore(IterType& itr):
                    m_itr (itr)
            {
            }
        };

        template <typename IterType>
        void instantiate(iterator_type& arg, IterType& itr)
        {
            static_assert (sizeof (_IterStore) <= IterMemSize, "container_fwd_iter_impl: IterMemSize too small.");
            void* buffer = _fwd_iter_impl_base<value_type, IterMemSize>::mem (arg);
            _IterStore* store = new (buffer) _IterStore (itr);
        }

        void instantiate(fwd_iter<value_type, IterMemSize>& lhs,
                         const fwd_iter<value_type, IterMemSize>& rhs) const override
        {
            auto rhsStore = reinterpret_cast<_IterStore*>(_fwd_iter_impl_base<value_type, IterMemSize>::mem (rhs));

            void* buffer = _fwd_iter_impl_base<value_type, IterMemSize>::mem (lhs);
            _IterStore* lhsStore = new (buffer) _IterStore (rhsStore->m_itr);
        }

        const fwd_iter<value_type, IterMemSize>&
        plusplus(const fwd_iter<value_type, IterMemSize>& obj) const override
        {
            auto iterStore = reinterpret_cast<_IterStore*>(_fwd_iter_impl_base<value_type, IterMemSize>::mem (obj));

            ++iterStore->m_itr;
            return obj;
        }

        iterator_type& plusplus(iterator_type& obj) const override
        {
            auto iterStore = reinterpret_cast<_IterStore*>(_fwd_iter_impl_base<value_type, IterMemSize>::mem (obj));
            ++iterStore->m_itr;
            return obj;
        }

        void destroy(iterator_type& obj) const override
        {
            _IterStore* iterStore =
                    reinterpret_cast<_IterStore*>(_fwd_iter_impl_base<value_type, IterMemSize>::mem (obj));
            iterStore->~_IterStore();
        }

        bool equals(const iterator_type& lhs, const iterator_type& rhs) const override
        {
            auto lhs_store = reinterpret_cast<_IterStore*>(_fwd_iter_impl_base<value_type, IterMemSize>::mem (
                    lhs));
            auto rhs_store = reinterpret_cast<_IterStore*>(_fwd_iter_impl_base<value_type, IterMemSize>::mem (
                    rhs));

            return lhs_store->m_itr == rhs_store->m_itr;
        }

        ssize_t distance(const iterator_type& lhs, const iterator_type& rhs) const override
        {
            auto lhs_store = reinterpret_cast<_IterStore*>(_fwd_iter_impl_base<value_type, IterMemSize>::mem (lhs));
            auto rhs_store = reinterpret_cast<_IterStore*>(_fwd_iter_impl_base<value_type, IterMemSize>::mem (rhs));

            return lhs_store->m_itr - rhs_store->m_itr;
        }

        virtual iterator_type plus(const iterator_type& lhs, ssize_t offset) const override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(_fwd_iter_impl_base<value_type, IterMemSize>::mem (lhs));
            return iterator_type (std_fwd_iter_impl<Container, IterMemSize> (),
                                  iter_store->m_itr + offset);
        }

        virtual const value_type* pointer(const iterator_type& arg) const override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(_fwd_iter_impl_base<value_type, IterMemSize>::mem (arg));
            return (iter_store->m_itr).operator-> ();
        }

        virtual const value_type& reference(const iterator_type& arg) const override
        {
            auto iter_store = reinterpret_cast<_IterStore*>(_fwd_iter_impl_base<value_type, IterMemSize>::mem (arg));
            return (iter_store->m_itr).operator* ();
        }


        // This function should be specialized for the specific type of iterator.  This currently
        // assumes a random access iterator supporting  a constant time distance (subtraction) op.
        virtual size_t copy(value_type* result_ptr, size_t max_items, void* iter, void* end_iter) const override
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

        virtual void visit(void* iter, void* end_iter, std::function<bool(const value_type&)>& f) const override
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

    // Some convenient using statements for common container types.  std::deque's iterator type requires
    // the most storage on GCC.  Todo: Refactor this logic a bit to work correctly on more platforms.
    // The goal is to successfully store all these iterator objects overlayed on a fixed size struct.
    // The reason for that will be apparent in the snapshotvector implementation.
    template <typename T>
    using vector_fwd_iter_impl = std_fwd_iter_impl<std::vector<T>, 32>;

    template <typename T>
    using deque_fwd_iter_impl = std_fwd_iter_impl<std::deque<T>, 32>;

    template <typename T>
    using set_fwd_iter_impl = std_fwd_iter_impl<std::set<T>, 32>;
}
