
module axi_bram #(
    parameter AXI_IDWIDTH = 4,
    parameter AXI_AWIDTH  = 64,
    parameter AXI_DWIDTH  = 256,
    parameter MEM_AWIDTH  = 14      // BRAM size = MEM_AWIDTH*C_M_AXI_DATA_WIDTH (bits) = MEM_AWIDTH*C_M_AXI_DATA_WIDTH/8 (bytes)
) (
    input  wire                        rstn,
    input  wire                        clk,
    // AXI-MM AW interface ----------------------------------------------------
    output wire                        s_axi_awready,
    input  wire                        s_axi_awvalid,
    input  wire   [    AXI_AWIDTH-1:0] s_axi_awaddr,
    input  wire   [               7:0] s_axi_awlen,
    input  wire   [   AXI_IDWIDTH-1:0] s_axi_awid,
    // AXI-MM W  interface ----------------------------------------------------
    output wire                        s_axi_wready,
    input  wire                        s_axi_wvalid,
    input  wire                        s_axi_wlast,
    input  wire   [    AXI_DWIDTH-1:0] s_axi_wdata,
    input  wire   [(AXI_DWIDTH/8)-1:0] s_axi_wstrb,
    // AXI-MM B  interface ----------------------------------------------------
    input  wire                        s_axi_bready,
    output wire                        s_axi_bvalid,
    output wire   [   AXI_IDWIDTH-1:0] s_axi_bid,
    output wire   [               1:0] s_axi_bresp,
    // AXI-MM AR interface ----------------------------------------------------
    output wire                        s_axi_arready,
    input  wire                        s_axi_arvalid,
    input  wire   [    AXI_AWIDTH-1:0] s_axi_araddr,
    input  wire   [               7:0] s_axi_arlen,
    input  wire   [   AXI_IDWIDTH-1:0] s_axi_arid,
    // AXI-MM R  interface ----------------------------------------------------
    input  wire                        s_axi_rready,
    output wire                        s_axi_rvalid,
    output wire                        s_axi_rlast,
    output reg    [    AXI_DWIDTH-1:0] s_axi_rdata,
    output wire   [   AXI_IDWIDTH-1:0] s_axi_rid,
    output wire   [               1:0] s_axi_rresp 
);


function automatic int log2 (input int x);
    int xtmp = x, y = 0;
    while (xtmp != 0) begin
        y ++;
        xtmp >>= 1;
    end
    return (y == 0) ? 0 : (y - 1);
endfunction



// ---------------------------------------------------------------------------------------
// AXI READ state machine
// ---------------------------------------------------------------------------------------

reg calc_active = 0;
enum reg [0:0] {R_IDLE, R_BUSY} rstate = R_IDLE;

reg  [AXI_IDWIDTH-1:0] rid = '0;
reg  [            7:0] rcount = '0;
reg  [ MEM_AWIDTH-1:0] mem_raddr, mem_raddr_last;

