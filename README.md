# N910 Firmware

本目录是 N910 firmware 的 Vitis 工作区。当前工程位于 `0602_fix_cdma`，主要 firmware 源码位于：

```text
0602_fix_cdma/npu_app/src
```

工程配合 `tcl_scripts` 中的 XSCT Tcl 脚本使用，用于加载测试 case、烧录 bitstream、运行 firmware，并收集 FPGA 测试结果。

## 目录结构

```text
.
+-- 0602_fix_cdma/              # Vitis 工程
|   +-- npu_app/src/            # Firmware 源码
|   +-- npu_app/Debug/          # 本地编译输出
|   +-- npu/                    # Platform 和硬件导出相关文件
+-- cases/                      # 本地测试 case
+-- fpga_results/               # 本地测试结果 CSV
+-- tcl_scripts/                # XSCT 自动化脚本
+-- README.md
```

## 运行测试

1. 在 case 根目录下准备测试目录，例如：

   ```text
   /home/test/work_ssx/cases/00001_v013_vgg16_Conv1
   ```

2. 启动 `xsct`。

3. 设置 case 根目录和 Vitis 工程目录：

   ```tcl
   set AUTOTEST_CASE_ROOT /home/test/work_ssx/cases
   set AUTOTEST_PROJECT_ROOT /home/test/work_ssx/V2023/vitis/0602_fix_cdma
   ```

4. 加载运行脚本：

   ```tcl
   source /home/test/work_ssx/vitis-ws/tcl_scripts/run_case_vgg_0531_1135.tcl
   ```

5. 观察串口输出，波特率为 `9600`。

脚本会将 CSV 结果写入 `RESULT_DIR`。运行前请根据本机环境修改 Tcl 脚本中的 `RESULT_DIR`。

Linux 环境下如需终止自动测试，可在另一个 shell 中创建停止文件：

```sh
touch /home/test/work_ssx/fpga_results/STOP_AUTOTEST
```

Windows 下的停止文件行为尚未验证。

## 版本管理

根目录 `.gitignore` 会忽略本地编译输出、日志、FPGA 测试结果、case 数据、压缩包和 IDE 元数据。源码、Tcl 脚本、工程描述文件和文档默认可以被 Git 跟踪。

如果某个二进制文件确实需要纳入版本库，可以使用：

```sh
git add -f <path>
```
