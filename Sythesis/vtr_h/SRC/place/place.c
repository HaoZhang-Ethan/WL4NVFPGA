/*#include <stdlib.h> */
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "util.h"
#include "vpr_types.h"
#include "globals.h"
#include "place.h"
#include "read_place.h"
#include "draw.h"
#include "place_and_route.h"
#include "net_delay.h"
#include "path_delay.h"
#include "timing_place_lookup.h"
#include "timing_place.h"
#include "place_stats.h"
#include "read_xml_arch_file.h"
#include "ReadOptions.h"
#include "vpr_utils.h"
#include "place_macro.h"
#include<iostream>
#include<string.h>
#include <fstream>
#include <sstream>
#include <vector>
#include<stdlib.h>
#include <time.h> 
#include <malloc.h>

#define wl_with_pro 1 //flag，如果为1，则表示带考虑性能的WL，为0就是CLB不固定的WL，为2是CLB固定的。
/************** Types and defines local to place.c ***************************/

/* Cut off for incremental bounding box updates.                          *
 * 4 is fastest -- I checked.                                             */
/* To turn off incremental bounding box updates, set this to a huge value */
#define SMALL_NET 4

/* This defines the error tolerance for floating points variables used in *
 * cost computation. 0.01 means that there is a 1% error tolerance.       */
#define ERROR_TOL .01

/* This defines the maximum number of swap attempts before invoking the   *
 * once-in-a-while placement legality check as well as floating point     *
 * variables round-offs check.                                            */
#define MAX_MOVES_BEFORE_RECOMPUTE 50000

/* The maximum number of tries when trying to place a carry chain at a    *
 * random location before trying exhaustive placement - find the fist     *
 * legal position and place it during initial placement.                  */
#define MAX_NUM_TRIES_TO_PLACE_MACROS_RANDOMLY 4

/* Flags for the states of the bounding box.                              *
 * Stored as char for memory efficiency.                                  */
#define NOT_UPDATED_YET 'N'
#define UPDATED_ONCE 'U'
#define GOT_FROM_SCRATCH 'S'

/* For comp_cost.  NORMAL means use the method that generates updateable  *
 * bounding boxes for speed.  CHECK means compute all bounding boxes from *
 * scratch using a very simple routine to allow checks of the other       *
 * costs.                                                                 */
enum cost_methods {
	NORMAL, CHECK
};

/* This is for the placement swap routines. A swap attempt could be       *
 * rejected, accepted or aborted (due to the limitations placed on the    *
 * carry chain support at this point).                                    */
enum swap_result {
	REJECTED, ACCEPTED, ABORTED
};

#define MAX_INV_TIMING_COST 1.e9
/* Stops inverse timing cost from going to infinity with very lax timing constraints, 
which avoids multiplying by a gigantic inverse_prev_timing_cost when auto-normalizing. 
The exact value of this cost has relatively little impact, but should not be
large enough to be on the order of timing costs for normal constraints. */

/********************** Data Sturcture Definition ***************************/
/* Stores the information of the move for a block that is       *
 * moved during placement                                       *
 * block_num: the index of the moved block                      *
 * xold: the x_coord that the block is moved from               *
 * xnew: the x_coord that the block is moved to                 *
 * yold: the y_coord that the block is moved from               *
 * xnew: the x_coord that the block is moved to                 *
 */
typedef struct s_pl_moved_block {
	int block_num;
	int xold;
	int xnew;
	int yold;
	int ynew;
	int zold;
	int znew;
	int swapped_to_empty;
}t_pl_moved_block;

/* Stores the list of blocks to be moved in a swap during       *
 * placement.                                                   *
 * num_moved_blocks: total number of blocks moved when          *
 *                   swapping two blocks.                       *
 * moved blocks: a list of moved blocks data structure with     *
 *               information on the move.                       *
 *               [0...num_moved_blocks-1]                       *
 */
typedef struct s_pl_blocks_to_be_moved {
	int num_moved_blocks;
	t_pl_moved_block * moved_blocks;
}t_pl_blocks_to_be_moved;


/********************** Variables local to place.c ***************************/

/* Cost of a net, and a temporary cost of a net used during move assessment. */
static float *net_cost = NULL, *temp_net_cost = NULL; /* [0..num_nets-1] */

/* legal positions for type */
typedef struct s_legal_pos {
	int x;
	int y;
	int z;
}t_legal_pos;

/* 记录每个逻辑block-ram的写的频率*/
typedef struct logic_bram_rate{
	char * name;
	float rate;
	int pos;
	int try_pos;
}logic_bram_rate;

/*记录物理每个block-ram的写的多少*/
typedef struct physical_bram_rate{
	int x;
	int y;
	float rate;
	int pos;
}physical_bram_rate;

typedef struct location_struct{
	char* name;
	int x;
	int y;
	int subblk;
}location_struct;


static logic_bram_rate *lbr = NULL;
static physical_bram_rate *pbr = NULL;
static location_struct *loca_st = NULL;


static t_legal_pos **legal_pos = NULL; /* [0..num_types-1][0..type_tsize - 1] */
static int *num_legal_pos = NULL; /* [0..num_legal_pos-1] */

/* [0...num_nets-1]                                                              *
 * A flag array to indicate whether the specific bounding box has been updated   *
 * in this particular swap or not. If it has been updated before, the code       *
 * must use the updated data, instead of the out-of-date data passed into the    *
 * subroutine, particularly used in try_swap(). The value NOT_UPDATED_YET        *
 * indicates that the net has not been updated before, UPDATED_ONCE indicated    *
 * that the net has been updated once, if it is going to be updated again, the   *
 * values from the previous update must be used. GOT_FROM_SCRATCH is only        *
 * applicable for nets larger than SMALL_NETS and it indicates that the          *
 * particular bounding box cannot be updated incrementally before, hence the     *
 * bounding box is got from scratch, so the bounding box would definitely be     *
 * right, DO NOT update again.                                                   *
 * [0...num_nets-1]                                                              */
static char * bb_updated_before = NULL;

/* [0..num_nets-1][1..num_pins-1]. What is the value of the timing   */
/* driven portion of the cost function. These arrays will be set to  */
/* (criticality * delay) for each point to point connection. */
static float **point_to_point_timing_cost = NULL;
static float **temp_point_to_point_timing_cost = NULL;

/* [0..num_nets-1][1..num_pins-1]. What is the value of the delay */
/* for each connection in the circuit */
static float **point_to_point_delay_cost = NULL;
static float **temp_point_to_point_delay_cost = NULL;

/* [0..num_blocks-1][0..pins_per_clb-1]. Indicates which pin on the net */
/* this block corresponds to, this is only required during timing-driven */
/* placement. It is used to allow us to update individual connections on */
/* each net */
static int **net_pin_index = NULL;

/* [0..num_nets-1].  Store the bounding box coordinates and the number of    *
 * blocks on each of a net's bounding box (to allow efficient updates),      *
 * respectively.                                                             */

static struct s_bb *bb_coords = NULL, *bb_num_on_edges = NULL;

/* Store the information on the blocks to be moved in a swap during     *
 * placement, in the form of array of structs instead of struct with    *
 * arrays for cache effifiency                                          *
 */
static t_pl_blocks_to_be_moved blocks_affected;

/* The arrays below are used to precompute the inverse of the average   *
 * number of tracks per channel between [subhigh] and [sublow].  Access *
 * them as chan?_place_cost_fac[subhigh][sublow].  They are used to     *
 * speed up the computation of the cost function that takes the length  *
 * of the net bounding box in each dimension, divided by the average    *
 * number of tracks in that direction; for other cost functions they    *
 * will never be used.                                                  *
 *                [0...ny]                [0...nx]                      */
static float **chanx_place_cost_fac, **chany_place_cost_fac;

/* The following arrays are used by the try_swap function for speed.   */
/* [0...num_nets-1] */
static struct s_bb *ts_bb_coord_new = NULL;
static struct s_bb *ts_bb_edge_new = NULL;
static int *ts_nets_to_update = NULL;

/* The pl_macros array stores all the carry chains placement macros.   *
 * [0...num_pl_macros-1]                                                  */
static t_pl_macro * pl_macros = NULL;
static int num_pl_macros;

/* These file-scoped variables keep track of the number of swaps       *
 * rejected, accepted or aborted. The total number of swap attempts    *
 * is the sum of the three number.                                     */
static int num_swap_rejected = 0;
static int num_swap_accepted = 0;
static int num_swap_aborted = 0;
static int num_ts_called = 0;

/* Expected crossing counts for nets with different #'s of pins.  From *
 * ICCAD 94 pp. 690 - 695 (with linear interpolation applied by me).   *
 * Multiplied to bounding box of a net to better estimate wire length  *
 * for higher fanout nets. Each entry is the correction factor for the *
 * fanout index-1                                                      */
static const float cross_count[50] = { /* [0..49] */1.0, 1.0, 1.0, 1.0828, 1.1536, 1.2206, 1.2823, 1.3385, 1.3991, 1.4493, 1.4974,
		1.5455, 1.5937, 1.6418, 1.6899, 1.7304, 1.7709, 1.8114, 1.8519, 1.8924,
		1.9288, 1.9652, 2.0015, 2.0379, 2.0743, 2.1061, 2.1379, 2.1698, 2.2016,
		2.2334, 2.2646, 2.2958, 2.3271, 2.3583, 2.3895, 2.4187, 2.4479, 2.4772,
		2.5064, 2.5356, 2.5610, 2.5864, 2.6117, 2.6371, 2.6625, 2.6887, 2.7148,
		2.7410, 2.7671, 2.7933 };

/********************* Static subroutines local to place.c *******************/
#ifdef VERBOSE
	static void print_clb_placement(const char *fname);
#endif

static void alloc_and_load_placement_structs(
		float place_cost_exp, float ***old_region_occ_x,
		float ***old_region_occ_y, struct s_placer_opts placer_opts,
		t_direct_inf *directs, int num_directs);

static void alloc_and_load_try_swap_structs();

static void free_placement_structs(
		float **old_region_occ_x, float **old_region_occ_y,
		struct s_placer_opts placer_opts);

static void alloc_and_load_for_fast_cost_update(float place_cost_exp);

static void free_fast_cost_update(void);

static void alloc_legal_placements();
static void load_legal_placements();

static void free_legal_placements();

static int check_macro_can_be_placed(int imacro, int itype, int x, int y, int z);

static int try_place_macro(int itype, int ichoice, int imacro, int * free_locations);

static void initial_placement_pl_macros(int macros_max_num_tries, int * free_locations);

static void initial_placement_blocks(int * free_locations, enum e_pad_loc_type pad_loc_type);

static void initial_placement(enum e_pad_loc_type pad_loc_type,
		char *pad_loc_file);

static float comp_bb_cost(enum cost_methods method);

static int setup_blocks_affected(int b_from, int x_to, int y_to, int z_to);

static int find_affected_blocks(int b_from, int x_to, int y_to, int z_to);

static enum swap_result try_swap(float t, float *cost, float *bb_cost, float *timing_cost,
		float rlim, float **old_region_occ_x,
		float **old_region_occ_y,
		enum e_place_algorithm place_algorithm, float timing_tradeoff,
		float inverse_prev_bb_cost, float inverse_prev_timing_cost,
		float *delay_cost);

static void check_place(float bb_cost, float timing_cost,
		enum e_place_algorithm place_algorithm,
		float delay_cost);

static float starting_t(float *cost_ptr, float *bb_cost_ptr,
		float *timing_cost_ptr, float **old_region_occ_x,
		float **old_region_occ_y,
		struct s_annealing_sched annealing_sched, int max_moves, float rlim,
		enum e_place_algorithm place_algorithm, float timing_tradeoff,
		float inverse_prev_bb_cost, float inverse_prev_timing_cost,
		float *delay_cost_ptr);

static void update_t(float *t, float std_dev, float rlim, float success_rat,
		struct s_annealing_sched annealing_sched);

static void update_rlim(float *rlim, float success_rat);

static int exit_crit(float t, float cost,
		struct s_annealing_sched annealing_sched);

static int count_connections(void);

static double get_std_dev(int n, double sum_x_squared, double av_x);

static float recompute_bb_cost(void);

static float comp_td_point_to_point_delay(int inet, int ipin);

static void update_td_cost(void);

static void comp_delta_td_cost(float *delta_timing, float *delta_delay);

static void comp_td_costs(float *timing_cost, float *connection_delay_sum);

static enum swap_result assess_swap(float delta_c, float t);

static boolean find_to(int x_from, int y_from, t_type_ptr type, float rlim, int *x_to, int *y_to,char* block_name);

static void get_non_updateable_bb(int inet, struct s_bb *bb_coord_new);

static void update_bb(int inet, struct s_bb *bb_coord_new,
		struct s_bb *bb_edge_new, int xold, int yold, int xnew, int ynew);
		
static int find_affected_nets(int *nets_to_update);

static float get_net_cost(int inet, struct s_bb *bb_ptr);

static void get_bb_from_scratch(int inet, struct s_bb *coords,
		struct s_bb *num_on_edges);

static double get_net_wirelength_estimate(int inet, struct s_bb *bbptr);

static void free_try_swap_arrays(void);

static int location_bool(const char* type, const char* name,int x,int y,int if_init);
static void initial_l_py(void);
static void initial_location(void);
static void SplitString(const std::string& s, std::vector<std::string>& v, const std::string& c);
static float stringToNum(const std::string& str);
static int place_can_continue(const char* name,int x,int y,int ilbr,int ipbr);
int phy_cmp(const void *a,const void *b);
int lgc_cmp(const void *a,const void *b);
static void sort();
static int phy = 0,used = 0, num_all = 0;
static int exist_config_file = 1;
static int exit_location_file = 0;
/*****************************************************************************/
/* RESEARCH TODO: Bounding Box and rlim need to be redone for heterogeneous to prevent a QoR penalty */
void try_place(struct s_placer_opts placer_opts,
		struct s_annealing_sched annealing_sched,
		t_chan_width_dist chan_width_dist, struct s_router_opts router_opts,
		struct s_det_routing_arch det_routing_arch, t_segment_inf * segment_inf,
		t_timing_inf timing_inf, t_direct_inf *directs, int num_directs) {

	/* Does almost all the work of placing a circuit.  Width_fac gives the   *
	 * width of the widest channel.  Place_cost_exp says what exponent the   *
	 * width should be taken to when calculating costs.  This allows a       *
	 * greater bias for anisotropic architectures.                           */

	int tot_iter, inner_iter, success_sum, move_lim, moves_since_cost_recompute, width_fac,
		num_connections, inet, ipin, outer_crit_iter_count, inner_crit_iter_count,
		inner_recompute_limit, swap_result;
	float t, success_rat, rlim, cost, timing_cost, bb_cost, new_bb_cost, new_timing_cost,
		delay_cost, new_delay_cost, place_delay_value, inverse_prev_bb_cost, inverse_prev_timing_cost,
		oldt, **old_region_occ_x, **old_region_occ_y, **net_delay = NULL, crit_exponent,
		first_rlim, final_rlim, inverse_delta_rlim, critical_path_delay = UNDEFINED,
		**remember_net_delay_original_ptr; /*used to free net_delay if it is re-assigned */
	double av_cost, av_bb_cost, av_timing_cost, av_delay_cost, sum_of_squares, std_dev;
	int total_swap_attempts;
	float reject_rate;
	float accept_rate;
	float abort_rate;
	char msg[BUFSIZE];
	t_slack * slacks = NULL;

	/* Allocated here because it goes into timing critical code where each memory allocation is expensive */

	remember_net_delay_original_ptr = NULL; /*prevents compiler warning */

	/* init file scope variables */
	num_swap_rejected = 0;
	num_swap_accepted = 0;
	num_swap_aborted = 0;
	num_ts_called = 0;

	if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE
			|| placer_opts.place_algorithm == PATH_TIMING_DRIVEN_PLACE
			|| placer_opts.enable_timing_computations) {
		/*do this before the initial placement to avoid messing up the initial placement */
		slacks = alloc_lookups_and_criticalities(chan_width_dist, router_opts,
				det_routing_arch, segment_inf, timing_inf, &net_delay, directs, num_directs);

		remember_net_delay_original_ptr = net_delay;

		/*#define PRINT_LOWER_BOUND */
#ifdef PRINT_LOWER_BOUND
		/*print the crit_path, assuming delay between blocks that are*
		 *block_dist apart*/

		if (placer_opts.block_dist <= nx)
		place_delay_value =
		delta_clb_to_clb[placer_opts.block_dist][0];
		else if (placer_opts.block_dist <= ny)
		place_delay_value =
		delta_clb_to_clb[0][placer_opts.block_dist];
		else
		place_delay_value = delta_clb_to_clb[nx][ny];

		vpr_printf(TIO_MESSAGE_INFO, "\n");
		vpr_printf(TIO_MESSAGE_INFO, "Lower bound assuming delay of %g\n", place_delay_value);

		load_constant_net_delay(net_delay, place_delay_value);
		load_timing_graph_net_delays(net_delay);
		do_timing_analysis(slacks, FALSE, FALSE, TRUE);

		if (getEchoEnabled()) {
			if(isEchoFileEnabled(E_ECHO_PLACEMENT_CRITICAL_PATH))
				print_critical_path(getEchoFileName(E_ECHO_PLACEMENT_CRITICAL_PATH));
			if(isEchoFileEnabled(E_ECHO_PLACEMENT_LOWER_BOUND_SINK_DELAYS))
				print_sink_delays(getEchoFileName(E_ECHO_PLACEMENT_LOWER_BOUND_SINK_DELAYS));
			if(isEchoFileEnabled(E_ECHO_PLACEMENT_LOGIC_SINK_DELAYS))
				print_sink_delays(getEchoFileName(E_ECHO_PLACEMENT_LOGIC_SINK_DELAYS));
		}

		/*also print sink delays assuming 0 delay between blocks, 
		 * this tells us how much logic delay is on each path */

		load_constant_net_delay(net_delay, 0);
		load_timing_graph_net_delays(net_delay);
		do_timing_analysis(slacks, FALSE, FALSE, TRUE);

#endif

	}
	initial_l_py();
	width_fac = placer_opts.place_chan_width;

	init_chan(width_fac, chan_width_dist);

	alloc_and_load_placement_structs(
			placer_opts.place_cost_exp,
			&old_region_occ_x, &old_region_occ_y, placer_opts,
			directs, num_directs);
	initial_placement(placer_opts.pad_loc_type, placer_opts.pad_loc_file);
	init_draw_coords((float) width_fac);
