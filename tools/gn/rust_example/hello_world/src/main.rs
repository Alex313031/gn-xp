// foo.rs
extern crate foo;

fn main() {
    let f = foo::Foo::new("hello");
    println!("Hello world!");
    println!("I'm from a dependency: {:?}!", f);
}