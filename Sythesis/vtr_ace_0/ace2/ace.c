#include <stdio.h>

#include "ace.h"
#include "io_ace.h"
#include "blif.h"
#include "cycle.h"
#include "sim.h"
#include "bdd.h"
#include "depth.h"
#include "cube.h"

// ABC Headers
#include "abc.h"
#include "main.h"
#include "io.h"

//#include "vecInt.h"

#include <string.h>

st_table * ace_info_hash_table;

void print_status(Abc_Ntk_t * ntk) {
	int i;
	Abc_Obj_t * obj;

	Abc_NtkForEachNode(ntk, obj, i)
	{
		Ace_Obj_Info_t * info = Ace_ObjInfo(obj);
		switch (info->status) {
		case ACE_UNDEF:
			printf("%d: UNDEFINED\n", i);
			break;
		case ACE_DEF:
			printf("%d: DEFINED\n", i);
			break;
		case ACE_SIM:
			printf("%d: SIM\n", i);
			break;
		case ACE_NEW:
			printf("%d: NEW\n", i);
			break;
		case ACE_OLD:
			printf("%d: OLD\n", i);
			break;
		}
	}
}

void alloc_and_init_activity_info(Abc_Ntk_t * ntk) {
	Vec_Ptr_t * node_vec;
	Abc_Obj_t * obj_ptr;
	int i;

	node_vec = Abc_NtkDfsSeq(ntk);
	Vec_PtrForEachEntry(node_vec, obj_ptr, i)
	{
		Ace_Obj_Info_t * info = Ace_ObjInfo(obj_ptr);
		info->values = NULL;
		info->status = ACE_UNDEF;
		info->num_toggles = 0;
		info->num_ones = 0;
	}
	Vec_PtrFree(node_vec);
}

void ace_update_latch_probs(Abc_Ntk_t * ntk) {
	Abc_Obj_t * obj_ptr;
	Abc_Obj_t * fanin_ptr;
	Abc_Obj_t * fanout_ptr;
	Ace_Obj_Info_t * fanin_info;
	Ace_Obj_Info_t * fanout_info;
	int i;

	Abc_NtkForEachLatch(ntk, obj_ptr, i)
	{
		fanin_ptr = Abc_ObjFanin0(obj_ptr);
		fanout_ptr = Abc_ObjFanout0(obj_ptr);

		fanin_info = Ace_ObjInfo(fanin_ptr);
		fanout_info = Ace_ObjInfo(fanout_ptr);

		fanout_info->static_prob = fanin_info->static_prob;
		fanout_info->switch_prob = fanin_info->switch_prob;
		fanout_info->status = fanin_info->status;
	}
}

void print_node_bdd(Abc_Ntk_t * ntk) {
	Abc_Obj_t * obj;
	int i;

	Abc_NtkForEachNode(ntk, obj, i)
	{
		DdNode * node = obj->pData;

		printf("Object: %d\n", obj->Id);
		fflush(0);
		//printf("Fanin: %d\n", Abc_ObjFaninNum(obj)); fflush(0);
		while (1) {
			if (node == Cudd_ReadOne(ntk->pManFunc)) {
				//printf("one!\n");
				break;
			} else if (node == Cudd_ReadLogicZero(ntk->pManFunc)) {
				//printf("zero!\n");
				break;
			}

			printf("\tVar: %hd (%08x)\n", Cudd_Regular(node)->index,
					(unsigned int) node);
			fflush(0);

			DdNode * first_node;
			Cudd_FirstNode(ntk->pManFunc, node, &first_node);
			node = Cudd_E(node);

		}
	}
}

void print_nodes(Vec_Ptr_t * nodes) {
	Abc_Obj_t * obj;
	int i;

	printf("Printing Nodes\n");
	Vec_PtrForEachEntry(nodes, obj, i)
	{
		printf("\t%d. %d-%d-%s\n", i, Abc_ObjId(obj), Abc_ObjType(obj),
				Abc_ObjName(obj));
	}
	fflush(0);
}