//-------------------------------------------------------------------------------

//到这里已经完成初始布局

//--------------------------------------------------------------------------------
	/* Storing the number of pins on each type of block makes the swap routine *
	 * slightly more efficient.                                                */

	/* Gets initial cost and loads bounding boxes. */

	if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE
			|| placer_opts.place_algorithm == PATH_TIMING_DRIVEN_PLACE) {
		bb_cost = comp_bb_cost(NORMAL);

		crit_exponent = placer_opts.td_place_exp_first; /*this will be modified when rlim starts to change */

		num_connections = count_connections();
		vpr_printf(TIO_MESSAGE_INFO, "\n");
		vpr_printf(TIO_MESSAGE_INFO, "There are %d point to point connections in this circuit.\n", num_connections);
		vpr_printf(TIO_MESSAGE_INFO, "\n");

		if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE) {
			for (inet = 0; inet < num_nets; inet++)
				for (ipin = 1; ipin <= clb_net[inet].num_sinks; ipin++)
					timing_place_crit[inet][ipin] = 0; /*dummy crit values */

			comp_td_costs(&timing_cost, &delay_cost); /*first pass gets delay_cost, which is used 
			 * in criticality computations in the next call
			 * to comp_td_costs. */
			place_delay_value = delay_cost / num_connections; /*used for computing criticalities */
			load_constant_net_delay(net_delay, place_delay_value, clb_net,
					num_nets);

		} else
			place_delay_value = 0;

		if (placer_opts.place_algorithm == PATH_TIMING_DRIVEN_PLACE) {
			net_delay = point_to_point_delay_cost; /*this keeps net_delay up to date with      *
			 * *the same values that the placer is using  *
			 * *point_to_point_delay_cost is computed each*
			 * *time that comp_td_costs is called, and is *
			 * *also updated after any swap is accepted   */
		}

		load_timing_graph_net_delays(net_delay);
		do_timing_analysis(slacks, FALSE, FALSE, FALSE);
		load_criticalities(slacks, crit_exponent);
		if (getEchoEnabled()) {
			if(isEchoFileEnabled(E_ECHO_INITIAL_PLACEMENT_TIMING_GRAPH))
				print_timing_graph(getEchoFileName(E_ECHO_INITIAL_PLACEMENT_TIMING_GRAPH));
			if(isEchoFileEnabled(E_ECHO_INITIAL_PLACEMENT_SLACK))
				print_slack(slacks->slack, FALSE, getEchoFileName(E_ECHO_INITIAL_PLACEMENT_SLACK));
			if(isEchoFileEnabled(E_ECHO_INITIAL_PLACEMENT_CRITICALITY))
				print_criticality(slacks, FALSE, getEchoFileName(E_ECHO_INITIAL_PLACEMENT_CRITICALITY));
		}
		outer_crit_iter_count = 1;

		/*now we can properly compute costs  */
		comp_td_costs(&timing_cost, &delay_cost); /*also vpr_printf proper values into point_to_point_delay_cost */

		inverse_prev_timing_cost = 1 / timing_cost;
		inverse_prev_bb_cost = 1 / bb_cost;
		cost = 1; /*our new cost function uses normalized values of           */
		/*bb_cost and timing_cost, the value of cost will be reset  */
		/*to 1 at each temperature when *_TIMING_DRIVEN_PLACE is true */
	} else { /*BOUNDING_BOX_PLACE */
		cost = bb_cost = comp_bb_cost(NORMAL);
		timing_cost = 0;
		delay_cost = 0;
		place_delay_value = 0;
		outer_crit_iter_count = 0;
		num_connections = 0;
		crit_exponent = 0;

		inverse_prev_timing_cost = 0; /*inverses not used */
		inverse_prev_bb_cost = 0;
	}

	move_lim = (int) (annealing_sched.inner_num * pow(num_blocks, 1.3333));

	if (placer_opts.inner_loop_recompute_divider != 0)
		inner_recompute_limit = (int) (0.5
				+ (float) move_lim
						/ (float) placer_opts.inner_loop_recompute_divider);
	else
		/*don't do an inner recompute */
		inner_recompute_limit = move_lim + 1;

	/* Sometimes I want to run the router with a random placement.  Avoid *
	 * using 0 moves to stop division by 0 and 0 length vector problems,  *
	 * by setting move_lim to 1 (which is still too small to do any       *
	 * significant optimization).                                         */

	if (move_lim <= 0)
		move_lim = 1;

	rlim = (float) std::max(nx + 1, ny + 1);

	first_rlim = rlim; /*used in timing-driven placement for exponent computation */
	final_rlim = 1;
	inverse_delta_rlim = 1 / (first_rlim - final_rlim);

	t = starting_t(&cost, &bb_cost, &timing_cost,
			old_region_occ_x, old_region_occ_y,
			annealing_sched, move_lim, rlim,
			placer_opts.place_algorithm, placer_opts.timing_tradeoff,
			inverse_prev_bb_cost, inverse_prev_timing_cost, &delay_cost);
	tot_iter = 0;
	moves_since_cost_recompute = 0;
	vpr_printf(TIO_MESSAGE_INFO, "Initial placement cost: %g bb_cost: %g td_cost: %g delay_cost: %g\n",
				cost, bb_cost, timing_cost, delay_cost);
	vpr_printf(TIO_MESSAGE_INFO, "\n");

#ifndef SPEC
	vpr_printf(TIO_MESSAGE_INFO, "%9s %9s %11s %11s %11s %11s %8s %8s %7s %7s %7s %9s %7s\n",
			"---------", "---------", "-----------", "-----------", "-----------", "-----------", 
			"--------", "--------", "-------", "-------", "-------", "---------", "-------");
	vpr_printf(TIO_MESSAGE_INFO, "%9s %9s %11s %11s %11s %11s %8s %8s %7s %7s %7s %9s %7s\n",
			"T", "Cost", "Av BB Cost", "Av TD Cost", "Av Tot Del",
			"P to P Del", "d_max", "Ac Rate", "Std Dev", "R limit", "Exp",
			"Tot Moves", "Alpha");
	vpr_printf(TIO_MESSAGE_INFO, "%9s %9s %11s %11s %11s %11s %8s %8s %7s %7s %7s %9s %7s\n",
			"---------", "---------", "-----------", "-----------", "-----------", "-----------", 
			"--------", "--------", "-------", "-------", "-------", "---------", "-------");
#endif

	sprintf(msg, "Initial Placement.  Cost: %g  BB Cost: %g  TD Cost %g  Delay Cost: %g \t Channel Factor: %d", 
		cost, bb_cost, timing_cost, delay_cost, width_fac);
	update_screen(MAJOR, msg, PLACEMENT, FALSE);

	while (exit_crit(t, cost, annealing_sched) == 0) {

		if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE
				|| placer_opts.place_algorithm == PATH_TIMING_DRIVEN_PLACE) {
			cost = 1;
		}

		av_cost = 0.;
		av_bb_cost = 0.;
		av_delay_cost = 0.;
		av_timing_cost = 0.;
		sum_of_squares = 0.;
		success_sum = 0;

		if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE
				|| placer_opts.place_algorithm == PATH_TIMING_DRIVEN_PLACE) {

			if (outer_crit_iter_count >= placer_opts.recompute_crit_iter
					|| placer_opts.inner_loop_recompute_divider != 0) {
#ifdef VERBOSE
				vpr_printf(TIO_MESSAGE_INFO, "Outer loop recompute criticalities\n");
#endif
				place_delay_value = delay_cost / num_connections;

				if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE)
					load_constant_net_delay(net_delay, place_delay_value,
							clb_net, num_nets);
				/*note, for path_based, the net delay is not updated since it is current,
				 *because it accesses point_to_point_delay array */

				load_timing_graph_net_delays(net_delay);
				do_timing_analysis(slacks, FALSE, FALSE, FALSE);
				load_criticalities(slacks, crit_exponent);
				/*recompute costs from scratch, based on new criticalities */
				comp_td_costs(&timing_cost, &delay_cost);
				outer_crit_iter_count = 0;
			}
			outer_crit_iter_count++;

			/*at each temperature change we update these values to be used     */
			/*for normalizing the tradeoff between timing and wirelength (bb)  */
			inverse_prev_bb_cost = 1 / bb_cost;
			/*Prevent inverse timing cost from going to infinity */
			inverse_prev_timing_cost = std::min(1 / timing_cost, (float)MAX_INV_TIMING_COST);
		}

		inner_crit_iter_count = 1;

		for (inner_iter = 0; inner_iter < move_lim; inner_iter++) {
			swap_result = try_swap(t, &cost, &bb_cost, &timing_cost, rlim,
					old_region_occ_x,
					old_region_occ_y, 
					placer_opts.place_algorithm, placer_opts.timing_tradeoff,
					inverse_prev_bb_cost, inverse_prev_timing_cost, &delay_cost);
			if (swap_result == ACCEPTED) {

				/* Move was accepted.  Update statistics that are useful for the annealing schedule. */
				success_sum++;
				av_cost += cost;
				av_bb_cost += bb_cost;
				av_timing_cost += timing_cost;
				av_delay_cost += delay_cost;
				sum_of_squares += cost * cost;
				num_swap_accepted++;
			} else if (swap_result == ABORTED) {
				num_swap_aborted++;
			} else { // swap_result == REJECTED
				num_swap_rejected++;
			}


			if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE
					|| placer_opts.place_algorithm
							== PATH_TIMING_DRIVEN_PLACE) {

				/* Do we want to re-timing analyze the circuit to get updated slack and criticality values? 
				 * We do this only once in a while, since it is expensive.
				 */
				if (inner_crit_iter_count >= inner_recompute_limit
						&& inner_iter != move_lim - 1) { /*on last iteration don't recompute */

					inner_crit_iter_count = 0;
#ifdef VERBOSE
					vpr_printf(TIO_MESSAGE_TRACE, "Inner loop recompute criticalities\n");
#endif
					if (placer_opts.place_algorithm
							== NET_TIMING_DRIVEN_PLACE) {
					    /* Use a constant delay per connection as the delay estimate, rather than
						 * estimating based on the current placement.  Not a great idea, but not the 
						 * default.
						 */
						place_delay_value = delay_cost / num_connections;
						load_constant_net_delay(net_delay, place_delay_value,
								clb_net, num_nets);
					}

					/* Using the delays in net_delay, do a timing analysis to update slacks and
					 * criticalities; then update the timing cost since it will change.
					 */
					load_timing_graph_net_delays(net_delay);
					do_timing_analysis(slacks, FALSE, FALSE, FALSE);
					load_criticalities(slacks, crit_exponent);
					comp_td_costs(&timing_cost, &delay_cost);
				}
				inner_crit_iter_count++;
			}
#ifdef VERBOSE
			vpr_printf(TIO_MESSAGE_TRACE, "t = %g  cost = %g   bb_cost = %g timing_cost = %g move = %d dmax = %g\n",
					t, cost, bb_cost, timing_cost, inner_iter, delay_cost);
			if (fabs(bb_cost - comp_bb_cost(CHECK)) > bb_cost * ERROR_TOL)
				exit(1);
#endif
		}

//		std::cout<<"rrrrrrrrrrr"<<std::endl;
//update_screen(MAJOR, msg, PLACEMENT, FALSE);


		/* Lines below prevent too much round-off error from accumulating *
		 * in the cost over many iterations.  This round-off can lead to  *
		 * error checks failing because the cost is different from what   *
		 * you get when you recompute from scratch.                       */

		moves_since_cost_recompute += move_lim;
		if (moves_since_cost_recompute > MAX_MOVES_BEFORE_RECOMPUTE) {
			new_bb_cost = recompute_bb_cost();
			if (fabs(new_bb_cost - bb_cost) > bb_cost * ERROR_TOL) {
				vpr_printf(TIO_MESSAGE_ERROR, "in try_place: new_bb_cost = %g, old bb_cost = %g\n", 
						new_bb_cost, bb_cost);
				exit(1);
			}
			bb_cost = new_bb_cost;

			if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE
					|| placer_opts.place_algorithm
							== PATH_TIMING_DRIVEN_PLACE) {
				comp_td_costs(&new_timing_cost, &new_delay_cost);
				if (wl_with_pro != 2 && exit_location_file == 1 && fabs(new_timing_cost - timing_cost) > timing_cost * ERROR_TOL) {
					vpr_printf(TIO_MESSAGE_ERROR, "in try_place: new_timing_cost = %g, old timing_cost = %g\n",
							new_timing_cost, timing_cost);
					exit(1);
				}
				if (wl_with_pro != 2 && exit_location_file == 1 && fabs(new_delay_cost - delay_cost) > delay_cost * ERROR_TOL) {
					vpr_printf(TIO_MESSAGE_ERROR, "in try_place: new_delay_cost = %g, old delay_cost = %g\n",
							new_delay_cost, delay_cost);
					exit(1);
				}
				timing_cost = new_timing_cost;
			}

			if (placer_opts.place_algorithm == BOUNDING_BOX_PLACE) {
				cost = new_bb_cost;
			}
			moves_since_cost_recompute = 0;
		}

		tot_iter += move_lim;
		success_rat = ((float) success_sum) / move_lim;
		if (success_sum == 0) {
			av_cost = cost;
			av_bb_cost = bb_cost;
			av_timing_cost = timing_cost;
			av_delay_cost = delay_cost;
		} else {
			av_cost /= success_sum;
			av_bb_cost /= success_sum;
			av_timing_cost /= success_sum;
			av_delay_cost /= success_sum;
		}
		std_dev = get_std_dev(success_sum, sum_of_squares, av_cost);

		oldt = t; /* for finding and printing alpha. */
		update_t(&t, std_dev, rlim, success_rat, annealing_sched);

#ifndef SPEC
		critical_path_delay = get_critical_path_delay();
		vpr_printf(TIO_MESSAGE_INFO, "%9.5f %9.5g %11.6g %11.6g %11.6g %11.6g %8.4f %8.4f %7.4f %7.4f %7.4f %9d %7.4f\n",
				oldt, av_cost, av_bb_cost, av_timing_cost, av_delay_cost, place_delay_value, 
				critical_path_delay, success_rat, std_dev, rlim, crit_exponent, tot_iter, t / oldt);
#endif

		sprintf(msg, "Cost: %g  BB Cost %g  TD Cost %g  Temperature: %g",
				cost, bb_cost, timing_cost, t);
		update_screen(MINOR, msg, PLACEMENT, FALSE);
		update_rlim(&rlim, success_rat);

		if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE
				|| placer_opts.place_algorithm == PATH_TIMING_DRIVEN_PLACE) {
			crit_exponent = (1 - (rlim - final_rlim) * inverse_delta_rlim)
					* (placer_opts.td_place_exp_last
							- placer_opts.td_place_exp_first)
					+ placer_opts.td_place_exp_first;
		}
#ifdef VERBOSE
		if (getEchoEnabled()) {
			print_clb_placement("first_iteration_clb_placement.echo");
		}
#endif
	}

	t = 0; /* freeze out */
	av_cost = 0.;
	av_bb_cost = 0.;
	av_timing_cost = 0.;
	sum_of_squares = 0.;
	av_delay_cost = 0.;
	success_sum = 0;

	if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE
			|| placer_opts.place_algorithm == PATH_TIMING_DRIVEN_PLACE) {
		/*at each temperature change we update these values to be used     */
		/*for normalizing the tradeoff between timing and wirelength (bb)  */
		if (outer_crit_iter_count >= placer_opts.recompute_crit_iter
				|| placer_opts.inner_loop_recompute_divider != 0) {

#ifdef VERBOSE
			vpr_printf(TIO_MESSAGE_INFO, "Outer loop recompute criticalities\n");
#endif
			place_delay_value = delay_cost / num_connections;

			if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE)
				load_constant_net_delay(net_delay, place_delay_value, clb_net,
						num_nets);

			load_timing_graph_net_delays(net_delay);
			do_timing_analysis(slacks, FALSE, FALSE, FALSE);
			load_criticalities(slacks, crit_exponent);
			/*recompute criticaliies */
			comp_td_costs(&timing_cost, &delay_cost);
			outer_crit_iter_count = 0;
		}
		outer_crit_iter_count++;

		inverse_prev_bb_cost = 1 / (bb_cost);
		/*Prevent inverse timing cost from going to infinity */		
		inverse_prev_timing_cost = std::min(1 / timing_cost, (float)MAX_INV_TIMING_COST);
	}

	inner_crit_iter_count = 1;

	for (inner_iter = 0; inner_iter < move_lim; inner_iter++) {
		swap_result = try_swap(t, &cost, &bb_cost, &timing_cost, rlim,
				old_region_occ_x, old_region_occ_y,
				placer_opts.place_algorithm, placer_opts.timing_tradeoff,
				inverse_prev_bb_cost, inverse_prev_timing_cost, &delay_cost);
		
		if (swap_result == ACCEPTED) {
			success_sum++;
			av_cost += cost;
			av_bb_cost += bb_cost;
			av_delay_cost += delay_cost;
			av_timing_cost += timing_cost;
			sum_of_squares += cost * cost;

			if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE
					|| placer_opts.place_algorithm
							== PATH_TIMING_DRIVEN_PLACE) {

				if (inner_crit_iter_count >= inner_recompute_limit
						&& inner_iter != move_lim - 1) {

					inner_crit_iter_count = 0;
#ifdef VERBOSE
					vpr_printf(TIO_MESSAGE_TRACE, "Inner loop recompute criticalities\n");
#endif
					if (placer_opts.place_algorithm
							== NET_TIMING_DRIVEN_PLACE) {
						place_delay_value = delay_cost / num_connections;
						load_constant_net_delay(net_delay, place_delay_value,
								clb_net, num_nets);
					}

					load_timing_graph_net_delays(net_delay);
					do_timing_analysis(slacks, FALSE, FALSE, FALSE);
					load_criticalities(slacks, crit_exponent);
					comp_td_costs(&timing_cost, &delay_cost);
				}
				inner_crit_iter_count++;
			}
			num_swap_accepted++;
		} else if (swap_result == ABORTED) {
			num_swap_aborted++;
		} else {
			num_swap_rejected++;
		}

