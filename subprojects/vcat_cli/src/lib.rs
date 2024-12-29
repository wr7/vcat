mod ffi_macros;
mod shared;

#[no_mangle]
extern "C" fn rust_hello() {
    let x: shared::Vector<u8>;

    std::eprintln!("Hello from rust");
}