int ace_calc_activity(Abc_Ntk_t * ntk, int num_vectors,short * P_Find_mark_Vec,int * P_Dict) {
	int error = 0;
	Vec_Ptr_t * nodes_all;
	Vec_Ptr_t * nodes_logic;
	Vec_Ptr_t * next_state_node_vec;
	Vec_Ptr_t * latches_in_cycles_vec;
	Abc_Obj_t * obj;
	int i, j;
	Ace_Obj_Info_t * info;
	// *(P_Find_mark_Vec+1) = 255;
	// printf("\nzh _ \t\t %d\n",*(P_Find_mark_Vec+1));
	//Build BDD
	Abc_NtkSopToBdd(ntk);

	nodes_all = Abc_NtkDfsSeq(ntk);
	nodes_logic = Abc_NtkDfs(ntk, TRUE);

	//print_nodes(nodes_logic);

	Vec_PtrForEachEntry(nodes_all, obj, i)
	{
		info = Ace_ObjInfo(obj);
		info->status = ACE_UNDEF;
	}

	Abc_NtkForEachPi(ntk, obj, i)
	{
		info = Ace_ObjInfo(obj);
		assert(info->static_prob >= 0 && info->static_prob <= 1.0);
		assert(info->switch_prob >= 0 && info->switch_prob <= 1.0);
		assert(info->switch_act >= 0 && info->switch_act <= 1.0);
		assert(info->switch_prob <= 2.0 * (1.0 - info->static_prob));
		assert(info->switch_prob <= 2.0 * info->static_prob);
		info->status = ACE_DEF;
	}

	latches_in_cycles_vec = latches_in_cycles(ntk);
	printf("%d/%d latches are part of cycle(s)\n", latches_in_cycles_vec->nSize,
			Abc_NtkLatchNum(ntk));
	fflush(0);

	//if (latches_in_cycles_vec->nSize)
	if (TRUE) {
		//print_status(ntk);

		printf("Stage 1: Simulating Probabilities...\n");
		fflush(0);

		next_state_node_vec = Abc_NtkDfsSeq(ntk);

		//print_nodes(next_state_node_vec);
		//zh_lab
		ace_sim_activities(ntk, next_state_node_vec, num_vectors, 0.05,P_Find_mark_Vec,P_Dict);
		//ace_sim_activities(ntk, nodes_logic, num_vectors, 0.05);

		ace_update_latch_probs(ntk);

		Vec_PtrFree(next_state_node_vec);
	}

	//print_status(ntk);
	printf("Stage 2: Computing Probabilities...\n");
	fflush(0);
	// Currently this stage does nothing

#if 0
	ace_bdd_get_literals (ntk, &leaves, &literals);

	i = 0;
	while(1)
	{
		//printf("Calc Iteration = %d\n", i++); fflush(0);
		if (ace_bdd_build_network_bdds(ntk, leaves, literals, ACE_MAX_BDD_SIZE, ACE_MIN_BDD_PROB) < 1)
		{
			break;
		}
		ace_update_latch_static_probs(ntk);
		ace_update_latch_switch_probs(ntk);
	}
	st_free_table(leaves);
	Vec_PtrFree(literals);
#endif

	/*------------- Computing Register Output Activities. ---------------------*/
	printf("Stage 3: Computing Register Output Activities...\n");
	fflush(0);
	Abc_NtkForEachLatchOutput(ntk, obj, i)
	{
		Ace_Obj_Info_t * info = Ace_ObjInfo(obj);

		info->switch_act = info->switch_prob;
		assert(info->switch_act >= 0.0);
	}
	Abc_NtkForEachPi(ntk, obj, i)
	{
		Ace_Obj_Info_t * info = Ace_ObjInfo(obj);
		assert(info->switch_act >= 0.0);
	}

	/*------------- Calculate switching activities. ---------------------*/
	printf("Stage 4: Computing Switching Activities...\n");
	fflush(0);

	/* Do latches first, then logic after */
	Vec_PtrForEachEntry(nodes_all, obj, i)
	{
		Ace_Obj_Info_t * info = Ace_ObjInfo(obj);

		switch (Abc_ObjType(obj)) {
		case ABC_OBJ_PI:
		case ABC_OBJ_BO:
		case ABC_OBJ_LATCH:
			info->switch_act = info->switch_prob;
			break;

		default:
			break;
		}
	}

	Vec_PtrForEachEntry(nodes_logic, obj, i)
	{
		Ace_Obj_Info_t * info = Ace_ObjInfo(obj);
		//Ace_Obj_Info_t * fanin_info;

		assert(Abc_ObjType(obj) == ABC_OBJ_NODE);

		if (Abc_ObjFaninNum(obj) < 1) {
			info->switch_act = 0.0;
			continue;
		} else {
			Vec_Ptr_t * literals = Vec_PtrAlloc(0);
			Abc_Obj_t * fanin;

			assert(obj->Type == ABC_OBJ_NODE);

			Abc_ObjForEachFanin(obj, fanin, j)
			{
				Vec_PtrPush(literals, fanin);
			}
			info->switch_act = ace_bdd_calc_switch_act(ntk->pManFunc, obj,
					literals);
			Vec_PtrFree(literals);
		}
		assert(info->switch_act >= 0);
	}

	return error;
}