#ifdef VERBOSE
		vpr_printf(TIO_MESSAGE_INFO, "t = %g, cost = %g, move = %d\n", t, cost, tot_iter);
#endif
	}
	tot_iter += move_lim;
	success_rat = ((float) success_sum) / move_lim;
	if (success_sum == 0) {
		av_cost = cost;
		av_bb_cost = bb_cost;
		av_delay_cost = delay_cost;
		av_timing_cost = timing_cost;
	} else {
		av_cost /= success_sum;
		av_bb_cost /= success_sum;
		av_delay_cost /= success_sum;
		av_timing_cost /= success_sum;
	}

	std_dev = get_std_dev(success_sum, sum_of_squares, av_cost);

#ifndef SPEC
	vpr_printf(TIO_MESSAGE_INFO, "%9.5f %9.5g %11.6g %11.6g %11.6g %11.6g %8s %8.4f %7.4f %7.4f %7.4f %9d\n",
			t, av_cost, av_bb_cost, av_timing_cost, av_delay_cost, place_delay_value, 
			" ", success_rat, std_dev, rlim, crit_exponent, tot_iter);
#endif

	// TODO:  
	// 1. print a message about number of aborted moves.
	// 2. add some subroutine hierarchy!  Too big!
	// 3. put statistics counters (av_cost, success_sum, etc.) in a struct so a 
	// pointer to it can be passed around. 

#ifdef VERBOSE
	if (getEchoEnabled() && isEchoFileEnabled(E_ECHO_END_CLB_PLACEMENT)) {
		print_clb_placement(getEchoFileName(E_ECHO_END_CLB_PLACEMENT));
	}
#endif

	check_place(bb_cost, timing_cost,
			placer_opts.place_algorithm, delay_cost);

	if (placer_opts.enable_timing_computations
			&& placer_opts.place_algorithm == BOUNDING_BOX_PLACE) {
		/*need this done since the timing data has not been kept up to date*
		 *in bounding_box mode */
		for (inet = 0; inet < num_nets; inet++)
			for (ipin = 1; ipin <= clb_net[inet].num_sinks; ipin++)
				timing_place_crit[inet][ipin] = 0; /*dummy crit values */
		comp_td_costs(&timing_cost, &delay_cost); /*computes point_to_point_delay_cost */
	}

	if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE
			|| placer_opts.place_algorithm == PATH_TIMING_DRIVEN_PLACE
			|| placer_opts.enable_timing_computations) {
		net_delay = point_to_point_delay_cost; /*this makes net_delay up to date with    *
		 *the same values that the placer is using*/
		load_timing_graph_net_delays(net_delay);

		do_timing_analysis(slacks, FALSE, FALSE, FALSE);

		if (getEchoEnabled()) {
			if(isEchoFileEnabled(E_ECHO_PLACEMENT_SINK_DELAYS))
				print_sink_delays(getEchoFileName(E_ECHO_PLACEMENT_SINK_DELAYS));
			if(isEchoFileEnabled(E_ECHO_FINAL_PLACEMENT_SLACK))
				print_slack(slacks->slack, FALSE, getEchoFileName(E_ECHO_FINAL_PLACEMENT_SLACK));
			if(isEchoFileEnabled(E_ECHO_FINAL_PLACEMENT_CRITICALITY))
				print_criticality(slacks, FALSE, getEchoFileName(E_ECHO_FINAL_PLACEMENT_CRITICALITY));
			if(isEchoFileEnabled(E_ECHO_FINAL_PLACEMENT_TIMING_GRAPH))
				print_timing_graph(getEchoFileName(E_ECHO_FINAL_PLACEMENT_TIMING_GRAPH));
			if(isEchoFileEnabled(E_ECHO_PLACEMENT_CRIT_PATH))
				print_critical_path(getEchoFileName(E_ECHO_PLACEMENT_CRIT_PATH));
		}

		/* Print critical path delay. */
		critical_path_delay = get_critical_path_delay();
		vpr_printf(TIO_MESSAGE_INFO, "\n");
		vpr_printf(TIO_MESSAGE_INFO, "Placement estimated critical path delay: %g ns\n", critical_path_delay);
	}

	sprintf(msg, "Placement. Cost: %g  bb_cost: %g td_cost: %g Channel Factor: %d",
			cost, bb_cost, timing_cost, width_fac);
	vpr_printf(TIO_MESSAGE_INFO, "Placement cost: %g, bb_cost: %g, td_cost: %g, delay_cost: %g\n", 
			cost, bb_cost, timing_cost, delay_cost);
	update_screen(MAJOR, msg, PLACEMENT, FALSE);
	 
	// Print out swap statistics
	total_swap_attempts = num_swap_rejected + num_swap_accepted + num_swap_aborted;
	reject_rate = num_swap_rejected / total_swap_attempts;
	accept_rate = num_swap_accepted / total_swap_attempts;
	abort_rate = num_swap_aborted / total_swap_attempts;
	vpr_printf(TIO_MESSAGE_INFO, "Placement total # of swap attempts: %d\n", total_swap_attempts);
	vpr_printf(TIO_MESSAGE_INFO, "\tSwap reject rate: %g\n", reject_rate);
	vpr_printf(TIO_MESSAGE_INFO, "\tSwap accept rate: %g\n", accept_rate);
	vpr_printf(TIO_MESSAGE_INFO, "\tSwap abort rate: %g\n",	abort_rate);
	

#ifdef SPEC
	vpr_printf(TIO_MESSAGE_INFO, "Total moves attempted: %d.0\n", tot_iter);
#endif

	free_placement_structs(
				old_region_occ_x, old_region_occ_y,
				placer_opts);
	if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE
			|| placer_opts.place_algorithm == PATH_TIMING_DRIVEN_PLACE
			|| placer_opts.enable_timing_computations) {

		net_delay = remember_net_delay_original_ptr;
		free_lookups_and_criticalities(&net_delay, slacks);
	}

	free_try_swap_arrays();
}

static int count_connections() {
	/*only count non-global connections */

	int count, inet;

	count = 0;

	for (inet = 0; inet < num_nets; inet++) {

		if (clb_net[inet].is_global)
			continue;

		count += clb_net[inet].num_sinks;
	}
	return (count);
}

static double get_std_dev(int n, double sum_x_squared, double av_x) {

	/* Returns the standard deviation of data set x.  There are n sample points, *
	 * sum_x_squared is the summation over n of x^2 and av_x is the average x.   *
	 * All operations are done in double precision, since round off error can be *
	 * a problem in the initial temp. std_dev calculation for big circuits.      */

	double std_dev;

	if (n <= 1)
		std_dev = 0.;
	else
		std_dev = (sum_x_squared - n * av_x * av_x) / (double) (n - 1);

	if (std_dev > 0.) /* Very small variances sometimes round negative */
		std_dev = sqrt(std_dev);
	else
		std_dev = 0.;

	return (std_dev);
}

static void update_rlim(float *rlim, float success_rat) {

	/* Update the range limited to keep acceptance prob. near 0.44.  Use *
	 * a floating point rlim to allow gradual transitions at low temps.  */

	float upper_lim;

	*rlim = (*rlim) * (1. - 0.44 + success_rat);
	upper_lim = std::max(nx + 1, ny + 1);
	*rlim = std::min(*rlim, upper_lim);
	*rlim = std::max(*rlim, (float)1.);
}

/* Update the temperature according to the annealing schedule selected. */
static void update_t(float *t, float std_dev, float rlim, float success_rat,
		struct s_annealing_sched annealing_sched) {

	/*  float fac; */

	if (annealing_sched.type == USER_SCHED) {
		*t = annealing_sched.alpha_t * (*t);
	}

	/* Old standard deviation based stuff is below.  This bogs down horribly 
	 * for big circuits (alu4 and especially bigkey_mod). */
	/* #define LAMBDA .7  */
	/* ------------------------------------ */
#if 0
	else if (std_dev == 0.)
	{
		*t = 0.;
	}
	else
	{
		fac = exp(-LAMBDA * (*t) / std_dev);
		fac = max(0.5, fac);
		*t = (*t) * fac;
	}
#endif
	/* ------------------------------------- */

	else { /* AUTO_SCHED */
		if (success_rat > 0.96) {
			*t = (*t) * 0.5;
		} else if (success_rat > 0.8) {
			*t = (*t) * 0.9;
		} else if (success_rat > 0.15 || rlim > 1.) {
			*t = (*t) * 0.95;
		} else {
			*t = (*t) * 0.8;
		}
	}
}

static int exit_crit(float t, float cost,
		struct s_annealing_sched annealing_sched) {

	/* Return 1 when the exit criterion is met.                        */

	if (annealing_sched.type == USER_SCHED) {
		if (t < annealing_sched.exit_t) {
			return (1);
		} else {
			return (0);
		}
	}

	/* Automatic annealing schedule */

	if (t < 0.005 * cost / num_nets) {
		return (1);
	} else {
		return (0);
	}
}

static float starting_t(float *cost_ptr, float *bb_cost_ptr,
		float *timing_cost_ptr, float **old_region_occ_x,
		float **old_region_occ_y, 
		struct s_annealing_sched annealing_sched, int max_moves, float rlim,
		enum e_place_algorithm place_algorithm, float timing_tradeoff,
		float inverse_prev_bb_cost, float inverse_prev_timing_cost,
		float *delay_cost_ptr) {

	/* Finds the starting temperature (hot condition).              */

	int i, num_accepted, move_lim, swap_result;
	double std_dev, av, sum_of_squares; /* Double important to avoid round off */

	if (annealing_sched.type == USER_SCHED)
		return (annealing_sched.init_t);

	move_lim = std::min(max_moves, num_blocks);

	num_accepted = 0;
	av = 0.;
	sum_of_squares = 0.;

	/* Try one move per block.  Set t high so essentially all accepted. */

	for (i = 0; i < move_lim; i++) {
		swap_result = try_swap(HUGE_POSITIVE_FLOAT, cost_ptr, bb_cost_ptr, timing_cost_ptr, rlim,
				old_region_occ_x, old_region_occ_y,
				place_algorithm, timing_tradeoff,
				inverse_prev_bb_cost, inverse_prev_timing_cost, delay_cost_ptr);
		
		if (swap_result == ACCEPTED) {
			num_accepted++;
			av += *cost_ptr;
			sum_of_squares += *cost_ptr * (*cost_ptr);
			num_swap_accepted++;
		} else if (swap_result == ABORTED) {
			num_swap_aborted++;
		} else {
			num_swap_rejected++;
		}
	}

	if (num_accepted != 0)
		av /= num_accepted;
	else
		av = 0.;

	std_dev = get_std_dev(num_accepted, sum_of_squares, av);

#ifdef DEBUG
	if (num_accepted != move_lim) {
		vpr_printf(TIO_MESSAGE_WARNING, "Starting t: %d of %d configurations accepted.\n", num_accepted, move_lim);
	}
#endif

#ifdef VERBOSE
	vpr_printf(TIO_MESSAGE_INFO, "std_dev: %g, average cost: %g, starting temp: %g\n", std_dev, av, 20. * std_dev);
#endif

	/* Set the initial temperature to 20 times the standard of deviation */
	/* so that the initial temperature adjusts according to the circuit */
	return (20. * std_dev);
}


static int setup_blocks_affected(int b_from, int x_to, int y_to, int z_to) {

	/* Find all the blocks affected when b_from is swapped with b_to. 
	 * Returns abort_swap.                  */

	int imoved_blk, imacro;
	int x_from, y_from, z_from, b_to;
	int abort_swap = FALSE;

	x_from = block[b_from].x;
	y_from = block[b_from].y;
	z_from = block[b_from].z;

	b_to = grid[x_to][y_to].blocks[z_to];

	// Check whether the to_location is empty
	if (b_to == EMPTY) {

		// Swap the block, dont swap the nets yet
		block[b_from].x = x_to;
		block[b_from].y = y_to;
		block[b_from].z = z_to;

		// Sets up the blocks moved
		imoved_blk = blocks_affected.num_moved_blocks;
		blocks_affected.moved_blocks[imoved_blk].block_num = b_from;
		blocks_affected.moved_blocks[imoved_blk].xold = x_from;
		blocks_affected.moved_blocks[imoved_blk].xnew = x_to;
		blocks_affected.moved_blocks[imoved_blk].yold = y_from;
		blocks_affected.moved_blocks[imoved_blk].ynew = y_to;
		blocks_affected.moved_blocks[imoved_blk].zold = z_from;
		blocks_affected.moved_blocks[imoved_blk].znew = z_to;
		blocks_affected.moved_blocks[imoved_blk].swapped_to_empty = TRUE;
		blocks_affected.num_moved_blocks ++;
				
	} else {

		// Does not allow a swap with a macro yet
		get_imacro_from_iblk(&imacro, b_to, pl_macros, num_pl_macros);
		if (imacro != -1) {
			abort_swap = TRUE;
			return (abort_swap);
		}

		// Swap the block, dont swap the nets yet
		block[b_to].x = x_from;
		block[b_to].y = y_from;
		block[b_to].z = z_from;

		block[b_from].x = x_to;
		block[b_from].y = y_to;
		block[b_from].z = z_to;
		
		// Sets up the blocks moved
		imoved_blk = blocks_affected.num_moved_blocks;
		blocks_affected.moved_blocks[imoved_blk].block_num = b_from;
		blocks_affected.moved_blocks[imoved_blk].xold = x_from;
		blocks_affected.moved_blocks[imoved_blk].xnew = x_to;
		blocks_affected.moved_blocks[imoved_blk].yold = y_from;
		blocks_affected.moved_blocks[imoved_blk].ynew = y_to;
		blocks_affected.moved_blocks[imoved_blk].zold = z_from;
		blocks_affected.moved_blocks[imoved_blk].znew = z_to;
		blocks_affected.moved_blocks[imoved_blk].swapped_to_empty = FALSE;
		blocks_affected.num_moved_blocks ++;
				
		imoved_blk = blocks_affected.num_moved_blocks;
		blocks_affected.moved_blocks[imoved_blk].block_num = b_to;
		blocks_affected.moved_blocks[imoved_blk].xold = x_to;
		blocks_affected.moved_blocks[imoved_blk].xnew = x_from;
		blocks_affected.moved_blocks[imoved_blk].yold = y_to;
		blocks_affected.moved_blocks[imoved_blk].ynew = y_from;
		blocks_affected.moved_blocks[imoved_blk].zold = z_to;
		blocks_affected.moved_blocks[imoved_blk].znew = z_from;
		blocks_affected.moved_blocks[imoved_blk].swapped_to_empty = FALSE;
		blocks_affected.num_moved_blocks ++;

	} // Finish swapping the blocks and setting up blocks_affected
			
	return (abort_swap);

}

static int find_affected_blocks(int b_from, int x_to, int y_to, int z_to) {

	/* Finds and set ups the affected_blocks array. 
	 * Returns abort_swap. */

	int imacro, imember;
	int x_swap_offset, y_swap_offset, z_swap_offset, x_from, y_from, z_from, b_to;
	int curr_b_from, curr_x_from, curr_y_from, curr_z_from, curr_b_to, curr_x_to, curr_y_to, curr_z_to;
	int abort_swap = FALSE;
	
	x_from = block[b_from].x;
	y_from = block[b_from].y;
	z_from = block[b_from].z;

	b_to = grid[x_to][y_to].blocks[z_to];

	get_imacro_from_iblk(&imacro, b_from, pl_macros, num_pl_macros);
	if ( imacro != -1) {
		// b_from is part of a macro, I need to swap the whole macro
		
		// Record down the relative position of the swap
		x_swap_offset = x_to - x_from;
		y_swap_offset = y_to - y_from;
		z_swap_offset = z_to - z_from;
		
		for (imember = 0; imember < pl_macros[imacro].num_blocks && abort_swap == FALSE; imember++) {

			// Gets the new from and to info for every block in the macro
			// cannot use the old from and to info
			curr_b_from = pl_macros[imacro].members[imember].blk_index;
			
			curr_x_from = block[curr_b_from].x;
			curr_y_from = block[curr_b_from].y;
			curr_z_from = block[curr_b_from].z;

			curr_x_to = curr_x_from + x_swap_offset;
			curr_y_to = curr_y_from + y_swap_offset;
			curr_z_to = curr_z_from + z_swap_offset;
			
			// Make sure that the swap_to location is still on the chip
			if (curr_x_to < 1 || curr_x_to > nx || curr_y_to < 1 || curr_y_to > ny || curr_z_to < 0) {
				abort_swap = TRUE;
			} else {
				curr_b_to = grid[curr_x_to][curr_y_to].blocks[curr_z_to];
				abort_swap = setup_blocks_affected(curr_b_from, curr_x_to, curr_y_to, curr_z_to);
			}

		} // Finish going through all the blocks in the macro

	} else { 
		
		// This is not a macro - I could use the from and to info from before
		abort_swap = setup_blocks_affected(b_from, x_to, y_to, z_to);

	} // Finish handling cases for blocks in macro and otherwise

	return (abort_swap);

}

