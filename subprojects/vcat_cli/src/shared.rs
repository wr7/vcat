use std::{ffi::c_void, ptr};

pub use shared::*;

mod shared {
    use crate::ffi_macros::*;

    use u8 as uint8_t;
    use usize as size_t;

    core::include! {"../inc/shared.rs"}
}

#[allow(unused)]
impl<T> shared::Vector<T> {
    pub fn new() -> Self {
        Self {
            length: 0,
            capacity: 0,
            data: ptr::null_mut(),
        }
    }

    pub fn reserve_exact(&mut self, additional: usize) {
        if self.length + additional > self.capacity {
            unsafe {
                self.capacity = self.length + additional;
                self.data = libc::realloc(
                    self.data.cast::<c_void>(),
                    self.capacity * std::mem::size_of::<T>(),
                )
                .cast::<T>()
            };
        }
    }

    pub fn reserve(&mut self, additional: usize) {
        let mut new_capacity = self.capacity;

        while new_capacity < self.length + additional {
            new_capacity = new_capacity * 2 + 3;
        }

        self.reserve_exact(new_capacity - self.length);
    }

    pub fn push(&mut self, item: T) {
        self.reserve(1);

        unsafe {
            self.data.offset(self.length as isize).write(item);
        }

        self.length += 1;
    }
}

impl<T> Drop for shared::Vector<T> {
    fn drop(&mut self) {
        if self.data.is_null() {
            return;
        }

        unsafe {
            libc::free(self.data.cast::<c_void>());
        }
    }
}

impl<T> From<&[T]> for shared::Vector<T> {
    fn from(value: &[T]) -> Self {
        let mut ret_val = Self::new();
        ret_val.reserve(value.len());

        unsafe {
            value
                .as_ptr()
                .copy_to_nonoverlapping(ret_val.data, value.len());
        }

        ret_val.length = value.len();

        ret_val
    }
}