assign s_axi_arready = (rstate == R_IDLE) && !calc_active;
assign s_axi_rvalid  = (rstate == R_BUSY);
assign s_axi_rlast   = (rstate == R_BUSY) && (rcount == 8'd0);
assign s_axi_rid     = rid;
assign s_axi_rresp   = '0;

always @ (posedge clk or negedge rstn)
    if (~rstn) begin
        rstate <= R_IDLE;
        rid    <= '0;
        rcount <= '0;
    end else begin
        case (rstate)
            R_IDLE :
                if (s_axi_arvalid && s_axi_arready) begin
                    rstate <= R_BUSY;
                    rid    <= s_axi_arid;
                    rcount <= s_axi_arlen;
                end
            R_BUSY :
                if (s_axi_rready) begin
                    if (rcount == 8'd0)       // the last data of read session
                        rstate <= R_IDLE;
                    rcount <= rcount - 8'd1;
                end
        endcase
    end

always_comb
    if      (rstate == R_IDLE && s_axi_arvalid && s_axi_arready)
        mem_raddr = (MEM_AWIDTH)'(s_axi_araddr >> log2(AXI_DWIDTH/8));
    else if (rstate == R_BUSY && s_axi_rready)
        mem_raddr = mem_raddr_last + (MEM_AWIDTH)'(1);
    else
        mem_raddr = mem_raddr_last;

always @ (posedge clk)
    mem_raddr_last <= mem_raddr;



// ---------------------------------------------------------------------------------------
// AXI WRITE state machine
// ---------------------------------------------------------------------------------------

enum reg [1:0] {W_IDLE, W_BUSY, W_RESP} wstate = W_IDLE;

reg  [AXI_IDWIDTH-1:0] wid = '0;
reg  [            7:0] wcount = '0;
reg  [ MEM_AWIDTH-1:0] mem_waddr = '0;
reg                    calc_done = '0;

assign s_axi_awready = (wstate == W_IDLE) && !calc_active;
assign s_axi_wready  = (wstate == W_BUSY);
assign s_axi_bvalid  = (wstate == W_RESP);
assign s_axi_bid     = wid;
assign s_axi_bresp   = '0;

always @ (posedge clk or negedge rstn)
    if (~rstn) begin
        wstate <= W_IDLE;
        wid <= '0;
        wcount <= '0;
        mem_waddr <= '0;
    end else begin
        case (wstate)
            W_IDLE :
                if (s_axi_awvalid) begin
                    wstate <= W_BUSY;
                    wid <= s_axi_awid;
                    wcount <= s_axi_awlen;
                    mem_waddr <= (MEM_AWIDTH)'(s_axi_awaddr >> log2(AXI_DWIDTH/8));
                end
            W_BUSY :
                if (s_axi_wvalid) begin
                    if (wcount == 8'd0 || s_axi_wlast) begin
                        wstate <= W_RESP;
                    end
                    wcount <= wcount - 8'd1;
                    mem_waddr <= mem_waddr + (MEM_AWIDTH)'(1);
                end
            W_RESP :
                if (s_axi_bready)
                    wstate <= W_IDLE;
            default :
                wstate <= W_IDLE;
        endcase
    end



// ---------------------------------------------------------------------------------------
// a block RAM & FP32 Calculation
// ---------------------------------------------------------------------------------------

(* ram_style = "block" *) reg [31:0]  mem_C_0 [ 0:8191 ];
(* ram_style = "block" *) reg [31:0]  mem_C_1 [ 0:8191 ];
(* ram_style = "block" *) reg [31:0]  mem_C_2 [ 0:8191 ];
(* ram_style = "block" *) reg [31:0]  mem_C_3 [ 0:8191 ];
(* ram_style = "block" *) reg [31:0]  mem_C_4 [ 0:8191 ];
(* ram_style = "block" *) reg [31:0]  mem_C_5 [ 0:8191 ];
(* ram_style = "block" *) reg [31:0]  mem_C_6 [ 0:8191 ];
(* ram_style = "block" *) reg [31:0]  mem_C_7 [ 0:8191 ];

always @ (posedge clk) begin
    s_axi_rdata[  0 +: 32] <= mem_C_0[mem_raddr[12:0]];
    s_axi_rdata[ 32 +: 32] <= mem_C_1[mem_raddr[12:0]];
    s_axi_rdata[ 64 +: 32] <= mem_C_2[mem_raddr[12:0]];
    s_axi_rdata[ 96 +: 32] <= mem_C_3[mem_raddr[12:0]];
    s_axi_rdata[128 +: 32] <= mem_C_4[mem_raddr[12:0]];
    s_axi_rdata[160 +: 32] <= mem_C_5[mem_raddr[12:0]];
    s_axi_rdata[192 +: 32] <= mem_C_6[mem_raddr[12:0]];
    s_axi_rdata[224 +: 32] <= mem_C_7[mem_raddr[12:0]];
end

genvar k;
generate
    for (k = 0; k < 256; k = k + 1) begin : gen_mem_A
        (* ram_style = "block" *) reg [31:0] mem_A [ 0:255 ];
        always @ (posedge clk) begin
            if (s_axi_wvalid & s_axi_wready) begin
                if (mem_waddr[4:0] == k / 8) begin
                    if (mem_waddr[MEM_AWIDTH-1] == 1'b1) begin
                        mem_A[mem_waddr[12:5]] <= s_axi_wdata[(k % 8)*32 +: 32];
                    end
                end
            end
        end
    end
endgenerate

genvar kb;
generate
    for (kb = 0; kb < 1024; kb = kb + 1) begin : gen_mem_B
        (* ram_style = "block" *) reg [31:0] mem_B [ 0:63 ];
        always @ (posedge clk) begin
            if (s_axi_wvalid & s_axi_wready) begin
                if (mem_waddr[MEM_AWIDTH-1] == 1'b0) begin
                    if (mem_waddr[6:0] == kb / 8) begin
                        mem_B[mem_waddr[12:7]] <= s_axi_wdata[(kb % 8)*32 +: 32];
                    end
                end
            end
        end
    end
endgenerate
    
    reg [8:0] calc_a_row = 0;
    reg [5:0] calc_b_row = 0;
    reg       mul_in_valid = 0;
    reg [8191:0] mem_A_rd = 0;
    reg [32767:0] mem_B_rd = 0;
    
    wire [8191:0] mem_A_rd_wire;
    wire [32767:0] mem_B_rd_wire;
    
    genvar g_rd_a;
    generate
        for (g_rd_a = 0; g_rd_a < 256; g_rd_a = g_rd_a + 1) begin : gen_rd_a
            assign mem_A_rd_wire[g_rd_a*32 +: 32] = gen_mem_A[g_rd_a].mem_A[calc_a_row[7:0]];
        end
    endgenerate

    genvar g_rd_b;
    generate
        for (g_rd_b = 0; g_rd_b < 1024; g_rd_b = g_rd_b + 1) begin : gen_rd_b
            assign mem_B_rd_wire[g_rd_b*32 +: 32] = gen_mem_B[g_rd_b].mem_B[calc_b_row[5:0]];
        end
    endgenerate
    
    always @(posedge clk or negedge rstn) begin
        if (!rstn) begin
            calc_active <= 0;
            calc_a_row <= 0;
            calc_b_row <= 0;
            mul_in_valid <= 0;
        end else begin
            if (wstate == W_BUSY && s_axi_wvalid && (wcount == 8'd0 || s_axi_wlast) && mem_waddr == {MEM_AWIDTH{1'b1}}) begin
                calc_active <= 1;
                calc_a_row <= 0;
                calc_b_row <= 0;
            end else if (calc_active) begin
                if (calc_a_row < 256) begin
                    mem_A_rd <= mem_A_rd_wire;
                    mem_B_rd <= mem_B_rd_wire;
                    mul_in_valid <= 1;
                    if (calc_b_row == 63) begin
                        calc_b_row <= 0;
                        calc_a_row <= calc_a_row + 1;
                    end else begin
                        calc_b_row <= calc_b_row + 1;
                    end
                end else begin
                    mul_in_valid <= 0;
                    calc_active <= 0;
                end
            end else begin
                mul_in_valid <= 0;
            end
        end
    end

    wire [31:0] mul_out_data [0:1023];
    wire        mul_out_valid [0:1023];
    wire        mul_a_tready [0:1023];
    wire        mul_b_tready [0:1023];

    genvar gi;
    generate
        for (gi = 0; gi < 1024; gi = gi + 1) begin : gen_mul
            fp32_mul_0 u_mul (
                .aclk                 (clk),
                .s_axis_a_tvalid      (mul_in_valid),
                .s_axis_a_tdata       (mem_A_rd[(gi%256)*32 +: 32]),
                .s_axis_a_tready      (mul_a_tready[gi]),
                .s_axis_b_tvalid      (mul_in_valid),
                .s_axis_b_tdata       (mem_B_rd[gi*32 +: 32]),
                .s_axis_b_tready      (mul_b_tready[gi]),
                .m_axis_result_tvalid (mul_out_valid[gi]),
                .m_axis_result_tdata  (mul_out_data[gi]),
                .m_axis_result_tready (1'b1)
            );
        end
    endgenerate

    wire [31:0] add_tree_out [0:3];
    wire        add_tree_valid [0:3];

    genvar tree_idx;
    generate
        for (tree_idx = 0; tree_idx < 4; tree_idx = tree_idx + 1) begin : gen_adder_trees
            wire [31:0] t1_data [0:127]; wire t1_vld [0:127];
            wire [31:0] t2_data [0:63];  wire t2_vld [0:63];
            wire [31:0] t3_data [0:31];  wire t3_vld [0:31];
            wire [31:0] t4_data [0:15];  wire t4_vld [0:15];
            wire [31:0] t5_data [0:7];   wire t5_vld [0:7];
            wire [31:0] t6_data [0:3];   wire t6_vld [0:3];
            wire [31:0] t7_data [0:1];   wire t7_vld [0:1];
            
            genvar i;
            for (i = 0; i < 128; i = i + 1) begin : gen_t1
                fp32_add_0 u_t1 (.aclk(clk), .s_axis_a_tvalid(mul_out_valid[tree_idx*256 + i*2]), .s_axis_a_tdata(mul_out_data[tree_idx*256 + i*2]), .s_axis_a_tready(), .s_axis_b_tvalid(mul_out_valid[tree_idx*256 + i*2+1]), .s_axis_b_tdata(mul_out_data[tree_idx*256 + i*2+1]), .s_axis_b_tready(), .m_axis_result_tvalid(t1_vld[i]), .m_axis_result_tdata(t1_data[i]), .m_axis_result_tready(1'b1));
            end
            for (i = 0; i < 64; i = i + 1) begin : gen_t2
                fp32_add_0 u_t2 (.aclk(clk), .s_axis_a_tvalid(t1_vld[i*2]), .s_axis_a_tdata(t1_data[i*2]), .s_axis_a_tready(), .s_axis_b_tvalid(t1_vld[i*2+1]), .s_axis_b_tdata(t1_data[i*2+1]), .s_axis_b_tready(), .m_axis_result_tvalid(t2_vld[i]), .m_axis_result_tdata(t2_data[i]), .m_axis_result_tready(1'b1));
            end
            for (i = 0; i < 32; i = i + 1) begin : gen_t3
                fp32_add_0 u_t3 (.aclk(clk), .s_axis_a_tvalid(t2_vld[i*2]), .s_axis_a_tdata(t2_data[i*2]), .s_axis_a_tready(), .s_axis_b_tvalid(t2_vld[i*2+1]), .s_axis_b_tdata(t2_data[i*2+1]), .s_axis_b_tready(), .m_axis_result_tvalid(t3_vld[i]), .m_axis_result_tdata(t3_data[i]), .m_axis_result_tready(1'b1));
            end
            for (i = 0; i < 16; i = i + 1) begin : gen_t4
                fp32_add_0 u_t4 (.aclk(clk), .s_axis_a_tvalid(t3_vld[i*2]), .s_axis_a_tdata(t3_data[i*2]), .s_axis_a_tready(), .s_axis_b_tvalid(t3_vld[i*2+1]), .s_axis_b_tdata(t3_data[i*2+1]), .s_axis_b_tready(), .m_axis_result_tvalid(t4_vld[i]), .m_axis_result_tdata(t4_data[i]), .m_axis_result_tready(1'b1));
            end
            for (i = 0; i < 8; i = i + 1) begin : gen_t5
                fp32_add_0 u_t5 (.aclk(clk), .s_axis_a_tvalid(t4_vld[i*2]), .s_axis_a_tdata(t4_data[i*2]), .s_axis_a_tready(), .s_axis_b_tvalid(t4_vld[i*2+1]), .s_axis_b_tdata(t4_data[i*2+1]), .s_axis_b_tready(), .m_axis_result_tvalid(t5_vld[i]), .m_axis_result_tdata(t5_data[i]), .m_axis_result_tready(1'b1));
            end
            for (i = 0; i < 4; i = i + 1) begin : gen_t6
                fp32_add_0 u_t6 (.aclk(clk), .s_axis_a_tvalid(t5_vld[i*2]), .s_axis_a_tdata(t5_data[i*2]), .s_axis_a_tready(), .s_axis_b_tvalid(t5_vld[i*2+1]), .s_axis_b_tdata(t5_data[i*2+1]), .s_axis_b_tready(), .m_axis_result_tvalid(t6_vld[i]), .m_axis_result_tdata(t6_data[i]), .m_axis_result_tready(1'b1));
            end
            for (i = 0; i < 2; i = i + 1) begin : gen_t7
                fp32_add_0 u_t7 (.aclk(clk), .s_axis_a_tvalid(t6_vld[i*2]), .s_axis_a_tdata(t6_data[i*2]), .s_axis_a_tready(), .s_axis_b_tvalid(t6_vld[i*2+1]), .s_axis_b_tdata(t6_data[i*2+1]), .s_axis_b_tready(), .m_axis_result_tvalid(t7_vld[i]), .m_axis_result_tdata(t7_data[i]), .m_axis_result_tready(1'b1));
            end
            fp32_add_0 u_t8 (.aclk(clk), .s_axis_a_tvalid(t7_vld[0]), .s_axis_a_tdata(t7_data[0]), .s_axis_a_tready(), .s_axis_b_tvalid(t7_vld[1]), .s_axis_b_tdata(t7_data[1]), .s_axis_b_tready(), .m_axis_result_tvalid(add_tree_valid[tree_idx]), .m_axis_result_tdata(add_tree_out[tree_idx]), .m_axis_result_tready(1'b1));
        end
    endgenerate

    reg [7:0] out_a_row = 0;
    reg [5:0] out_b_slice = 0;
    reg [14:0] result_count = 0;

    always @(posedge clk or negedge rstn) begin
        if (!rstn) begin
            out_a_row <= 0;
            out_b_slice <= 0;
            result_count <= 0;
            calc_done <= 0;
        end else begin
            calc_done <= 0;
            if (wstate == W_BUSY && s_axi_wvalid && (wcount == 8'd0 || s_axi_wlast) && mem_waddr == {MEM_AWIDTH{1'b1}}) begin
                out_a_row <= 0;
                out_b_slice <= 0;
                result_count <= 0;
            end else if (add_tree_valid[0]) begin
                result_count <= result_count + 1;
                if (result_count == 15'd16383) begin // 16384 total
                    calc_done <= 1;
                end
                
                if (out_b_slice == 6'd63) begin
                    out_b_slice <= 0;
                    out_a_row <= out_a_row + 1;
                end else begin
                    out_b_slice <= out_b_slice + 1;
                end
            end
        end
    end

    wire [12:0] mem_c_waddr = {out_a_row, out_b_slice[5:1]};

    always @(posedge clk) begin
        if (add_tree_valid[0]) begin
            if (out_b_slice[0] == 1'b0) begin
                mem_C_0[mem_c_waddr] <= add_tree_out[0];
                mem_C_1[mem_c_waddr] <= add_tree_out[1];
                mem_C_2[mem_c_waddr] <= add_tree_out[2];
                mem_C_3[mem_c_waddr] <= add_tree_out[3];
            end else begin
                mem_C_4[mem_c_waddr] <= add_tree_out[0];
                mem_C_5[mem_c_waddr] <= add_tree_out[1];
                mem_C_6[mem_c_waddr] <= add_tree_out[2];
                mem_C_7[mem_c_waddr] <= add_tree_out[3];
            end
        end
    end
    
endmodule


