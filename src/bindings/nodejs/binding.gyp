{
  "targets": [
    {
      "target_name": "nimcp_nodejs",
      "sources": [ "binding.c" ],
      "include_dirs": [
        "../../../include",
        "/usr/include/python3.12"
      ],
      "defines": [ "_GNU_SOURCE" ],
      "libraries": [
        "-L<(module_root_dir)/../../../build/lib",
        "-lnimcp"
      ],
      "cflags": ["-fPIC", "-std=c11"],
      "ldflags": ["-Wl,-rpath,<(module_root_dir)/../../../build/lib"]
    }
  ]
}
