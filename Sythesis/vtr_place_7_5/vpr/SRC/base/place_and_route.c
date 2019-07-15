#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include "util.h"
#include "vpr_types.h"
#include "vpr_utils.h"
#include "globals.h"
#include "place_and_route.h"
#include "place.h"
#include "read_place.h"
#include "route_export.h"
#include "draw.h"
#include "stats.h"
#include "check_route.h"
#include "rr_graph.h"
#include "path_delay.h"
#include "net_delay.h"
#include "timing_place.h"
#include "read_xml_arch_file.h"
#include "ReadOptions.h"
#include "route_common.h"
#include "place_macro.h"
#include "verilog_writer.h"
#include "power.h"

//zh_lab
#define CONFIG_WIDTH 64
#define PRINT_BRAM_INFO
static void load_log_pin_info(pin_dict * p_dict, BRAMS_log * p_log_brams);
static void load_log_BRAMS_info(char * info_path ,BRAMS_log * p_log_brams);
static void malloc_write_unit_space(BRAMS_log * p_log_brams);
static BRAM_write_unit *  buile_locat_map(BRAM_write_unit * p_write_unit_vector,int width,int add_range );
static void match_log_bram_id(BRAMS_log *p_log_brams);
/******************* Subroutines local to this module ************************/

static int binary_search_place_and_route(struct s_placer_opts placer_opts,
		char *place_file, char *net_file, char *arch_file, char *route_file,
		boolean full_stats, boolean verify_binary_search,
		struct s_annealing_sched annealing_sched,
		struct s_router_opts router_opts,
		struct s_det_routing_arch det_routing_arch, t_segment_inf * segment_inf,
		t_timing_inf timing_inf, t_chan_width_dist chan_width_dist,
		t_model *models, t_direct_inf *directs, int num_directs);

static float comp_width(t_chan * chan, float x, float separation);

void post_place_sync(INP int L_num_blocks,
		INOUTP const struct s_block block_list[]);

void free_pb_data(t_pb *pb);

/************************* Subroutine Definitions ****************************/

