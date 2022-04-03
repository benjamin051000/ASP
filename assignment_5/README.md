# VSCode Linux kernel header include paths for Intellisense
Add this to `.vscode/c_cpp_properties.json`:

```json
{
    "configurations": [
        {
            "name": "Linux",
            "browse": {
                "path":[
                    "/usr/include",
                    "/usr/local/include",
                    "/usr/src/linux-headers-5.13.0-39-generic/include",
                    "/usr/src/linux-headers-5.13.0-39-generic/arch/x86/include/",
                    "${workspaceFolder}"
                ]
            },
            "defines": [
                "__KERNEL__",
                "MODULE"
            ],
            "compilerPath": "/usr/bin/gcc",
            "cStandard": "c99",
            "cppStandard": "c++14",
            "intelliSenseMode": "gcc-x64",
            "includePath": [
                "${workspaceFolder}/**",
                "/usr/include/**",
                "/usr/src/linux-headers-5.13.0-39-generic/arch/x86/include",
                "/usr/src/linux-headers-5.13.0-39-generic/arch/x86/include/generated",
                "/usr/src/linux-headers-5.13.0-39-generic/include",
                "/usr/src/linux-headers-5.13.0-39-generic/arch/x86/include/uapi",
                "/usr/src/linux-headers-5.13.0-39-generic/arch/x86/include/generated/uapi",
                "/usr/src/linux-headers-5.13.0-39-generic/include/uapi",
                "/usr/src/linux-headers-5.13.0-39-generic/include/generated/uapi",
                "/usr/src/linux-headers-5.13.0-39-generic/ubuntu/include",
                "/usr/lib/gcc/x86_64-linux-gnu/11/include"
            ],
            "compilerArgs": [
                "-nostdinc",
                "-include", "/usr/src/linux-headers-5.13.0-39-generic/include/linux/kconfig.h",
                "-include", "/usr/src/linux-headers-5.13.0-39-generic/include/linux/compiler_types.h"
            ]
        }
    ],
    "version": 4
}
```