static enum swap_result try_swap(float t, float *cost, float *bb_cost, float *timing_cost,
		float rlim, float **old_region_occ_x,
		float **old_region_occ_y, 
		enum e_place_algorithm place_algorithm, float timing_tradeoff,
		float inverse_prev_bb_cost, float inverse_prev_timing_cost,
		float *delay_cost) {

	/* Picks some block and moves it to another spot.  If this spot is   *
	 * occupied, switch the blocks.  Assess the change in cost function  *
	 * and accept or reject the move.  If rejected, return 0.  If        *
	 * accepted return 1.  Pass back the new value of the cost function. * 
	 * rlim is the range limiter.                                        */

	enum swap_result keep_switch;
	int b_from, x_from, y_from, z_from, x_to, y_to, z_to;
	int num_nets_affected;
	float delta_c, bb_delta_c, timing_delta_c, delay_delta_c;
	int inet, iblk, bnum, iblk_pin, inet_affected;
	int abort_swap = FALSE;	

	num_ts_called ++;

	/* I'm using negative values of temp_net_cost as a flag, so DO NOT   *
	 * use cost functions that can go negative.                          */

	delta_c = 0; /* Change in cost due to this swap. */
	bb_delta_c = 0;
	timing_delta_c = 0;
	delay_delta_c = 0.0;
	
	/* Pick a random block to be swapped with another random block    */
	b_from = my_irand(num_blocks - 1);

	/* If the pins are fixed we never move them from their initial    *
	 * random locations.  The code below could be made more efficient *
	 * by using the fact that pins appear first in the block list,    *
	 * but this shouldn't cause any significant slowdown and won't be *
	 * broken if I ever change the parser so that the pins aren't     *
	 * necessarily at the start of the block list.                    */
	while (block[b_from].isFixed == TRUE) {
		b_from = my_irand(num_blocks - 1);
	}

	x_from = block[b_from].x;
	y_from = block[b_from].y;
	z_from = block[b_from].z;

	if (!find_to(x_from, y_from, block[b_from].type, rlim, &x_to,
			&y_to,block[b_from].name))
		return REJECTED;

	z_to = 0;
	if (grid[x_to][y_to].type->capacity > 1) {
		z_to = my_irand(grid[x_to][y_to].type->capacity - 1);
	}

	/* Make the switch in order to make computing the new bounding *
	 * box simpler.  If the cost increase is too high, switch them *
	 * back.  (block data structures switched, clbs not switched   *
	 * until success of move is determined.)                       *
	 * Also check that whether those are the only 2 blocks         *
	 * to be moved - check for carry chains and other placement    *
	 * macros.                                                     */
	
	/* Check whether the from_block is part of a macro first.      *
	 * If it is, the whole macro has to be moved. Calculate the    *
	 * x, y, z offsets of the swap to maintain relative placements *
	 * of the blocks. Abort the swap if the to_block is part of a  *
	 * macro (not supported yet).                                  */
	
	abort_swap = find_affected_blocks(b_from, x_to, y_to, z_to);

	//int from_x = block[b_from].x;
	//int from_y = block[b_from].y;

	if (abort_swap == FALSE) {

		// Find all the nets affected by this swap
		num_nets_affected = find_affected_nets(ts_nets_to_update);

		/* Go through all the pins in all the blocks moved and update the bounding boxes.  *
		 * Do not update the net cost here since it should only be updated once per net,   *
		 * not once per pin                                                                */
		for (iblk = 0; iblk < blocks_affected.num_moved_blocks; iblk++)
		{
			bnum = blocks_affected.moved_blocks[iblk].block_num;

			/* Go through all the pins in the moved block */
			for (iblk_pin = 0; iblk_pin < block[bnum].type->num_pins; iblk_pin++)
			{
				inet = block[bnum].nets[iblk_pin];
				if (inet == OPEN)
					continue;
				if (clb_net[inet].is_global)
					continue;
			
				if (clb_net[inet].num_sinks < SMALL_NET) {
					if(bb_updated_before[inet] == NOT_UPDATED_YET)
						/* Brute force bounding box recomputation, once only for speed. */
						get_non_updateable_bb(inet, &ts_bb_coord_new[inet]);
				} else {
					update_bb(inet, &ts_bb_coord_new[inet],
							&ts_bb_edge_new[inet], 
							blocks_affected.moved_blocks[iblk].xold, 
							blocks_affected.moved_blocks[iblk].yold + block[bnum].type->pin_height[iblk_pin],
							blocks_affected.moved_blocks[iblk].xnew, 
							blocks_affected.moved_blocks[iblk].ynew + block[bnum].type->pin_height[iblk_pin]);
				}
			}
		}
			
		/* Now update the cost function. The cost is only updated once for every net  *
		 * May have to do major optimizations here later.                             */
		for (inet_affected = 0; inet_affected < num_nets_affected; inet_affected++) {
			inet = ts_nets_to_update[inet_affected];

			temp_net_cost[inet] = get_net_cost(inet, &ts_bb_coord_new[inet]);
			bb_delta_c += temp_net_cost[inet] - net_cost[inet];
		}

		if (place_algorithm == NET_TIMING_DRIVEN_PLACE
				|| place_algorithm == PATH_TIMING_DRIVEN_PLACE) {
			/*in this case we redefine delta_c as a combination of timing and bb.  *
			 *additionally, we normalize all values, therefore delta_c is in       *
			 *relation to 1*/

			comp_delta_td_cost(&timing_delta_c, &delay_delta_c);

			delta_c = (1 - timing_tradeoff) * bb_delta_c * inverse_prev_bb_cost
					+ timing_tradeoff * timing_delta_c * inverse_prev_timing_cost;
		} else {
			delta_c = bb_delta_c;
		}

		/* 1 -> move accepted, 0 -> rejected. */
		keep_switch = assess_swap(delta_c, t);
		
		if (keep_switch == ACCEPTED) {
			*cost = *cost + delta_c;
			*bb_cost = *bb_cost + bb_delta_c;
	
			if (place_algorithm == NET_TIMING_DRIVEN_PLACE
					|| place_algorithm == PATH_TIMING_DRIVEN_PLACE) {
				/*update the point_to_point_timing_cost and point_to_point_delay_cost 
				 * values from the temporary values */
				*timing_cost = *timing_cost + timing_delta_c;
				*delay_cost = *delay_cost + delay_delta_c;

				update_td_cost();
			}

			/* update net cost functions and reset flags. */
			for (inet_affected = 0; inet_affected < num_nets_affected; inet_affected++) {
				inet = ts_nets_to_update[inet_affected];

				bb_coords[inet] = ts_bb_coord_new[inet];
				if (clb_net[inet].num_sinks >= SMALL_NET)
					bb_num_on_edges[inet] = ts_bb_edge_new[inet];
			
				net_cost[inet] = temp_net_cost[inet];

				/* negative temp_net_cost value is acting as a flag. */
				temp_net_cost[inet] = -1;
				bb_updated_before[inet] = NOT_UPDATED_YET;
			}

			/* Update clb data structures since we kept the move. */
			/* Swap physical location */
			/*if(blocks_affected.num_moved_blocks>2){
				std::cout<<blocks_affected.num_moved_blocks<<std::endl;
				exit(1);
			}*/
			//blocks_affected.num_moved_blocks 应该是指的交换的两个物理块。
			for (iblk = 0; iblk < blocks_affected.num_moved_blocks; iblk++) {

				x_to = blocks_affected.moved_blocks[iblk].xnew;
				y_to = blocks_affected.moved_blocks[iblk].ynew;
				z_to = blocks_affected.moved_blocks[iblk].znew;

				x_from = blocks_affected.moved_blocks[iblk].xold;
				y_from = blocks_affected.moved_blocks[iblk].yold;
				z_from = blocks_affected.moved_blocks[iblk].zold;

				b_from = blocks_affected.moved_blocks[iblk].block_num;

				grid[x_to][y_to].blocks[z_to] = b_from;

				if (blocks_affected.moved_blocks[iblk].swapped_to_empty == TRUE) {
					grid[x_to][y_to].usage++;
					grid[x_from][y_from].usage--;
					grid[x_from][y_from].blocks[z_from] = -1;
				}
			
			/*	if ((x_from != x_to ||y_from != y_to ) && strcmp((*(block[b_from].type)).name,"memory") ==0){
					std::cout<<"type:\t"<<(*(block[b_from].type)).name<<"\nname:\t"<<block[b_from].name<<"\nto:\t"<<x_to<<"  "<<y_to<<"\nfrom:\t"<<x_from<<"  "<<y_from<<"\nabort:\t"<<abort_swap<<std::endl<<std::endl;
				}*/

			} // Finish updating clb for all blocks

			


		} else { /* Move was rejected.  */

			/* Reset the net cost function flags first. */
			for (inet_affected = 0; inet_affected < num_nets_affected; inet_affected++) {
				inet = ts_nets_to_update[inet_affected];
				temp_net_cost[inet] = -1;
				bb_updated_before[inet] = NOT_UPDATED_YET;
			}

			/* Restore the block data structures to their state before the move. */
			for (iblk = 0; iblk < blocks_affected.num_moved_blocks; iblk++) {
				b_from = blocks_affected.moved_blocks[iblk].block_num;

				block[b_from].x = blocks_affected.moved_blocks[iblk].xold;
				block[b_from].y = blocks_affected.moved_blocks[iblk].yold;
				block[b_from].z = blocks_affected.moved_blocks[iblk].zold;
			}
		}

		/* Resets the num_moved_blocks, but do not free blocks_moved array. Defensive Coding */
		blocks_affected.num_moved_blocks = 0;

		//check_place(*bb_cost, *timing_cost, place_algorithm, *delay_cost);
	
		return (keep_switch);
	} else {

		/* Restore the block data structures to their state before the move. */
		for (iblk = 0; iblk < blocks_affected.num_moved_blocks; iblk++) {
			b_from = blocks_affected.moved_blocks[iblk].block_num;

			block[b_from].x = blocks_affected.moved_blocks[iblk].xold;
			block[b_from].y = blocks_affected.moved_blocks[iblk].yold;
			block[b_from].z = blocks_affected.moved_blocks[iblk].zold;
		}

		/* Resets the num_moved_blocks, but do not free blocks_moved array. Defensive Coding */
		blocks_affected.num_moved_blocks = 0;
		
		return ABORTED;
	}
}

static int find_affected_nets(int *nets_to_update) {

	/* Puts a list of all the nets that are changed by the swap into          *
	 * nets_to_update.  Returns the number of affected nets.                  */

	int iblk, iblk_pin, inet, bnum, num_affected_nets;

	num_affected_nets = 0;
	/* Go through all the blocks moved */
	for (iblk = 0; iblk < blocks_affected.num_moved_blocks; iblk++)
	{
		bnum = blocks_affected.moved_blocks[iblk].block_num;

		/* Go through all the pins in the moved block */
		for (iblk_pin = 0; iblk_pin < block[bnum].type->num_pins; iblk_pin++)
		{
			/* Updates the pins_to_nets array, set to -1 if   *
			 * that pin is not connected to any net or it is a  *
			 * global pin that does not need to be updated      */
			inet = block[bnum].nets[iblk_pin];
			if (inet == OPEN)
				continue;
			if (clb_net[inet].is_global)
				continue;
			
			if (temp_net_cost[inet] < 0.) { 
				/* Net not marked yet. */
				nets_to_update[num_affected_nets] = inet;
				num_affected_nets++;

				/* Flag to say we've marked this net. */
				temp_net_cost[inet] = 1.;
			}
		}
	}
	return num_affected_nets;
}

static boolean find_to(int x_from, int y_from, t_type_ptr type, float rlim, int *x_to, int *y_to,char* block_name) {

	/* Returns the point to which I want to swap, properly range limited. 
	 * rlim must always be between 1 and nx (inclusive) for this routine  
	 * to work.  Assumes that a column only contains blocks of the same type.
	 */

	int x_rel, y_rel, rlx, rly, min_x, max_x, min_y, max_y;
	int num_tries;
	int active_area;
	boolean is_legal;
	int block_index, ipos;

	assert(type == grid[x_from][y_from].type);

	rlx = (int)std::min((float)nx + 1, rlim); 
	rly = (int)std::min((float)ny + 1, rlim); /* Added rly for aspect_ratio != 1 case. */
	active_area = 4 * rlx * rly;

	min_x = std::max(0, x_from - rlx);
	max_x = std::min(nx + 1, x_from + rlx);
	min_y = std::max(0, y_from - rly);
	max_y = std::min(ny + 1, y_from + rly);

#ifdef DEBUG
	if (rlx < 1 || rlx > nx + 1) {
		vpr_printf(TIO_MESSAGE_ERROR, "in find_to: rlx = %d\n", rlx);
		exit(1);
	}
#endif

	num_tries = 0;
	block_index = type->index;

	do { /* Until legal */
		is_legal = TRUE;

		/* Limit the number of tries when searching for an alternative position */
		if(num_tries >= 2 * std::min(active_area / type->height, num_legal_pos[block_index]) + 10) {
			/* Tried randomly searching for a suitable position */
			return FALSE;
		} else {
			num_tries++;
		}
		if(nx / 4 < rlx || 
			ny / 4 < rly ||
			num_legal_pos[block_index] < active_area) {
			ipos = my_irand(num_legal_pos[block_index] - 1);
			*x_to = legal_pos[block_index][ipos].x;
			*y_to = legal_pos[block_index][ipos].y;
		} else {
			x_rel = my_irand(std::max(0, max_x - min_x));
			*x_to = min_x + x_rel;
			y_rel = my_irand(std::max(0, max_y - min_y));
			*y_to = min_y + y_rel;
			*y_to = (*y_to) - grid[*x_to][*y_to].offset; /* align it */
		}
		//这里可以加入，交换到写多的块非法操作。
		if((x_from == *x_to) && (y_from == *y_to)) {
			is_legal = FALSE;
		} else if(*x_to > max_x || *x_to < min_x || *y_to > max_y || *y_to < min_y) {
			is_legal = FALSE;
		} else if(grid[*x_to][*y_to].type != grid[x_from][y_from].type) {
			is_legal = FALSE;
		}
		//加入禁止向模块布局写多的块：
	/*	else if( ( ( (*x_to)==10 && (*y_to)==7 ) ||( (*x_to)==2 && (*y_to)==7 )||( (*x_to)==10 && (*y_to)==13 ) ||( (*x_to)==18 && (*y_to)==7 ) ||( (*x_to)==2 && (*y_to)==13 ) ||( (*x_to)==10 && (*y_to)==1 )||( (*x_to)==2 && (*y_to)==1 )||( (*x_to)==18 && (*y_to)==13 ) ) && strcmp(type->name,"memory") ==0) {
			is_legal = FALSE;
		}*/
		else if(location_bool(type->name,block_name,(*x_to),(*y_to),0)){
			is_legal = FALSE;
		}

		else if((grid[*x_to][*y_to].blocks[0]>=0)&&location_bool(type->name,block[grid[*x_to][*y_to].blocks[0]].name,x_from,y_from,0)){
			is_legal = FALSE;
		}
		if (strcmp(type->name,"memory") == 0 && grid[x_from][y_from].blocks[0] >=0 ){
			
			if(strcmp(block_name,block[grid[x_from][y_from].blocks[0]].name)!=0){
				std::cout<<block_name<<"\n"<<block[grid[x_from][y_from].blocks[0]].name<<std::endl;
				std::cout<<std::endl;
			}
		}
		assert(*x_to >= 0 && *x_to <= nx + 1);
		assert(*y_to >= 0 && *y_to <= ny + 1);
	} while (is_legal == FALSE);

#ifdef DEBUG
	if (*x_to < 0 || *x_to > nx + 1 || *y_to < 0 || *y_to > ny + 1) {
		vpr_printf(TIO_MESSAGE_ERROR, "in routine find_to: (x_to,y_to) = (%d,%d)\n", *x_to, *y_to);
		exit(1);
	}
#endif


//加入将CLB固定死的策略
	if (strcmp(type->name,"memory") != 0 && wl_with_pro == 2 && exit_location_file == 1){
		*x_to = x_from;
		*y_to = y_from;

	}


	assert(type == grid[*x_to][*y_to].type);

	return TRUE;
}

static enum swap_result assess_swap(float delta_c, float t) {

	/* Returns: 1 -> move accepted, 0 -> rejected. */

	enum swap_result accept;
	float prob_fac, fnum;

	if (delta_c <= 0) {

#ifdef SPEC			/* Reduce variation in final solution due to round off */
		fnum = my_frand();
#endif

		accept = ACCEPTED;
		return (accept);
	}

	if (t == 0.)
		return (REJECTED);

	fnum = my_frand();
	prob_fac = exp(-delta_c / t);
	if (prob_fac > fnum) {
		accept = ACCEPTED;
	} else {
		accept = REJECTED;
	}
	return (accept);
}

static float recompute_bb_cost(void) {

	/* Recomputes the cost to eliminate roundoff that may have accrued.  *
	 * This routine does as little work as possible to compute this new  *
	 * cost.                                                             */

	int inet;
	float cost;

	cost = 0;

	for (inet = 0; inet < num_nets; inet++) { /* for each net ... */
		if (clb_net[inet].is_global == FALSE) { /* Do only if not global. */

			/* Bounding boxes don't have to be recomputed; they're correct. */
			cost += net_cost[inet];
		}
	}

	return (cost);
}

