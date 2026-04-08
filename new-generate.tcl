#!/usr/bin/tclsh
#----------------------------------------------------------------------
# 适配Vivado 2022.2，基于project_name.xpr工程配置
# 强制impl_1阶段使用16 个CPU核心
#----------------------------------------------------------------------

# 配置参数（根据你的实际路径修改）
set project_name "pcie_xdma_test"
set project_dir "./"
set cpu_cores 16  ;# 指定使用16个CPU核心


# 1. 全局多核配置（Vivado顶层配置）
puts "设置Vivado全局并行核心数为: $cpu_cores"
set_param general.maxThreads $cpu_cores

# 2. 打开工程
puts "打开工程: $project_dir/$project_name.xpr"
if {[file exists "$project_dir/$project_name.xpr"]} {
    open_project "$project_dir/$project_name.xpr"
} else {
    puts "工程文件不存在: $project_dir/$project_name.xpr"
}

# 2.5 运行综合流程
puts "开始运行synth_1综合流程（${cpu_cores}核并行）"
reset_run synth_1
launch_runs synth_1 -jobs $cpu_cores
wait_on_run synth_1

# 检查综合是否成功
set synth_status [get_property STATUS [get_runs synth_1]]
if {$synth_status != "synth_design Complete!"} {
    puts "synth_1综合流程失败，状态: $synth_status"
    exit 1
}
puts "synth_1综合流程成功完成"

# 3. 为impl_1配置16核并行（关键步骤）
puts "为impl_1配置16核并行参数"
current_run -implementation [get_runs impl_1]

# 4. 运行impl_1实现流程（16核并行）
puts "开始运行impl_1实现流程（16核并行）"
reset_run impl_1
launch_runs impl_1 -to_step route_design -jobs $cpu_cores
wait_on_run impl_1

# 检查实现是否成功
set run_status [get_property STATUS [get_runs impl_1]]
if {[string match "route_design Complete*" $run_status]} {
    puts "impl_1路由完成 (Status: $run_status)，生成自定义名称的比特流文件..."
    open_run impl_1
    write_bitstream -force "$project_dir/${project_name}.runs/impl_1/${project_name}.bit"
} else {
    puts "impl_1实现流程失败，状态: $run_status"
}

# 6. 检查比特流文件是否生成成功
if {[file exists "$project_dir/$project_name.runs/impl_1/$project_name.bit"]} {
    puts "比特流生成成功！文件路径: $project_dir/$project_name.runs/impl_1/$project_name.bit"
} else {
    puts "比特流文件生成失败！"
}

# write_cfgmem  -format mcs -size 128 -interface SPIx4 -loadbit {up 0x01002000 "/root/DOWNLOAD/U50/U50-PCIE/netfpga_pcie_x1_xdma_bram/pcie_xdma_test.runs/impl_1/pcie_xdma_test.bit" } -file "/root/DOWNLOAD/U50/U50-PCIE/netfpga_pcie_x1_xdma_bram/pcie_xdma_test.mcs"

# 7. 关闭工程
close_project
