#pragma once
/*
 * The MIT License
 *
 * Copyright 2020 Kuberan Naganathan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "snapshot_iterator.h"
#include "virtual_iter.h"
#include <iterator>


namespace snapshot_container
{
    // TODO: Support custom allocators. This is going to take quite some effort so will likely occur
    // on a separate branch or in a separate project itself.
    template <typename T, typename StorageCreator=deque_storage_creator<T>, typename ConfigTraits=_iterator_kernel_config_traits>
    class snapshot;


    template <typename T, typename StorageCreator=deque_storage_creator<T>, typename ConfigTraits=_iterator_kernel_config_traits>
    class container
    {
    public:
        typedef StorageCreator storage_creator_t;
        typedef _iterator_kernel<T, StorageCreator, ConfigTraits> kernel_t;
        typedef typename kernel_t::fwd_iter_type fwd_iter_type;
        typedef typename kernel_t::rand_iter_type rand_iter_type;
        typedef typename kernel_t::iterator iterator;
        typedef typename kernel_t::const_iterator const_iterator;
        typedef std::shared_ptr<kernel_t> shared_kernel_t;
        typedef size_t size_type;
        typedef ssize_t difference_type;
        typedef T* pointer;
        typedef T& reference;
        typedef T value_type;

        typedef container<T, StorageCreator, ConfigTraits> container_t;
        typedef snapshot<T, StorageCreator, ConfigTraits> snapshot_t;

        container():
            m_kernel(kernel_t::create(storage_creator_t()))
        {
        }

        container(const storage_creator_t& creator):
            m_kernel(kernel_t::create(storage_creator_t()))
        {
        }

        template<typename IterType>
        container(const IterType& start_pos, const IterType& end_pos):
            m_kernel(kernel_t::create(storage_creator_t(), start_pos, end_pos))
        {
        }

        template <typename IterType>
        container(const IterType& start_pos, const IterType& end_pos, const storage_creator_t& creator):
            m_kernel(kernel_t::create(creator, start_pos, end_pos))
        {
        }

        container(const container_t& rhs)
        {
            m_kernel->deep_copy(rhs.m_kernel);
        }

        container(container_t && rhs) = default;

        container_t& operator=(const container_t& rhs)
        {
            m_kernel->deep_copy(rhs.m_kernel);
            return *this;
        }

        container_t& operator=(container_t && rhs) = default;
        ~container() {}


        // construction from a snapshot.
        container (const snapshot_t& rhs); // must be defined after snapshot is defined. see below.
        container& operator=(const snapshot_t& rhs);

        iterator begin() {return iterator(m_kernel, 0);}
        iterator end() {return iterator(m_kernel, size());}
        const_iterator begin() const {return const_iterator(m_kernel, 0);}
        const_iterator end() const {return const_iterator(m_kernel, size());}

        const_iterator cbegin() const {return const_iterator(m_kernel, 0);}
        const_iterator cend() const {return const_iterator(m_kernel, size());}

        // It is unsafe to keep pointers or references to elements in container beyond
        // immediate ops. Non-updating actions can invalidate direct references and pointers to elements.
        // These are provided for convenience only. Use the iterator interface instead in order to refer back to a
        // position in the container when any read/write actions intercede obtaining the iterator and (re)-using it.
        reference operator[](size_t index) {return (*m_kernel)[index];}
        const reference operator[](size_t index) const {return (*m_kernel)[index];}
        size_type size() const {return m_kernel->size();}

        void clear()
        {
            m_kernel->clear();
        }

        iterator insert(const_iterator insert_pos, const T& value)
        {
            m_kernel->insert(insert_pos.pos(), value);
            return insert_pos;
        }

        iterator insert(const_iterator insert_pos,
                        const fwd_iter_type& begin_pos,
                        const fwd_iter_type& end_pos)
        {
            auto insert_point = m_kernel->insert(insert_pos.pos(), begin_pos, end_pos);
            return iterator(m_kernel, insert_point);
        }

        iterator insert(const_iterator insert_pos,
                             const rand_iter_type& begin_pos,
                             const rand_iter_type& end_pos)
        {
            auto insert_point = m_kernel->insert(insert_pos.pos(), begin_pos, end_pos);
            return iterator(m_kernel, insert_point);
        }

        template <typename IterType,
                  std::enable_if_t<std::is_same<typename std::iterator_traits<IterType>::iterator_category,
                  std::random_access_iterator_tag>::value, int> = 0>
        iterator insert(const_iterator insert_pos, IterType start_pos, IterType end_pos)
        {
            auto impl = virtual_iter::std_iter_impl_creator::create(start_pos);
            auto iter1 = rand_iter_type(impl, start_pos);
            auto iter2 = rand_iter_type(impl, end_pos);
            return insert(insert_pos, iter1, iter2);
        }

        template <typename IterType,
            std::enable_if_t<!std::is_same<typename std::iterator_traits<IterType>::iterator_category,
                              std::random_access_iterator_tag>::value, int> = 0>
        iterator insert(const_iterator insert_pos, IterType start_pos, IterType end_pos)
        {
            auto impl = virtual_iter::std_fwd_iter_impl_creator::create(start_pos);
            auto iter1 = fwd_iter_type(impl, start_pos);
            auto iter2 = fwd_iter_type(impl, end_pos);
            return insert(insert_pos, iter1, iter2);
        }

        iterator erase(const_iterator remove_pos)
        {
            auto result = m_kernel->remove(remove_pos.pos());
            return iterator(m_kernel, result);
        }

        iterator erase(const_iterator start_pos, const_iterator end_pos)
        {
            auto result = m_kernel->remove(start_pos.pos(), end_pos.pos());
            return iterator(m_kernel, result);
        }

        template<typename IterType>
        iterator append(const_iterator start_pos, const_iterator end_pos)
        {
            auto result = m_kernel->append(start_pos, end_pos);
            return iterator(m_kernel, result);
        }

        void push_back(const T& value)
        {
            m_kernel->push_back(value);
        }

        void swap(container_t& other) noexcept
        {
            // This is safer than std::swap(m_kernel, other.m_kernel) as
            // doing it this way will not suddenly switch which of this or other that
            // iterators reference.
            // TODO: ensure std::swap works correctly on these types.
            m_kernel->swap(*other.m_kernel);
        }

        bool empty() const noexcept
        {
            return m_kernel->size() == 0;
        }

        // TODO: Improve std::vector compat. Support for emplace, emplace_back etc. Will require some thought.
        // TODO: Reverse iterators

        snapshot_t create_snapshot();
    private:
        shared_kernel_t m_kernel;
    };


    template <typename T, typename StorageCreator, typename ConfigTraits>
    class snapshot
    {
    public:

        friend class container<T, StorageCreator, ConfigTraits>;
        typedef container<T, StorageCreator, ConfigTraits> container_t;

        typedef StorageCreator storage_creator_t;
        typedef _iterator_kernel<T, StorageCreator, ConfigTraits> kernel_t;
        typedef typename kernel_t::fwd_iter_type fwd_iter_type;
        typedef typename kernel_t::rand_iter_type rand_iter_type;
        typedef typename kernel_t::const_iterator const_iterator;
        typedef std::shared_ptr<kernel_t> shared_kernel_t;
        typedef size_t size_type;
        typedef ssize_t difference_type;
        typedef T const* pointer;
        typedef T const& reference;
        typedef T value_type;

        snapshot():
        m_kernel(kernel_t::create(storage_creator_t()))
        {
        }

        snapshot(const snapshot& rhs) = default;
        snapshot& operator = (const snapshot& rhs) = default;
        snapshot(snapshot && rhs) = default;
        snapshot& operator=(snapshot&& rhs) = default;

        reference operator[](size_t index) const {return (*m_kernel)[index];}
        size_type size() const {return m_kernel->size();}
        void swap(snapshot& other) noexcept
        {
            m_kernel->swap(other->m_kernel);
        }

        bool empty() const noexcept
        {
            return m_kernel->size() == 0;
        }

        const_iterator begin() const {return const_iterator(m_kernel, 0);}
        const_iterator end() const {return const_iterator(m_kernel, size());}

        // snapshots provide access to the storage creator object and storage ids of storage
        // elements. This is to provide support for doing things like interfacing snapshots to buffer objects
        // in python efficiently. Theoretically, user code could do this without support from snapshots anyway
        // so these features are provided as a convenience.
        std::vector<size_t> storage_ids() const
        {
            return m_kernel->storage_ids();
        }

        storage_creator_t& get_storage_creator() const
        {
            return m_kernel->get_storage_creator();
        }

    protected:

        snapshot(const shared_kernel_t& rhs):
        m_kernel(kernel_t::create(rhs->get_storage_creator()))
        {
            // shallow copy. The outer shared pointers are independent but
            // all the internal shared pointers are shared. This is how the
            // container type constructs a snapshot.
            *m_kernel = *rhs;
        }

        shared_kernel_t m_kernel;
    };


    template <typename T, typename StorageCreator, typename ConfigTraits>
    auto container<T, StorageCreator, ConfigTraits>::create_snapshot() -> snapshot_t
    {
        return snapshot_t(m_kernel);
    }


    template <typename T, typename StorageCreator, typename ConfigTraits>
    container<T, StorageCreator, ConfigTraits>::container(const snapshot_t& rhs)
    {
        m_kernel->deepcopy(rhs.m_kernel);
    }


    template <typename T, typename StorageCreator, typename ConfigTraits>
    auto container<T, StorageCreator, ConfigTraits>::operator=(const snapshot_t& rhs) -> container_t&
    {
        m_kernel->deep_copy(rhs.m_kernel);
        return *this;
    }
}



