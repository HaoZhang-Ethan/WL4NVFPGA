#include "ace.h"
#include "sim.h"

#include "cudd.h"

void get_pi_values(Abc_Ntk_t * ntk, Vec_Ptr_t * nodes, int cycle) {
	Abc_Obj_t * obj;
	Ace_Obj_Info_t * info;
	int i;
	double prob0to1, prob1to0, rand_num;

	//Vec_PtrForEachEntry(Abc_Obj_t *, nodes, obj, i)
	Abc_NtkForEachObj(ntk, obj, i)
	{
		info = Ace_ObjInfo(obj);
		if (Abc_ObjType(obj) == ABC_OBJ_PI) {
			if (info->values) {
				if (info->status == ACE_UNDEF) {
					info->status = ACE_NEW;
					if (info->values[cycle] == 1) {
						info->value = 1;
						info->num_toggles = 1;
						info->num_ones = 1;
					} else {
						info->value = 0;
						info->num_toggles = 0;
						info->num_ones = 0;
					}
				} else {
					switch (info->value) {
					case 0:
						if (info->values[cycle] == 1) {
							info->value = 1;
							info->status = ACE_NEW;
							info->num_toggles++;
							info->num_ones++;
						} else {
							info->status = ACE_OLD;
						}
						break;

					case 1:
						if (info->values[cycle] == 0) {
							info->value = 0;
							info->status = ACE_NEW;
							info->num_toggles++;
						} else {
							info->num_ones++;
							info->status = ACE_OLD;
						}
						break;

					default:
						printf("Bad Value\n");
						assert(0);
						break;
					}
				}
			} else {
				prob0to1 = ACE_P0TO1(info->static_prob, info->switch_prob);
				prob1to0 = ACE_P1TO0(info->static_prob, info->switch_prob);

				rand_num = (double) rand() / (double) RAND_MAX;

				if (info->status == ACE_UNDEF) {
					info->status = ACE_NEW;
					if (rand_num < prob0to1) {
						info->value = 1;
						info->num_toggles = 1;
						info->num_ones = 1;
					} else {
						info->value = 0;
						info->num_toggles = 0;
						info->num_ones = 0;
					}
				} else {
					switch (info->value) {
					case 0:
						if (rand_num < prob0to1) {
							info->value = 1;
							info->status = ACE_NEW;
							info->num_toggles++;
							info->num_ones++;
						} else {
							info->status = ACE_OLD;
						}
						break;

					case 1:
						if (rand_num < prob1to0) {
							info->value = 0;
							info->status = ACE_NEW;
							info->num_toggles++;
						} else {
							info->num_ones++;
							info->status = ACE_OLD;
						}
						break;

					default:
						printf("Bad value\n");
						assert(FALSE);
						break;
					}
				}
			}
		}
	}
}

int * getFaninValues(Abc_Obj_t * obj_ptr) {
	Abc_Obj_t * fanin;
	int i;
	Ace_Obj_Info_t * info;
	int * faninValues;

	Abc_ObjForEachFanin(obj_ptr, fanin, i)
	{
		info = Ace_ObjInfo(fanin);
		if (info->status == ACE_UNDEF) {
			printf("Fan-in is undefined\n");
			assert(FALSE);
		} else if (info->status == ACE_NEW) {
			break;
		}
	}

	if (i >= Abc_ObjFaninNum(obj_ptr)) {
		// inputs haven't changed
		return NULL;
	}

	faninValues = malloc(Abc_ObjFaninNum(obj_ptr) * sizeof(int));
	Abc_ObjForEachFanin(obj_ptr, fanin, i)
	{
		info = Ace_ObjInfo(fanin);
		faninValues[i] = info->value;
	}

	return faninValues;
}

ace_status_t getFaninStatus(Abc_Obj_t * obj_ptr) {
	Abc_Obj_t * fanin;
	int i;
	Ace_Obj_Info_t * info;

	Abc_ObjForEachFanin(obj_ptr, fanin, i)
	{
		info = Ace_ObjInfo(fanin);
		if (info->status == ACE_UNDEF) {
			return ACE_UNDEF;
		}
	}

	Abc_ObjForEachFanin(obj_ptr, fanin, i)
	{
		info = Ace_ObjInfo(fanin);
		if (info->status == ACE_NEW || info->status == ACE_SIM) {
			return ACE_NEW;
		}
	}

	return ACE_OLD;
}

