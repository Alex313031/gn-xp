// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: compile this with ninja instead.
fn main() -> miette::Result<()> {
    let here = std::path::PathBuf::from(".");
    let src = std::path::PathBuf::from("../");
    let out = std::path::PathBuf::from("../../out");
    let mut b = autocxx_build::Builder::new("stargn_main.rs", &[&here, &src, &out])
        .extra_clang_args(&["-std=c++17"])
        .build()?;
    b.file("gn_helpers.cc")
        .file("stargn_main.cc")
        .file("starlark_loader.cc")
        .flag_if_supported("-Wno-unused-parameter")
        .flag_if_supported("-std=c++17")
        .compile("stargn");
    println!("cargo:rerun-if-changed=gn_helpers.h");
    println!("cargo:rerun-if-changed=gn_helpers.cc");
    println!("cargo:rerun-if-changed=stargn_main.h");
    println!("cargo:rerun-if-changed=stargn_main.cc");
    println!("cargo:rerun-if-changed=stargn_main.rs");
    println!("cargo:rerun-if-changed=starlark_loader.h");
    println!("cargo:rerun-if-changed=starlark_loader.cc");
    println!("cargo:rustc-link-search=../../out");
    println!("cargo:rustc-link-lib=base");
    println!("cargo:rustc-link-lib=gn");
    Ok(())
}