void place_and_route(enum e_operation operation,
		struct s_placer_opts placer_opts, char *place_file, char *net_file,
		char *arch_file, char *route_file,
		struct s_annealing_sched annealing_sched,
		struct s_router_opts router_opts,
		struct s_det_routing_arch det_routing_arch, t_segment_inf * segment_inf,
		t_timing_inf timing_inf, t_chan_width_dist chan_width_dist,
		struct s_model *models,
		t_direct_inf *directs, int num_directs) {

#ifdef PRINT_BRAM_INFO			
	//zh_lab
	//打印BRAM信息
	print_BRAM_info();
#endif


	//zh_lab
	//创建pin_写频率词典
	pin_dict pin_dict_inits={0};
	update_pin_dict("/home/zhlab/BRAM/s_run/LU8PEEng/src/pin_dict",&pin_dict_inits);

	//创建BRAM phy cell 存储队列
	Array * p_Arrays = (Array *)malloc(MAX_BRAM_NUM * sizeof(Array));
	placer_opts.p_Arrays = p_Arrays;
	//创建phy BRAM集合BRAMS
	BRAMS_phy phy_brams;
	phy_brams.BRAM_num = 0;
	placer_opts.p_phy_brams = &phy_brams;

	//log
	//创建逻辑属性
	BRAMS_log log_brams;
	log_brams.BRAM_num = 0;
	placer_opts.p_log_brams = &log_brams;
	load_log_BRAMS_info("/home/zhlab/BRAM/s_run/LU8PEEng/src/pre_info_src/LU8PEEng_log_pin.info",&log_brams);
	match_log_bram_id(&log_brams);
	//加载pin脚信息
	load_log_pin_info(&pin_dict_inits,&log_brams);
	malloc_write_unit_space(&log_brams);

	/* This routine controls the overall placement and routing of a circuit. */
	char msg[BUFSIZE];
	int width_fac, i;
	boolean success, Fc_clipped;
	float **net_delay = NULL;
	t_slack * slacks = NULL;
	t_chunk net_delay_ch = {NULL, 0, NULL};

	/*struct s_linked_vptr *net_delay_chunk_list_head;*/
	t_ivec **clb_opins_used_locally = NULL; /* [0..num_blocks-1][0..num_class-1] */
	int max_pins_per_clb;
	clock_t begin, end;

	Fc_clipped = FALSE;

	max_pins_per_clb = 0;
	for (i = 0; i < num_types; i++) {
		if (type_descriptors[i].num_pins > max_pins_per_clb) {
			max_pins_per_clb = type_descriptors[i].num_pins;
		}
	}

	if (placer_opts.place_freq == PLACE_NEVER) {
		/* Read the placement from a file */
		read_place(place_file, net_file, arch_file, nx, ny, num_blocks, block);
		sync_grid_to_blocks(num_blocks, block, nx, ny, grid);
	} else {
		assert(
				(PLACE_ONCE == placer_opts.place_freq) || (PLACE_ALWAYS == placer_opts.place_freq));
		begin = clock();
		try_place(placer_opts, annealing_sched, chan_width_dist, router_opts,
				det_routing_arch, segment_inf, timing_inf, directs, num_directs);
		print_place(place_file, net_file, arch_file);
		print_place_pin(place_file,&log_brams);
		end = clock();
#ifdef CLOCKS_PER_SEC
		vpr_printf(TIO_MESSAGE_INFO, "Placement took %g seconds.\n", (float)(end - begin) / CLOCKS_PER_SEC);
#else
		vpr_printf(TIO_MESSAGE_INFO, "Placement took %g seconds.\n", (float)(end - begin) / CLK_PER_SEC);
#endif
	}
	begin = clock();
	post_place_sync(num_blocks, block);

	fflush(stdout);

	if (!router_opts.doRouting)
		return;

	width_fac = router_opts.fixed_channel_width;

	/* If channel width not fixed, use binary search to find min W */
	if (NO_FIXED_CHANNEL_WIDTH == width_fac) {
		g_solution_inf.channel_width = binary_search_place_and_route(placer_opts, place_file, net_file,
				arch_file, route_file, router_opts.full_stats,
				router_opts.verify_binary_search, annealing_sched, router_opts,
				det_routing_arch, segment_inf, timing_inf, chan_width_dist,
				models, directs, num_directs);
	} else {
		g_solution_inf.channel_width = width_fac;
		if (det_routing_arch.directionality == UNI_DIRECTIONAL) {
			if (width_fac % 2 != 0) {
				vpr_printf(TIO_MESSAGE_ERROR, "in pack_place_and_route.c: Given odd chan width (%d) for udsd architecture.\n",
						width_fac);
				exit(1);
			}
		}
		/* Other constraints can be left to rr_graph to check since this is one pass routing */

		/* Allocate the major routing structures. */

		clb_opins_used_locally = alloc_route_structs();

		slacks = alloc_and_load_timing_graph(timing_inf);
		net_delay = alloc_net_delay(&net_delay_ch, clb_net,
					num_nets);

		success = try_route(width_fac, router_opts, det_routing_arch,
				segment_inf, timing_inf, net_delay, slacks, chan_width_dist,
				clb_opins_used_locally, &Fc_clipped, directs, num_directs);

		if (Fc_clipped) {
			vpr_printf(TIO_MESSAGE_WARNING, "Fc_output was too high and was clipped to full (maximum) connectivity.\n");
		}

		if (success == FALSE) {
			vpr_printf(TIO_MESSAGE_INFO, "Circuit is unrouteable with a channel width factor of %d.\n", width_fac);
			vpr_printf(TIO_MESSAGE_INFO, "\n");
			sprintf(msg, "Routing failed with a channel width factor of %d. ILLEGAL routing shown.", width_fac);
		}

		else {
			check_route(router_opts.route_type, det_routing_arch.num_switch, clb_opins_used_locally);
			get_serial_num();

			vpr_printf(TIO_MESSAGE_INFO, "Circuit successfully routed with a channel width factor of %d.\n", width_fac);
			vpr_printf(TIO_MESSAGE_INFO, "\n");

			routing_stats(router_opts.full_stats, router_opts.route_type,
					det_routing_arch.num_switch, segment_inf,
					det_routing_arch.num_segment, det_routing_arch.R_minW_nmos,
					det_routing_arch.R_minW_pmos,
					det_routing_arch.directionality,
					timing_inf.timing_analysis_enabled, net_delay, slacks);

			print_route(route_file);

			if (getEchoEnabled() && isEchoFileEnabled(E_ECHO_ROUTING_SINK_DELAYS)) {
				print_sink_delays(getEchoFileName(E_ECHO_ROUTING_SINK_DELAYS));
			}

			sprintf(msg, "Routing succeeded with a channel width factor of %d.\n\n",
					width_fac);


		}

		init_draw_coords(max_pins_per_clb);
		update_screen(MAJOR, msg, ROUTING, timing_inf.timing_analysis_enabled);
		

		if (timing_inf.timing_analysis_enabled) {
			assert(slacks->slack);

			if (getEchoEnabled() && isEchoFileEnabled(E_ECHO_POST_FLOW_TIMING_GRAPH)) {
				print_timing_graph_as_blif (getEchoFileName(E_ECHO_POST_FLOW_TIMING_GRAPH),
						models);
			}

			if(GetPostSynthesisOption())
			  {
			    verilog_writer();
			  }

			free_timing_graph(slacks);

			assert(net_delay);
			free_net_delay(net_delay, &net_delay_ch);
		}

		fflush(stdout);
	}

	if (clb_opins_used_locally != NULL) {
		for (i = 0; i < num_blocks; i++) {
			free_ivec_vector(clb_opins_used_locally[i], 0,
					block[i].type->num_class - 1);
		}
		free(clb_opins_used_locally);
		clb_opins_used_locally = NULL;
	}

	/* Frees up all the data structure used in vpr_utils. */
	free_port_pin_from_blk_pin();
	free_blk_pin_from_port_pin();

	end = clock();
#ifdef CLOCKS_PER_SEC
	vpr_printf(TIO_MESSAGE_INFO, "Routing took %g seconds.\n", (float) (end - begin) / CLOCKS_PER_SEC);
#else
	vpr_printf(TIO_MESSAGE_INFO, "Routing took %g seconds.\n", (float)(end - begin) / CLK_PER_SEC);
#endif

	/*WMF: cleaning up memory usage */

	/*	if (g_heap_free_head)
		free(g_heap_free_head);
	if (g_trace_free_head)
		free(g_trace_free_head);
	if (g_linked_f_pointer_free_head)
		free(g_linked_f_pointer_free_head);*/
}

