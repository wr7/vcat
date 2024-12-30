mod ffi_macros;
mod shared;

#[no_mangle]
extern "C" fn rust_hello() {
    std::eprintln!("Hello from rust");
}
