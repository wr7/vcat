use std::{alloc::Layout, mem::ManuallyDrop, str::FromStr};

pub use shared::*;

mod shared {
    use crate::ffi_macros::*;

    use f64 as double;
    use i32 as int32_t;
    use u64 as uint64_t;
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

impl FromStr for SampleFormat {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let s = s
            .chars()
            .filter(|c| !c.is_whitespace())
            .map(|c| c.to_ascii_lowercase())
            .collect::<String>();

        let s = s
            .strip_prefix('s')
            .or_else(|| s.strip_prefix('u'))
            .or_else(|| s.strip_prefix('i'))
            .unwrap_or(&s);

        let s = s.strip_suffix('p').unwrap_or(s);

        Ok(match s {
            "16" => SampleFormat::s16,
            "32" => SampleFormat::s32,
            "float" => SampleFormat::flt,
            "flt" => SampleFormat::flt,
            _ => return Err(()),
        })
    }
}