static int binary_search_place_and_route(struct s_placer_opts placer_opts,
		char *place_file, char *net_file, char *arch_file, char *route_file,
		boolean full_stats, boolean verify_binary_search,
		struct s_annealing_sched annealing_sched,
		struct s_router_opts router_opts,
		struct s_det_routing_arch det_routing_arch, t_segment_inf * segment_inf,
		t_timing_inf timing_inf, t_chan_width_dist chan_width_dist,
		 t_model *models, t_direct_inf *directs, int num_directs) {

	/* This routine performs a binary search to find the minimum number of      *
	 * tracks per channel required to successfully route a circuit, and returns *
	 * that minimum width_fac.                                                  */

	struct s_trace **best_routing; /* Saves the best routing found so far. */
	int current, low, high, final;
	int max_pins_per_clb, i;
	boolean success, prev_success, prev2_success, Fc_clipped = FALSE;
	char msg[BUFSIZE];
	float **net_delay = NULL;
	t_slack * slacks = NULL;

	t_chunk net_delay_ch = {NULL, 0, NULL};

	/*struct s_linked_vptr *net_delay_chunk_list_head;*/
	t_ivec **clb_opins_used_locally, **saved_clb_opins_used_locally;

	/* [0..num_blocks-1][0..num_class-1] */
	int attempt_count;
	int udsd_multiplier;
	int warnings;

	t_graph_type graph_type;

	/* Allocate the major routing structures. */

	if (router_opts.route_type == GLOBAL) {
		graph_type = GRAPH_GLOBAL;
	} else {
		graph_type = (
				det_routing_arch.directionality == BI_DIRECTIONAL ?
						GRAPH_BIDIR : GRAPH_UNIDIR);
	}

	max_pins_per_clb = 0;
	for (i = 0; i < num_types; i++) {
		max_pins_per_clb = std::max(max_pins_per_clb, type_descriptors[i].num_pins);
	}

	clb_opins_used_locally = alloc_route_structs();
	best_routing = alloc_saved_routing(clb_opins_used_locally,
			&saved_clb_opins_used_locally);

	slacks = alloc_and_load_timing_graph(timing_inf);
	net_delay = alloc_net_delay(&net_delay_ch, clb_net, num_nets);

	/* UDSD by AY Start */
	if (det_routing_arch.directionality == BI_DIRECTIONAL)
		udsd_multiplier = 1;
	else
		udsd_multiplier = 2;
	/* UDSD by AY End */

	if (router_opts.fixed_channel_width != NO_FIXED_CHANNEL_WIDTH) {
		current = router_opts.fixed_channel_width + 5 * udsd_multiplier;
		low = router_opts.fixed_channel_width - 1 * udsd_multiplier;
	} else {
		current = max_pins_per_clb + max_pins_per_clb % 2; /* Binary search part */
		low = -1;
	}

	/* Constraints must be checked to not break rr_graph generator */
	if (det_routing_arch.directionality == UNI_DIRECTIONAL) {
		if (current % 2 != 0) {
			vpr_printf(TIO_MESSAGE_ERROR, "in pack_place_and_route.c: Tried odd chan width (%d) for udsd architecture.\n",
					current);
			exit(1);
		}
	}

	else {
		if (det_routing_arch.Fs % 3) {
			vpr_printf(TIO_MESSAGE_ERROR, "Fs must be three in bidirectional mode.\n");
			exit(1);
		}
	}

	high = -1;
	final = -1;

	attempt_count = 0;

	while (final == -1) {

		vpr_printf(TIO_MESSAGE_INFO, "Using low: %d, high: %d, current: %d\n", low, high, current);
		fflush(stdout);

		/* Check if the channel width is huge to avoid overflow.  Assume the *
		 * circuit is unroutable with the current router options if we're    *
		 * going to overflow.                                                */
		if (router_opts.fixed_channel_width != NO_FIXED_CHANNEL_WIDTH) {
			if (current > router_opts.fixed_channel_width * 4) {
				vpr_printf(TIO_MESSAGE_ERROR, "This circuit appears to be unroutable with the current router options. Last failed at %d.\n", low);
				vpr_printf(TIO_MESSAGE_INFO, "Aborting routing procedure.\n");
				exit(1);
			}
		} else {
			if (current > 1000) {
				vpr_printf(TIO_MESSAGE_ERROR, "This circuit requires a channel width above 1000, probably is not going to route.\n");
				vpr_printf(TIO_MESSAGE_INFO, "Aborting routing procedure.\n");
				exit(1);
			}
		}

		if ((current * 3) < det_routing_arch.Fs) {
			vpr_printf(TIO_MESSAGE_INFO, "Width factor is now below specified Fs. Stop search.\n");
			final = high;
			break;
		}

		if (placer_opts.place_freq == PLACE_ALWAYS) {
			placer_opts.place_chan_width = current;
			try_place(placer_opts, annealing_sched, chan_width_dist,
					router_opts, det_routing_arch, segment_inf, timing_inf,
					directs, num_directs);
		}
		success = try_route(current, router_opts, det_routing_arch, segment_inf,
				timing_inf, net_delay, slacks, chan_width_dist,
				clb_opins_used_locally, &Fc_clipped, directs, num_directs);
		attempt_count++;
		fflush(stdout);
#if 1
		if (success && (Fc_clipped == FALSE)) {
#else
			if (success
					&& (Fc_clipped == FALSE
							|| det_routing_arch.Fc_type == FRACTIONAL))
			{
#endif
			if (current == high) {
				/* Can't go any lower */
				final = current;
			}
			high = current;

			/* If Fc_output is too high, set to full connectivity but warn the user */
			if (Fc_clipped) {
				vpr_printf(TIO_MESSAGE_WARNING, "Fc_output was too high and was clipped to full (maximum) connectivity.\n");
			}

			/* If we're re-placing constantly, save placement in case it is best. */
#if 0
			if (placer_opts.place_freq == PLACE_ALWAYS)
			{
				print_place(place_file, net_file, arch_file);
			}
#endif

			/* Save routing in case it is best. */
			save_routing(best_routing, clb_opins_used_locally,
					saved_clb_opins_used_locally);

			if ((high - low) <= 1 * udsd_multiplier)
				final = high;

			if (low != -1) {
				current = (high + low) / 2;
			} else {
				current = high / 2; /* haven't found lower bound yet */
			}
		} else { /* last route not successful */
			if (success && Fc_clipped) {
				vpr_printf(TIO_MESSAGE_INFO, "Routing rejected, Fc_output was too high.\n");
				success = FALSE;
			}
			low = current;
			if (high != -1) {

				if ((high - low) <= 1 * udsd_multiplier)
					final = high;

				current = (high + low) / 2;
			} else {
				if (router_opts.fixed_channel_width != NO_FIXED_CHANNEL_WIDTH) {
					/* FOR Wneed = f(Fs) search */
					if (low < router_opts.fixed_channel_width + 30) {
						current = low + 5 * udsd_multiplier;
					} else {
						vpr_printf(TIO_MESSAGE_ERROR, "Aborting: Wneed = f(Fs) search found exceedingly large Wneed (at least %d).\n", low);
						exit(1);
					}
				} else {
					current = low * 2; /* Haven't found upper bound yet */
				}
			}
		}
		current = current + current % udsd_multiplier;
	}

	/* The binary search above occassionally does not find the minimum    *
	 * routeable channel width.  Sometimes a circuit that will not route  *
	 * in 19 channels will route in 18, due to router flukiness.  If      *  
	 * verify_binary_search is set, the code below will ensure that FPGAs *
	 * with channel widths of final-2 and final-3 wil not route           *  
	 * successfully.  If one does route successfully, the router keeps    *
	 * trying smaller channel widths until two in a row (e.g. 8 and 9)    *
	 * fail.                                                              */

	if (verify_binary_search) {

		vpr_printf(TIO_MESSAGE_INFO, "\n");
		vpr_printf(TIO_MESSAGE_INFO, "Verifying that binary search found min channel width...\n");

		prev_success = TRUE; /* Actually final - 1 failed, but this makes router */
		/* try final-2 and final-3 even if both fail: safer */
		prev2_success = TRUE;

		current = final - 2;

		while (prev2_success || prev_success) {
			if ((router_opts.fixed_channel_width != NO_FIXED_CHANNEL_WIDTH)
					&& (current < router_opts.fixed_channel_width)) {
				break;
			}
			fflush(stdout);
			if (current < 1)
				break;
			if (placer_opts.place_freq == PLACE_ALWAYS) {
				placer_opts.place_chan_width = current;
				try_place(placer_opts, annealing_sched, chan_width_dist,
						router_opts, det_routing_arch, segment_inf, timing_inf,
						directs, num_directs);
			}
			success = try_route(current, router_opts, det_routing_arch,
					segment_inf, timing_inf, net_delay, slacks,
					chan_width_dist, clb_opins_used_locally, &Fc_clipped, directs, num_directs);

			if (success && Fc_clipped == FALSE) {
				final = current;
				save_routing(best_routing, clb_opins_used_locally,
						saved_clb_opins_used_locally);

				if (placer_opts.place_freq == PLACE_ALWAYS) {
					print_place(place_file, net_file, arch_file);
				}
			}

			prev2_success = prev_success;
			prev_success = success;
			current--;
			if (det_routing_arch.directionality == UNI_DIRECTIONAL) {
				current--; /* width must be even */
			}
		}
	}

	/* End binary search verification. */
	/* Restore the best placement (if necessary), the best routing, and  *
	 * * the best channel widths for final drawing and statistics output.  */
	init_chan(final, chan_width_dist);
#if 0
	if (placer_opts.place_freq == PLACE_ALWAYS)
	{
		vpr_printf(TIO_MESSAGE_INFO, "Reading best placement back in.\n");
		placer_opts.place_chan_width = final;
		read_place(place_file, net_file, arch_file, placer_opts,
				router_opts, chan_width_dist, det_routing_arch,
				segment_inf, timing_inf);
	}
#endif
	free_rr_graph();

	build_rr_graph(graph_type, num_types, type_descriptors, nx, ny, grid,
			chan_width_x[0], NULL, det_routing_arch.switch_block_type,
			det_routing_arch.Fs, det_routing_arch.num_segment,
			det_routing_arch.num_switch, segment_inf,
			det_routing_arch.global_route_switch,
			det_routing_arch.delayless_switch, timing_inf,
			det_routing_arch.wire_to_ipin_switch, router_opts.base_cost_type,
			directs, num_directs, FALSE,
			&warnings);

	restore_routing(best_routing, clb_opins_used_locally,
			saved_clb_opins_used_locally);
	check_route(router_opts.route_type, det_routing_arch.num_switch,
			clb_opins_used_locally);
	get_serial_num();
	if (Fc_clipped) {
		vpr_printf(TIO_MESSAGE_WARNING, "Best routing Fc_output too high, clipped to full (maximum) connectivity.\n");
	}
	vpr_printf(TIO_MESSAGE_INFO, "Best routing used a channel width factor of %d.\n", final);

	routing_stats(full_stats, router_opts.route_type,
			det_routing_arch.num_switch, segment_inf,
			det_routing_arch.num_segment, det_routing_arch.R_minW_nmos,
			det_routing_arch.R_minW_pmos, det_routing_arch.directionality,
			timing_inf.timing_analysis_enabled, net_delay, slacks);

	print_route(route_file);

	if (getEchoEnabled() && isEchoFileEnabled(E_ECHO_ROUTING_SINK_DELAYS)) {
		print_sink_delays(getEchoFileName(E_ECHO_ROUTING_SINK_DELAYS));
	}

	init_draw_coords(max_pins_per_clb);
	sprintf(msg, "Routing succeeded with a channel width factor of %d.", final);
	update_screen(MAJOR, msg, ROUTING, timing_inf.timing_analysis_enabled);

	if (timing_inf.timing_analysis_enabled) {
		if (getEchoEnabled() && isEchoFileEnabled(E_ECHO_POST_FLOW_TIMING_GRAPH)) {
			print_timing_graph_as_blif (getEchoFileName(E_ECHO_POST_FLOW_TIMING_GRAPH), models);
		}
		
		if(GetPostSynthesisOption())
		  {
		    verilog_writer();
		  }

		free_timing_graph(slacks);
		free_net_delay(net_delay, &net_delay_ch);
	}
	
	for (i = 0; i < num_blocks; i++) {
		free_ivec_vector(clb_opins_used_locally[i], 0,
				block[i].type->num_class - 1);
	}
	free(clb_opins_used_locally);
	clb_opins_used_locally = NULL;

	free_saved_routing(best_routing, saved_clb_opins_used_locally);
	fflush(stdout);

	return (final);

}

void init_chan(int cfactor, t_chan_width_dist chan_width_dist) {

	/* Assigns widths to channels (in tracks).  Minimum one track          * 
	 * per channel.  io channels are io_rat * maximum in interior          * 
	 * tracks wide.  The channel distributions read from the architecture  *
	 * file are scaled by cfactor.                                         */

	float x, separation, chan_width_io;
	int nio, i;
	t_chan chan_x_dist, chan_y_dist;

	chan_width_io = chan_width_dist.chan_width_io;
	chan_x_dist = chan_width_dist.chan_x_dist;
	chan_y_dist = chan_width_dist.chan_y_dist;

	/* io channel widths */

	nio = (int) floor(cfactor * chan_width_io + 0.5);
	if (nio == 0)
		nio = 1; /* No zero width channels */

	chan_width_x[0] = chan_width_x[ny] = nio;
	chan_width_y[0] = chan_width_y[nx] = nio;

	if (ny > 1) {
		separation = 1. / (ny - 2.); /* Norm. distance between two channels. */
		x = 0.; /* This avoids div by zero if ny = 2. */
		chan_width_x[1] = (int) floor(
				cfactor * comp_width(&chan_x_dist, x, separation) + 0.5);

		/* No zero width channels */
		chan_width_x[1] = std::max(chan_width_x[1], 1);

		for (i = 1; i < ny - 1; i++) {
			x = (float) i / ((float) (ny - 2.));
			chan_width_x[i + 1] = (int) floor(
					cfactor * comp_width(&chan_x_dist, x, separation) + 0.5);
			chan_width_x[i + 1] = std::max(chan_width_x[i + 1], 1);
		}
	}

	if (nx > 1) {
		separation = 1. / (nx - 2.); /* Norm. distance between two channels. */
		x = 0.; /* Avoids div by zero if nx = 2. */
		chan_width_y[1] = (int) floor(
				cfactor * comp_width(&chan_y_dist, x, separation) + 0.5);

		chan_width_y[1] = std::max(chan_width_y[1], 1);

		for (i = 1; i < nx - 1; i++) {
			x = (float) i / ((float) (nx - 2.));
			chan_width_y[i + 1] = (int) floor(
					cfactor * comp_width(&chan_y_dist, x, separation) + 0.5);
			chan_width_y[i + 1] = std::max(chan_width_y[i + 1], 1);
		}
	}
#ifdef VERBOSE
	vpr_printf(TIO_MESSAGE_INFO, "\n");
	vpr_printf(TIO_MESSAGE_INFO, "chan_width_x:\n");
	for (i = 0; i <= ny; i++)
		vpr_printf(TIO_MESSAGE_INFO, "%d  ", chan_width_x[i]);
	vpr_printf(TIO_MESSAGE_INFO, "\n");
	vpr_printf(TIO_MESSAGE_INFO, "chan_width_y:\n");
	for (i = 0; i <= nx; i++)
		vpr_printf(TIO_MESSAGE_INFO, "%d  ", chan_width_y[i]);
	vpr_printf(TIO_MESSAGE_INFO, "\n");
#endif

}

static float comp_width(t_chan * chan, float x, float separation) {

	/* Return the relative channel density.  *chan points to a channel   *
	 * functional description data structure, and x is the distance      *   
	 * (between 0 and 1) we are across the chip.  separation is the      *   
	 * distance between two channels, in the 0 to 1 coordinate system.   */

	float val;

	switch (chan->type) {

	case UNIFORM:
		val = chan->peak;
		break;

	case GAUSSIAN:
		val = (x - chan->xpeak) * (x - chan->xpeak)
				/ (2 * chan->width * chan->width);
		val = chan->peak * exp(-val);
		val += chan->dc;
		break;

	case PULSE:
		val = (float) fabs((double) (x - chan->xpeak));
		if (val > chan->width / 2.) {
			val = 0;
		} else {
			val = chan->peak;
		}
		val += chan->dc;
		break;

	case DELTA:
		val = x - chan->xpeak;
		if (val > -separation / 2. && val <= separation / 2.)
			val = chan->peak;
		else
			val = 0.;
		val += chan->dc;
		break;

	default:
		vpr_printf(TIO_MESSAGE_ERROR, "in comp_width: Unknown channel type %d.\n", chan->type);
		exit(1);
		break;
	}

	return (val);
}

/* After placement, logical pins for blocks, and nets must be updated to correspond with physical pins of type */
/* This function should only be called once */
void post_place_sync(INP int L_num_blocks,
		INOUTP const struct s_block block_list[]) {
	int iblk, j, k, inet;
	t_type_ptr type;
	int max_num_block_pins;

	/* Go through each block */
	for (iblk = 0; iblk < L_num_blocks; ++iblk) {
		type = block[iblk].type;
		assert(type->num_pins % type->capacity == 0);
		max_num_block_pins = type->num_pins / type->capacity;
		/* Logical location and physical location is offset by z * max_num_block_pins */
		/* Sync blocks and nets */
		for (j = 0; j < max_num_block_pins; j++) {
			inet = block[iblk].nets[j];
			if (inet != OPEN && block[iblk].z > 0) {
				assert(
						block[iblk]. nets[j + block[iblk].z * max_num_block_pins] == OPEN);
				block[iblk].nets[j + block[iblk].z * max_num_block_pins] =
						block[iblk].nets[j];
				block[iblk].nets[j] = OPEN;
				for (k = 0; k <= clb_net[inet].num_sinks; k++) {
					if (clb_net[inet].node_block[k] == iblk && clb_net[inet]. node_block_pin[k] == j) {
						clb_net[inet].node_block_pin[k] = j
								+ block[iblk].z * max_num_block_pins;
						break;
					}
				}
				assert(k <= clb_net[inet].num_sinks);
			}
		}
	}
}

void free_pb_data(t_pb *pb) {
	int i, j;
	const t_pb_type *pb_type;
	t_rr_node *temp;

	if (pb == NULL || pb->name == NULL) {
		return;
	}

	pb_type = pb->pb_graph_node->pb_type;

	/* free existing rr graph for pb */
	if (pb->rr_graph) {
		temp = rr_node;
		rr_node = pb->rr_graph;
		num_rr_nodes = pb->pb_graph_node->total_pb_pins;
		free_rr_graph();
		rr_node = temp;
	}

	if (pb_type->num_modes > 0) {
		/* Free children of pb */
		for (i = 0; i < pb_type->modes[pb->mode].num_pb_type_children; i++) {
			for (j = 0; j < pb_type->modes[pb->mode].pb_type_children[i].num_pb;
					j++) {
				if (pb->child_pbs[i]) {
					free_pb_data(&pb->child_pbs[i][j]);
				}
			}
		}
	}

	/* Frees all the pb data structures.                                 */
	if (pb->name) {
		free(pb->name);
		if (pb->child_pbs) {
			free(pb->child_pbs);
		}
	}
}


static void load_log_BRAMS_info(char * info_path ,BRAMS_log * p_log_brams)
{
	int MAX_LINE = 1000;
	char buf[MAX_LINE];  /*缓冲区*/
	FILE *fp;            /*文件指针*/
	int len;             /*行字符个数*/
	int flag = 0;
	int MAT = 1;
	int Row_array = 0;
    int Col_array = 0;
	if((fp = fopen(info_path,"r")) == NULL)
	{
		perror("fail to read");
		exit (1) ;
	}
	while (fgets(buf, MAX_LINE, (FILE*)fp) != NULL)
	{
		
		// vpr_printf(TIO_MESSAGE_INFO,"%s\n",buf);
		int i;
		int choose = -1;
		IString istr;
		if (  Split(buf," ",&istr) )
		{
			// vpr_printf(TIO_MESSAGE_INFO,"%s\n",istr.str[0]);
			choose = char_to_int(istr.str[0]);
			switch ( choose )            
			{
				case 1: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"1\n");
				}break;  
				case 2: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].mode,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"2\n");
				}break;   
				case 3: {
					(*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].is_dp = char_to_int(istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"3\n");
				}break;  
				case 4: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].name_port_a,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"4\n");
				}break;   
				case 5: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[0].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"5\n");
				}break;  
				case 6: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[1].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"6\n");
				}break;   
				case 7: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[2].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"7\n");
				}break;  
				case 8: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[3].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"8\n");
				}break;   
				case 9: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[4].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"9\n");
				}break;  
				case 10: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[5].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"10\n");
				}break;   
				case 11: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[6].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"11\n");
				}break;  
				case 12: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[7].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"12\n");
				}break;   
				case 13: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[8].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"13\n");
				}break;  
				case 14: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[9].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"14\n");
				}break;   
				case 15: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[10].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"15\n");
				}break;  
				case 16: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[11].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"16\n");
				}break;   
				case 17: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[12].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"17\n");
				}break;  
				case 18: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[13].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"18\n");
				}break;   
				case 19: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].A_port.pins[14].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"19\n");
				}break;  
				case 20: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].name_port_b,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"20\n");
				}break;   
				case 21: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[0].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"21\n");
				}break;  
				case 22: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[1].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"22\n");
				}break;   
				case 23: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[2].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"23\n");
				}break;  
				case 24: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[3].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"24\n");
				}break;   
				case 25: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[4].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"25\n");
				}break;  
				case 26: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[5].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"26\n");
				}break;   
				case 27: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[6].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"27\n");
				}break;  
				case 28: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[7].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"28\n");
				}break;   
				case 29: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[8].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"29\n");
				}break;  
				case 30: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[9].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"30\n");
				}break;  
				case 31: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[10].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"31\n");
				}break;  
				case 32: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[11].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"32\n");
				}break;   
				case 33: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[12].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"33\n");
				}break;  
				case 34: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[13].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"34\n");
				}break;  
				case 35: {
					strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].B_port.pins[14].pin_name,istr.str[2]);
					// vpr_printf(TIO_MESSAGE_INFO,"35\n");
					(*p_log_brams).BRAM_num ++;
				}break; 
			}

			for (i=0;i<istr.num;i++)
				free(istr.str[i]);
			free(istr.str);
		}
		else
		{
			vpr_printf(TIO_MESSAGE_INFO,"str_to_long_int faild\n");
		}
		int find_i = 0;
		for(find_i = 0;find_i < (*p_log_brams).BRAM_num;find_i ++ )
				{
					char_replace((*p_log_brams).BRAM_LIST[find_i].name,'\n','\000');
				}

	}
}
		// if (flag == MAT )
        // {
		// 	Col_array = 0;
		// 	int i;
		// 	IString istr;
		// 	if (  Split(buf," ",&istr) )
		// 	{
		// 		for (i=0;i<MAX_COL_B;i++)
		// 		{
		// 			long int write_num = char_to_long_int(istr.str[i]);
		// 			*(p_Array+64*Row_array+Col_array) = write_num;
		// 			Col_array ++;
		// 			// vpr_printf(TIO_MESSAGE_INFO,"%ld  ",write_num);
		// 		}
		// 			// vpr_printf(TIO_MESSAGE_INFO,"\n");
		// 		for (i=0;i<istr.num;i++)
		// 			free(istr.str[i]);
		// 		free(istr.str);
		// 	}
		// 	else
		// 	{
		// 		vpr_printf(TIO_MESSAGE_INFO,"str_to_long_int faild\n");
		// 	}
		// 	Row_array ++;
		// 	// vpr_printf(TIO_MESSAGE_INFO,"%s\n",buf);
		// }
		// if (buf[1]=='a')				//找到mat
        // {
        //     flag = MAT;
        // }







