/***********************************************************************************************************************
 * snapshot_container:
 * A temporal sequentially accessible container type.
 * Copyright 2019 Kuberan Naganathan
 * Released under the terms of the MIT license:
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

    template <typename T, size_t MemSize, typename IteratorType>
    class _fwd_iter_impl_base
    {
    public:

        typedef IteratorType iterator_type;
        typedef ssize_t difference_type;
        
        virtual void instantiate(iterator_type& lhs, const iterator_type& rhs) const = 0;

        virtual iterator_type& plusplus(iterator_type& obj) = 0;

        virtual void destroy(iterator_type& obj) const = 0;

        virtual bool equals(const iterator_type& lhs, const iterator_type& rhs) const = 0;

        virtual difference_type distance(const iterator_type& lhs,
                                         const iterator_type& rhs) const = 0;

        virtual iterator_type plus(const iterator_type& lhs, difference_type offset) const = 0;
        virtual iterator_type minus(const iterator_type& lhs, difference_type offset) const = 0;
        
        virtual const T* pointer(const iterator_type& arg) const = 0;

        virtual const T& reference(const iterator_type& arg) const = 0;

        virtual size_t copy(T* resultPtr, size_t maxItems, void* iter, void* endItr) const = 0;

        virtual void visit(void* iter, void* end_iter, std::function<bool(const T&)>&) = 0;

        void* mem(const iterator_type& arg) const
        {
            return arg.mem ();
        }
               
    };

    
    template <typename T, size_t MemSize, typename IteratorType>
    class _rand_iter_impl_base : virtual public _fwd_iter_impl_base<T, MemSize, IteratorType>
    {
    public:
        typedef IteratorType iterator_type;
        typedef _fwd_iter_impl_base<T, MemSize, IteratorType> base_t;
        using difference_type = typename base_t::difference_type;
        
        // In addition to the features provided by forward iterators,
        // random access iterators need operator -- , +=, -=
        virtual iterator_type& minusminus(iterator_type& obj) = 0;        
        virtual iterator_type& pluseq(iterator_type& obj, difference_type incr) = 0;        
        virtual iterator_type& minuseq(iterator_type& obj, difference_type decr) = 0;                       
        using base_t::mem;        
    };
    
    
    template <typename T, size_t MemSize, typename IterType, typename BaseImpl>
    class iter_base
    {
    public:
        typedef BaseImpl base_impl_t;
        typedef T value_type;
        typedef ssize_t difference_type;
        typedef T* pointer;
        typedef T& reference;
        static constexpr size_t mem_size = MemSize;
        typedef IterType iterator_type;    
        
        friend base_impl_t;
        
        iter_base(const std::shared_ptr<base_impl_t>& shared):
            m_impl(shared),
            m_iter_mem()
        {            
        }
        
        bool operator==(const iterator_type& rhs) const
        {return m_impl->equals (static_cast<const iterator_type&>(*this), rhs);}

        bool operator!=(const iterator_type& rhs) const
        {return !(m_impl->equals (static_cast<const iterator_type&>(*this), rhs));}

        difference_type operator-(const iterator_type& rhs) const
        {return m_impl->distance (static_cast<const iterator_type&>(*this), rhs);}

        iterator_type operator-(difference_type offset) const
        {
            return m_impl->minus(static_cast<const iterator_type&>(*this), offset);
        }
        
        iterator_type operator+(difference_type offset) const
        {return m_impl->plus (static_cast<const iterator_type&>(*this), offset);}
        
        iterator_type& operator++()
        {return m_impl->plusplus (static_cast<iterator_type&>(*this));}

        iterator_type& operator=(const iterator_type& rhs)
        {
            m_impl->destroy (static_cast<iterator_type&>(*this));
            m_impl = rhs.m_impl;
            m_impl->instantiate (static_cast<iterator_type&>(*this), rhs);
            return static_cast<iterator_type&>(*this);
        }

        bool operator<(const iterator_type& rhs) const
        {return (static_cast<const iterator_type&>(*this) - rhs) < 0;}
  
        
        const T* operator->() const
        {return m_impl->pointer (*this);}

        const T& operator*() const
        {return m_impl->reference (static_cast<const iterator_type&>(*this));}

        // The copy function exists as a workaround for the slowness of iterating via the wrapper vs
        // iterating directly via the iterator.  The copy function brings the performance of a
        // fwd_iter wrapping an std::vector<int>::iterator within a factor of 2.5 of the performance
        // of iteration via std::vector<int>::iterator when grabbing ~ 1000 elements at a time.
        size_t copy(T* resultPtr, size_t maxItems, const iterator_type& endPos) const
        {
            return m_impl->copy (resultPtr, maxItems, m_iter_mem, endPos.m_iter_mem);
        }

        // This function exists as a workaround for situations where copying an object is too expensive.
        // copy works better for simple native types such as int.  A compound type such as std::string may
        // be better to visit than to copy.
        void visit(const iterator_type& endItr, std::function<bool(const value_type&)>& f)
        {
            m_impl->visit (m_iter_mem, endItr.m_iter_mem, f);
        }        
        
    protected:
        void* mem() const
        {return m_iter_mem;}

        std::shared_ptr<base_impl_t> m_impl;
        // This is guaranteed 8 byte aligned.  This should be large enough to map most common iter types into.
        mutable size_t m_iter_mem[MemSize / 8];        
    };
    
    
    template <typename T, size_t MemSize>
    class fwd_iter: public iter_base<T, MemSize, fwd_iter<T, MemSize>, _fwd_iter_impl_base<T, MemSize, fwd_iter<T, MemSize>>>
    {
    public:

        typedef iter_base<T, MemSize, fwd_iter<T, MemSize>, _fwd_iter_impl_base<T, MemSize, fwd_iter<T, MemSize>>> base_t;        
        typedef std::forward_iterator_tag iterator_category;
        using value_type = typename base_t::value_type;
        using difference_type = typename base_t::difference_type;
        using pointer = typename base_t::pointer;
        using reference = typename base_t::reference;
        using base_impl_t = typename base_t::base_impl_t;
        
        template <typename Impl, typename WrappedIter>
        fwd_iter(Impl impl, WrappedIter iter):
        base_t(impl.create_fwd_iter_impl(iter))
        {
            impl.instantiate (*this, iter);    
        }
                
        fwd_iter(const fwd_iter<T, MemSize>& rhs):
        base_t(rhs.m_impl)
        {
            base_t::m_impl->instantiate (*this, rhs);    
        }
                
        ~fwd_iter()
        {
            base_t::m_impl->destroy(*this);            
        }    
    };
    

    template <typename T, size_t MemSize>
    class rand_iter : public iter_base<T, MemSize, rand_iter<T, MemSize>, _rand_iter_impl_base<T, MemSize, rand_iter<T, MemSize>>>
    {
        public:                    
        typedef std::random_access_iterator_tag iterator_category;          
        typedef iter_base<T, MemSize, rand_iter<T, MemSize>, _rand_iter_impl_base<T, MemSize, rand_iter<T, MemSize>>> base_t;
        using value_type = typename base_t::value_type;
        using difference_type = typename base_t::difference_type;
        using pointer = typename base_t::pointer;
        using reference = typename base_t::reference;
        using base_impl_t = typename base_t::base_impl_t;
        
        friend _fwd_iter_impl_base<T, MemSize, rand_iter<T, MemSize>>;
        
        template <typename Impl, typename WrappedIter>
        rand_iter(Impl impl, WrappedIter iter):
            base_t(impl.create_rand_iter_impl(iter))
        {
            impl.instantiate (*this, iter); 
        }
        
        rand_iter(const rand_iter<T, MemSize>& rhs):
        base_t(rhs.m_impl)
        {
            base_t::m_impl->instantiate (*this, rhs);    
        }
        
        ~rand_iter()
        {
            base_t::m_impl->destroy(*this);
        }
                
        rand_iter& operator--()
        {return base_t::m_impl->minusminus(*this);}
                
        rand_iter& operator+=(difference_type incr)
        {return base_t::m_impl->pluseq(*this, incr);}
    
        rand_iter& operator-=(difference_type decr)
        {return base_t::m_impl->minuseq(*this, decr);}              
    };    
}
