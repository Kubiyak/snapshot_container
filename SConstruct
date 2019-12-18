optimized_env = Environment(CXX="g++-8", CXXFLAGS="--std=c++17 -O2")
optimized_env.VariantDir("build/optimized", "./")
optimized = optimized_env.Program('build/optimized/snapshot_container_test',
                                 ['build/optimized/compile_check.cpp'],
				 LIBS=['pthread'])
Depends('build/optimized/snapshot_container_test',
['virtual_iter.h', 'virtual_std_iter.h',
'snapshot_container.h', 'snapshot_slice.h', 'snapshot_storage_registry.h',
'snapshot_storage.h', 'snapshot_container_impl.h'])
optimized_env.Alias('optimized', optimized)