//zh_lab   查询逻辑pin脚信息
static void load_log_pin_info(pin_dict * p_dict, BRAMS_log * p_log_brams)
{
	int i_bram_num = 0;
	int i_pin_num = 0;
	int i_pin_dict = 0;
	for (i_bram_num = 0;i_bram_num<(*p_log_brams).BRAM_num;i_bram_num++)
	{
		if ((*p_log_brams).BRAM_LIST[i_bram_num].is_dp != 1)
		{
			for (i_pin_num = 0;i_pin_num<MAX_ADDR_NUM;i_pin_num++)
			{
				char tmp_pin_name[MAX_name_len];
				char_replace((*p_log_brams).BRAM_LIST[i_bram_num].A_port.pins[i_pin_num].pin_name,'\n','\000');
				strcpy(tmp_pin_name,(*p_log_brams).BRAM_LIST[i_bram_num].A_port.pins[i_pin_num].pin_name);
				if(strlen(tmp_pin_name)>6)
				{
					for(i_pin_dict = 0 ;i_pin_dict < (*p_dict).pin_num;i_pin_dict++)
					{
						if(strcmp(tmp_pin_name,(*p_dict).pins[i_pin_dict].pin_name)==0)
						{
							((*p_log_brams).BRAM_LIST[i_bram_num].A_port.pins[i_pin_num].pin_ratio) = ((*p_dict).pins[i_pin_dict].pin_ratio);
						}
							
					}
				}
			}
			(*p_log_brams).BRAM_LIST[i_bram_num].C_port=(*p_log_brams).BRAM_LIST[i_bram_num].A_port;

		}
		else
		{
			for (i_pin_num = 0;i_pin_num<MAX_ADDR_NUM;i_pin_num++)
			{
				char tmp_pin_name[MAX_name_len];
				char_replace((*p_log_brams).BRAM_LIST[i_bram_num].A_port.pins[i_pin_num].pin_name,'\n','\000');
				strcpy(tmp_pin_name,(*p_log_brams).BRAM_LIST[i_bram_num].A_port.pins[i_pin_num].pin_name);
				if(strlen(tmp_pin_name)>6)
				{
					for(i_pin_dict = 0 ;i_pin_dict < (*p_dict).pin_num;i_pin_dict++)
					{
						if(strcmp(tmp_pin_name,(*p_dict).pins[i_pin_dict].pin_name)==0)
						{
							((*p_log_brams).BRAM_LIST[i_bram_num].A_port.pins[i_pin_num].pin_ratio) = ((*p_dict).pins[i_pin_dict].pin_ratio);
						}
							
					}
				}
			}
			for (i_pin_num = 0;i_pin_num<MAX_ADDR_NUM;i_pin_num++)
			{
				char tmp_pin_name[MAX_name_len];
				char_replace((*p_log_brams).BRAM_LIST[i_bram_num].B_port.pins[i_pin_num].pin_name,'\n','\000');
				strcpy(tmp_pin_name,(*p_log_brams).BRAM_LIST[i_bram_num].B_port.pins[i_pin_num].pin_name);
				if(strlen(tmp_pin_name)>6)
				{
					for(i_pin_dict = 0 ;i_pin_dict < (*p_dict).pin_num;i_pin_dict++)
					{
						if(strcmp(tmp_pin_name,(*p_dict).pins[i_pin_dict].pin_name)==0)
						{
							((*p_log_brams).BRAM_LIST[i_bram_num].B_port.pins[i_pin_num].pin_ratio) = ((*p_dict).pins[i_pin_dict].pin_ratio);
						}
							
					}
				}
			}
			 for (i_pin_num = 0;i_pin_num<MAX_ADDR_NUM;i_pin_num++)
			 {
				 strcpy((*p_log_brams).BRAM_LIST[i_bram_num].C_port.pins[i_pin_num].pin_name,(*p_log_brams).BRAM_LIST[i_bram_num].A_port.pins[i_pin_num].pin_name);
				 float tmp_A = (*p_log_brams).BRAM_LIST[i_bram_num].A_port.pins[i_pin_num].pin_ratio;
				 float tmp_B = (*p_log_brams).BRAM_LIST[i_bram_num].B_port.pins[i_pin_num].pin_ratio;
				 (*p_log_brams).BRAM_LIST[i_bram_num].C_port.pins[i_pin_num].used = -1;
				 if (tmp_A>tmp_B)
				 {
					(*p_log_brams).BRAM_LIST[i_bram_num].C_port.pins[i_pin_num].pin_ratio = tmp_A;
				 }
				 else
				 {
					 (*p_log_brams).BRAM_LIST[i_bram_num].C_port.pins[i_pin_num].pin_ratio = tmp_B;
				 }
				 
			 }
			// (*p_log_brams).BRAM_LIST[i_bram_num].C_port=(*p_log_brams).BRAM_LIST[i_bram_num].A_port;
		}
		// for (i_pin_num = 0;i_pin_num<MAX_ADDR_NUM;i_pin_num++)
		// {
		// 	char tmp_pin_name[MAX_name_len];
		// 	char_replace((*p_log_brams).BRAM_LIST[i_bram_num].A_port.pins[i_pin_num].pin_name,'\n','\000');
		// 	strcpy(tmp_pin_name,(*p_log_brams).BRAM_LIST[i_bram_num].A_port.pins[i_pin_num].pin_name);
		// 	if(strlen(tmp_pin_name)>6)
		// 	{
		// 		for(i_pin_dict = 0 ;i_pin_dict < (*p_dict).pin_num;i_pin_dict++)
		// 		{
		// 			if(strcmp(tmp_pin_name,(*p_dict).pins[i_pin_dict].pin_name)==0)
		// 			{
		// 				((*p_log_brams).BRAM_LIST[i_bram_num].A_port.pins[i_pin_num].pin_ratio) = ((*p_dict).pins[i_pin_dict].pin_ratio);
		// 			}
						
		// 		}
		// 	}
		// }
			
	}
}




