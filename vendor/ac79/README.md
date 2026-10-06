# JieLi AC79 SDK files

The three files Felucca's package needs from the JieLi `fw-AC79_AIoT_SDK` (tag
`AC79NN_SDK_V1.2.1_2023-12-13`, https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK, Apache-2.0, see LICENSE):

- `cpu/wl82/tools/uboot.boot` (the SPL the package carries)
- `cpu/wl82/tools/cfg_tool.bin`
- `cpu/wl82/tools/cfg/eq_cfg_hw.bin`

Vendored so the build does not depend on Gitee being reachable. `AC79_SDK=$PWD/vendor/ac79 ./build.sh`
uses them; a full SDK checkout works the same way.