void evaluate_circuit(Abc_Ntk_t * ntk, Vec_Ptr_t * node_vec, int cycle) {
	Abc_Obj_t * obj;
	Ace_Obj_Info_t * info;
	int i;
	int value;
	int * faninValues;
	ace_status_t status;
	DdNode * dd_node;

	Vec_PtrForEachEntry(node_vec, obj, i)
	{
		info = Ace_ObjInfo(obj);
		// if (Abc_ObjFanout0(obj)->find_flag == 1)
		// 		{
		// 			printf("find_mark\n");
		// 		}
		switch (Abc_ObjType(obj)) {
		case ABC_OBJ_PI:
		case ABC_OBJ_BO:
			break;

		case ABC_OBJ_PO:
		case ABC_OBJ_BI:
		case ABC_OBJ_LATCH:			
		case ABC_OBJ_NODE:
			status = getFaninStatus(obj);
			switch (status) {
			case ACE_UNDEF:
				info->status = ACE_UNDEF;
				break;
			case ACE_OLD:
				info->status = ACE_OLD;
				info->num_ones += info->value;
				break;
			case ACE_NEW:
				if (Abc_ObjIsNode(obj)) {
					faninValues = getFaninValues(obj);
					assert(faninValues);
					dd_node = Cudd_Eval(ntk->pManFunc, obj->pData, faninValues);
					assert(Cudd_IsConstant(dd_node));
					if (dd_node == Cudd_ReadOne(ntk->pManFunc)) {
						value = 1;
						// printf("1\n");
					} else if (dd_node == Cudd_ReadLogicZero(ntk->pManFunc)) {
						value = 0;
						// printf("0\n");
					} else {
						assert(0);
					}
					free(faninValues);
				} else {
					Ace_Obj_Info_t * fanin_info = Ace_ObjInfo(
							Abc_ObjFanin0(obj));
					value = fanin_info->value;
				}
				if (info->value != value || info->status == ACE_UNDEF) {
					info->value = value;
					if (info->status != ACE_UNDEF) {
						/* Don't count the first value as a toggle */
						info->num_toggles++;
					}
					info->status = ACE_NEW;
				} else {
					info->status = ACE_OLD;
				}
				info->num_ones += info->value;
				break;
			default:
				assert(0);
				break;
			}
			// if (Abc_ObjFanout0(obj)->find_flag == 1)
			// {	info->status = ACE_NEW;
			// 	printf("marled %d\n",info->value);
			// }
			break;
		default:
			assert(0);
			break;
		}
	}
}

//zh_lab
int find_Dict(int * P_Dict ,int Id )
{
	int j = 0;
	for (j=0; j<=300; j++)
	{
		if(*(P_Dict+j) == Id)
		{
			return j;
		}
	}
	j = -1;
	return j;
}

void update_FFs(Abc_Ntk_t * ntk,short * P_Find_mark_Vec,int cycle,int * P_Dict) {
	Abc_Obj_t * obj;
	int i;
	Ace_Obj_Info_t * bi_fanin_info;
	Ace_Obj_Info_t * bi_info;
	Ace_Obj_Info_t * latch_info;
	Ace_Obj_Info_t * bo_info;
	// *(P_Find_mark_Vec+1) = 255;
	// printf("\nzh _ \t\t %d\n",*(P_Find_mark_Vec+1));

	// printf("\nupdate_FFs_begin\n");
	Abc_NtkForEachLatch(ntk, obj, i)
	{
		// printf("obj_type %d \n",obj->Type);
		bi_fanin_info = Ace_ObjInfo(Abc_ObjFanin0(Abc_ObjFanin0(obj)));
		bi_info = Ace_ObjInfo(Abc_ObjFanin0(obj));
		bo_info = Ace_ObjInfo(Abc_ObjFanout0(obj));
		latch_info = Ace_ObjInfo(obj);

		// Value
		bi_info->value = bi_fanin_info->value;
		latch_info->value = bi_fanin_info->value;
		bo_info->value = bi_fanin_info->value;

		//zh_lab    统计latch_valve
		if(Abc_ObjFanout0(obj)->find_flag == 1)
			{
				// printf("%d\t%d\n",Abc_ObjFanout0(obj)->Id,bo_info->value);
				// *(P_Find_mark_Vec+5010*1+0)
				int find_num = find_Dict(P_Dict,Abc_ObjFanout0(obj)->Id);
				if(find_num == -1 )
				{
					// printf("Find_P_DICT_FAILD\n\n\n");
					// exit(1);
				}
				else
				{
					*(P_Find_mark_Vec+5010*find_num+cycle)=bo_info->value;
				}
				
				
				// *(P_Find_mark_Vec+cycle) = bo_info->value;
			}

		// Status
		bi_info->status = bi_fanin_info->status;
		latch_info->status = bi_fanin_info->status;
		bo_info->status = bi_fanin_info->status;

		// Ones
		bi_info->num_ones = bi_fanin_info->num_ones;
		latch_info->num_ones = bi_fanin_info->num_ones;
		bo_info->num_ones = bi_fanin_info->num_ones;

		// Toggles
		bi_info->num_toggles = bi_fanin_info->num_toggles;
		latch_info->num_toggles = bi_fanin_info->num_toggles;
		bo_info->num_toggles = bi_fanin_info->num_toggles;
	}
	// printf("\nupdate_FFs_end\n");
}