//zh_lab
//根据逻辑信息 mode 分配最小写单元空间
static void malloc_write_unit_space(BRAMS_log * p_log_brams)
{
	int log_num = 0;
	for (log_num = 0;log_num < (*p_log_brams).BRAM_num ;log_num++)
	{
		char * tmp_mode;
		tmp_mode = (*p_log_brams).BRAM_LIST[log_num].mode;
		int i;
		IString istr;
		int add_range = 0;
		int con_width = 0;
		int vaild_add_num = 0;
		if (  Split(tmp_mode,"_",&istr) )
			{
				add_range = char_to_int(istr.str[1]);
			 	(*p_log_brams).BRAM_LIST[log_num].add_range = add_range;
				vpr_printf(TIO_MESSAGE_INFO,"range %d \n",add_range);
				con_width = char_to_int(istr.str[2]);
				(*p_log_brams).BRAM_LIST[log_num].con_width = con_width;
				vpr_printf(TIO_MESSAGE_INFO,"con_width %d \n",con_width);
				vaild_add_num = log_2_n(add_range);
				(*p_log_brams).BRAM_LIST[log_num].valid_add_num = vaild_add_num;

				for (i=0;i<istr.num;i++)
					free(istr.str[i]);
				free(istr.str);
			}
			else
			{
				vpr_printf(TIO_MESSAGE_INFO,"str_to_long_int faild\n");
			}
		
		BRAM_write_unit * p_write_unit_vector;
    	p_write_unit_vector = (BRAM_write_unit *)malloc(((*p_log_brams).BRAM_LIST[log_num].add_range)*sizeof(BRAM_write_unit));
		(*p_log_brams).BRAM_LIST[log_num].p_write_unit_vector = p_write_unit_vector;
		buile_locat_map((*p_log_brams).BRAM_LIST[log_num].p_write_unit_vector,(*p_log_brams).BRAM_LIST[log_num].con_width,(*p_log_brams).BRAM_LIST[log_num].add_range);
	}
}

