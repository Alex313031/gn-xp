# Metadata in GN

[TOC]

Metadata is information attached to targets throughout the dependency tree. GN allows for the collection of this data into files written during the generation step, enabing users to expose and aggregate this data based on the dependency tree.

## `generated_file`

Similar to the `write_file` function, the `generated_file` target type creates a file in the specified location with the specified content. The primary difference between the function and the target type is that the `write_file` function does the file write at parse time, while the `generated_file` target type writes at target resolution time. See [the reference docs](reference.md#func_generated_file) for more detail. When written at target resolution time, the `generated_file` enables GN to collect and write aggregated metadata from dependents.

A `generated_file` target can declare either `contents` (to write statically known contents to a file) or `data_keys`(to aggregate metadata and write the result to a file). It can also specify `walk_keys` (to restrict the metadata collection), [`output_conversion`](reference.md#io_conversion), and [`rebase`](reference.md#var_rebase).


## Collection and Aggregation

Targets can declare a `metadata` variable containing a scope, and this metadata is collected and written to file by `generated_file` aggregation targets. The `metadata` scope must contain only list values, since the aggregation step collects a list of these values.

During the target resolution, `generated_file` targets will walk their dependencies recursively, collecting metadata based on the specified `data_keys`. `data_keys` is specified as a list of strings, used by the walk to identify which variables in dependencies' `metadata` scopes to collect.

The walk begins with the listed dependencies of the `generated_file` target, for each checking the `metadata` scope for any of the `data_keys`. If present, the data in those variables is appended to the aggregate list. Note that this means that if more than one walk key is specified, the data in all of them will be aggregated into one list. From there, the walk will then recurse into the dependencies of each target it encounters, collecting the specified metadata for each.

For example: 

```
group("a") {
  metadata = {
    doom_melon = [ "enable" ]
    my_files = [ "foo.cpp" ]
    my_extra_files = [ "bar.cpp" ]
  }

  deps = [ ":b" ]
}

group("b") {
  metadata = {
    my_files = [ "baz.cpp" ]
  }
}

generated_file("metadata") {
  outputs = [ "$root_build_dir/my_files.json" ]
  data_keys = [ "my_files", "my_extra_files" ]

  deps = [ ":a" ]
}
```

The above will produce the following file data:

```
foo.cpp
bar.cpp
baz.cpp
```

The dependency walk can be limited by using the `walk_keys`. This is a list of [labels](reference.md#labels) that should be included in the walk. All labels specified here should also be in one of the deps lists. These keys act as barriers, where the walk will only recurse into targets listed here. An empty list in all specified barriers will end that portion of the walk.

```
group("a") {
  metadata = {
    my_files = [ "foo.cpp" ]
    my_files_barrier [ ":b" ]
  }

  deps = [ ":b", ":c" ]
}

group("b") {
  metadata = {
    my_files = [ "bar.cpp" ]
  }
}

group("c") {
  metadata = {
    my_files = [ "doom_melon.cpp" ]
  }
}

generated_file("metadata") {
  outputs = [ "$root_build_dir/my_files.json" ]
  data_keys = [ "my_files", "my_extra_files" ]

  deps = [ ":a" ]
}
```

The above will produce the following file data (note that `doom_melon.cpp` is not included):

```
foo.cpp
bar.cpp
```

A common example of this sort of barrier is in builds that have host tools built as part of the tree, but do not want the metadata from those host tools to be collected with the target-side code.

## Common Uses

Metadata can be used to collect information about the different targets in the build, and so a common use is to provide post-build tooling with a set of data necessary to do aggregation tasks. For example, if each test target specifies the output location of its binary to run in a metadata field, that can be collected into a single file listing the locations of all tests in the dependency tree. A local build tool (or continuous integration infrastructure) can then use that file to know which tests exist, and where, and run them accordingly.

Another use is in image creation, where a post-build image tool needs to know various pieces of information about the components it should include in order to put together the correct image.