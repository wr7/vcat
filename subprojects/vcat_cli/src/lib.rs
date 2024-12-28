#[no_mangle]
extern "C" fn rust_hello() {
    std::eprintln!("Hello from rust");
}