static float comp_td_point_to_point_delay(int inet, int ipin) {

	/*returns the delay of one point to point connection */

	int source_block, sink_block;
	int delta_x, delta_y;
	t_type_ptr source_type, sink_type;
	float delay_source_to_sink;

	delay_source_to_sink = 0.;

	source_block = clb_net[inet].node_block[0];
	source_type = block[source_block].type;

	sink_block = clb_net[inet].node_block[ipin];
	sink_type = block[sink_block].type;

	assert(source_type != NULL);
	assert(sink_type != NULL);

	delta_x = abs(block[sink_block].x - block[source_block].x);
	delta_y = abs(block[sink_block].y - block[source_block].y);

	/* TODO low priority: Could be merged into one look-up table */
	/* Note: This heuristic is terrible on Quality of Results.  
	 * A much better heuristic is to create a more comprehensive lookup table but
	 * it's too late in the release cycle to do this.  Pushing until the next release */
	if (source_type == IO_TYPE) {
		if (sink_type == IO_TYPE)
			delay_source_to_sink = delta_io_to_io[delta_x][delta_y];
		else
			delay_source_to_sink = delta_io_to_clb[delta_x][delta_y];
	} else {
		if (sink_type == IO_TYPE)
			delay_source_to_sink = delta_clb_to_io[delta_x][delta_y];
		else
			delay_source_to_sink = delta_clb_to_clb[delta_x][delta_y];
	}
	if (delay_source_to_sink < 0) {
		vpr_printf(TIO_MESSAGE_ERROR, "in comp_td_point_to_point_delay: Bad delay_source_to_sink value delta(%d, %d) delay of %g\n", delta_x, delta_y, delay_source_to_sink);
		vpr_printf(TIO_MESSAGE_ERROR, "in comp_td_point_to_point_delay: Delay is less than 0\n");
		exit(1);
	}

	return (delay_source_to_sink);
}

static void update_td_cost(void) {
	/* Update the point_to_point_timing_cost values from the temporary *
	 * values for all connections that have changed.                   */

	int iblk_pin, net_pin, inet, ipin;
	int iblk, iblk2, bnum, driven_by_moved_block;
	
	/* Go through all the blocks moved. */
	for (iblk = 0; iblk < blocks_affected.num_moved_blocks; iblk++)
	{
		bnum = blocks_affected.moved_blocks[iblk].block_num;
		for (iblk_pin = 0; iblk_pin < block[bnum].type->num_pins; iblk_pin++) {

			inet = block[bnum].nets[iblk_pin];

			if (inet == OPEN)
				continue;

			if (clb_net[inet].is_global)
				continue;

			net_pin = net_pin_index[bnum][iblk_pin];

			if (net_pin != 0) {

				driven_by_moved_block = FALSE;
				for (iblk2 = 0; iblk2 < blocks_affected.num_moved_blocks; iblk2++)
				{	if (clb_net[inet].node_block[0] == blocks_affected.moved_blocks[iblk2].block_num)
						driven_by_moved_block = TRUE;
				}
				
				/* The following "if" prevents the value from being updated twice. */
				if (driven_by_moved_block == FALSE) {
					point_to_point_delay_cost[inet][net_pin] =
							temp_point_to_point_delay_cost[inet][net_pin];
					temp_point_to_point_delay_cost[inet][net_pin] = -1;
					point_to_point_timing_cost[inet][net_pin] =
							temp_point_to_point_timing_cost[inet][net_pin];
					temp_point_to_point_timing_cost[inet][net_pin] = -1;
				}
			} else { /* This net is being driven by a moved block, recompute */
				/* All point to point connections on this net. */
				for (ipin = 1; ipin <= clb_net[inet].num_sinks; ipin++) {
					point_to_point_delay_cost[inet][ipin] =
							temp_point_to_point_delay_cost[inet][ipin];
					temp_point_to_point_delay_cost[inet][ipin] = -1;
					point_to_point_timing_cost[inet][ipin] =
							temp_point_to_point_timing_cost[inet][ipin];
					temp_point_to_point_timing_cost[inet][ipin] = -1;
				} /* Finished updating the pin */
			}
		} /* Finished going through all the pins in the moved block */
	} /* Finished going through all the blocks moved */
}

static void comp_delta_td_cost(float *delta_timing, float *delta_delay) {

	/*a net that is being driven by a moved block must have all of its  */
	/*sink timing costs recomputed. A net that is driving a moved block */
	/*must only have the timing cost on the connection driving the input */
	/*pin computed */

	int inet, net_pin, ipin;
	float delta_timing_cost, delta_delay_cost, temp_delay;
	int iblk, iblk2, bnum, iblk_pin, driven_by_moved_block;

	delta_timing_cost = 0.;
	delta_delay_cost = 0.;

	/* Go through all the blocks moved */
	for (iblk = 0; iblk < blocks_affected.num_moved_blocks; iblk++)
	{
		bnum = blocks_affected.moved_blocks[iblk].block_num;
		/* Go through all the pins in the moved block */
		for (iblk_pin = 0; iblk_pin < block[bnum].type->num_pins; iblk_pin++) {
			inet = block[bnum].nets[iblk_pin];

			if (inet == OPEN)
				continue;

			if (clb_net[inet].is_global)
				continue;

			net_pin = net_pin_index[bnum][iblk_pin];

			if (net_pin != 0) { 
				/* If this net is being driven by a block that has moved, we do not    *
				 * need to compute the change in the timing cost (here) since it will  *
				 * be computed in the fanout of the net on  the driving block, also    *
				 * computing it here would double count the change, and mess up the    *
				 * delta_timing_cost value.                                            */
				driven_by_moved_block = FALSE;
				for (iblk2 = 0; iblk2 < blocks_affected.num_moved_blocks; iblk2++)
				{	if (clb_net[inet].node_block[0] == blocks_affected.moved_blocks[iblk2].block_num)
						driven_by_moved_block = TRUE;
				}
				
				if (driven_by_moved_block == FALSE) {
					temp_delay = comp_td_point_to_point_delay(inet, net_pin);
					temp_point_to_point_delay_cost[inet][net_pin] = temp_delay;

					temp_point_to_point_timing_cost[inet][net_pin] =
						timing_place_crit[inet][net_pin] * temp_delay;
					delta_timing_cost += temp_point_to_point_timing_cost[inet][net_pin]
						- point_to_point_timing_cost[inet][net_pin];
					delta_delay_cost += temp_point_to_point_delay_cost[inet][net_pin]
							- point_to_point_delay_cost[inet][net_pin];
				}
			} else { /* This net is being driven by a moved block, recompute */
				/* All point to point connections on this net. */
				for (ipin = 1; ipin <= clb_net[inet].num_sinks; ipin++) {
					temp_delay = comp_td_point_to_point_delay(inet, ipin);
					temp_point_to_point_delay_cost[inet][ipin] = temp_delay;

					temp_point_to_point_timing_cost[inet][ipin] =
						timing_place_crit[inet][ipin] * temp_delay;
					delta_timing_cost += temp_point_to_point_timing_cost[inet][ipin]
						- point_to_point_timing_cost[inet][ipin];
					delta_delay_cost += temp_point_to_point_delay_cost[inet][ipin]
							- point_to_point_delay_cost[inet][ipin];

				} /* Finished updating the pin */
			}
		} /* Finished going through all the pins in the moved block */
	} /* Finished going through all the blocks moved */
	
	*delta_timing = delta_timing_cost;
	*delta_delay = delta_delay_cost;
}

static void comp_td_costs(float *timing_cost, float *connection_delay_sum) {
	/* Computes the cost (from scratch) due to the delays and criticalities  *
	 * on all point to point connections, we define the timing cost of       *
	 * each connection as criticality*delay.                                 */

	int inet, ipin;
	float loc_timing_cost, loc_connection_delay_sum, temp_delay_cost,
			temp_timing_cost;

	loc_timing_cost = 0.;
	loc_connection_delay_sum = 0.;

	for (inet = 0; inet < num_nets; inet++) { /* For each net ... */
		if (clb_net[inet].is_global == FALSE) { /* Do only if not global. */

			for (ipin = 1; ipin <= clb_net[inet].num_sinks; ipin++) {

				temp_delay_cost = comp_td_point_to_point_delay(inet, ipin);
				temp_timing_cost = temp_delay_cost * timing_place_crit[inet][ipin];

				loc_connection_delay_sum += temp_delay_cost;
				point_to_point_delay_cost[inet][ipin] = temp_delay_cost;
				temp_point_to_point_delay_cost[inet][ipin] = -1; /* Undefined */

				point_to_point_timing_cost[inet][ipin] = temp_timing_cost;
				temp_point_to_point_timing_cost[inet][ipin] = -1; /* Undefined */
				loc_timing_cost += temp_timing_cost;
			}
		}
	}

	/* Make sure timing cost does not go above MIN_TIMING_COST. */
	*timing_cost = loc_timing_cost;

	*connection_delay_sum = loc_connection_delay_sum;
}

static float comp_bb_cost(enum cost_methods method) {

	/* Finds the cost from scratch.  Done only when the placement   *
	 * has been radically changed (i.e. after initial placement).   *
	 * Otherwise find the cost change incrementally.  If method     *
	 * check is NORMAL, we find bounding boxes that are updateable  *
	 * for the larger nets.  If method is CHECK, all bounding boxes *
	 * are found via the non_updateable_bb routine, to provide a    *
	 * cost which can be used to check the correctness of the       *
	 * other routine.                                               */

	int inet;
	float cost;
	double expected_wirelength;

	cost = 0;
	expected_wirelength = 0.0;

	for (inet = 0; inet < num_nets; inet++) { /* for each net ... */

		if (clb_net[inet].is_global == FALSE) { /* Do only if not global. */

			/* Small nets don't use incremental updating on their bounding boxes, *
			 * so they can use a fast bounding box calculator.                    */

			if (clb_net[inet].num_sinks >= SMALL_NET && method == NORMAL) {
				get_bb_from_scratch(inet, &bb_coords[inet],
						&bb_num_on_edges[inet]);
			} else {
				get_non_updateable_bb(inet, &bb_coords[inet]);
			}

			net_cost[inet] = get_net_cost(inet, &bb_coords[inet]);
			cost += net_cost[inet];
			if (method == CHECK)
				expected_wirelength += get_net_wirelength_estimate(inet,
						&bb_coords[inet]);
		}
	}

	if (method == CHECK) {
		vpr_printf(TIO_MESSAGE_INFO, "\n");
		vpr_printf(TIO_MESSAGE_INFO, "BB estimate of min-dist (placement) wirelength: %.0f\n", expected_wirelength);
	}
	return (cost);
}

static void free_placement_structs(
		float **old_region_occ_x, float **old_region_occ_y,
		struct s_placer_opts placer_opts) {

	/* Frees the major structures needed by the placer (and not needed       *
	 * elsewhere).   */

	int inet, imacro;

	free_legal_placements();
	free_fast_cost_update();

	if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE
			|| placer_opts.place_algorithm == PATH_TIMING_DRIVEN_PLACE
			|| placer_opts.enable_timing_computations) {
		for (inet = 0; inet < num_nets; inet++) {
			/*add one to the address since it is indexed from 1 not 0 */

			point_to_point_delay_cost[inet]++;
			free(point_to_point_delay_cost[inet]);

			point_to_point_timing_cost[inet]++;
			free(point_to_point_timing_cost[inet]);

			temp_point_to_point_delay_cost[inet]++;
			free(temp_point_to_point_delay_cost[inet]);

			temp_point_to_point_timing_cost[inet]++;
			free(temp_point_to_point_timing_cost[inet]);
		}
		free(point_to_point_delay_cost);
		free(temp_point_to_point_delay_cost);

		free(point_to_point_timing_cost);
		free(temp_point_to_point_timing_cost);

		free_matrix(net_pin_index, 0, num_blocks - 1, 0, sizeof(int));
	}

	free(net_cost);
	free(temp_net_cost);
	free(bb_num_on_edges);
	free(bb_coords);
	
	free_placement_macros_structs();
	
	for (imacro = 0; imacro < num_pl_macros; imacro ++)
		free(pl_macros[imacro].members);
	free(pl_macros);
	
	net_cost = NULL; /* Defensive coding. */
	temp_net_cost = NULL;
	bb_num_on_edges = NULL;
	bb_coords = NULL;
	pl_macros = NULL;

	/* Frees up all the data structure used in vpr_utils. */
	free_port_pin_from_blk_pin();
	free_blk_pin_from_port_pin();

}

static void alloc_and_load_placement_structs(
		float place_cost_exp, float ***old_region_occ_x,
		float ***old_region_occ_y, struct s_placer_opts placer_opts,
		t_direct_inf *directs, int num_directs) {

	/* Allocates the major structures needed only by the placer, primarily for *
	 * computing costs quickly and such.                                       */

	int inet, ipin, max_pins_per_clb, i;

	alloc_legal_placements();
	load_legal_placements();

	max_pins_per_clb = 0;
	for (i = 0; i < num_types; i++) {
		max_pins_per_clb = std::max(max_pins_per_clb, type_descriptors[i].num_pins);
	}

	if (placer_opts.place_algorithm == NET_TIMING_DRIVEN_PLACE
			|| placer_opts.place_algorithm == PATH_TIMING_DRIVEN_PLACE
			|| placer_opts.enable_timing_computations) {
		/* Allocate structures associated with timing driven placement */
		/* [0..num_nets-1][1..num_pins-1]  */
		point_to_point_delay_cost = (float **) my_malloc(
				num_nets * sizeof(float *));
		temp_point_to_point_delay_cost = (float **) my_malloc(
				num_nets * sizeof(float *));

		point_to_point_timing_cost = (float **) my_malloc(
				num_nets * sizeof(float *));
		temp_point_to_point_timing_cost = (float **) my_malloc(
				num_nets * sizeof(float *));

		for (inet = 0; inet < num_nets; inet++) {

			/* In the following, subract one so index starts at *
			 * 1 instead of 0 */
			point_to_point_delay_cost[inet] = (float *) my_malloc(
					clb_net[inet].num_sinks * sizeof(float));
			point_to_point_delay_cost[inet]--;

			temp_point_to_point_delay_cost[inet] = (float *) my_malloc(
					clb_net[inet].num_sinks * sizeof(float));
			temp_point_to_point_delay_cost[inet]--;

			point_to_point_timing_cost[inet] = (float *) my_malloc(
					clb_net[inet].num_sinks * sizeof(float));
			point_to_point_timing_cost[inet]--;

			temp_point_to_point_timing_cost[inet] = (float *) my_malloc(
					clb_net[inet].num_sinks * sizeof(float));
			temp_point_to_point_timing_cost[inet]--;
		}
		for (inet = 0; inet < num_nets; inet++) {
			for (ipin = 1; ipin <= clb_net[inet].num_sinks; ipin++) {
				point_to_point_delay_cost[inet][ipin] = 0;
				temp_point_to_point_delay_cost[inet][ipin] = 0;
			}
		}
	}

	net_cost = (float *) my_malloc(num_nets * sizeof(float));
	temp_net_cost = (float *) my_malloc(num_nets * sizeof(float));
	bb_updated_before = (char*)my_calloc(num_nets, sizeof(char));
	
	/* Used to store costs for moves not yet made and to indicate when a net's   *
	 * cost has been recomputed. temp_net_cost[inet] < 0 means net's cost hasn't *
	 * been recomputed.                                                          */

	for (inet = 0; inet < num_nets; inet++){
		bb_updated_before[inet] = NOT_UPDATED_YET;
		temp_net_cost[inet] = -1.;
	}
	
	bb_coords = (struct s_bb *) my_malloc(num_nets * sizeof(struct s_bb));
	bb_num_on_edges = (struct s_bb *) my_malloc(num_nets * sizeof(struct s_bb));

	/* Shouldn't use them; crash hard if I do!   */
	*old_region_occ_x = NULL;
	*old_region_occ_y = NULL;
	
	alloc_and_load_for_fast_cost_update(place_cost_exp);
		
	net_pin_index = alloc_and_load_net_pin_index();

	alloc_and_load_try_swap_structs();

	num_pl_macros = alloc_and_load_placement_macros(directs, num_directs, &pl_macros);
}

static void alloc_and_load_try_swap_structs() {
	/* Allocate the local bb_coordinate storage, etc. only once. */
	/* Allocate with size num_nets for any number of nets affected. */
	ts_bb_coord_new = (struct s_bb *) my_calloc(
			num_nets, sizeof(struct s_bb));
	ts_bb_edge_new = (struct s_bb *) my_calloc(
			num_nets, sizeof(struct s_bb));
	ts_nets_to_update = (int *) my_calloc(num_nets, sizeof(int));
		
	/* Allocate with size num_blocks for any number of moved block. */
	blocks_affected.moved_blocks = (t_pl_moved_block*)my_calloc(
			num_blocks, sizeof(t_pl_moved_block) );
	blocks_affected.num_moved_blocks = 0;
	
}

static void get_bb_from_scratch(int inet, struct s_bb *coords,
		struct s_bb *num_on_edges) {

	/* This routine finds the bounding box of each net from scratch (i.e.    *
	 * from only the block location information).  It updates both the       *
	 * coordinate and number of pins on each edge information.  It         *
	 * should only be called when the bounding box information is not valid. */

	int ipin, bnum, pnum, x, y, xmin, xmax, ymin, ymax;
	int xmin_edge, xmax_edge, ymin_edge, ymax_edge;
	int n_pins;

	n_pins = clb_net[inet].num_sinks + 1;
	
	bnum = clb_net[inet].node_block[0];
	pnum = clb_net[inet].node_block_pin[0];

	x = block[bnum].x;
	y = block[bnum].y + block[bnum].type->pin_height[pnum];

	x = std::max(std::min(x, nx), 1);
	y = std::max(std::min(y, ny), 1);

	xmin = x;
	ymin = y;
	xmax = x;
	ymax = y;
	xmin_edge = 1;
	ymin_edge = 1;
	xmax_edge = 1;
	ymax_edge = 1;

	for (ipin = 1; ipin < n_pins; ipin++) {
		bnum = clb_net[inet].node_block[ipin];
		pnum = clb_net[inet].node_block_pin[ipin];
		x = block[bnum].x;
		y = block[bnum].y + block[bnum].type->pin_height[pnum];

		/* Code below counts IO blocks as being within the 1..nx, 1..ny clb array. *
		 * This is because channels do not go out of the 0..nx, 0..ny range, and   *
		 * I always take all channels impinging on the bounding box to be within   *
		 * that bounding box.  Hence, this "movement" of IO blocks does not affect *
		 * the which channels are included within the bounding box, and it         *
		 * simplifies the code a lot.                                              */

		x = std::max(std::min(x, nx), 1);
		y = std::max(std::min(y, ny), 1);

		if (x == xmin) {
			xmin_edge++;
		}
		if (x == xmax) { /* Recall that xmin could equal xmax -- don't use else */
			xmax_edge++;
		} else if (x < xmin) {
			xmin = x;
			xmin_edge = 1;
		} else if (x > xmax) {
			xmax = x;
			xmax_edge = 1;
		}

		if (y == ymin) {
			ymin_edge++;
		}
		if (y == ymax) {
			ymax_edge++;
		} else if (y < ymin) {
			ymin = y;
			ymin_edge = 1;
		} else if (y > ymax) {
			ymax = y;
			ymax_edge = 1;
		}
	}

	/* Copy the coordinates and number on edges information into the proper   *
	 * structures.                                                            */
	coords->xmin = xmin;
	coords->xmax = xmax;
	coords->ymin = ymin;
	coords->ymax = ymax;

	num_on_edges->xmin = xmin_edge;
	num_on_edges->xmax = xmax_edge;
	num_on_edges->ymin = ymin_edge;
	num_on_edges->ymax = ymax_edge;
}

