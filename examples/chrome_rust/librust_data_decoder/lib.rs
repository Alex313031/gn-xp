use std::convert::TryInto;

extern "C" {
    pub fn callback_into_c() -> i32;
}


#[no_mangle]
pub fn rust_lib_c_thing(length: u64) -> bool {
    let l: i32 = length.try_into().unwrap();
    let _ = l * unsafe { callback_into_c() };
    true
}
