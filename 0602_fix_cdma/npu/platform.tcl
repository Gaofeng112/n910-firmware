# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct /home/test/work_ssx/V2023/vitis/0602_fix_cdma/npu/platform.tcl
# 
# OR launch xsct and run below command.
# source /home/test/work_ssx/V2023/vitis/0602_fix_cdma/npu/platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {npu}\
-hw {/home/test/work_ssx/V2023/vivado/0602_fix_cdma/project_1.xpr/project_1/export/design_1_wrapper.xsa}\
-proc {microblaze_0} -os {standalone} -out {/home/test/work_ssx/V2023/vitis/0602_fix_cdma}

platform write
platform generate -domains 
platform active {npu}
platform generate
