void try_place(struct s_placer_opts placer_opts,
		struct s_annealing_sched annealing_sched,
		t_chan_width_dist chan_width_dist, struct s_router_opts router_opts,
		struct s_det_routing_arch det_routing_arch, t_segment_inf * segment_inf,
		t_timing_inf timing_inf, t_direct_inf *directs, int num_directs);


//***********************************
// zh_lab
int Split(char *src, char *delim, IString *istr);
float char_to_float(char *test);
long int char_to_long_int(char *test);
int char_to_int(char *test);
int log_2_n(int old);
int zh_pow(int x, int y);
long int zh_pow_long(long int x, long int y);
void char_replace(char *S, char A, char B);
static void readTxt_mem(char *mem_path, long int *p_Array);
void update_mem_phy(BRAMS_phy *p_phy_brams, Array_s *p_Arrays, char *p_mem_path);
int d_2_b(int n, int bit_num, int *A);