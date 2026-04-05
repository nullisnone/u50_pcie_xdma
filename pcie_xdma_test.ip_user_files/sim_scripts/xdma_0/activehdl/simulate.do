onbreak {quit -force}
onerror {quit -force}

asim +access +r +m+xdma_0  -L xilinx_vip -L xpm -L gtwizard_ultrascale_v1_7_14 -L xil_defaultlib -L blk_mem_gen_v8_4_5 -L xdma_v4_1_20 -L unisims_ver -L unimacro_ver -L secureip -O5 xil_defaultlib.xdma_0 xil_defaultlib.glbl

set NumericStdNoWarnings 1
set StdArithNoWarnings 1

do {wave.do}

view wave
view structure

do {xdma_0.udo}

run

endsim

quit -force
