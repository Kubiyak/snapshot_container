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
#define CATCH_CONFIG_MAIN
#include <vector>
#include "snapshot_container.h"
#include "catch.hpp"
#include <algorithm>


template <typename T>
using container_t = snapshot_container::container<T>;


TEST_CASE("Snapshot does not change when container is updated", "[container]")
{
    auto vec = std::vector<int>(1024, 0xdeadbeef);
    auto container = container_t<int>(vec.begin(), vec.end());
    REQUIRE(std::equal(container.begin(), container.end(), vec.begin()));
    auto snapshot = container.create_snapshot();
    REQUIRE(std::equal(snapshot.begin(), snapshot.end(), vec.begin()));
    REQUIRE(std::equal(snapshot.begin(), snapshot.end(), container.cbegin()));

    // insert into the container. Snapshot should not change
    auto vec2 = std::vector<int>(1024, 0xcafef00d);
    container.insert(container.begin() + 512, vec2.begin(), vec2.end());
    REQUIRE(container.size() == 2048);
    REQUIRE(std::equal(snapshot.begin(), snapshot.end(), vec.begin()));
    REQUIRE(std::equal(snapshot.begin(), snapshot.begin() + 512, container.begin()));
    REQUIRE(std::equal(snapshot.begin() + 512, snapshot.end(), container.begin() + 1536));
    container.clear();
    REQUIRE(std::equal(snapshot.begin(), snapshot.end(), vec.begin()));

}