static double get_net_wirelength_estimate(int inet, struct s_bb *bbptr) {

	/* WMF: Finds the estimate of wirelength due to one net by looking at   *
	 * its coordinate bounding box.                                         */

	double ncost, crossing;

	/* Get the expected "crossing count" of a net, based on its number *
	 * of pins.  Extrapolate for very large nets.                      */

	if (((clb_net[inet].num_sinks + 1) > 50)
			&& ((clb_net[inet].num_sinks + 1) < 85)) {
		crossing = 2.7933 + 0.02616 * ((clb_net[inet].num_sinks + 1) - 50);
	} else if ((clb_net[inet].num_sinks + 1) >= 85) {
		crossing = 2.7933 + 0.011 * (clb_net[inet].num_sinks + 1)
				- 0.0000018 * (clb_net[inet].num_sinks + 1)
						* (clb_net[inet].num_sinks + 1);
	} else {
		crossing = cross_count[(clb_net[inet].num_sinks + 1) - 1];
	}

	/* Could insert a check for xmin == xmax.  In that case, assume  *
	 * connection will be made with no bends and hence no x-cost.    *
	 * Same thing for y-cost.                                        */

	/* Cost = wire length along channel * cross_count / average      *
	 * channel capacity.   Do this for x, then y direction and add.  */

	ncost = (bbptr->xmax - bbptr->xmin + 1) * crossing;

	ncost += (bbptr->ymax - bbptr->ymin + 1) * crossing;

	return (ncost);
}

static float get_net_cost(int inet, struct s_bb *bbptr) {

	/* Finds the cost due to one net by looking at its coordinate bounding  *
	 * box.                                                                 */

	float ncost, crossing;

	/* Get the expected "crossing count" of a net, based on its number *
	 * of pins.  Extrapolate for very large nets.                      */

	if ((clb_net[inet].num_sinks + 1) > 50) {
		crossing = 2.7933 + 0.02616 * ((clb_net[inet].num_sinks + 1) - 50);
		/*    crossing = 3.0;    Old value  */
	} else {
		crossing = cross_count[(clb_net[inet].num_sinks + 1) - 1];
	}

	/* Could insert a check for xmin == xmax.  In that case, assume  *
	 * connection will be made with no bends and hence no x-cost.    *
	 * Same thing for y-cost.                                        */

	/* Cost = wire length along channel * cross_count / average      *
	 * channel capacity.   Do this for x, then y direction and add.  */

	ncost = (bbptr->xmax - bbptr->xmin + 1) * crossing
			* chanx_place_cost_fac[bbptr->ymax][bbptr->ymin - 1];

	ncost += (bbptr->ymax - bbptr->ymin + 1) * crossing
			* chany_place_cost_fac[bbptr->xmax][bbptr->xmin - 1];

	return (ncost);
}

static void get_non_updateable_bb(int inet, struct s_bb *bb_coord_new) {

	/* Finds the bounding box of a net and stores its coordinates in the  *
	 * bb_coord_new data structure.  This routine should only be called   *
	 * for small nets, since it does not determine enough information for *
	 * the bounding box to be updated incrementally later.                *
	 * Currently assumes channels on both sides of the CLBs forming the   *
	 * edges of the bounding box can be used.  Essentially, I am assuming *
	 * the pins always lie on the outside of the bounding box.            */

	int k, xmax, ymax, xmin, ymin, x, y;
	int bnum, pnum;

	bnum = clb_net[inet].node_block[0];
	pnum = clb_net[inet].node_block_pin[0];
	x = block[bnum].x;
	y =	block[bnum].y + block[bnum].type->pin_height[pnum];
	
	xmin = x;
	ymin = y;
	xmax = x;
	ymax = y;

	for (k = 1; k < (clb_net[inet].num_sinks + 1); k++) {
		bnum = clb_net[inet].node_block[k];
		pnum = clb_net[inet].node_block_pin[k];
		x = block[bnum].x;
		y =	block[bnum].y + block[bnum].type->pin_height[pnum];

		if (x < xmin) {
			xmin = x;
		} else if (x > xmax) {
			xmax = x;
		}

		if (y < ymin) {
			ymin = y;
		} else if (y > ymax) {
			ymax = y;
		}
	}

	/* Now I've found the coordinates of the bounding box.  There are no *
	 * channels beyond nx and ny, so I want to clip to that.  As well,   *
	 * since I'll always include the channel immediately below and the   *
	 * channel immediately to the left of the bounding box, I want to    *
	 * clip to 1 in both directions as well (since minimum channel index *
	 * is 0).  See route.c for a channel diagram.                        */

	bb_coord_new->xmin = std::max(std::min(xmin, nx), 1);
	bb_coord_new->ymin = std::max(std::min(ymin, ny), 1);
	bb_coord_new->xmax = std::max(std::min(xmax, nx), 1);
	bb_coord_new->ymax = std::max(std::min(ymax, ny), 1);
}

static void update_bb(int inet, struct s_bb *bb_coord_new,
		struct s_bb *bb_edge_new, int xold, int yold, int xnew, int ynew) {

	/* Updates the bounding box of a net by storing its coordinates in    *
	 * the bb_coord_new data structure and the number of blocks on each   *
	 * edge in the bb_edge_new data structure.  This routine should only  *
	 * be called for large nets, since it has some overhead relative to   *
	 * just doing a brute force bounding box calculation.  The bounding   *
	 * box coordinate and edge information for inet must be valid before  *
	 * this routine is called.                                            *
	 * Currently assumes channels on both sides of the CLBs forming the   *
	 * edges of the bounding box can be used.  Essentially, I am assuming *
	 * the pins always lie on the outside of the bounding box.            *
	 * The x and y coordinates are the pin's x and y coordinates.         */
	/* IO blocks are considered to be one cell in for simplicity.         */
	
	struct s_bb *curr_bb_edge, *curr_bb_coord;
		
	xnew = std::max(std::min(xnew, nx), 1);
	ynew = std::max(std::min(ynew, ny), 1);
	xold = std::max(std::min(xold, nx), 1);
	yold = std::max(std::min(yold, ny), 1);

	/* Check if the net had been updated before. */
	if (bb_updated_before[inet] == GOT_FROM_SCRATCH)
	{	/* The net had been updated from scratch, DO NOT update again! */
		return;
	}
	else if (bb_updated_before[inet] == NOT_UPDATED_YET)
	{	/* The net had NOT been updated before, could use the old values */
		curr_bb_coord = &bb_coords[inet];
		curr_bb_edge = &bb_num_on_edges[inet];
		bb_updated_before[inet] = UPDATED_ONCE;
	}
	else
	{	/* The net had been updated before, must use the new values */
		curr_bb_coord = bb_coord_new;
		curr_bb_edge = bb_edge_new;
	}

	/* Check if I can update the bounding box incrementally. */

	if (xnew < xold) { /* Move to left. */

		/* Update the xmax fields for coordinates and number of edges first. */

		if (xold == curr_bb_coord->xmax) { /* Old position at xmax. */
			if (curr_bb_edge->xmax == 1) {
				get_bb_from_scratch(inet, bb_coord_new, bb_edge_new);
				bb_updated_before[inet] = GOT_FROM_SCRATCH;
				return;
			} else {
				bb_edge_new->xmax = curr_bb_edge->xmax - 1;
				bb_coord_new->xmax = curr_bb_coord->xmax;
			}
		}

		else { /* Move to left, old postion was not at xmax. */
			bb_coord_new->xmax = curr_bb_coord->xmax;
			bb_edge_new->xmax = curr_bb_edge->xmax;
		}

		/* Now do the xmin fields for coordinates and number of edges. */

		if (xnew < curr_bb_coord->xmin) { /* Moved past xmin */
			bb_coord_new->xmin = xnew;
			bb_edge_new->xmin = 1;
		}

		else if (xnew == curr_bb_coord->xmin) { /* Moved to xmin */
			bb_coord_new->xmin = xnew;
			bb_edge_new->xmin = curr_bb_edge->xmin + 1;
		}

		else { /* Xmin unchanged. */
			bb_coord_new->xmin = curr_bb_coord->xmin;
			bb_edge_new->xmin = curr_bb_edge->xmin;
		}
	}

	/* End of move to left case. */
	else if (xnew > xold) { /* Move to right. */

		/* Update the xmin fields for coordinates and number of edges first. */

		if (xold == curr_bb_coord->xmin) { /* Old position at xmin. */
			if (curr_bb_edge->xmin == 1) {
				get_bb_from_scratch(inet, bb_coord_new, bb_edge_new);
				bb_updated_before[inet] = GOT_FROM_SCRATCH;
				return;
			} else {
				bb_edge_new->xmin = curr_bb_edge->xmin - 1;
				bb_coord_new->xmin = curr_bb_coord->xmin;
			}
		}

		else { /* Move to right, old position was not at xmin. */
			bb_coord_new->xmin = curr_bb_coord->xmin;
			bb_edge_new->xmin = curr_bb_edge->xmin;
		}

		/* Now do the xmax fields for coordinates and number of edges. */

		if (xnew > curr_bb_coord->xmax) { /* Moved past xmax. */
			bb_coord_new->xmax = xnew;
			bb_edge_new->xmax = 1;
		}

		else if (xnew == curr_bb_coord->xmax) { /* Moved to xmax */
			bb_coord_new->xmax = xnew;
			bb_edge_new->xmax = curr_bb_edge->xmax + 1;
		}

		else { /* Xmax unchanged. */
			bb_coord_new->xmax = curr_bb_coord->xmax;
			bb_edge_new->xmax = curr_bb_edge->xmax;
		}
	}
	/* End of move to right case. */
	else { /* xnew == xold -- no x motion. */
		bb_coord_new->xmin = curr_bb_coord->xmin;
		bb_coord_new->xmax = curr_bb_coord->xmax;
		bb_edge_new->xmin = curr_bb_edge->xmin;
		bb_edge_new->xmax = curr_bb_edge->xmax;
	}

	/* Now account for the y-direction motion. */

	if (ynew < yold) { /* Move down. */

		/* Update the ymax fields for coordinates and number of edges first. */

		if (yold == curr_bb_coord->ymax) { /* Old position at ymax. */
			if (curr_bb_edge->ymax == 1) {
				get_bb_from_scratch(inet, bb_coord_new, bb_edge_new);
				bb_updated_before[inet] = GOT_FROM_SCRATCH;
				return;
			} else {
				bb_edge_new->ymax = curr_bb_edge->ymax - 1;
				bb_coord_new->ymax = curr_bb_coord->ymax;
			}
		}

		else { /* Move down, old postion was not at ymax. */
			bb_coord_new->ymax = curr_bb_coord->ymax;
			bb_edge_new->ymax = curr_bb_edge->ymax;
		}

		/* Now do the ymin fields for coordinates and number of edges. */

		if (ynew < curr_bb_coord->ymin) { /* Moved past ymin */
			bb_coord_new->ymin = ynew;
			bb_edge_new->ymin = 1;
		}

		else if (ynew == curr_bb_coord->ymin) { /* Moved to ymin */
			bb_coord_new->ymin = ynew;
			bb_edge_new->ymin = curr_bb_edge->ymin + 1;
		}

		else { /* ymin unchanged. */
			bb_coord_new->ymin = curr_bb_coord->ymin;
			bb_edge_new->ymin = curr_bb_edge->ymin;
		}
	}
	/* End of move down case. */
	else if (ynew > yold) { /* Moved up. */

		/* Update the ymin fields for coordinates and number of edges first. */

		if (yold == curr_bb_coord->ymin) { /* Old position at ymin. */
			if (curr_bb_edge->ymin == 1) {
				get_bb_from_scratch(inet, bb_coord_new, bb_edge_new);
				bb_updated_before[inet] = GOT_FROM_SCRATCH;
				return;
			} else {
				bb_edge_new->ymin = curr_bb_edge->ymin - 1;
				bb_coord_new->ymin = curr_bb_coord->ymin;
			}
		}

		else { /* Moved up, old position was not at ymin. */
			bb_coord_new->ymin = curr_bb_coord->ymin;
			bb_edge_new->ymin = curr_bb_edge->ymin;
		}

		/* Now do the ymax fields for coordinates and number of edges. */

		if (ynew > curr_bb_coord->ymax) { /* Moved past ymax. */
			bb_coord_new->ymax = ynew;
			bb_edge_new->ymax = 1;
		}

		else if (ynew == curr_bb_coord->ymax) { /* Moved to ymax */
			bb_coord_new->ymax = ynew;
			bb_edge_new->ymax = curr_bb_edge->ymax + 1;
		}

		else { /* ymax unchanged. */
			bb_coord_new->ymax = curr_bb_coord->ymax;
			bb_edge_new->ymax = curr_bb_edge->ymax;
		}
	}
	/* End of move up case. */
	else { /* ynew == yold -- no y motion. */
		bb_coord_new->ymin = curr_bb_coord->ymin;
		bb_coord_new->ymax = curr_bb_coord->ymax;
		bb_edge_new->ymin = curr_bb_edge->ymin;
		bb_edge_new->ymax = curr_bb_edge->ymax;
	}

	if (bb_updated_before[inet] == NOT_UPDATED_YET)
		bb_updated_before[inet] = UPDATED_ONCE;
}

static void alloc_legal_placements() {
	int i, j, k;

	legal_pos = (t_legal_pos **) my_malloc(num_types * sizeof(t_legal_pos *));
	num_legal_pos = (int *) my_calloc(num_types, sizeof(int));
	
	/* Initialize all occupancy to zero. */

	for (i = 0; i <= nx + 1; i++) {
		for (j = 0; j <= ny + 1; j++) {
			grid[i][j].usage = 0;
			for (k = 0; k < grid[i][j].type->capacity; k++) {
				grid[i][j].blocks[k] = EMPTY;
				if (grid[i][j].offset == 0) {
					num_legal_pos[grid[i][j].type->index]++;
				}
			}
		}
	}

	for (i = 0; i < num_types; i++) {
		legal_pos[i] = (t_legal_pos *) my_malloc(num_legal_pos[i] * sizeof(t_legal_pos));
	}
}

static void load_legal_placements() {
	int i, j, k, itype;
	int *index;

	index = (int *) my_calloc(num_types, sizeof(int));
	//grid[1][1].type->
	for (i = 0; i <= nx + 1; i++) {
		for (j = 0; j <= ny + 1; j++) {
			//std::cout<<"capacity:\t"<<i<<"\t"<<j<<"\t"<< grid[i][j].type->capacity<<std::endl;
			
			for (k = 0; k < grid[i][j].type->capacity; k++) {
			
			//	std::cout<<i<<"\t"<<j<<"\t"<<grid[i][j].offset<<std::endl;
				
				if (grid[i][j].offset == 0) {
					itype = grid[i][j].type->index;
					legal_pos[itype][index[itype]].x = i;
					legal_pos[itype][index[itype]].y = j;
					legal_pos[itype][index[itype]].z = k;
					index[itype]++;
				}
			}
		}
	}
	free(index);
}

static void free_legal_placements() {
	int i;
	for (i = 0; i < num_types; i++) {
		free(legal_pos[i]);
	}
	free(legal_pos); /* Free the mapping list */
	free(num_legal_pos);
}



static int check_macro_can_be_placed(int imacro, int itype, int x, int y, int z) {

	int imember;
	int member_x, member_y, member_z;

	// Every macro can be placed until proven otherwise
	int macro_can_be_placed = TRUE;

	// Check whether all the members can be placed
	for (imember = 0; imember < pl_macros[imacro].num_blocks; imember++) {
		member_x = x + pl_macros[imacro].members[imember].x_offset;
		member_y = y + pl_macros[imacro].members[imember].y_offset;
		member_z = z + pl_macros[imacro].members[imember].z_offset;

		// Check whether the location could accept block of this type
		// Then check whether the location could still accomodate more blocks
		// Also check whether the member position is valid, that is the member's location
		// still within the chip's dimemsion and the member_z is allowed at that location on the grid
		if (member_x <= nx+1 && member_y <= ny+1
				&& grid[member_x][member_y].type->index == itype
				&& grid[member_x][member_y].blocks[member_z] == OPEN) {
			// Can still accomodate blocks here, check the next position
			continue;
		} else {
			// Cant be placed here - skip to the next try
			macro_can_be_placed = FALSE;
			break;
		}
	}
	
	return (macro_can_be_placed);
}


