void try_place(struct s_placer_opts placer_opts,
		struct s_annealing_sched annealing_sched,
		t_chan_width_dist chan_width_dist, struct s_router_opts router_opts,
		struct s_det_routing_arch det_routing_arch, t_segment_inf * segment_inf,
		t_timing_inf timing_inf, t_direct_inf *directs, int num_directs);

//zh_lab
void print_BRAM_info();
void update_mem_phy(BRAMS_phy * p_phy_brams,Array * p_Arrays);
void update_pin_dict(char * pin_dict_path,pin_dict * p_pin_dict_inits);
float char_to_float(char * test);
long int char_to_long_int(char * test);
int Split(char *src, char *delim, IString* istr);
int char_to_int(char * test);
void char_replace(char * S, char A ,char B);
int log_2_n(int old);
int zh_pow(int x,int y);
long int zh_pow_long(long int x,long int y);
int d_2_b(int n,int *A);