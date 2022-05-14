`timescale 10 ps/10 ps

// The `timescale directive specifies that
// the simulation time unit is 100 ps and
// the simulator timestep is 10 ps

module rle_testbench;
	// signal declaration
	reg clkt, rstt;
	reg recv_readyt, send_readyt;
	reg [7:0] in_datat;
	reg end_of_streamt;
	wire [23:0] out_datat;
	wire rd_reqt, wr_reqt;
	reg [7:0] test1, test2, test3;


	// instantiate the circuit under test
	rle_enc uut
		(.clk(clkt), .rst(rstt), .recv_ready(recv_readyt), .send_ready(send_readyt), .in_data(in_datat), .end_of_stream(end_of_streamt), .out_data(out_datat), .rd_req(rd_reqt), .wr_req(wr_reqt));


	// clock running forever
	always
	begin
	// Clock signal assignment
		clkt = 0;
		forever #1 clkt = ~clkt;
	end

	// reset for a few cycles
	initial
	begin
	// Reset signal assignment
		rstt = 0;
		#1 rstt = 1;
  		#4 rstt = 0;

	end

	// test vector generator
	initial
	begin

/*	This set works for the time period
		test1 = 8'b11001100;
		test2 = 8'b00000111;
		test3 = 8'b11110000;
*/
		test1 = 8'b11001100;
		test2 = 8'b11100000;
		test3 = 8'b00001111;

		end_of_streamt <= 0;
		recv_readyt <= 1;
		send_readyt <= 1;

	// test vector 1
		in_datat = test1;
		
	// test vector 2
		#50
		in_datat = test2;

	// test vector n
		#50
		in_datat = test3;
		
		#50
		recv_readyt = 0;

		#50
		end_of_streamt = 1;
	end
initial
	$monitor($time," Output = %b, %b  ", wr_reqt, out_datat);
	//$stop;

endmodule