static int try_place_macro(int itype, int ichoice, int imacro, int * free_locations){

	int x, y, z, member_x, member_y, member_z, imember;

	int macro_placed = FALSE;

	// Choose a random position for the head
	x = legal_pos[itype][ichoice].x;
	y = legal_pos[itype][ichoice].y;
	z = legal_pos[itype][ichoice].z;
			
	// If that location is occupied, do nothing.
	if (grid[x][y].blocks[z] != OPEN) {
		return (macro_placed);
	} 
	
	int macro_can_be_placed = check_macro_can_be_placed(imacro, itype, x, y, z);

	if (macro_can_be_placed == TRUE) {
		
		// Place down the macro
		macro_placed = TRUE;
		for (imember = 0; imember < pl_macros[imacro].num_blocks; imember++) {
					
			member_x = x + pl_macros[imacro].members[imember].x_offset;
			member_y = y + pl_macros[imacro].members[imember].y_offset;
			member_z = z + pl_macros[imacro].members[imember].z_offset;
					
			block[pl_macros[imacro].members[imember].blk_index].x = member_x;
			block[pl_macros[imacro].members[imember].blk_index].y = member_y;
			block[pl_macros[imacro].members[imember].blk_index].z = member_z;

			grid[member_x][member_y].blocks[member_z] = pl_macros[imacro].members[imember].blk_index;
			grid[member_x][member_y].usage++;

			// Could not ensure that the randomiser would not pick this location again
			// So, would have to do a lazy removal - whenever I come across a block that could not be placed, 
			// go ahead and remove it from the legal_pos[][] array
						
		} // Finish placing all the members in the macro

	} // End of this choice of legal_pos

	return (macro_placed);

}


static void initial_placement_pl_macros(int macros_max_num_tries, int * free_locations) {

	int macro_placed;
	int imacro, iblk, itype, itry, ichoice;

	/* Macros are harder to place.  Do them first */
	for (imacro = 0; imacro < num_pl_macros; imacro++) {
		
		// Every macro are not placed in the beginnning
		macro_placed = FALSE;
		
		// Assume that all the blocks in the macro are of the same type
		iblk = pl_macros[imacro].members[0].blk_index;
		itype = block[iblk].type->index;
		if (free_locations[itype] < pl_macros[imacro].num_blocks) {
			vpr_printf (TIO_MESSAGE_ERROR, "Initial placement failed.\n");
			vpr_printf (TIO_MESSAGE_ERROR, "Could not place macro length %d with head block %s (#%d); not enough free locations of type %s (#%d).\n", 
					pl_macros[imacro].num_blocks, block[iblk].name, iblk, type_descriptors[itype].name, itype);
			vpr_printf (TIO_MESSAGE_INFO, "VPR cannot auto-size for your circuit, please resize the FPGA manually.\n");
			exit(1);
		}

		// Try to place the macro first, if can be placed - place them, otherwise try again
		for (itry = 0; itry < macros_max_num_tries && macro_placed == FALSE; itry++) {
			
			// Choose a random position for the head
			ichoice = my_irand(free_locations[itype] - 1);

			// Try to place the macro
			macro_placed = try_place_macro(itype, ichoice, imacro, free_locations);

		} // Finished all tries
		
		
		if (macro_placed == FALSE){
			// if a macro still could not be placed after macros_max_num_tries times, 
			// go through the chip exhaustively to find a legal placement for the macro
			// place the macro on the first location that is legal
			// then set macro_placed = TRUE;
			// if there are no legal positions, error out

			// Exhaustive placement of carry macros
			for (ichoice = 0; ichoice < free_locations[itype] && macro_placed == FALSE; ichoice++) {

				// Try to place the macro
				macro_placed = try_place_macro(itype, ichoice, imacro, free_locations);

			} // Exhausted all the legal placement position for this macro

			// If macro could not be placed after exhaustive placement, error out
			if (macro_placed == FALSE) {
				// Error out
				vpr_printf (TIO_MESSAGE_ERROR, "Initial placement failed.\n");
				vpr_printf (TIO_MESSAGE_ERROR, "Could not place macro length %d with head block %s (#%d); not enough free locations of type %s (#%d).\n", 
					pl_macros[imacro].num_blocks, block[iblk].name, iblk, type_descriptors[itype].name, itype);
				vpr_printf (TIO_MESSAGE_INFO, "Please manually size the FPGA because VPR can't do this yet.\n");
				exit(1);
			}

		} else {
			// This macro has been placed successfully, proceed to place the next macro
			continue;
		}
	} // Finish placing all the pl_macros successfully
}

static void initial_placement_blocks(int * free_locations, enum e_pad_loc_type pad_loc_type) {

	/* Place blocks that are NOT a part of any macro.
	 * We'll randomly place each block in the clustered netlist, one by one. 
	 */

	int iblk, itype;
	int ichoice, x, y, z;

	for (iblk = 0; iblk < num_blocks; iblk++) {
		if (block[iblk].x != -1) {
			// block placed.
			continue;
		}
		if(strcmp((*(block[iblk].type)).name,"memory")==0){
			std::cout<<block[iblk].name<<std::endl;
		}
		
		/* Don't do IOs if the user specifies IOs; we'll read those locations later. */
		if (!(block[iblk].type == IO_TYPE && pad_loc_type == USER)) {

		    /* Randomly select a free location of the appropriate type
			 * for iblk.  We have a linearized list of all the free locations
			 * that can accomodate a block of that type in free_locations[itype].
			 * Choose one randomly and put iblk there.  Then we don't want to pick that
			 * location again, so remove it from the free_locations array.
			 */
			itype = block[iblk].type->index;
			if (free_locations[itype] <= 0) {
				vpr_printf (TIO_MESSAGE_ERROR, "Initial placement failed.\n");
				vpr_printf (TIO_MESSAGE_ERROR, "Could not place block %s (#%d); no free locations of type %s (#%d).\n", 
						block[iblk].name, iblk, type_descriptors[itype].name, itype);
				exit(1);
			}

//初始时不能选择位置被写的多的块。
			int times = 0;
			int *xx = (int *) my_malloc(phy*sizeof(int));

			do{
			
				int kk = 0;

				int tmpd = 0;
				if (strcmp((*(block[iblk].type)).name,"memory") == 0){
					tmpd =  free_locations[itype];
					if (times == tmpd){
						std::cout<<"ffffffffffff"<<std::endl;
						exit(1);
					}
				}
					
				
				while (1) {
					ichoice = my_irand(free_locations[itype] - 1);
					for (kk = 0; kk< times ;kk++){
						if (ichoice == xx[kk])
							break;
					}
					if (kk < times)
						continue;
					else
						break;
				}

				x = legal_pos[itype][ichoice].x;
				y = legal_pos[itype][ichoice].y;
				z = legal_pos[itype][ichoice].z;

			// Make sure that the position is OPEN before placing the block down
				assert (grid[x][y].blocks[z] == OPEN);

				int is_mem = 0;
				if (strcmp((*(block[iblk].type)).name,"memory") == 0){
					std::cout<<block[iblk].name<<"\t"<<x<<":"<<y<<std::endl;
					is_mem = 1;
				}



				//这里写将CLB定死位置的策略：：
				if(wl_with_pro == 2 && is_mem == 0 ){

					initial_location();
					if(exit_location_file == 1){
						int i = 0;
						for(i = 0; i < num_all; i++){
							if(strcmp(block[iblk].name,loca_st[i].name) == 0)
								break;
						}
						x = loca_st[i].x;
						y = loca_st[i].y;
						z = loca_st[i].subblk;

						int j = 0;
						for(j = 0; j < free_locations[itype]; j++){
							if(	x == legal_pos[itype][j].x && y == legal_pos[itype][j].y && z == legal_pos[itype][j].z){
								ichoice = j;
								break;
							}

						}

					}
				}
				//CLB定死的策略结束


				xx[times] = ichoice;
				times++;
			}while(location_bool((*(block[iblk].type)).name,block[iblk].name,x,y,1));




// block 的x y保存位置信息，但是在图形界面是从下往上 从左往右
			grid[x][y].blocks[z] = iblk;
			grid[x][y].usage++;

			block[iblk].x = x;
			block[iblk].y = y;
			block[iblk].z = z;

/*	if (strcmp((*(block[iblk].type)).name,"memory") ==0){
		std::cout<<"type:\t"<<(*(block[iblk].type)).name<<"\nname:\t"<<block[iblk].name<<"\nlocation:\t"<<block[iblk].x<<"  "<<block[iblk].y<<std::endl<<std::endl;
	}*/


			/* Ensure randomizer doesn't pick this location again, since it's occupied. Could shift all the 
				* legal positions in legal_pos to remove the entry (choice) we just used, but faster to 
				* just move the last entry in legal_pos to the spot we just used and decrement the 
				* count of free_locations.
				*/
			legal_pos[itype][ichoice] = legal_pos[itype][free_locations[itype] - 1]; /* overwrite used block position */
			free_locations[itype]--;
			
		}
	}
}

static void initial_placement(enum e_pad_loc_type pad_loc_type,
		char *pad_loc_file) {

	/* Randomly places the blocks to create an initial placement. We rely on
	 * the legal_pos array already being loaded.  That legal_pos[itype] is an 
	 * array that gives every legal value of (x,y,z) that can accomodate a block.
	 * The number of such locations is given by num_legal_pos[itype].
	 */
	int i, j, k, iblk, itype, x, y, z, ichoice;
	int *free_locations; /* [0..num_types-1]. 
						  * Stores how many locations there are for this type that *might* still be free.
						  * That is, this stores the number of entries in legal_pos[itype] that are worth considering
						  * as you look for a free location.
						  */

	free_locations = (int *) my_malloc(num_types * sizeof(int));
	std::cout<<"---------------------------"<<std::endl;
	for (itype = 0; itype < num_types; itype++) {
		free_locations[itype] = num_legal_pos[itype];
		std::cout<<free_locations[itype]<<std::endl;
	}
		std::cout<<"---------------------------"<<std::endl;

		//------------------------------------

		//free_locations 保存每种type的可用数量


		//------------------------------------
	/* We'll use the grid to record where everything goes. Initialize to the grid has no 
	 * blocks placed anywhere.
	 */
	for (i = 0; i <= nx + 1; i++) {
		for (j = 0; j <= ny + 1; j++) {
			grid[i][j].usage = 0;
			itype = grid[i][j].type->index;
			for (k = 0; k < type_descriptors[itype].capacity; k++) {
				grid[i][j].blocks[k] = OPEN;
			}
		}
	}
	/* Similarly, mark all blocks as not being placed yet. */
	for (iblk = 0; iblk < num_blocks; iblk++) {
		block[iblk].x = -1;
		block[iblk].y = -1;
		block[iblk].z = -1;
	}

	initial_placement_pl_macros(MAX_NUM_TRIES_TO_PLACE_MACROS_RANDOMLY, free_locations);
	
	// All the macros are placed, update the legal_pos[][] array
	for (itype = 0; itype < num_types; itype++) {
		assert (free_locations[itype] >= 0);
//-------------------------
// itype 是类型（比如clb，bram等）
// ichoice 小于每个类型的可用的数目
// x y z 是三维。io有三维，别的只有两维，z=0 x 为位置的横坐标 y为位置的纵坐标 分别为每个组件的起始坐标
//--------------------------
		for (ichoice = 0; ichoice < free_locations[itype]; ichoice++) {
			x = legal_pos[itype][ichoice].x;
			y = legal_pos[itype][ichoice].y;
			z = legal_pos[itype][ichoice].z;
			
			// Check if that location is occupied.  If it is, remove from legal_pos
			if (grid[x][y].blocks[z] != OPEN) {
				legal_pos[itype][ichoice] = legal_pos[itype][free_locations[itype] - 1];
				free_locations[itype]--;

				// After the move, I need to check this particular entry again
				ichoice--;
				continue;
			}
		}
	} // Finish updating the legal_pos[][] and free_locations[] array

	initial_placement_blocks(free_locations, pad_loc_type);

	if (pad_loc_type == USER) {
		read_user_pad_loc(pad_loc_file);
	}

	/* Restore legal_pos */
	load_legal_placements();

#ifdef VERBOSE
	vpr_printf(TIO_MESSAGE_INFO, "At end of initial_placement.\n");
	if (getEchoEnabled() && isEchoFileEnabled(E_ECHO_INITIAL_CLB_PLACEMENT)) {
		print_clb_placement(getEchoFileName(E_ECHO_INITIAL_CLB_PLACEMENT));
	}
#endif
	free(free_locations);
	
}

static void free_fast_cost_update(void) {
	int i;

	for (i = 0; i <= ny; i++)
		free(chanx_place_cost_fac[i]);
	free(chanx_place_cost_fac);
	chanx_place_cost_fac = NULL;

	for (i = 0; i <= nx; i++)
		free(chany_place_cost_fac[i]);
	free(chany_place_cost_fac);
	chany_place_cost_fac = NULL;
}

static void alloc_and_load_for_fast_cost_update(float place_cost_exp) {

	/* Allocates and loads the chanx_place_cost_fac and chany_place_cost_fac *
	 * arrays with the inverse of the average number of tracks per channel   *
	 * between [subhigh] and [sublow].  This is only useful for the cost     *
	 * function that takes the length of the net bounding box in each        *
	 * dimension divided by the average number of tracks in that direction.  *
	 * For other cost functions, you don't have to bother calling this       *
	 * routine; when using the cost function described above, however, you   *
	 * must always call this routine after you call init_chan and before     *
	 * you do any placement cost determination.  The place_cost_exp factor   *
	 * specifies to what power the width of the channel should be taken --   *
	 * larger numbers make narrower channels more expensive.                 */

	int low, high, i;

	/* Access arrays below as chan?_place_cost_fac[subhigh][sublow].  Since   *
	 * subhigh must be greater than or equal to sublow, we only need to       *
	 * allocate storage for the lower half of a matrix.                       */

	chanx_place_cost_fac = (float **) my_malloc((ny + 1) * sizeof(float *));
	for (i = 0; i <= ny; i++)
		chanx_place_cost_fac[i] = (float *) my_malloc((i + 1) * sizeof(float));

	chany_place_cost_fac = (float **) my_malloc((nx + 1) * sizeof(float *));
	for (i = 0; i <= nx; i++)
		chany_place_cost_fac[i] = (float *) my_malloc((i + 1) * sizeof(float));

	/* First compute the number of tracks between channel high and channel *
	 * low, inclusive, in an efficient manner.                             */

	chanx_place_cost_fac[0][0] = chan_width_x[0];

	for (high = 1; high <= ny; high++) {
		chanx_place_cost_fac[high][high] = chan_width_x[high];
		for (low = 0; low < high; low++) {
			chanx_place_cost_fac[high][low] =
					chanx_place_cost_fac[high - 1][low] + chan_width_x[high];
		}
	}

	/* Now compute the inverse of the average number of tracks per channel *
	 * between high and low.  The cost function divides by the average     *
	 * number of tracks per channel, so by storing the inverse I convert   *
	 * this to a faster multiplication.  Take this final number to the     *
	 * place_cost_exp power -- numbers other than one mean this is no      *
	 * longer a simple "average number of tracks"; it is some power of     *
	 * that, allowing greater penalization of narrow channels.             */

	for (high = 0; high <= ny; high++)
		for (low = 0; low <= high; low++) {
			chanx_place_cost_fac[high][low] = (high - low + 1.)
					/ chanx_place_cost_fac[high][low];
			chanx_place_cost_fac[high][low] = pow(
					(double) chanx_place_cost_fac[high][low],
					(double) place_cost_exp);
		}

	/* Now do the same thing for the y-directed channels.  First get the  *
	 * number of tracks between channel high and channel low, inclusive.  */

	chany_place_cost_fac[0][0] = chan_width_y[0];

	for (high = 1; high <= nx; high++) {
		chany_place_cost_fac[high][high] = chan_width_y[high];
		for (low = 0; low < high; low++) {
			chany_place_cost_fac[high][low] =
					chany_place_cost_fac[high - 1][low] + chan_width_y[high];
		}
	}

	/* Now compute the inverse of the average number of tracks per channel * 
	 * between high and low.  Take to specified power.                     */

	for (high = 0; high <= nx; high++)
		for (low = 0; low <= high; low++) {
			chany_place_cost_fac[high][low] = (high - low + 1.)
					/ chany_place_cost_fac[high][low];
			chany_place_cost_fac[high][low] = pow(
					(double) chany_place_cost_fac[high][low],
					(double) place_cost_exp);
		}
}

