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
#pragma once
#include <type_traits>


namespace virtual_iter_detail {

    // Just enough MPL mechanics to turn an iterator type into a const_iterator type.
    // This is based on the idea that a const_iterator is obtained by substituting T const * for T* and
    // const T& for T& in the iterator template instantiation.
    // Note that this is designed to work with std iterator types. Some extra work may be required
    // to handle exotic iterator types such as pointers to arrays etc.
    
    template <typename ... Args>
    struct type_list {
    };


    template <typename ArgList>
    struct front_list;


    template <typename Head, typename ... ArgList>
    struct front_list<type_list<Head, ArgList...>>
    {
        using type = Head;
    };


    template <typename ArgList>
    struct popf_list;


    template <typename Head, typename ... ArgList>
    struct popf_list<type_list<Head, ArgList...>>
    {
        using type = type_list<ArgList...>;
    };


    template <typename Args, typename NewElem>
    struct pushf_list;

    template <typename ...ArgList, typename NewElem>
    struct pushf_list<type_list<ArgList...>, NewElem> {
        using type = type_list<NewElem, ArgList...>;
    };

    template <typename Args>
    struct is_empty {
        static constexpr bool value = false;
    };


    template <>
    struct is_empty<type_list<>>
    {
        static constexpr bool value = true;
    };


    template <typename Args, template <typename T> class MetaFun, bool = is_empty<Args>::value>
    struct transform_impl;

    template <typename Args, template <typename T> class MetaFun>
    struct transform_impl<Args, MetaFun, false> :
    public pushf_list< typename transform_impl< typename popf_list<Args>::type, MetaFun >::type,
    typename MetaFun< typename front_list<Args>::type>::type > {
    };

    template <typename Args, template <typename T> class MetaFun>
    struct transform_impl<Args, MetaFun, true> {
        using type = Args;
    };

    template <typename Args, template <typename T> class MetaFun>
    struct transform : public transform_impl<Args, MetaFun> {
    };


    // Expand a template class with args from a typelist
    template <template <typename ...> class Type, class Args>
    struct expand_type_with_type_list;


    template <template <typename ...> class Type, class ...Args>
    struct expand_type_with_type_list<Type, type_list<Args...>>
    {
        using type = Type<Args...>;
    };

    template <typename T>
    struct metafuncs {
        template <typename U,
        bool = !std::is_same<T, U>::value && std::is_same<T, std::remove_pointer_t<U>>::value>
        struct make_const_ptr;

        template <typename U,
        bool = !std::is_same<T, U>::value && std::is_same<T, std::remove_reference_t<U>>::value>
        struct make_const_ref;
    };

    template <typename T>
    template <typename U>
    struct metafuncs<T>::make_const_ptr<U, true> {
        using type = std::add_pointer_t<std::add_const_t<std::remove_pointer_t<U>>>;
    };

    template <typename T>
    template <typename U>
    struct metafuncs<T>::make_const_ptr<U, false> {
        using type = U;
    };

    template <typename T>
    template <typename U>
    struct metafuncs<T>::make_const_ref<U, true> {
        using type = std::add_const_t<U>;
    };

    template <typename T>
    template <typename U>
    struct metafuncs<T>::make_const_ref<U, false> {
        using type = U;
    };


    // finally I am ready to turn an iterator into its const counterpart
    template <template <typename ...> class Iterator, class T, class ... Args>
    struct make_const_iterator_impl {
        using const_ptr_args = typename transform<type_list<Args...>, metafuncs<T>::template make_const_ptr>::type;
        using const_ref_and_ptr_args = typename transform<const_ptr_args, metafuncs<T>::template make_const_ref>::type;
        using type = typename expand_type_with_type_list<Iterator, const_ref_and_ptr_args>::type;
    };

    // Note that make_const_iterator has to be used with a declspec something like this:
    // using const_iterator_type = declspec(make_const_iterator(std::vector<int>::iterator()))::type;
    struct make_const_iterator {

        template <template <typename ...> class Iterator, class ... Args>
        static auto create(Iterator<Args...> prototype) {
            return make_const_iterator_impl<Iterator, typename Iterator<Args...>::value_type, Args...>();
        }
 
        template <typename T>
        static auto create(T const* prototype)
        {
            return prototype;
        }
    };
}