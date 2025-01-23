use std::{alloc::Layout, mem::ManuallyDrop};

pub use shared::*;

mod shared {
    use crate::ffi_macros::*;

    use f64 as double;
    use i32 as int32_t;
    use u8 as uint8_t;
    use usize as size_t;

    core::include! {"../inc/shared.rs"}
}

#[no_mangle]
unsafe extern "C" fn rustalloc_free(data: *mut u8, size: usize, align: usize) {
    std::alloc::dealloc(data, Layout::from_size_align_unchecked(size, align));
}

impl<T> Drop for shared::Vector<T> {
    fn drop(&mut self) {
        if self.data.is_null() {
            return;
        }

        unsafe {
            core::mem::drop(Vec::from_raw_parts(self.data, self.length, self.capacity));
        }
    }
}

impl<T> From<Vec<T>> for shared::Vector<T> {
    fn from(value: Vec<T>) -> Self {
        let mut value = ManuallyDrop::new(value);

        Self {
            length: value.len(),
            capacity: value.capacity(),
            data: value.as_mut_ptr(),
        }
    }
}

impl From<String> for shared::Vector<u8> {
    fn from(value: String) -> Self {
        value.into_bytes().into()
    }
}
