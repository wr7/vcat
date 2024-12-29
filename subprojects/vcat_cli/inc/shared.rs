// This file includes the type definitions shared between the C++ and Rust codebases
//
// Through the use of macros and `vcat/codegen/shared_to_cpp.awk`, this file can be interpereted as either valid C++ or valid Rust

vcat_def!(
    template<typename T>
    struct Vector {
        field(length, size_t);
        field(capacity, size_t);
        field(data, pointer(T));
    };

    struct Parameters {
        field(file_name, Vector<uint8_t>);
    };
);