static void check_place(float bb_cost, float timing_cost, 
		enum e_place_algorithm place_algorithm,
		float delay_cost) {

	/* Checks that the placement has not confused our data structures. *
	 * i.e. the clb and block structures agree about the locations of  *
	 * every block, blocks are in legal spots, etc.  Also recomputes   *
	 * the final placement cost from scratch and makes sure it is      *
	 * within roundoff of what we think the cost is.                   */

	static int *bdone;
	int i, j, k, error = 0, bnum;
	float bb_cost_check;
	int usage_check;
	float timing_cost_check, delay_cost_check;
	int imacro, imember, head_iblk, member_iblk, member_x, member_y, member_z;

	bb_cost_check = comp_bb_cost(CHECK);
	vpr_printf(TIO_MESSAGE_INFO, "bb_cost recomputed from scratch: %g\n", bb_cost_check);
	if (fabs(bb_cost_check - bb_cost) > bb_cost * ERROR_TOL) {
		vpr_printf(TIO_MESSAGE_ERROR, "bb_cost_check: %g and bb_cost: %g differ in check_place.\n", bb_cost_check, bb_cost);
		error++;
	}

	if (place_algorithm == NET_TIMING_DRIVEN_PLACE
			|| place_algorithm == PATH_TIMING_DRIVEN_PLACE) {
		comp_td_costs(&timing_cost_check, &delay_cost_check);
		vpr_printf(TIO_MESSAGE_INFO, "timing_cost recomputed from scratch: %g\n", timing_cost_check);
		if (fabs(timing_cost_check - timing_cost) > timing_cost * ERROR_TOL) {
			vpr_printf(TIO_MESSAGE_ERROR, "timing_cost_check: %g and timing_cost: %g differ in check_place.\n", 
					   timing_cost_check, timing_cost);
			error++;
		}
		vpr_printf(TIO_MESSAGE_INFO, "delay_cost recomputed from scratch: %g\n", delay_cost_check);
		if (fabs(delay_cost_check - delay_cost) > delay_cost * ERROR_TOL) {
			vpr_printf(TIO_MESSAGE_ERROR, "delay_cost_check: %g and delay_cost: %g differ in check_place.\n", 
					delay_cost_check, delay_cost);
			error++;
		}
	}

	bdone = (int *) my_malloc(num_blocks * sizeof(int));
	for (i = 0; i < num_blocks; i++)
		bdone[i] = 0;

	/* Step through grid array. Check it against block array. */
	for (i = 0; i <= (nx + 1); i++)
		for (j = 0; j <= (ny + 1); j++) {
			if (grid[i][j].usage > grid[i][j].type->capacity) {
				vpr_printf(TIO_MESSAGE_ERROR, "Block at grid location (%d,%d) overused. Usage is %d.\n", 
						i, j, grid[i][j].usage);
				error++;
			}
			usage_check = 0;
			for (k = 0; k < grid[i][j].type->capacity; k++) {
				bnum = grid[i][j].blocks[k];
				if (EMPTY == bnum)
					continue;

				if (block[bnum].type != grid[i][j].type) {
					vpr_printf(TIO_MESSAGE_ERROR, "Block %d type does not match grid location (%d,%d) type.\n",
							bnum, i, j);
					error++;
				}
				if ((block[bnum].x != i) || (block[bnum].y != j)) {
					vpr_printf(TIO_MESSAGE_ERROR, "Block %d location conflicts with grid(%d,%d) data.\n", 
							bnum, i, j);
					error++;
				}
				++usage_check;
				bdone[bnum]++;
			}
			if (usage_check != grid[i][j].usage) {
				vpr_printf(TIO_MESSAGE_ERROR, "Location (%d,%d) usage is %d, but has actual usage %d.\n",
						i, j, grid[i][j].usage, usage_check);
				error++;
			}
		}

	/* Check that every block exists in the grid and block arrays somewhere. */
	for (i = 0; i < num_blocks; i++)
		if (bdone[i] != 1) {
			vpr_printf(TIO_MESSAGE_ERROR, "Block %d listed %d times in data structures.\n",
					i, bdone[i]);
			error++;
		}
	free(bdone);
	
	/* Check the pl_macro placement are legal - blocks are in the proper relative position. */
	for (imacro = 0; imacro < num_pl_macros; imacro++) {
		
		head_iblk = pl_macros[imacro].members[0].blk_index;
		
		for (imember = 0; imember < pl_macros[imacro].num_blocks; imember++) {
			
			member_iblk = pl_macros[imacro].members[imember].blk_index;

			// Compute the suppossed member's x,y,z location
			member_x = block[head_iblk].x + pl_macros[imacro].members[imember].x_offset;
			member_y = block[head_iblk].y + pl_macros[imacro].members[imember].y_offset;
			member_z = block[head_iblk].z + pl_macros[imacro].members[imember].z_offset;

			// Check the block data structure first
			if (block[member_iblk].x != member_x 
					|| block[member_iblk].y != member_y 
					|| block[member_iblk].z != member_z) {
				vpr_printf(TIO_MESSAGE_ERROR, "Block %d in pl_macro #%d is not placed in the proper orientation.\n", 
						member_iblk, imacro);
				error++;
			}

			// Then check the grid data structure
			if (grid[member_x][member_y].blocks[member_z] != member_iblk) {
				vpr_printf(TIO_MESSAGE_ERROR, "Block %d in pl_macro #%d is not placed in the proper orientation.\n", 
						member_iblk, imacro);
				error++;
			}
		} // Finish going through all the members
	} // Finish going through all the macros

	if (error == 0) {
		vpr_printf(TIO_MESSAGE_INFO, "\n");
		vpr_printf(TIO_MESSAGE_INFO, "Completed placement consistency check successfully.\n");
		vpr_printf(TIO_MESSAGE_INFO, "\n");
		vpr_printf(TIO_MESSAGE_INFO, "Swaps called: %d\n", num_ts_called);

#ifdef PRINT_REL_POS_DISTR
		print_relative_pos_distr(void);
#endif
	} else {
		vpr_printf(TIO_MESSAGE_INFO, "\n");
		vpr_printf(TIO_MESSAGE_ERROR, "Completed placement consistency check, %d errors found.\n", error);
		vpr_printf(TIO_MESSAGE_INFO, "Aborting program.\n");
		exit(1);
	}

}

#ifdef VERBOSE
static void print_clb_placement(const char *fname) {

	/* Prints out the clb placements to a file.  */

	FILE *fp;
	int i;
	
	fp = my_fopen(fname, "w", 0);
	fprintf(fp, "Complex block placements:\n\n");

	fprintf(fp, "Block #\tName\t(X, Y, Z).\n");
	for(i = 0; i < num_blocks; i++) {
		fprintf(fp, "#%d\t%s\t(%d, %d, %d).\n", i, block[i].name, block[i].x, block[i].y, block[i].z);
	}
	
	fclose(fp);	
}
#endif

static void free_try_swap_arrays(void) {
	if(ts_bb_coord_new != NULL) {
		free(ts_bb_coord_new);
		free(ts_bb_edge_new);
		free(ts_nets_to_update);
		free(blocks_affected.moved_blocks);
		free(bb_updated_before);
		
		ts_bb_coord_new = NULL;
		ts_bb_edge_new = NULL;
		ts_nets_to_update = NULL;
		blocks_affected.moved_blocks = NULL;
		blocks_affected.num_moved_blocks = 0;
		bb_updated_before = NULL;
	}
}

//初始化时禁止使用(x,y)位置的block-ram 接受返回0，退出循环
static int	location_bool(const char* type, const char* name,int x,int y,int if_init){
	//return 0;
	
	int accept = 0;
	int unaccept = 1;

	if(exist_config_file == 0){
		return accept;
	}
	if( strcmp(type,"memory")!=0){
		return accept;
	}
	else{
		if(wl_with_pro == 1){
			//----------------------------- 
			//  这里写改逻辑块放置这里是否合法的策略
			// lbr pbr used phy
			//--------------------------------
			//线性搜索信息：
			int ilbr = 0;
			for( ilbr = 0;ilbr < used;ilbr++){
				if( strcmp(lbr[ilbr].name,name)==0 )
					break;
			}
	
			assert( strcmp(lbr[ilbr].name,name)==0 );
			int ipbr = 0;
			for( ipbr = 0;ipbr < phy;ipbr++){
				if(x==pbr[ipbr].x && y==pbr[ipbr].y)
					break;
			}
			assert(x==pbr[ipbr].x && y==pbr[ipbr].y);

			//CASE0:某个逻辑块的写频率与(x,y)该物理块的使用频率之和小于阈值，则合法
			float pp = 0;
			if (lbr[ilbr].rate <= 0.02)
				pp = 1;
			else
				pp = (1-pbr[ipbr].rate)/lbr[ilbr].rate;


			if(pp > 0.8){
				if(if_init!=1 || place_can_continue(name,x,y,ilbr,ipbr)){
					lbr[ilbr].pos = ipbr;
					pbr[ipbr].pos = ilbr;
					return accept;
				}
			}
		}
		else{
			int ilbr = 0;
			for( ilbr = 0;ilbr < used;ilbr++){
				std::cout<<lbr[ilbr].name<<"\t"<<lbr[ilbr].rate<<std::endl;
				if( strcmp(lbr[ilbr].name,name)==0 )
					break;
			}
	
			assert( strcmp(lbr[ilbr].name,name)==0 );
			int ipbr = 0;
			for( ipbr = 0;ipbr < phy;ipbr++){
				std::cout<<pbr[ipbr].x<<"\t"<<pbr[ipbr].y<<"\t"<<pbr[ipbr].rate<<std::endl;
				if(x==pbr[ipbr].x && y==pbr[ipbr].y)
					break;
			}
			assert(x==pbr[ipbr].x && y==pbr[ipbr].y);
			if ( pbr[ilbr].rate - pbr[ipbr].rate >= -0.01 && pbr[ilbr].rate - pbr[ipbr].rate<=0.01)
				return accept;

		}
		

	}
	return unaccept;
}

static void initial_l_py(void){
	std::ifstream infile; //文件输入流
	std::string s; //输入缓冲区
	int i = 0;
	for(int i = 0;i<num_types;i++){
		if(strcmp(type_descriptors[i].name,"memory")==0){
			phy = num_phy[i];
			used = num_used[i];
			break;
		}
	}
/*********************************************************************************************
 * 
 * 对逻辑上的block-ram 的使用进行统计，结果保存在lbr中。
 * 
 * 
 * **/
	lbr = (logic_bram_rate *) my_malloc (used*sizeof(logic_bram_rate));
	std::cout<<full_circuit_name<<std::endl;

	int name_len = 0;
	while (1){
		if (full_circuit_name[name_len] == '\0')
			break;
		name_len ++;
	}

	char * log_name = (char*) malloc ((name_len + 9)*sizeof(char));
	char * phy_name = (char*) malloc ((name_len + 9)*sizeof(char));
	name_len = 0;
	while (1){
		if (full_circuit_name[name_len] == '\0')
			break;
		log_name[name_len] = full_circuit_name[name_len];
		phy_name[name_len] = full_circuit_name[name_len];
		name_len ++;
	}

	log_name[name_len] = '_';
	log_name[name_len+1] = 'l';
	log_name[name_len+2] = 'o';
	log_name[name_len+3] = 'g';
	log_name[name_len+4] = '.';
	log_name[name_len+5] = 't';
	log_name[name_len+6] = 'x';
	log_name[name_len+7] = 't';
	log_name[name_len+8] = '\0';

	phy_name[name_len] = '_';
	phy_name[name_len+1] = 'p';
	phy_name[name_len+2] = 'h';
	phy_name[name_len+3] = 'y';
	phy_name[name_len+4] = '.';
	phy_name[name_len+5] = 't';
	phy_name[name_len+6] = 'x';
	phy_name[name_len+7] = 't';
	phy_name[name_len+8] = '\0';


	infile.open(log_name);
	if (!infile.is_open()) {
		exist_config_file = 0;
		return ;
	}
	exist_config_file = 1;

	while(getline(infile,s) && i<used)
   	{
       	std::vector<std::string> strs;
	   	SplitString(s,strs," ");
		int len = 0; 
		while(1){
			if(strs[0][len] == '\0')
				break;
			len++;
		}
    	lbr[i].name = (char *)malloc((len+1)*sizeof(char));
		len = 0;
    	while(1){
			if(strs[0][len] == '\0')
				break;
			lbr[i].name[len] = strs[0][len];
			len ++;
		}
		lbr[i].name[len] = '\0';
	   	float rate = stringToNum(strs[1]);
	   	lbr[i].rate = rate;
		lbr[i].pos = -1;
		lbr[i].try_pos = -1;
	  	i++;
    }
    infile.close();
	/***************************************************************************
	 * 
	 * 对物理上的block-ram使用情况进行统计，结果保存在pbr中。
	 * 
	 * **/
	pbr = (physical_bram_rate *) my_malloc (phy*sizeof(physical_bram_rate));
	infile.open(phy_name);
	if (!infile.is_open()) {
		exist_config_file = 0;
		return;
	}
	exist_config_file = 1;
	i= 0;
	while(getline(infile,s) && i<phy){
		std::vector<std::string> strs;
	   	SplitString(s,strs," ");
		int x = (int)stringToNum(strs[0]);
		int y = (int)stringToNum(strs[1]);
		float rate = stringToNum(strs[2]);
		pbr[i].x = x;
		pbr[i].y = y;
		pbr[i].rate = rate;
		pbr[i].pos = -1;
		i++;
	}
	infile.close();
	sort();	
	for(int i = 0; i < phy;i++){
		std::cout<<pbr[i].x<<" "<<pbr[i].y<<" "<<pbr[i].rate<<std::endl;
	}
}
static void SplitString(const std::string& s, std::vector<std::string>& v, const std::string& c)
{
  std::string::size_type pos1, pos2;
  pos2 = s.find(c);
  pos1 = 0;
  while(std::string::npos != pos2)
  {
    v.push_back(s.substr(pos1, pos2-pos1));
 
    pos1 = pos2 + c.size();
    pos2 = s.find(c, pos1);
  }
  if(pos1 != s.length())
    v.push_back(s.substr(pos1));
}

static float stringToNum(const std::string& str)
{
	std::istringstream iss(str);
	float num;
	iss >> num;
	return num;
}
int lgc_cmp(const void *a,const void *b){
	 return (((struct logic_bram_rate *)a)->rate>((struct logic_bram_rate *)b)->rate?-1:1);
}
int phy_cmp(const void *a,const void *b){
	 return (((struct physical_bram_rate *)a)->rate>((struct physical_bram_rate *)b)->rate?1:-1);
}
static void sort(){
	 qsort(lbr,used,sizeof(logic_bram_rate),lgc_cmp);//从大到小
	 qsort(pbr,phy,sizeof(physical_bram_rate),phy_cmp);//从小到大
}
static int place_can_continue(const char* name,int x,int y,int ilbr,int ipbr){
	int accept = 1;
	int unaccept = 0;
	if(pbr[ipbr].pos != -1)
		return unaccept;
	if(lbr[ilbr].pos != -1)
		return unaccept;

	srand (time(NULL));
	float p = (double)rand() / (double)RAND_MAX ;

	float tmp = pbr[ipbr].rate + lbr[ilbr].rate - 1 -  p;

	if(tmp > 0)
		return unaccept;

	int *lbr_num = (int *) malloc(used*sizeof(int));
	int *pbr_num = (int *) malloc(phy*sizeof(int));
	int lbr_len = 0;
	int pbr_len = 0;
	for(int i = 0;i < used;i++){
		if(lbr[i].pos == -1 && i!=ilbr){
			lbr_num[lbr_len] = i;
			lbr_len++;
		}
	}
	std::cout<<std::endl;
	for(int i = 0;i < phy;i++){
		if(pbr[i].pos == -1 && i!=ipbr){
			pbr_num[pbr_len] = i;
			pbr_len++;
		}
	}
	std::cout<<std::endl;

	//	return unaccept;
/*		for (int k = 0;k<lbr_len;k++){
			std::cout<<lbr[k].rate<<std::endl;
		}
		std::cout<<std::endl;
		for (int k = 0;k<pbr_len;k++){
			std::cout<<pbr[k].rate<<std::endl;
		}
*/
	assert((used-lbr_len) == (phy-pbr_len));
	for(int i = 0; i< lbr_len;i++){
		//if(lbr[lbr_num[i]].rate + pbr[pbr_num[i]].rate > 1.1)
	
		if(lbr[lbr_num[i]].rate <= 0.1)
			continue;
		

		float ratio = (1-pbr[pbr_num[i]].rate) / lbr[lbr_num[i]].rate;

		if(ratio < 0.8){
			std::cout<<pbr[pbr_num[i]].x <<"  "<<pbr[pbr_num[i]].y<<"  "<<pbr[pbr_num[i]].rate<< std::endl;
			std::cout<<lbr[lbr_num[i]].name<<"\t"<<lbr[lbr_num[i]].rate <<std::endl;
			return unaccept;
		}
			

		else{
			lbr[lbr_num[i]].try_pos = pbr_num[i]; 
		}
	}
	for(int i = 0;i < used;i++){
		/*if (lbr[i].rate > 0.1 && lbr[i].pos == -1 && lbr[i].try_pos == -1 && i!=ilbr){
			std::cout<<lbr[i].name<<"\t"<<lbr[i].rate <<"\t"<<lbr[i].try_pos<<"\t"<<pbr[lbr[i].try_pos].rate<<std::endl;
		}*/
		
		lbr[i].try_pos = -1;
	}
	std::cout<<"accept"<<std::endl;
	return accept;
}


static void initial_location(void){
	std::ifstream infile; //文件输入流
	std::string s; //输入缓冲区
	int i = 0;
	
/*********************************************************************************************
 * 
 * 对每个block的位置进行读取，结果保存在loca_st中
 * 
 * 
 * **/

	std::cout<<full_circuit_name<<std::endl;

	int name_len = 0;
	while (1){
		if (full_circuit_name[name_len] == '\0')
			break;
		name_len ++;
	}

	char * location_name = (char*) malloc ((name_len + 12)*sizeof(char));
	name_len = 0;
	while (1){
		if (full_circuit_name[name_len] == '\0')
			break;
		location_name[name_len] = full_circuit_name[name_len];
		name_len ++;
	}

	location_name[name_len] = '_';
	location_name[name_len+1] = 'i';
	location_name[name_len+2] = 'n';
	location_name[name_len+3] = 'i';
	location_name[name_len+4] = 't';
	location_name[name_len+5] = '.';
	location_name[name_len+6] = 'p';
	location_name[name_len+7] = 'l';
	location_name[name_len+8] = 'a';
	location_name[name_len+9] = 'c';
	location_name[name_len+10] = 'e';
	location_name[name_len+11] = '\0';

	infile.open(location_name);
	if (!infile.is_open()) {
		exit_location_file = 0;
		return ;
	}

	getline(infile,s);
	
	std::istringstream iss(s);
	int num;
	iss >> num;
	num_all = num;

	loca_st = (location_struct*) my_malloc (num * sizeof(location_struct));

	exit_location_file = 1;

	while(getline(infile,s))
   	{
       	std::vector<std::string> strs;
	   	SplitString(s,strs," ");
		int len = 0; 
		while(1){
			if(strs[0][len] == '\0')
				break;
			len++;
		}
    	loca_st[i].name = (char *)malloc((len+1)*sizeof(char));
		len = 0;
    	while(1){
			if(strs[0][len] == '\0')
				break;
			loca_st[i].name[len] = strs[0][len];
			len ++;
		}
		loca_st[i].name[len] = '\0';

	   	int x = (int)stringToNum(strs[1]);
		int y = (int)stringToNum(strs[2]);
		int subblk = (int)stringToNum(strs[3]);

	   	loca_st[i].x = x;
		loca_st[i].y = y;
		loca_st[i].subblk = subblk;
	  	i++;
    }
    infile.close();

}