static BRAM_write_unit *  buile_locat_map(BRAM_write_unit * p_write_unit_vector,int width,int add_range )
{
    int row_num = CONFIG_WIDTH / width;
    int address_range = add_range;
    int tmp_i ;
    int tmp_j ;
    int tmp_row = 0;
    int tmp_col = 0;
    int tmp_row_num = 0;
    for (tmp_i = 0; tmp_i < address_range ;tmp_i++)
        {
            if (tmp_row_num == row_num)
            {
                tmp_row++;
                tmp_row_num = 0;
            }
            (*(p_write_unit_vector+tmp_i)).unit_num = width;
            (*(p_write_unit_vector+tmp_i)).address = tmp_i;
            (*(p_write_unit_vector+tmp_i)).dirty = false;
            (*(p_write_unit_vector+tmp_i)).write_num = 0;
            (*(p_write_unit_vector+tmp_i)).relocation_table_x = tmp_row;
            int begin_j =  width * tmp_row_num;
            int tmp_y = 0;
            for (tmp_j = begin_j;tmp_j < (begin_j+width) ;tmp_j++)
                {
                    (*(p_write_unit_vector+tmp_i)).relocation_table_y[tmp_y++] = tmp_j; 
                }
            tmp_row_num++;
        }
    return p_write_unit_vector;
}



static void match_log_bram_id(BRAMS_log *p_log_brams)
{
	int iblk;
	int counter = 0;
	for (iblk = 0; iblk < num_blocks; iblk++) {
		int find_i = 0;
		if(block[iblk].type->index == 4)
		{
			for(find_i = 0;find_i < (*p_log_brams).BRAM_num;find_i ++ )
				{
					if(strcmp(block[iblk].name,(*p_log_brams).BRAM_LIST[find_i].name)==0)
					{
						block[iblk].log_bram_ID = find_i;
						counter++;
					}
				}

			// strcpy((*p_log_brams).BRAM_LIST[(*p_log_brams).BRAM_num].name,block[iblk].name);
			//匹配
			// block[iblk].log_bram_ID = (*p_log_brams).BRAM_num;
			// (*p_log_brams).BRAM_num ++;
			// vpr_printf(TIO_MESSAGE_INFO,"%s",block[iblk].name);
		}
	}
	vpr_printf(TIO_MESSAGE_INFO," find_ BRAMS %d \n ",counter);
}