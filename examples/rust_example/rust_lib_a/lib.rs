use std::iter;

#[no_mangle]
pub fn rust_lib_a_thing(a: i32) -> i32 {
  println!("Hello {:?}!", a);
  let data: Vec<u8> = iter::repeat(a as u8).take(a as usize).collect();
  let b = std::str::from_utf8(&data);
  println!("Was it UTF? {:?}", b);
  42
}
