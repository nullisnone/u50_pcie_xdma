# 1. Connect to Hardware Server and Open Target
open_hw_manager
connect_hw_server -allow_non_jtag
open_hw_target

# 2. Select the Device
current_hw_device [get_hw_devices xcu50_u55n_0]
refresh_hw_device -update_hw_probes false [lindex [get_hw_devices xcu50_u55n_0] 0]

# 3. Create Configuration Memory Object
create_hw_cfgmem -hw_device [lindex [get_hw_devices xcu50_u55n_0] 0] [lindex [get_cfgmem_parts {mt25qu01g-spi-x1_x2_x4}] 0]

# 4. Set initial properties
set_property PROGRAM.BLANK_CHECK  0 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
set_property PROGRAM.ERASE  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
set_property PROGRAM.CFG_PROGRAM  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
set_property PROGRAM.VERIFY  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
set_property PROGRAM.CHECKSUM  0 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
refresh_hw_device [lindex [get_hw_devices xcu50_u55n_0] 0]

# 5. Generate the MCS file (Note the correct 0x01002000 start address)
write_cfgmem -force -format mcs -size 128 -interface SPIx4 -loadbit {up 0x01002000 "/root/DOWNLOAD/U50/U50-PCIE/u50_pcie_xdma/pcie_xdma_test.runs/impl_1/pcie_xdma_test.bit" } -file "/root/DOWNLOAD/U50/U50-PCIE/u50_pcie_xdma/pcie_xdma_test.mcs"

# 6. Configure Flash Programming Properties
set_property PROGRAM.ADDRESS_RANGE  {use_file} [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
set_property PROGRAM.FILES [list "/root/DOWNLOAD/U50/U50-PCIE/u50_pcie_xdma/pcie_xdma_test.mcs" ] [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
set_property PROGRAM.PRM_FILE {} [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
set_property PROGRAM.UNUSED_PIN_TERMINATION {pull-none} [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
set_property PROGRAM.BLANK_CHECK  0 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
set_property PROGRAM.ERASE  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
set_property PROGRAM.CFG_PROGRAM  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
set_property PROGRAM.VERIFY  1 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
set_property PROGRAM.CHECKSUM  0 [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]

# 7. Execute Programming
startgroup 
create_hw_bitstream -hw_device [lindex [get_hw_devices xcu50_u55n_0] 0] [get_property PROGRAM.HW_CFGMEM_BITFILE [ lindex [get_hw_devices xcu50_u55n_0] 0]]
program_hw_devices [lindex [get_hw_devices xcu50_u55n_0] 0]
refresh_hw_device [lindex [get_hw_devices xcu50_u55n_0] 0]
program_hw_cfgmem -hw_cfgmem [ get_property PROGRAM.HW_CFGMEM [lindex [get_hw_devices xcu50_u55n_0] 0]]
endgroup

# Optional: Close the connection when done
# close_hw_target
# disconnect_hw_server
#
boot_hw_device  [lindex [get_hw_devices xcu50_u55n_0] 0]
refresh_hw_device [lindex [get_hw_devices xcu50_u55n_0] 0]
