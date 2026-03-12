// secure_switch_axi.v
//
// Minimal AXI4-Lite slave that exposes slide switch states as a read-only
// register. This is the FPGA peripheral used as the target for TrustZone
// benchmark measurements -- the actual data (switch values) doesn't matter,
// what matters is that it's a real hardware register behind the PS-PL bridge.
//
// Two instances are placed in the block design:
//   secure_switch_0 (M00, TrustZone-protected) -- accessed via OP-TEE PTA
//   ns_switch_0     (M01, non-secure)          -- accessed via /dev/mem
// Both read the same physical switches.
//
// Register map:
//   Offset 0x00 (read):  bits [1:0] = sw[1:0], bits [31:2] = 0
//   All writes:          accepted and silently discarded (BRESP = OKAY)
//
// Board-agnostic -- identical RTL on both PYNQ-Z2 and AUP-ZU3.

`timescale 1ns / 1ps

module secure_switch_axi #(
    parameter C_S_AXI_DATA_WIDTH = 32,
    parameter C_S_AXI_ADDR_WIDTH = 4
)(
    input  wire [1:0] sw,               // physical slide switches

    // AXI4-Lite slave interface (active-low reset)
    input  wire                                s_axi_aclk,
    input  wire                                s_axi_aresetn,

    input  wire [C_S_AXI_ADDR_WIDTH-1:0]       s_axi_awaddr,
    input  wire [2:0]                          s_axi_awprot,
    input  wire                                s_axi_awvalid,
    output wire                                s_axi_awready,

    input  wire [C_S_AXI_DATA_WIDTH-1:0]       s_axi_wdata,
    input  wire [(C_S_AXI_DATA_WIDTH/8)-1:0]   s_axi_wstrb,
    input  wire                                s_axi_wvalid,
    output wire                                s_axi_wready,

    output wire [1:0]                          s_axi_bresp,
    output wire                                s_axi_bvalid,
    input  wire                                s_axi_bready,

    input  wire [C_S_AXI_ADDR_WIDTH-1:0]       s_axi_araddr,
    input  wire [2:0]                          s_axi_arprot,
    input  wire                                s_axi_arvalid,
    output wire                                s_axi_arready,

    output wire [C_S_AXI_DATA_WIDTH-1:0]       s_axi_rdata,
    output wire [1:0]                          s_axi_rresp,
    output wire                                s_axi_rvalid,
    input  wire                                s_axi_rready
);

    reg                                axi_awready;
    reg                                axi_wready;
    reg [1:0]                          axi_bresp;
    reg                                axi_bvalid;
    reg                                axi_arready;
    reg [C_S_AXI_DATA_WIDTH-1:0]       axi_rdata;
    reg [1:0]                          axi_rresp;
    reg                                axi_rvalid;

    // 2-stage synchronizer for the switch inputs (async -> aclk domain)
    reg [1:0] sw_sync1, sw_sync2;
    always @(posedge s_axi_aclk) begin
        sw_sync1 <= sw;
        sw_sync2 <= sw_sync1;
    end

    assign s_axi_awready = axi_awready;
    assign s_axi_wready  = axi_wready;
    assign s_axi_bresp   = axi_bresp;
    assign s_axi_bvalid  = axi_bvalid;
    assign s_axi_arready = axi_arready;
    assign s_axi_rdata   = axi_rdata;
    assign s_axi_rresp   = axi_rresp;
    assign s_axi_rvalid  = axi_rvalid;

    // Write channels -- accept and discard (this is a read-only peripheral)
    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            axi_awready <= 1'b0;
            axi_wready  <= 1'b0;
            axi_bvalid  <= 1'b0;
            axi_bresp   <= 2'b00;
        end else begin
            if (~axi_awready && s_axi_awvalid && s_axi_wvalid) begin
                axi_awready <= 1'b1;
            end else begin
                axi_awready <= 1'b0;
            end

            if (~axi_wready && s_axi_awvalid && s_axi_wvalid) begin
                axi_wready <= 1'b1;
            end else begin
                axi_wready <= 1'b0;
            end

            if (axi_awready && s_axi_awvalid && axi_wready && s_axi_wvalid && ~axi_bvalid) begin
                axi_bvalid <= 1'b1;
                axi_bresp  <= 2'b00;  // OKAY
            end else if (s_axi_bready && axi_bvalid) begin
                axi_bvalid <= 1'b0;
            end
        end
    end

    // Read channel -- return switch state on any read
    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            axi_arready <= 1'b0;
            axi_rvalid  <= 1'b0;
            axi_rresp   <= 2'b00;
            axi_rdata   <= {C_S_AXI_DATA_WIDTH{1'b0}};
        end else begin
            if (~axi_arready && s_axi_arvalid) begin
                axi_arready <= 1'b1;
            end else begin
                axi_arready <= 1'b0;
            end

            if (axi_arready && s_axi_arvalid && ~axi_rvalid) begin
                axi_rvalid <= 1'b1;
                axi_rresp  <= 2'b00;  // OKAY
                axi_rdata  <= {{(C_S_AXI_DATA_WIDTH-2){1'b0}}, sw_sync2};
            end else if (axi_rvalid && s_axi_rready) begin
                axi_rvalid <= 1'b0;
            end
        end
    end

endmodule