inline Ace_Obj_Info_t * Ace_ObjInfo(Abc_Obj_t * obj) {
	Ace_Obj_Info_t * info;

	if (st_lookup(ace_info_hash_table, (char *) obj, (char **) &info)) {
		return info;
	}
	assert(0);
}

void prob_epsilon_fix(double * d) {
	if (*d < 0) {
		assert(*d > 0 - EPSILON);
		*d = 0;
	} else if (*d > 1) {
		assert(*d < 1 + EPSILON);
		*d = 1.;
	}
}


// zh_lab 修改find_flag 
void mark_find_flag(Abc_Obj_t * obj)
	{
		// printf("  %d /n",obj->Id);
		obj->find_flag = 1;
	}
// lab_end



// zh_lab 读取find_flag信息
int 
read_find_flag(char * File_path,char * Find_vec)
	{
		FILE *fp;
		char buff[255];
		bool add_get = 0;
		int find_num = 0;
		// char find_vec[1000][255];
		fp = fopen(File_path,"r");
		if( fp == NULL)
        {
                printf("info_read_faild\n\n\n");
                return -2;
        }
		else
		{
			// printf("address_begin:");
			while(fscanf(fp,"%s",buff),!feof(fp))
			{
				// printf("%s\n", buff );
				
				if (strcmp("ALL_ADDRESS_END",buff) == 0)
				{
					add_get = 0;
				}
				else if (add_get == 1)
				{
					//Find_vec[find_num++] = &buff;
					printf("%s\n",buff);
					strcpy(Find_vec+255*find_num++,buff);
				}
				else if (strcmp("ALL_ADDRESS_BEGIN",buff) == 0)
				{
					add_get = 1;
				}
			}
			fclose(fp);
			return find_num;
		}
		
		

	}
// lab_end



