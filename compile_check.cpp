/***********************************************************************************************************************
 * snapshot_container:
 * A temporal sequentially accessible container type.
 * Released under the terms of the MIT license:
 * https://opensource.org/licenses/MIT
 **********************************************************************************************************************/

#include <iostream>
#include "snapshot_slice.h"
using namespace std;
int main() {

    auto store = snapshot_container::deque_storage<size_t>::create();
    std::vector<size_t> s(10, 1);

    virtual_iter::std_fwd_iter_impl<std::vector<size_t>, snapshot_container::storage_base<size_t>::IterMemSize> impl;
    snapshot_container::storage_base<size_t>::fwd_iter_type start(impl, s.begin());
    snapshot_container::storage_base<size_t>::fwd_iter_type end(impl, s.end());

    store->insert(0, start, end);

    auto startItr = store->begin();
    auto endItr = store->end();

    for (; startItr != endItr; ++startItr)
    {
        fprintf (stdout, "%ld\n", *startItr);
    }

    return 0;
}
