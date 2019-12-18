header_files = ['virtual_iter.h', 'virtual_std_iter.h',
                'snapshot_container.h', 'snapshot_slice.h', 'snapshot_storage_registry.h',
                'snapshot_storage.h']

optimized_env = Environment(CXX="g++-8", CXXFLAGS="--std=c++17 -O2")
optimized_env.VariantDir("build/optimized", "./")
optimized = optimized_env.Program('build/optimized/snapshot_container_test',
                                 ['build/optimized/compile_check.cpp'],
				 LIBS=['pthread'])

Depends('build/optimized/snapshot_container_test', header_files)
optimized_env.Alias('optimized', optimized)


debug_env = Environment(CXX="g++-8", CXXFLAGS="--std=c++17 -g")
optimized_env.VariantDir("build/debug", "./")
debug = debug_env.Program('build/debug/snapshot_container_test',
                          ['build/debug/compile_check.cpp'],
                          LIBS=['pthread'])

Depends('build/debug/snapshot_container_test', header_files)
optimized_env.Alias('debug', debug)
