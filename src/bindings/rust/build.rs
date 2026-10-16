fn main() {
    println!("cargo:rustc-link-search=native=../../../build/lib");
    println!("cargo:rustc-link-lib=nimcp");
    println!("cargo:rerun-if-changed=../../include/nimcp.h");
}