int main(int argc, char * argv[]) {
	printf("\n\n\n\n\t\t\t ZH_LAB \n\n\n");
	FILE * BLIF = NULL;
	FILE * IN_ACT = NULL;
	FILE * OUT_ACT = stdout;
	ace_pi_format_t pi_format = ACE_CODED;
	double p, d;
	int i;
	int depth;
	char clk_name[ACE_CHAR_BUFFER_SIZE];
	int error = 0;
	Abc_Frame_t * pAbc;
	Abc_Ntk_t * ntk;
	Abc_Obj_t * obj;
	// strcpy(clk_name,"top^clk");
	srand(0);

	p = ACE_PI_STATIC_PROB;
	d = ACE_PI_SWITCH_PROB;

	char blif_file_name[BLIF_FILE_NAME_LEN];
	char new_blif_file_name[BLIF_FILE_NAME_LEN];
	ace_io_parse_argv(argc, argv, &BLIF, &IN_ACT, &OUT_ACT, blif_file_name,
			new_blif_file_name, &pi_format, &p, &d);



	//zh_lab 添加变量
	char Info_Path[255]={};			//路径
	char Info_Path_Add[255]={};
	// char ACE_OUT[255]={};
	char Find_vec[3000][255]={};	//标志信息
	int  Find_vec_num = 0;			//标志数量
	int  Find_marked_num_obj = 0;		//obj中mark的数量
	int  j,k;							//临时计数量
	short  Find_mark_Vec[500][ACE_NUM_VECTORS+10]; //触发信号
	int Dict[3000] ;
	int Dict_num = 0;
	memset(Find_mark_Vec,-1,sizeof(Find_mark_Vec));
	memset(Dict,-1,sizeof(Dict));
	//lab_end

	//zh_lab   //路径信息
	strncpy(Info_Path,blif_file_name,strlen(blif_file_name)-5);
	strcat(Info_Path,"_init.info");
	Find_vec_num = read_find_flag(Info_Path,(char * )Find_vec);
	printf("\n\n Find_vec_num  = %d \n\n",Find_vec_num);

	// strncpy(Info_Path_Add,blif_file_name,strlen(blif_file_name)-5);
	// strcat(Info_Path_Add,"_add.txt");
 	// char *ss = argv[6];
    // char *pp_1;
	// char *pp_2;
    // pp_1 = strrchr(ss, '/');
	// pp_2 = strrchr(ss, '.');
	// char tmp_num[100] ={};
	// printf("%ld\n", pp_1);
    // printf("%ld\n", pp_2);
	strncpy(Info_Path_Add,argv[6],strlen(argv[6])-3);
	strcat(Info_Path_Add,"ace");
	// Info_Path_Add
	// printf("sa");
	//lab_end




	// Check # of clocks
#if 0
	int num_clks;
	blif_clock_info(blif_file_name, &num_clks, clk_name);
	if (num_clks > 1)
	{
		printf("Multiple clocks detected in blif file.  This is not supported.\n");
		exit(0);
	}
	else if (num_clks == 1)
	{
		printf("Clock Detected: %s\n", clk_name);
	}
#endif

	pAbc = Abc_FrameGetGlobalFrame();

	ntk = Io_Read(blif_file_name, IO_FILE_BLIF, 1);

	printf("Objects in network: %d\n", Abc_NtkObjNum(ntk));
	printf("PIs in network: %d\n", Abc_NtkPiNum(ntk));

	printf("POs in network: %d\n", Abc_NtkPoNum(ntk));

	printf("Nodes in network: %d\n", Abc_NtkNodeNum(ntk));

	printf("Latches in network: %d\n", Abc_NtkLatchNum(ntk));

	if (!Abc_NtkIsAcyclic(ntk)) {
		printf("Circuit has combinational loops\n");
		exit(0);
	}

	// Alloc Aux Info Array

	// Full Allocation
	Ace_Obj_Info_t * info = calloc(Abc_NtkObjNum(ntk), sizeof(Ace_Obj_Info_t));
	ace_info_hash_table = st_init_table(st_ptrcmp, st_ptrhash);
	Abc_NtkForEachObj(ntk, obj, i)
	{
		st_insert(ace_info_hash_table, (char *) obj, (char *) &info[i]);
		//Ace_InfoPtrSet(obj, & info[i]);
	}

	/* DFS Allocation
	 Vec_Ptr_t * node_vec = Abc_NtkDfsSeq(ntk);
	 Ace_Obj_Info_t * info = malloc(node_vec->nSize * sizeof(Ace_Obj_Info_t));
	 Vec_PtrForEachEntry(Abc_Obj_t *, node_vec, obj_ptr, i)
	 {
	 Ace_InfoPtrSet(obj_ptr, & info[i]);
	 }
	 Vec_PtrFree(node_vec);
	 */

	// Check Depth
	depth = ace_calc_network_depth(ntk);
	printf("Max Depth: %d\n", depth);
	assert(depth > 0);

	alloc_and_init_activity_info(ntk);

	switch (pi_format) {
	case ACE_CODED:
		printf("Input activities will be assumed (%f, %f, %f)...\n",
				ACE_PI_STATIC_PROB, ACE_PI_SWITCH_PROB, ACE_PI_SWITCH_ACT);
		break;
	case ACE_PD:
		printf("Input activities will be (%f, %f, %f)...\n", p, d, d);
		fflush(0);
		break;
	case ACE_ACT:
		printf("Input activities will be read from an activity file...\n");
		break;
	case ACE_VEC:
		printf("Input activities will be read from a vector file...\n");
		break;
	default:
		printf("Error reading activities.\n");
		error = ACE_ERROR;
		break;
	}


	//zh_lab    打印name 和 id
	printf("\nprint_obj_info\n");
	Abc_NtkForEachObj(ntk, obj, i)
	{
		char * s;
		// printf("%d \n",obj->Id);
		s = Nm_ManFindNameById(ntk->pManName, obj->Id);
		// printf("%d \n",obj->Type);
		if (s != NULL)
			{

				if (strcmp("top.memory_controller+memtroll^MULTI_PORT_MUX~8^MUX_2~544", s) == 0)
				{
					// printf("find_\n");
					printf("%s\t%d\t%d\n",s,obj->Type,obj->Id);
				}
				// printf("%s\t%d\t%d\n",s,obj->Type,obj->Id);
				
			}
	}
	// printf("\nFind_marked_num_obj %d\n",Find_marked_num_obj);
	//lab_end


	//zh_lab    在obj集合中加标记
	printf("\nprint_find_info\n");
	Abc_NtkForEachObj(ntk, obj, i)
	{
		char * s;
		// printf("%d \n",obj->Id);
		s = Nm_ManFindNameById(ntk->pManName, obj->Id);
		// printf("%d \n",obj->Type);
		if (s != NULL)
			{
				// printf("%s\n",s);
				for(j = 0; j < Find_vec_num; j++)
				{
					// printf("%s \n",Find_vec[j]);
					if (strcmp(Find_vec[j], s) == 0)
					{
						// printf("find_\n");
						printf("\n%s\tId=%d\tType=%d\t\n",Find_vec[j],obj->Id,obj->Type);
						mark_find_flag(obj);
						Dict[Dict_num++]=obj->Id;
						Find_marked_num_obj++;

					}
				}
			}
	}
	printf("\nFind_marked_num_obj %d\n",Find_marked_num_obj);
	//lab_end
	
	//zh_lab Dict去重
	for (j=0; j<Dict_num; j++)
		{
			char * point_obj_name;
			int point_obj = -2;	
			point_obj = Dict[j];
			if(point_obj != -1)
			{
				point_obj_name = Nm_ManFindNameById(ntk->pManName, point_obj);
				for(k=j+1; k < Dict_num; k++)
				{
					char * point_tmp_name;
					int point_tmp = -2;	
					point_tmp =  Dict[k];
					if(point_tmp != -1)
					{
						point_tmp_name = Nm_ManFindNameById(ntk->pManName,point_tmp);
						if (strcmp(point_tmp_name,point_obj_name)==0)
						{
							Dict[j] = -1;
						}
					}

				}
			}
			
		}

	// //zh_end

	//zh_lab 
	printf("\nprint Dict\n");
	for (j=0; j<Dict_num; j++)
			{
				char * point_obj_name;
				int point_obj = -2;	
				point_obj = Dict[j];
				if(point_obj != -1)
				{
					point_obj_name = Nm_ManFindNameById(ntk->pManName, point_obj);
					printf("%s\n",point_obj_name);
				}
				
			}
	printf("\nprint Dict end\n");
	//zh_end


	//zh_lab      打印所有obj
	printf("\nPrint_all_obj \n");
	Abc_NtkForEachObj(ntk, obj, i)
	{

		char * s;
		// printf("%d \n",obj->Id);
		s = Nm_ManFindNameById(ntk->pManName, obj->Id);
		// printf("%d \n",obj->Type);
		if (s != NULL)
		{
			printf("\nId=%d\tType=%d\t%d\t%s\n",obj->Id,obj->Type,obj->find_flag,s);
		}
		else
		{
			printf("\nId=%d\tType=%d\t%d\t\n",obj->Id,obj->Type,obj->find_flag);
		}
				
		// s = Nm_ManFindNameById(ntk->pManName, obj->Id);
		// printf(" \n",obj->Type);
	}
	//lab_end

	/*
	 Abc_NtkForEachPi(ntk, obj_ptr, i)
	 {
	 char * s;
	 s = Nm_ManFindNameById(ntk->pManName, obj_ptr->Id);
	 if (s != NULL)
	 {
	 //printf("%s\n",s);
	 }

	 }
	 */

	// Read Activities
	if (!error) {
		error = ace_io_read_activity(ntk, IN_ACT, pi_format, p, d, clk_name);
	}

	if (!error) {
		error = ace_calc_activity(ntk, ACE_NUM_VECTORS,(short *) Find_mark_Vec,(int *) &Dict);
	}

	//Abc_NtkToSop(ntk, 0);
	Abc_Ntk_t * new_ntk;
	new_ntk = Abc_NtkToNetlist(ntk);

	if (!error) {
		ace_io_print_activity(ntk, OUT_ACT);
	}

//zh_lab
	FILE *add_dis_file = NULL;
	add_dis_file = fopen(Info_Path_Add, "w");
	// short  Find_mark_Vec[500][5010]; //触发信号
	int row = 0;
	int col = 0;
	char * s;
		
		// printf("%d \n",obj->Type);
		
	for(row = 0;row < Find_marked_num_obj;row++)
		if (Find_mark_Vec[row][0] == -1)
		{
			continue;
		}
		else
		{
			s = Nm_ManFindNameById(ntk->pManName, Dict[row]);
			if (s != NULL)
			{
				fprintf(add_dis_file,"%s ",s);
			}
			else
			{
				fprintf(add_dis_file,"%d ",Dict[row]);
			}
			for(col= 0;col < 5010;col++)
			{
				int tmp_num = Find_mark_Vec[row][col];
				if(tmp_num == -1)
				{
					break;
				}
				else
				{
					fprintf(add_dis_file,"%d ",Find_mark_Vec[row][col]);
				}
			
			
			}
			fprintf(add_dis_file,"\n");
		}
		
		
			
	fclose(add_dis_file);
//zh_end


	Io_WriteHie(ntk, blif_file_name, new_blif_file_name);

	printf("Done\n");
	fflush(0);
	return 0;
}
