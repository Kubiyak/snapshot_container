header_files = ['virtual_iter.h', 'virtual_std_iter.h',
                'snapshot_iterator.h', 'snapshot_slice.h',
                'snapshot_storage.h']


slice_test_env = Environment(CXX="g++-8", CXXFLAGS="--std=c++17 -g --coverage -fprofile-arcs -ftest-coverage -D_SNAPSHOTCONTAINER_TEST=1")
slice_test_env.VariantDir("build/slice_test", "./")
slice_test = slice_test_env.Program("build/slice_test/slice_test",
                              ['build/slice_test/test_snapshot_slice.cpp'], LIBS=['gcov'])
Depends('build/slice_test/slice_test', header_files)
slice_test_env.Alias('slice_test', slice_test)


slice_simulation_env = Environment(CXX="g++-8", CXXFLAGS="--std=c++17 -O2 -D_SNAPSHOTCONTAINER_TEST=1")
slice_simulation_env.VariantDir("build/slice_simulation", "./")
slice_simulation = slice_simulation_env.Program("build/slice_simulation/slice_simulation",
                                                ['build/slice_simulation/slice_simulation.cpp'])
Depends('build/slice_simulation/slice_simulation', header_files)
slice_simulation_env.Alias('slice_simulation', slice_simulation)


container_test_env = Environment(CXX="g++-8", CXXFLAGS="--std=c++17 -g")
container_test_env.VariantDir("build/container_test", "./")
container_test = container_test_env.Program("build/container_test/container_test",
                                            ["build/container_test/container_test.cpp"])
Depends("build/container_test/container_test", header_files)
container_test_env.Alias("container_test", container_test)