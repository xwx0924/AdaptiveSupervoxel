#ifndef OPTIMIZER_H
#define OPTIMIZER_H
#include <vector>
#include <set>
#include "covariance.h"
#include "MxStdSlim.h"


typedef int PixelIdx;
typedef int ClusterIdx;
const float energy_convergence = 0.0001;// 0.0000001;
using namespace std;

class Cluster_Pair : public MxHeapable
{
public:
	PixelIdx p1, p2;
};

class Pixel_swap : public MxHeapable
{
public:
	ClusterIdx from_id, to_id;
	PixelIdx face_id;

	Pixel_swap() { }
};
typedef Cluster_Pair cluster_pair;

class Optimizer
{
public:
	MxHeap heap;
	MxHeap heap_s;
	vector<CovDet> __cov;
	vector<vector<Cluster_Pair*>> contractions;
	vector<vector<Cluster_Pair*>> contractions_small;
	vector<vector<cluster_pair>> cluster_neighbor_links;
	vector<double> cluster_energy;
	vector<set<PixelIdx>> cluster_boundary_pixels;
	vector<Pixel_swap *> info_set;
	int width, height, depth;
	float * p_sp_belong;//cluster Id
	set<int> * p_clusters;
	int target;
	int leaf_num;
	int nClusters;
	Optimizer();
	~Optimizer();

	void init_cov(int target);
	void allocate_space();
	void init_covariance();
	void collect_contraction();
	void create_pair(PixelIdx i, PixelIdx j);
	void create_pair_s(PixelIdx i, PixelIdx j);
	void compute_pair_info(Cluster_Pair* info);
	void compute_pair_info_s(Cluster_Pair* info);
	void compute_energy(Cluster_Pair *info);
	void finalize_pair_update(Cluster_Pair *info);
	void decimate(int target_num);
	void decimate_s();
	void apply_contraction(Cluster_Pair *info);
	void update_valid_pairs(Cluster_Pair *info);
	int find_contracted_neighbors(PixelIdx v1, set<PixelIdx> & neighbors);
	void find_neighbors(int cId, set<int>& neighbors);
	void erase_pair_counterpart(ClusterIdx u, Cluster_Pair *info);
	void erase_pair_counterpart_s(ClusterIdx u, Cluster_Pair *info);
	void relabel_cluster_label();
	float* get_cluster_label();
	void update_energy_ratio();
	void collect_cov_info(vector<float>& cluster_cov);
	void init_pixel_swap();
	void update_cluster_information(ClusterIdx i);
	void collect_cluster_neighbor();
	void collect_cluster_swaps(int id);
	double calc_energy_delta(ClusterIdx from, ClusterIdx to, PixelIdx face_id);
	void store_swap(float energy_saved, ClusterIdx from, ClusterIdx to, PixelIdx face_id, Pixel_swap *info);
	bool swap_pixel_once();
	void apply_swap_set(vector<Pixel_swap *> info_set, set<ClusterIdx>& recollect_domain);
	void apply_swap(Pixel_swap* info, set<ClusterIdx>& update_domain);

	void print_label();
	void print_swap_info(vector<Pixel_swap *>& info_set);
	void execute_cluster_boundary_swap(float* res);
	void test();
	void print_swap_total(vector<Pixel_swap *>& info_set);

	void check_label_contineous();
};


#endif