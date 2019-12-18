/**********************************************************************************************************************
 * virtual_iter:
 * Iterator types for opaque sequence collections.
 * Released under the terms of the MIT License:
 * https://opensource.org/licenses/MIT
 **********************************************************************************************************************/
#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <sys/types.h>
#include <vector>


namespace virtual_iter
{
    template <typename T, size_t MemSize>
    class fwd_iter;

    template <typename T, size_t MemSize>
    class _fwd_iter_impl_base
    {
    public:

        virtual void instantiate(fwd_iter<T, MemSize>& lhs, const fwd_iter<T, MemSize>& rhs) const = 0;

        virtual fwd_iter<T, MemSize>& plusplus(fwd_iter<T, MemSize>& obj) const = 0;

        virtual const fwd_iter<T, MemSize>& plusplus(const fwd_iter<T, MemSize>& obj) const = 0;

        virtual void destroy(fwd_iter<T, MemSize>& obj) const = 0;

        virtual bool equals(const fwd_iter<T, MemSize>& lhs, const fwd_iter<T, MemSize>& rhs) const = 0;

        virtual ssize_t distance(const fwd_iter<T, MemSize>& lhs,
                                 const fwd_iter<T, MemSize>& rhs) const = 0;

        virtual fwd_iter<T, MemSize> plus(const fwd_iter<T, MemSize>& lhs, ssize_t offset) const = 0;

        virtual const T* pointer(const fwd_iter<T, MemSize>& arg) const = 0;

        virtual const T& reference(const fwd_iter<T, MemSize>& arg) const = 0;

        virtual size_t copy(T* resultPtr, size_t maxItems, void* iter, void* endItr) const = 0;

        virtual void visit(void* iter, void* endItr, std::function<bool(const T&)>&) const = 0;

        void* mem(const fwd_iter<T, MemSize>& arg) const;
    };

    template <typename T, size_t MemSize>
    class fwd_iter
    {
    public:

        friend class _fwd_iter_impl_base<T, MemSize>;

        typedef std::forward_iterator_tag iterator_category;
        typedef T value_type;
        typedef ssize_t difference_type;
        typedef T* pointer;
        typedef T& reference;

        static const size_t mem_size = MemSize;

        // Impl must be a type which returns a shared pointer to a
        // forward_iterator_base_impl object via a shared_forward_iterator_impl method.
        // All data should be instantiated into the memory area obtained via example.mem().
        // The size of that area is example.mem_size;
        // This idiom erases the type of WrappedIter itself.  As a result, care must be taken
        // to ensure that any iter comparisons are only made between wrappings of the same
        // iter type.
        template <typename Impl, typename WrappedIter>
        fwd_iter(Impl impl, WrappedIter iter);

        fwd_iter(const fwd_iter<T, MemSize>& rhs) :
                m_impl (rhs.m_impl)
        {
            m_impl->instantiate (*this, rhs);
        }

        const fwd_iter<T, MemSize>& operator++() const
        {return m_impl->plusplus (*this);}

        fwd_iter<T, MemSize>& operator++()
        {return m_impl->plusplus (*this);}

        fwd_iter<T, MemSize>& operator=(const fwd_iter<T, MemSize>& rhs)
        {
            m_impl->destroy (*this);
            m_impl = rhs.m_impl;
            return *this;
        }

        bool operator<(const fwd_iter<T, MemSize>& rhs) const
        {return (*this - rhs) < 0;}

        bool operator==(const fwd_iter<T, MemSize>& rhs) const
        {return m_impl->equals (*this, rhs);}

        bool operator!=(const fwd_iter<T, MemSize>& rhs) const
        {return !(m_impl->equals (*this, rhs));}

        difference_type operator-(const fwd_iter<T, MemSize>& rhs) const
        {return m_impl->distance (*this, rhs);}

        fwd_iter<T, MemSize> operator+(difference_type offset)
        {return m_impl->plus (*this, offset);}

        ~fwd_iter()
        {
            m_impl->destroy (*this);
        }

        const T* operator->() const
        {return m_impl->pointer (*this);}

        const T& operator*() const
        {return m_impl->reference (*this);}

        // The copy function exists as a workaround for the slowness of iterating via the wrapper vs
        // iterating directly via the iterator.  The copy function brings the performance of a
        // fwd_iter wrapping an std::vector<int>::iterator within a factor of 2.5 of the performance
        // of iteration via std::vector<int>::iterator when grabbing ~ 1000 elements at a time.
        size_t copy(T* resultPtr, size_t maxItems, const fwd_iter<T, MemSize>& endPos) const
        {
            return m_impl->copy (resultPtr, maxItems, m_iterMem, endPos.m_iterMem);
        }

        // This function exists as a workaround for situations where copying an object is too expensive.
        // copy works better for simple native types such as int.  A compound type such as std::string may
        // be better to visit than to copy.
        void visit(const fwd_iter<T, MemSize>& endItr, std::function<bool(const value_type&)>& f) const
        {
            m_impl->visit (m_iterMem, endItr.m_iterMem, f);
        }

    private:

        void* mem() const
        {return m_iterMem;}

        std::shared_ptr<_fwd_iter_impl_base<T, MemSize>> m_impl;
        // This is guaranteed 8 byte aligned.  This should be large enough to map most common iter types into.
        mutable size_t m_iterMem[MemSize / 8];

    };

    template <typename T, size_t MemSize>
    template <typename Impl, typename WrapperIter>
    fwd_iter<T, MemSize>::fwd_iter(Impl impl, WrapperIter iter):
            m_impl (impl.shared_forward_iterator_impl (iter))
    {
        impl.instantiate (*this, iter);
    }

    template <typename T, size_t IterMemSize>
    void* _fwd_iter_impl_base<T, IterMemSize>::mem(const fwd_iter<T, IterMemSize>& arg) const
    {
        return arg.mem ();
    }
}