//zh_lab print_logic_node
void pinrt_logic_node(Vec_Ptr_t * node_vec)
{
	Abc_Obj_t * obj;
	int i;
	printf("\nprint_logic_node\n");
	Vec_PtrForEachEntry(node_vec, obj, i)
	{
		printf("\nId=%d\tType=%d\t%d\t\n",obj->Id,obj->Type,obj->find_flag);
	}

}
//zh_lab
void print_all_obj_info(Abc_Ntk_t * ntk,short * P_Find_mark_Vec,int cycle,int * P_Dict)
{
	Abc_Obj_t * obj;
	Ace_Obj_Info_t * info;
	int i;
	char *s;


	// int value;
	// int * faninValues;
	// ace_status_t status;
	// DdNode * dd_node;

	Abc_NtkForEachObj(ntk, obj, i)
	{
		info = Ace_ObjInfo(obj);
		// if(obj->vFanins.nSize>1)//
		// {
		// 	if(obj->Type != 9)
		// 	{
		// 		printf("find_no_9");
		// 	}
		// 	printf("zhagnhao_obj %d\n",obj->vFanins.nSize);
		// 	faninValues = getFaninValues(obj);
		// 	assert(faninValues);
		// 	dd_node = Cudd_Eval(ntk->pManFunc, obj->pData, faninValues);
		// 	assert(Cudd_IsConstant(dd_node));
		// 	if (dd_node == Cudd_ReadOne(ntk->pManFunc)) {
		// 		value = 1;
		// 		printf("zhagnhao_1 %d\n",value);
		// 	} else if (dd_node == Cudd_ReadLogicZero(ntk->pManFunc)) {
		// 		value = 0;
		// 		printf("zhagnhao_0 %d\n",value);
		// 	} else {
		// 		assert(0);
		// 	}
		// 	free(faninValues);
			
		// }
		if (Abc_ObjType(obj) == ABC_OBJ_PO && obj->find_flag == 1) 
		{	
			Ace_Obj_Info_t * fanin_info = Ace_ObjInfo(Abc_ObjFanin0(obj));
			// printf("%d\t%d\t\n",obj->Id,fanin_info->value);
	 		int find_num = find_Dict(P_Dict,obj->Id);
			 if(find_num == -1 )
			 {
				//  printf("Find_P_DICT_FAILD\n\n\n");
				//  exit(1);
			 }
			 else
			 {
				//  printf("z %d\n",fanin_info->value);
				 *(P_Find_mark_Vec+5010*find_num+cycle)=(fanin_info->value);
			 }
			
			
			// printf("%s",*(P_Find_mark_Vec+5010*find_num+cycle));
			// *(P_Find_mark_Vec+cycle) = bo_info->value;
			// printf("%d",*P_Dict);
			
		}

	}
}


//zh_lab
void ace_sim_activities(Abc_Ntk_t * ntk, Vec_Ptr_t * nodes, int max_cycles,
		double threshold,short * P_Find_mark_Vec,int * P_Dict) {
	Abc_Obj_t * obj;
	Ace_Obj_Info_t * info;
	int i;

	assert(max_cycles > 0);
	assert(threshold > 0.0);

	srand((unsigned) time(NULL));

	//Vec_PtrForEachEntry(Abc_Obj_t *, nodes, obj, i)
	Abc_NtkForEachObj(ntk, obj, i)
	{
		info = Ace_ObjInfo(obj);
		info->value = 0;

		if (Abc_ObjType(obj) == ABC_OBJ_BO) {
			info->status = ACE_NEW;
		} else {
			info->status = ACE_UNDEF;
		}
		info->num_ones = 0;
		info->num_toggles = 0;
	}

	Vec_Ptr_t * logic_nodes = Abc_NtkDfs(ntk, TRUE);
	//zh_lab
	pinrt_logic_node(logic_nodes);
	for (i = 0; i < max_cycles; i++) {
		get_pi_values(ntk, nodes, i);
		evaluate_circuit(ntk, logic_nodes, i);
		update_FFs(ntk,P_Find_mark_Vec,i,P_Dict);
		print_all_obj_info(ntk,P_Find_mark_Vec,i,P_Dict);
		
	}

	//Vec_PtrForEachEntry(Abc_Obj_t *, nodes, obj, i)
	Abc_NtkForEachObj(ntk, obj, i)
	{
		info = Ace_ObjInfo(obj);
		info->static_prob = info->num_ones / (double) max_cycles;
		assert(info->static_prob >= 0.0 && info->static_prob <= 1.0);
		info->switch_prob = info->num_toggles / (double) max_cycles;
		assert(info->switch_prob >= 0.0 && info->switch_prob <= 1.0);

		assert(info->switch_prob - EPSILON <= 2.0 * (1.0 - info->static_prob));
		assert(info->switch_prob - EPSILON <= 2.0 * (info->static_prob));

		info->status = ACE_SIM;
	}
}
