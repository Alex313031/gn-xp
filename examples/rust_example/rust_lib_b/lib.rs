extern crate rust_lib_c;

use std::convert::TryInto;

extern "C" {
    pub fn callback_into_c() -> i32;
}


#[no_mangle]
pub fn rust_lib_c_thing(length: u64) -> bool {
    let l: i32 = length.try_into().unwrap();
    let l = l * unsafe { callback_into_c() };
    rust_lib_c::footle(l);
    true
}
