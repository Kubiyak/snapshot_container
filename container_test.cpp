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

#include <vector>
#include "snapshot_container.h"

template <typename T>
using container_t = snapshot_container::container<T>;

int main()
{
    // quick compile check for now. I will add tests based on the libcxx vector and deque tests
    // to test this.
    auto container = container_t<int>();
    auto data =std::vector<int>(10, 0xdeadbeef);
    // TODO: Hide this in the insert logic itself if possible
    auto impl = virtual_iter::std_iter_impl_creator::create(data);
    auto begin_pos = virtual_iter::rand_iter<int, 48>(impl, data.begin());
    auto end_pos = virtual_iter::rand_iter<int, 48>(impl, data.end());
    container.insert(container.end(), begin_pos, end_pos);
    
    auto impl2 = virtual_iter::std_iter_impl_creator::create(data.begin());
    auto begin_pos2 = virtual_iter::rand_iter<int, 48>(impl2, data.begin());
    auto end_pos2 = virtual_iter::rand_iter<int, 48>(impl2, data.end());
    container.insert(container.end(), begin_pos2, end_pos2);
    
    container.insert(container.end(), data.begin(), data.end());
    
    return 0;
}
