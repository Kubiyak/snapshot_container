/***********************************************************************************************************************
 * snapshot_container:
 * A temporal sequentially accessible container type.
 * Released under the terms of the MIT license:
 * https://opensource.org/licenses/MIT
 **********************************************************************************************************************/
#pragma  once

#include "virtual_std_iter.h"

namespace snapshot_container
{
    // The basic functions that the storage type must support.
    template <typename T>
    class storage_base
    {
    public:

        static const size_t npos = 0xFFFFFFFFFFFFFFFF;
        static const size_t iter_mem_size = 48;
        typedef T value_type;
        typedef std::shared_ptr<storage_base<value_type>> shared_base_t;
        typedef virtual_iter::fwd_iter<value_type, iter_mem_size> fwd_iter_type;

        virtual void append(const value_type&) = 0;

        virtual void append(const fwd_iter_type& start_pos, const fwd_iter_type& end_pos) = 0;

        // Create a deep copy of the object between startIndex and endIndex and return it.
        virtual shared_base_t copy(size_t start_index = 0, size_t end_index = npos) const = 0;

        virtual void insert(size_t index, const value_type&) = 0;

        virtual void insert(size_t index, fwd_iter_type& start_pos, const fwd_iter_type& end_pos) = 0;

        virtual void remove(size_t index) = 0;

        virtual size_t size() const = 0;
        
        virtual const value_type& operator[](size_t index) const = 0;

        virtual value_type& operator[](size_t index) = 0;

        virtual ~storage_base()
        {}

        virtual fwd_iter_type begin() = 0;
        virtual fwd_iter_type end() = 0;
        virtual fwd_iter_type iterator(size_t offset) = 0;

        virtual const fwd_iter_type begin() const = 0;
        virtual const fwd_iter_type end() const = 0;
        virtual const fwd_iter_type iterator(size_t offset) const = 0;

    protected:
        // Must create this via a derivation.
        storage_base()
        {}
    };

    
    // This is just an example of what an implementation of storage_base<T> can look like.
    // In general, it could be just about anything supporting the storage_base<T> interface. For example it can be
    // loading and writing records to disk or doing the same in some shared memory segment. The primary assumption made
    // by the higher level abstractions is that appending records to the storage is efficient. Inserting to the middle
    // is permissible.
    template <typename T>
    class deque_storage : public storage_base<T>
    {
    public:

        static const size_t npos = 0xFFFFFFFFFFFFFFFF;
        using storage_base<T>::iter_mem_size;
        typedef T value_type;
        typedef std::shared_ptr<deque_storage<T>> shared_t;
        typedef std::shared_ptr<storage_base<T>> shared_base_t;
        typedef typename storage_base<T>::fwd_iter_type fwd_iter_type;

        void append(const T& value) override
        {
            m_data.push_back (value);
        }

        void append(const fwd_iter_type& start_pos, const fwd_iter_type& end_pos) override
        {
            fwd_iter_type start_pos_copy(start_pos);
            
            static std::function<bool(const value_type& v)> f =
                    [this](const value_type& v) 
                    {
                        m_data.push_back (v);
                        return true;
                    };

            start_pos_copy.visit (end_pos, f);
        }

        shared_base_t copy(size_t start_index = 0, size_t end_index = npos) const override;

        void insert(size_t index, const T& value) override
        {
            m_data.insert (m_data.begin () + index, value);
        }

        void insert(size_t index, fwd_iter_type& start_pos, const fwd_iter_type& end_pos) override
        {
            m_data.insert(m_data.begin() + index, start_pos, end_pos);
        }

        void remove(size_t index) override
        {
            m_data.erase(m_data.begin() + index);
        }

        size_t size() const override
        {return m_data.size ();}

        const T& operator[](size_t index) const override
        {return m_data[index];}

        T& operator[](size_t index) override
        {return m_data[index];}

        const fwd_iter_type begin() const override
        {
            return fwd_iter_type(_iter_impl, m_data.begin());
        }

        const fwd_iter_type end() const override
        {
            return fwd_iter_type(_iter_impl, m_data.end());
        }

        const fwd_iter_type iterator(size_t offset) const override
        {
            if (offset > m_data.size())
                offset = m_data.size();
            return fwd_iter_type(_iter_impl, m_data.begin() + offset);
        }

        fwd_iter_type begin() override
        {
            return fwd_iter_type(_iter_impl, m_data.begin());
        }

        fwd_iter_type end() override
        {
            return fwd_iter_type(_iter_impl, m_data.end());
        }

        fwd_iter_type iterator(size_t offset) override
        {
            if (offset > m_data.size())
                offset = m_data.size();
            return fwd_iter_type(_iter_impl, m_data.begin() + offset);
        }

        static shared_base_t create();

        template <typename InputIter>
        static shared_base_t create(InputIter start_pos, InputIter end_pos);
       
        deque_storage(const deque_storage<T>& rhs) = default;
       
    private:
        
        deque_storage() {}        
        
        template <typename InputIter>
        deque_storage(InputIter start_pos, InputIter end_pos);
        
        static virtual_iter::std_fwd_iter_impl<std::deque<value_type>, iter_mem_size> _iter_impl;        
        std::deque<T> m_data;
    };

    template <typename T>
    template <typename InputIter>
    deque_storage<T>::deque_storage(InputIter start_pos, InputIter end_pos) :
            m_data (start_pos, end_pos)
    {}

    template <typename T>
    typename deque_storage<T>::shared_base_t deque_storage<T>::copy(size_t start_index, size_t end_index) const
    {
        if (end_index == npos)
            end_index = m_data.size();

        auto new_storage = new deque_storage<T> (m_data.begin () + start_index, m_data.begin () + end_index);
        return deque_storage<T>::shared_base_t (new_storage);
    }

    template <typename T>
    typename deque_storage<T>::shared_base_t deque_storage<T>::create()
    {
        return shared_base_t (new deque_storage<T> ());
    }

    template <typename T>
    template <typename InputItr>
    typename deque_storage<T>::shared_base_t deque_storage<T>::create(InputItr start_pos, InputItr end_pos)
    {
        auto storage = new deque_storage<T> (start_pos, end_pos);
        return shared_base_t (storage);
    }

    template <typename T>
    virtual_iter::std_fwd_iter_impl<std::deque<T>, deque_storage<T>::iter_mem_size> deque_storage<T>::_iter_impl;

    
    // Storage creation may need to be stateful. To support this, the higher level abstraction takes a storage creator
    // object as an arg on which operator () is called to create storage. This is a wrapper around deque_storage
    // supporting this usage
    template <typename T>
    struct deque_storage_creator
    {
        typedef typename deque_storage<T>::shared_base_t shared_base_t;
        shared_base_t operator() ()
        {
            return deque_storage<T>::create();
        }
        
        template <typename IterType>
        shared_base_t operator() (IterType start_pos, IterType end_pos)
        {
            return deque_storage<T>::create(start_pos, end_pos);
        }
    };    
}