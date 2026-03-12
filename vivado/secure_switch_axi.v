// secure_switch_axi.v
// AXI4-Lite slave that exposes slide switch states as a read-only register.
// Register 0 (offset 0x00): bits [1:0] = sw[1:0], bits [31:2] = 0
// All writes are ignored.
//
// Board-agnostic — works on both PYNQ-Z2 (Zynq-7000) and AUP-ZU3 (ZU+).

`timescale 1ns / 1ps

module secure_switch_axi #(
    parameter C_S_AXI_DATA_WIDTH = 32,
    parameter C_S_AXI_ADDR_WIDTH = 4
)(
    // Switch inputs
    input  wire [1:0] sw,

    // AXI4-Lite Slave Interface
    input  wire                                s_axi_aclk,
    input  wire                                s_axi_aresetn,

    // Write address channel
    input  wire [C_S_AXI_ADDR_WIDTH-1:0]       s_axi_awaddr,
    input  wire [2:0]                          s_axi_awprot,
    input  wire                                s_axi_awvalid,
    output wire                                s_axi_awready,

    // Write data channel
    input  wire [C_S_AXI_DATA_WIDTH-1:0]       s_axi_wdata,
    input  wire [(C_S_AXI_DATA_WIDTH/8)-1:0]   s_axi_wstrb,
    input  wire                                s_axi_wvalid,
    output wire                                s_axi_wready,

    // Write response channel
    output wire [1:0]                          s_axi_bresp,
    output wire                                s_axi_bvalid,
    input  wire                                s_axi_bready,

    // Read address channel
    input  wire [C_S_AXI_ADDR_WIDTH-1:0]       s_axi_araddr,
    input  wire [2:0]                          s_axi_arprot,
    input  wire                                s_axi_arvalid,
    output wire                                s_axi_arready,

    // Read data channel
    output wire [C_S_AXI_DATA_WIDTH-1:0]       s_axi_rdata,
    output wire [1:0]                          s_axi_rresp,
    output wire                                s_axi_rvalid,
    input  wire                                s_axi_rready
);

    // ---------------------------------------------------------------
    // Internal signals
    // ---------------------------------------------------------------
    reg                                axi_awready;
    reg                                axi_wready;
    reg [1:0]                          axi_bresp;
    reg                                axi_bvalid;
    reg                                axi_arready;
    reg [C_S_AXI_DATA_WIDTH-1:0]       axi_rdata;
    reg [1:0]                          axi_rresp;
    reg                                axi_rvalid;

    // Synchronize switch inputs (2-stage sync to avoid metastability)
    reg [1:0] sw_sync1, sw_sync2;
    always @(posedge s_axi_aclk) begin
        sw_sync1 <= sw;
        sw_sync2 <= sw_sync1;
    end

    // ---------------------------------------------------------------
    // Output assignments
    // ---------------------------------------------------------------
    assign s_axi_awready = axi_awready;
    assign s_axi_wready  = axi_wready;
    assign s_axi_bresp   = axi_bresp;
    assign s_axi_bvalid  = axi_bvalid;
    assign s_axi_arready = axi_arready;
    assign s_axi_rdata   = axi_rdata;
    assign s_axi_rresp   = axi_rresp;
    assign s_axi_rvalid  = axi_rvalid;

    // ---------------------------------------------------------------
    // Write channels — accept and discard (read-only peripheral)
    // ---------------------------------------------------------------
    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            axi_awready <= 1'b0;
            axi_wready  <= 1'b0;
            axi_bvalid  <= 1'b0;
            axi_bresp   <= 2'b00;
        end else begin
            // Accept write address
            if (~axi_awready && s_axi_awvalid && s_axi_wvalid) begin
                axi_awready <= 1'b1;
            end else begin
                axi_awready <= 1'b0;
            end

            // Accept write data
            if (~axi_wready && s_axi_awvalid && s_axi_wvalid) begin
                axi_wready <= 1'b1;
            end else begin
                axi_wready <= 1'b0;
            end

            // Write response
            if (axi_awready && s_axi_awvalid && axi_wready && s_axi_wvalid && ~axi_bvalid) begin
                axi_bvalid <= 1'b1;
                axi_bresp  <= 2'b00; // OKAY (we just silently discard)
            end else if (s_axi_bready && axi_bvalid) begin
                axi_bvalid <= 1'b0;
            end
        end
    end

    // ---------------------------------------------------------------
    // Read channel — return switch state
    // ---------------------------------------------------------------
    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            axi_arready <= 1'b0;
            axi_rvalid  <= 1'b0;
            axi_rresp   <= 2'b00;
            axi_rdata   <= {C_S_AXI_DATA_WIDTH{1'b0}};
        end else begin
            // Accept read address
            if (~axi_arready && s_axi_arvalid) begin
                axi_arready <= 1'b1;
            end else begin
                axi_arready <= 1'b0;
            end

            // Provide read data
            if (axi_arready && s_axi_arvalid && ~axi_rvalid) begin
                axi_rvalid <= 1'b1;
                axi_rresp  <= 2'b00; // OKAY
                // All addresses return the switch state (only 1 register)
                axi_rdata  <= {{(C_S_AXI_DATA_WIDTH-2){1'b0}}, sw_sync2};
            end else if (axi_rvalid && s_axi_rready) begin
                axi_rvalid <= 1'b0;
            end
        end
    end

endmodule
