// This file includes the type definitions shared between the C++ and Rust codebases
//
// Through the use of the macros defined in `vcat/src/shared.hh`, `vcat_cli/src/ffi_macros.rs`, and
// `vcat/codegen/shared_to_cpp.awk`, this file can be interpereted as either C++ or Rust
//
// This is currently a little rediculus and overcomplicated, but it is scalable, and will be good as
// the Rust codebase grows.

vcat_def!(
    template<typename T>
    struct Vector {
        field(length, size_t);
        field(capacity, size_t);
        field(data, pointer(T));

        has_destructor();
    };

    struct Parameters {
        field(file_name, Vector<uint8_t>);

        has_destructor();
    };
    rust_drop(Parameters_drop);
);
