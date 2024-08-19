#include <numeric>
#include "optimizer.h"
#include "partition.h"
#include "Octree.h"
#include <opencv2/opencv.hpp>
#include "opencv2/imgproc/imgproc_c.h"
extern Partition* tess;
extern AdaptiveOctree* tree;
extern cv::Mat video_mat;
extern int * valid_mapping;
extern map<int, int> valid_map;
extern float location_ratio;

Optimizer::Optimizer()
{
}

Optimizer::~Optimizer()
{
	if (p_sp_belong) 
		delete[] p_sp_belong;
	if(p_clusters)
		delete[] p_clusters;

	delete tess;
	cout << "destroy optimizer" << endl;
}

void Optimizer::allocate_space()
{
	__cov = vector<CovDet>(leaf_num, CovDet());
	contractions = vector<vector<Cluster_Pair*>>(leaf_num);
	contractions_small = vector<vector<Cluster_Pair*>>(leaf_num);
}


void Optimizer::init_covariance()
{
	int w_ = tree->get_video_width();
	int h_ = tree->get_video_height();
	int d_ = tree->get_video_depth();

	int* leaf_label = tree->get_pixel_leaf_label();

	unsigned char* data = video_mat.data;
	int index_p = 0, in_leaf = 0;
	for (int k = 0; k < d_; k++)
	{
		for (int j = 0; j < h_; j++)
		{
			for (int i = 0; i < w_; i++)
			{
				Vector6d coor;
				coor << i, j, k, data[3 * index_p], data[3 * index_p + 1], data[3 * index_p + 2];
				CovDet cov(coor, 1.0);
				in_leaf = leaf_label[index_p];
				__cov[in_leaf] += cov;

				index_p++;
			}
		}
	}

}

void Optimizer::collect_contraction()
{
	//遍历所有的leaf，将大于当前leaf_id的邻居作为合并对
	int leaf_num = tree->nodes_number();
	vector<set<int>>& leaf_neighbors = tree->get_leaf_neighbors();
	for (int i = 0; i < leaf_num; i++)
	{
		set<int>& curr_neighbors = leaf_neighbors[i];
		for (set<int>::iterator it = curr_neighbors.begin(); it != curr_neighbors.end(); it++)
		{
			if (*it > i)
			{
				create_pair(i, *it);
			}
		}
	}
}


void Optimizer::create_pair(PixelIdx i, PixelIdx j)
{
	Cluster_Pair *info = new Cluster_Pair();
	info->p1 = i;
	info->p2 = j;
	compute_pair_info(info);

	contractions[i].push_back(info);
	contractions[j].push_back(info);
}


void Optimizer::create_pair_s(PixelIdx i, PixelIdx j)
{
	Cluster_Pair *info = new Cluster_Pair();
	info->p1 = i;
	info->p2 = j;

	CovDet &Ci = __cov[i], &Cj = __cov[j];
	CovDet C = Ci;  C += Cj;

	double err = (tess->clusters[i].size() + tess->clusters[j].size())*(C.energy() - Ci.energy() - Cj.energy());
	info->heap_key(-err);
	if (info->is_in_heap())
		heap_s.update(info);
	else
		heap_s.insert(info);


	contractions_small[i].push_back(info);
	contractions_small[j].push_back(info);
}

void Optimizer::compute_pair_info_s(Cluster_Pair* info)
{
	PixelIdx i = info->p1, j = info->p2;

	CovDet &Ci = __cov[i], &Cj = __cov[j];
	CovDet C = Ci;  C += Cj;

	double err = (tess->clusters[i].size() + tess->clusters[j].size())*(C.energy() - Ci.energy() - Cj.energy());
	info->heap_key(-err);
	if (info->is_in_heap())
		heap_s.update(info);
	else
		heap_s.insert(info);
}

void Optimizer::compute_pair_info(Cluster_Pair* info)
{
	compute_energy(info);
	finalize_pair_update(info);
}

void Optimizer::compute_energy(Cluster_Pair *info)
{
	PixelIdx i = info->p1, j = info->p2;

	CovDet &Ci = __cov[i], &Cj = __cov[j];
	CovDet C = Ci;  C += Cj;

	double err = C.energy() - Ci.energy() - Cj.energy();
	//double err = (tess->clusters[i].size() + tess->clusters[j].size())*(C.energy() - Ci.energy() - Cj.energy());
	info->heap_key(-err);
}

void Optimizer::finalize_pair_update(Cluster_Pair *info)
{
	if (info->is_in_heap())
		heap.update(info);
	else
		heap.insert(info);
}


void Optimizer::init_cov(int target_)
{
	leaf_num = tree->nodes_number();
	width = tree->get_video_width();
	height = tree->get_video_height();
	depth = tree->get_video_depth();
	nClusters = leaf_num;
	allocate_space();
	tess = new Partition(leaf_num);
	init_covariance();
	collect_contraction();
	target = target_;
	cluster_neighbor_links = vector<vector<cluster_pair>>(target_);
	cluster_boundary_pixels = vector<set<PixelIdx>>(target_);
}

void Optimizer::decimate_s()
{
	cout << "target=" << target << endl;
	//build small heap first
	for (PixelIdx j = 0; j < tess->size; j++)
	{
		if (tess->clusters[j].size()!=0)
		{
			for (int i = 0; i < contractions[j].size(); i++)
			{
				Cluster_Pair *e = contractions[j][i];
				int neighbor = (e->p1 == j) ? e->p2 : e->p1;
				if (neighbor > j)
				{
					create_pair_s(j, neighbor);
				}
			}
		}
	}

	
	//do the contraction
	while (nClusters > target)
	{
		Cluster_Pair *info = (Cluster_Pair *)heap_s.extract();

		PixelIdx v1 = info->p1, v2 = info->p2;
		if (!tess->clusters[v1].empty() && !tess->clusters[v2].empty())
		{
			//apply_contraction(info);
			tess->mergeDomain(v1, v2);
			__cov[v1] += __cov[v2];

			//find all the possible neighbors
			set<PixelIdx> neighbors;
			int n_neighbors = find_contracted_neighbors(v1, neighbors);

			//update the heap
			for (int i = 0; i < contractions_small[v1].size(); i++)
			{
				Cluster_Pair *e = contractions_small[v1][i];
				PixelIdx u = (e->p1 ==v1) ? e->p2 : e->p1;
				heap_s.remove(e);
				erase_pair_counterpart_s(u, e);
				if (u != v2)
					delete e;
			}
			for (int i = 0; i < contractions_small[v2].size(); i++)
			{
				Cluster_Pair *e = contractions_small[v2][i];
				PixelIdx u = (e->p1 == v2) ? e->p2 : e->p1;
				heap_s.remove(e);
				erase_pair_counterpart_s(u, e);
				if (u != v1)
					delete e;
			}
			contractions_small[v1].clear();
			contractions_small[v2].clear();

			for (set<PixelIdx>::iterator it = neighbors.begin(); it != neighbors.end(); it++)
			{
				Cluster_Pair * info_to_add = new Cluster_Pair();
				info_to_add->p1 = v1;
				info_to_add->p2 = *it;
				contractions_small[v1].push_back(info_to_add);
				contractions_small[*it].push_back(info_to_add);
			}

			for (uint i = 0; i < contractions_small[v1].size(); i++)
				compute_pair_info_s(contractions_small[v1][i]);


			nClusters--;
			delete info;
		}

	}

	if (valid_mapping)
		delete[] valid_mapping;
	valid_map.clear();
	
	valid_mapping = new int[nClusters];

	int n_domain = 0;
	for (PixelIdx j = 0; j < tess->size; j++)
	{
		if (tess->clusters[j].size() != 0)
		{
			valid_mapping[n_domain] = j;
			valid_map.insert(pair<int, int>(j, n_domain));
			/*cout << "<" << j << "," << n_domain << ">" << endl;
			cout<<__cov[j].area<<","<< __cov[j].energy()<<endl;*/
			n_domain++;
		}
	}
	cout << "decimate2, nClusters:" << nClusters << endl;


	//get the valid_map and valid_mapping
}


void Optimizer::decimate(int target_num)
{
	while (nClusters > target_num)
	{
		Cluster_Pair *info = (Cluster_Pair *)heap.extract();

		PixelIdx v1 = info->p1, v2 = info->p2;
		if (!tess->clusters[v1].empty() && !tess->clusters[v2].empty())
		{
			apply_contraction(info);
			nClusters--;
			delete info;
		}

	}

	if (valid_mapping)
		delete[] valid_mapping;
	valid_map.clear();
	
	valid_mapping = new int[nClusters];

	int n_domain = 0;
	for (PixelIdx j = 0; j < tess->size; j++)
	{
		if (tess->clusters[j].size() != 0)
		{
			valid_mapping[n_domain] = j;
			valid_map.insert(pair<int, int>(j, n_domain));
			/*cout << "<" << j << "," << n_domain << ">" << endl;
			cout<<__cov[j].area<<","<< __cov[j].energy()<<endl;*/
			n_domain++;
		}
	}
	cout << "nClusters:" << nClusters << endl;

}


void Optimizer::update_energy_ratio()
{
	vector<double> cluster_energy_location(nClusters, 0);
	vector<double> cluster_energy_color(nClusters, 0);
	for (int i = 0; i < nClusters; i++)
	{
		CovDet &Cj = __cov[valid_mapping[i]];
		cluster_energy_location[i] = Cj.energy1();
		cluster_energy_color[i] = Cj.energy2();
	}
	double energy_color = std::accumulate(cluster_energy_color.begin(), cluster_energy_color.end(), 0.0);
	double energy_location = std::accumulate(cluster_energy_location.begin(), cluster_energy_location.end(), 0.0);
	CovDet::ratio = location_ratio * energy_color / energy_location;
	cout << "location energy: " << energy_location << " color energy " << energy_color << " ratio " << CovDet::ratio << endl;

}

void Optimizer::collect_cov_info(vector<float>& cluster_cov)
{
	for (int i = 0; i < nClusters; i++)
	{
		CovDet &C = __cov[valid_mapping[i]];
		cluster_cov[i * 21] = i;
		cluster_cov[i * 21 + 1] = C.area;
		cluster_cov[i * 21 + 2] = C.energy();

		cluster_cov[i * 21 + 3] = C.centroid1(0);
		cluster_cov[i * 21 + 4] = C.centroid1(1);
		cluster_cov[i * 21 + 5] = C.centroid1(2);

		cluster_cov[i * 21 + 6] = C.centroid2(0);
		cluster_cov[i * 21 + 7] = C.centroid2(1);
		cluster_cov[i * 21 + 8] = C.centroid2(2);

		cluster_cov[i * 21 + 9] = C.cov1(0, 0);
		cluster_cov[i * 21 + 10] = C.cov1(0, 1);
		cluster_cov[i * 21 + 11] = C.cov1(0, 2);
		cluster_cov[i * 21 + 12] = C.cov1(1, 1);
		cluster_cov[i * 21 + 13] = C.cov1(1, 2);
		cluster_cov[i * 21 + 14] = C.cov1(2, 2);

		cluster_cov[i * 21 + 15] = C.cov2(0, 0);
		cluster_cov[i * 21 + 16] = C.cov2(0, 1);
		cluster_cov[i * 21 + 17] = C.cov2(0, 2);
		cluster_cov[i * 21 + 18] = C.cov2(1, 1);
		cluster_cov[i * 21 + 19] = C.cov2(1, 2);
		cluster_cov[i * 21 + 20] = C.cov2(2, 2);

	}
}


void Optimizer::test()
{
	string txtname = "test.txt";
	ofstream f_debug1(txtname);

	int texel = width * height*depth;
	int slice = width * height;
	int index = 0;
	int x_new, y_new, z_new;
	float energy_before = 0;
	float energy_after = 0;
	float energy_saved = 0;
	float energy_saved_closest = 0;
	int cluster_closest = 0;
	int n_id = 0;

	unsigned char* data = video_mat.data;

	cluster_energy = vector<double>(nClusters, 0);
	for (int i = 0; i < nClusters; i++)
		update_cluster_information(i);

	float off_set[18] =
	{ -1.0,-1.0,
	0,-1.0,
	1,-1.0,
	-1.0,0,
	0,0,
	1,0,
	-1.0,1,
	0,1,
	1,1
	};
	float off_z[3] = { -1.0,0,1.0 };

	for (int k = 0; k < 3; k++)
	{
		for (int j = 0; j < height; j++)
		{
			for (int i = 0; i < width; i++)
			{
				int sId = p_sp_belong[index];
				CovDet& Ci = __cov[valid_mapping[sId]];
				energy_saved_closest = 0;
				cluster_closest = -1;
				
	
				for (int a = 0; a < 3; a++)
				{
					n_id = 0;//
					for (int b = 0; b < 9; b++)
					{
						x_new = i +off_set[n_id];
						y_new = j +off_set[n_id+1];
						z_new = k + off_z[a];

						if (x_new >= 0 && x_new < width && y_new >= 0 && y_new < height  && z_new >= 0 && z_new < depth)
						{
							int nei = z_new * slice + y_new * width + x_new;
							int n_sId = p_sp_belong[nei];
							if (sId != n_sId)
							{
								CovDet& Cj = __cov[valid_mapping[n_sId]];
								energy_before = cluster_energy[sId] + cluster_energy[n_sId];
								
								Vector6d coor;
								coor << i, j, k, data[3 * index], data[3 * index + 1], data[3 * index + 2];
								CovDet C_init = CovDet(coor, 1.0);

								CovDet C_from = Ci;
								CovDet C_to = Cj;
								C_from -= C_init;
								C_to += C_init;
								energy_after = C_from.energy() + C_to.energy();
								energy_saved = energy_before - energy_after;
								if (index == 4)
								{
									cout << nei << "," << n_sId << "," << energy_saved << endl;
								}
								if (energy_saved > energy_saved_closest)
								{
									cluster_closest = n_sId;
									energy_saved_closest = energy_saved;
								}
								

							}
						}
						n_id += 2;
					}
				}
				cout << index <<","<< sId<<","<< cluster_closest<<"," << energy_saved_closest<< endl;
				if (energy_saved_closest > 0)
				{
					f_debug1 << std::left << std::setw(4) << index << " ";
					f_debug1 << std::left << std::setw(4) << sId << " ";
					f_debug1 << std::left << std::setw(4) << cluster_closest << " ";
					f_debug1 << std::left << std::setw(4) << energy_saved_closest << " ";
				}
				else
				{
					f_debug1 << std::left << std::setw(4) << 0 << " ";
					f_debug1 << std::left << std::setw(4) << 0 << " ";
					f_debug1 << std::left << std::setw(4) << 0 << " ";
					f_debug1 << std::left << std::setw(4) << -1 << " ";
				}
				
				index++;


			}
			f_debug1 << endl;
		}
		f_debug1 << endl;
		f_debug1 << endl;
	}
	f_debug1.close();
}

void Optimizer::apply_contraction(Cluster_Pair *info)
{
	int p1 = info->p1;
	int p2 = info->p2;
	tess->mergeDomain(p1, p2);
	__cov[p1] += __cov[p2];

	update_valid_pairs(info);
	for (uint i = 0; i < contractions[p1].size(); i++)
		compute_pair_info(contractions[p1][i]);
}

void Optimizer::update_valid_pairs(Cluster_Pair *info)
{
	//find all the possible neighbors
	PixelIdx p1 = info->p1, p2 = info->p2;
	set<PixelIdx> neighbors;
	int n_neighbors = find_contracted_neighbors(p1, neighbors);

	//update the heap
	for (int i = 0; i < contractions[p1].size(); i++)
	{
		Cluster_Pair *e = contractions[p1][i];
		PixelIdx u = (e->p1 == p1) ? e->p2 : e->p1;
		heap.remove(e);
		erase_pair_counterpart(u, e);
		if (u != p2)
			delete e;
	}

	for (int i = 0; i < contractions[p2].size(); i++)
	{
		Cluster_Pair *e = contractions[p2][i];
		PixelIdx u = (e->p1 == p2) ? e->p2 : e->p1;
		heap.remove(e);
		erase_pair_counterpart(u, e);
		if (u != p1)
			delete e;
	}
	contractions[p1].clear();
	contractions[p2].clear();

	for (set<PixelIdx>::iterator it = neighbors.begin(); it != neighbors.end(); it++)
	{
		Cluster_Pair * info_to_add = new Cluster_Pair();
		info_to_add->p1 = p1;
		info_to_add->p2 = *it;
		contractions[p1].push_back(info_to_add);
		contractions[*it].push_back(info_to_add);
	}

}


int Optimizer::find_contracted_neighbors(PixelIdx v1, set<PixelIdx> & neighbors)
{
	neighbors.clear();
	vector<set<int>>& leaf_neighbors = tree->get_leaf_neighbors();
	for (set<PixelIdx>::iterator it = tess->clusters[v1].begin(); it != tess->clusters[v1].end(); it++)
	{
		for (set<int>::iterator p_it = leaf_neighbors[*it].begin(); p_it != leaf_neighbors[*it].end(); p_it++)
		{
			PixelIdx neighbor_point = *p_it;
			if (tess->cluster_belong[neighbor_point] != v1)
				neighbors.insert(tess->cluster_belong[neighbor_point]);
		}
	}
	return neighbors.size();
}

void Optimizer::find_neighbors(int cId, set<int>& neighbors)
{
	neighbors.clear();
	int neighbor_Id = 0;
	vector<set<int>>& leaf_neighbors = tree->get_leaf_neighbors();
	for (set<PixelIdx>::iterator it = tess->clusters[cId].begin(); it != tess->clusters[cId].end(); it++)
	{
		for (set<int>::iterator p_it = leaf_neighbors[*it].begin(); p_it != leaf_neighbors[*it].end(); p_it++)
		{
			neighbor_Id = tess->cluster_belong[*p_it];
			if (neighbor_Id != cId)
			{
				neighbors.insert(neighbor_Id);//
				/*if (valid_map[neighbor_Id] == 29)
				{
					OctreeNode* a = tree->get_leaf(*p_it);
					cout << a->center()[0] << "," << a->center()[1] << "," << a->center()[2] << endl;;
				}*/
			}

		}
	}
}


void Optimizer::erase_pair_counterpart(ClusterIdx u, Cluster_Pair *info)
{
	for (int i = 0; i < contractions[u].size(); i++)
	{
		if (contractions[u][i] == info)
			contractions[u].erase(contractions[u].begin() + i);
	}
}


void Optimizer::erase_pair_counterpart_s(ClusterIdx u, Cluster_Pair *info)
{
	for (int i = 0; i < contractions_small[u].size(); i++)
	{
		if (contractions_small[u][i] == info)
			contractions_small[u].erase(contractions_small[u].begin() + i);
	}
}

float* Optimizer::get_cluster_label()
{
	if (p_sp_belong)
		return p_sp_belong;
	else
	{
		relabel_cluster_label();
		return p_sp_belong;
	}
}
void Optimizer::relabel_cluster_label()
{
	//pixel label [0,200)
	int texel_num = width * height*depth;
	p_sp_belong = new float[texel_num];
	p_clusters = new set<int>[nClusters];
	int* leaf_label = tree->get_pixel_leaf_label();//pixel->leaf
	int* leaf_belong = tess->cluster_belong;//leaf->cluster

	int p_ind = 0;
	int temp = 0;
	for (int i = 0; i < texel_num; i++)
	{
		temp = valid_map[leaf_belong[leaf_label[p_ind]]];
		p_sp_belong[p_ind] = float(valid_map[leaf_belong[leaf_label[p_ind]]]);
		p_clusters[temp].insert(p_ind);
		p_ind++;
	}

}
void Optimizer::print_label()
{
	string txtname = "pixel_label.txt";
	ofstream f_debug1(txtname);
	int slice_index = 0;
	for (int t = 0; t < depth; t++)
	{
		//slice_index = t * width*height * 3;
		for (int i = 0; i < height; i++) {
			//slice_index = (t * slice_num + i * width) * 4;
			for (int j = 0; j < width; j++) {
				f_debug1 << std::left << std::setw(4) << p_sp_belong[slice_index++] << " ";
				
			}
			f_debug1 << endl;
		}
		f_debug1 << endl;
		f_debug1 << endl;
	}

	f_debug1.close();

}
void Optimizer::init_pixel_swap()
{
	cluster_energy = vector<double>(nClusters, 0);
	//update_energy_ratio()
	CovDet::ratio = 0.1;
	collect_cluster_neighbor();
	for (int i = 0; i < nClusters; i++)
		update_cluster_information(i);
	for (int i = 0; i < nClusters; i++)
	{
		/*cout << i <<",";*/
		collect_cluster_swaps(i);
	}
	
	//print_swap_total(info_set);
}

void Optimizer::print_swap_info(vector<Pixel_swap *>& info_set)
{
	string txtname = "swap1.txt";
	ofstream f_debug1(txtname);

	for (vector<Pixel_swap *>::iterator info_it = info_set.begin(); info_it != info_set.end(); info_it++)
	{
		f_debug1 << std::left << std::setw(4) << (*info_it)->face_id << " ";
		f_debug1 << std::left << std::setw(4) << (*info_it)->from_id << " ";
		f_debug1 << std::left << std::setw(4) << (*info_it)->to_id << " ";
		f_debug1 << std::left << std::setw(4) << (*info_it)->heap_key() << " ";
		f_debug1 << endl;
	}
	f_debug1.close();
}
void Optimizer::print_swap_total(vector<Pixel_swap *>& info_set)
{
	int texel = width * height*depth;
	float* swap_txt = new float[texel * 4];
	int index = 0;
	for (int i = 0; i < texel; i++)
	{
		swap_txt[index++] = 0;
		swap_txt[index++] = 0;
		swap_txt[index++] = 0;
		swap_txt[index++] = -1;
	}
	for (vector<Pixel_swap *>::iterator info_it = info_set.begin(); info_it != info_set.end(); info_it++)
	{
		int pos = (*info_it)->face_id * 4;

		swap_txt[pos] = (*info_it)->face_id;
		swap_txt[pos+1] = (*info_it)->from_id;
		swap_txt[pos + 2] = (*info_it)->to_id;
		swap_txt[pos + 3] = (*info_it)->heap_key();
	}


	string txtname = "swap.txt";
	ofstream f_debug1(txtname);
	int slice_index = 0;
	for (int t = 0; t < 3; t++)
	{
		//slice_index = t * width*height * 3;
		for (int i = 0; i < height; i++) {
			//slice_index = (t * slice_num + i * width) * 4;
			for (int j = 0; j < width; j++) {
				f_debug1 << std::left << std::setw(4) << swap_txt[slice_index++] << " ";
				f_debug1 << std::left << std::setw(4) << swap_txt[slice_index++] << " ";
				f_debug1 << std::left << std::setw(4) << swap_txt[slice_index++] << " ";
				f_debug1 << std::left << std::setw(4) << swap_txt[slice_index++] << " ";

			}
			f_debug1 << endl;
		}
		f_debug1 << endl;
		f_debug1 << endl;
	}
	f_debug1.close();
	if (swap_txt) delete[] swap_txt;
}

void Optimizer::update_cluster_information(ClusterIdx i)
{
	CovDet &Cj = __cov[valid_mapping[i]];
	cluster_energy[i] = Cj.energy();
}
double Optimizer::calc_energy_delta(ClusterIdx from, ClusterIdx to, PixelIdx face_id)
{
	unsigned char* data = video_mat.data;
	int slice = width * height;
	int z_ = face_id / slice;
	int y_ = (face_id - z_ * slice) / width;
	int x_ = face_id - z_ * slice - y_ * width;
	Vector6d coor;
	coor << x_, y_, z_, data[3 * face_id], data[3 * face_id + 1], data[3 * face_id + 2];
	CovDet C_init = CovDet(coor, 1.0);

	const CovDet &Ci = __cov[valid_mapping[from]], &Cj = __cov[valid_mapping[to]];
	CovDet C_from = Ci;
	CovDet C_to = Cj;
	double energy_before = cluster_energy[from] + cluster_energy[to];
	C_from -= C_init;
	C_to += C_init;

	
	double energy_from = C_from.energy();
	double energy_to = C_to.energy();
	double energy_after = energy_from + energy_to;

	

	return energy_before - energy_after;
}
void Optimizer::collect_cluster_swaps(int id)
{
	bool full_neighbor;
	int x_, y_, z_, x_new, y_new, z_new;
	int slice = width * height;
	int cluster_closest = 0;
	double energy_saved_closest = 0, energy_saved=0;
	for (set<int>::iterator pixel_it = cluster_boundary_pixels[id].begin(); pixel_it != cluster_boundary_pixels[id].end(); pixel_it++)
	{
		//cout << *pixel_it << ":" << endl;
		cluster_closest = id;
		energy_saved_closest = 0;
		energy_saved = 0;
		Pixel_swap *info = new Pixel_swap();
		full_neighbor = false;
		z_ = (*pixel_it) / slice;
		y_ = ((*pixel_it) - z_ * slice) / width;
		x_ = (*pixel_it) - z_ * slice - y_ * width;
		if (x_ > 0 && x_ < (width - 1) && y_ > 0 && y_ < (height - 1) && z_ > 0 && z_ < (depth - 1))
			full_neighbor = true;
		
		int neighbor_id = 0, cluster_neighbor=0;
		for (int k = -1; k <= 1; k++)
		{
			for (int j = -1; j <= 1; j++)
			{
				for (int i = -1; i <= 1; i++)
				{
					if (k == 0 && j == 0 && i == 0)
						continue;
					x_new = x_ + i;
					y_new = y_ + j;
					z_new = z_ + k;
					if (full_neighbor)
					{
						neighbor_id = z_new * slice + y_new * width + x_new;
						cluster_neighbor = p_sp_belong[neighbor_id];
						if (id != cluster_neighbor)
						{
							//cout << neighbor_id << " in " << cluster_neighbor << endl;
							energy_saved = calc_energy_delta(id, cluster_neighbor,*pixel_it);
							if (energy_saved > energy_saved_closest)
							{
								cluster_closest = cluster_neighbor;
								energy_saved_closest = energy_saved;
							}
						}
					}
					else
					{
						if (x_new >= 0 && x_new < width && y_new >= 0 && y_new < height  && z_new >= 0 && z_new < depth)
						{
							neighbor_id = z_new * slice + y_new * width + x_new;
							cluster_neighbor = p_sp_belong[neighbor_id];
							if (id != cluster_neighbor)
							{
								energy_saved = calc_energy_delta(id, cluster_neighbor, *pixel_it);
								
								if (energy_saved > energy_saved_closest)
								{
									cluster_closest = cluster_neighbor;
									energy_saved_closest = energy_saved;
								}
							}
						}
					}
				}
			}
		}

		if (energy_saved_closest > 0)
		{
			store_swap(energy_saved_closest, id, cluster_closest, *pixel_it, info);
		}
		else
			delete info;



	} 
}
void Optimizer::store_swap(float energy_saved, ClusterIdx from, ClusterIdx to, PixelIdx face_id, Pixel_swap *info)
{
	info->from_id = from;
	info->to_id = to;
	info->face_id = face_id;
	info->heap_key(energy_saved);
	info_set.push_back(info);
}
void Optimizer::collect_cluster_neighbor()
{
	for (int i = 0; i < nClusters; i++)
	{
		cluster_neighbor_links[i].clear();
		cluster_boundary_pixels[i].clear();
	}
	int slice = width * height;
	int x_, y_, z_, x_new, y_new, z_new;
	bool full_neighbor = true;
	for (int c = 0; c < nClusters; c++)
	{
		set<int> neighbors;
		for (set<int>::iterator it = p_clusters[c].begin(); it != p_clusters[c].end(); it++)
		{
			full_neighbor = false;
			z_ = (*it) / slice;
			y_ = ((*it) - z_ * slice) / width;
			x_ = (*it) - z_ * slice - y_ * width;
			if (x_ > 0 && x_ < (width-1) && y_ > 0 && y_ < (height-1)  && z_ > 0 && z_ < (depth-1))
				full_neighbor = true;
			int neighbor_id = 0;
			for (int k = -1; k <= 1; k++)
			{
				for (int j = -1; j <= 1; j++)
				{
					for (int i = -1; i <= 1; i++)
					{
						if (k == 0 && j == 0 && i == 0)
							continue;
						x_new = x_ + i;
						y_new = y_ + j;
						z_new = z_ + k;
						if (full_neighbor)
						{
							neighbor_id = z_new * slice + y_new * width + x_new;
							if (c != p_sp_belong[neighbor_id])
							{
								/*if (c == 391)
								{
									cout << *it <<","<< neighbor_id <<","<< p_sp_belong[neighbor_id] << endl;
								}*/
								cluster_boundary_pixels[c].insert(*it);
							}
							if (c < p_sp_belong[neighbor_id])
							{
								neighbors.insert(p_sp_belong[neighbor_id]);
							}
						}
						else
						{
							if (x_new >= 0 && x_new < width && y_new >= 0 && y_new < height  && z_new >= 0 && z_new < depth)
							{
								neighbor_id = z_new * slice + y_new * width + x_new;
								if (c != p_sp_belong[neighbor_id])
								{									
									cluster_boundary_pixels[c].insert(*it);
								}
								if (c < p_sp_belong[neighbor_id])
								{
									neighbors.insert(p_sp_belong[neighbor_id]);
								}
							}
						}
							
					}
				}
			}
		}

		for (set<int>::iterator it = neighbors.begin(); it != neighbors.end(); it++)
		{
			cluster_pair info;
			info.p1 = c;
			info.p2 = *it;
			cluster_neighbor_links[c].push_back(info);
			cluster_neighbor_links[*it].push_back(info);
		}

	}
}

bool Optimizer::swap_pixel_once()
{
	set<ClusterIdx> recollect_domain;//the domains need update	
	apply_swap_set(info_set, recollect_domain);

	collect_cluster_neighbor();
	for (vector<Pixel_swap *>::iterator it = info_set.begin(); it != info_set.end(); it++)
		delete *it;
	info_set.clear();

	for (set<ClusterIdx>::iterator seed_it = recollect_domain.begin(); seed_it != recollect_domain.end(); seed_it++)
	{
		collect_cluster_swaps(*seed_it);
	}
	return true;
}
void Optimizer::apply_swap_set(vector<Pixel_swap *> info_set, set<ClusterIdx>& recollect_domain)
{
	set<ClusterIdx> update_domain;// the domain sets that need to update information
	update_domain.clear();

	for (vector<Pixel_swap *>::iterator info_it = info_set.begin(); info_it != info_set.end(); info_it++)
	{
		apply_swap(*info_it, update_domain);
	}

	//update information
	for (set<ClusterIdx>::iterator seed_it = update_domain.begin(); seed_it != update_domain.end(); seed_it++)
		update_cluster_information(*seed_it);

	//domain's neighbors also need get updated
	recollect_domain.clear();
	for (set<ClusterIdx>::iterator seed_it = update_domain.begin(); seed_it != update_domain.end(); seed_it++)
	{
		for (uint j = 0; j < cluster_neighbor_links[*seed_it].size(); j++)
		{
			recollect_domain.insert(cluster_neighbor_links[*seed_it][j].p1);
			recollect_domain.insert(cluster_neighbor_links[*seed_it][j].p2);
		}
	}

}

void Optimizer::apply_swap(Pixel_swap* info, set<ClusterIdx>& update_domain)
{
	int cov_from_id = info->from_id;
	int cov_to_id = info->to_id;
	int swap_p = info->face_id;
	CovDet& from = __cov[valid_mapping[cov_from_id]];
	CovDet& to = __cov[valid_mapping[cov_to_id]];

	if (from.area >= 2.0)
	{
		p_clusters[cov_from_id].erase(swap_p);
		p_clusters[cov_to_id].insert(swap_p);
		p_sp_belong[swap_p] = cov_to_id;

		unsigned char* data = video_mat.data;
		int slice = width * height;
		int z_ = swap_p / slice;
		int y_ = (swap_p - z_ * slice) / width;
		int x_ = swap_p - z_ * slice - y_ * width;
		Vector6d coor;
		coor << x_, y_, z_, data[3 * swap_p], data[3 * swap_p + 1], data[3 * swap_p + 2];
		CovDet C_init = CovDet(coor, 1.0);
		//update covariance of v1 and v2 
		from -= C_init;
		to += C_init;

		//collect the domains that need update
		update_domain.insert(cov_from_id);
		update_domain.insert(cov_to_id);
	}
}

void Optimizer::execute_cluster_boundary_swap(float* res)
{
	 //vec4(pId,curr_label,cluster_closest,energy_saved_closest)
	unsigned char* data = video_mat.data;
	int texel_num = width * height*depth;
	int slice = width * height;
	for (int i = 0; i < texel_num; i++)
	{
		if (res[i * 4 + 3] > 0)
		{
			int cov_from_id = int(res[i * 4 + 1]);
			int cov_to_id = int(res[i * 4 + 2]);
			int swap_p = int(floor(res[i * 4]+0.5));//
			swap_p = i;

			CovDet& from = __cov[valid_mapping[cov_from_id]];
			CovDet& to = __cov[valid_mapping[cov_to_id]];
			if (from.area > 2)
			{
				p_clusters[cov_from_id].erase(swap_p);
				p_clusters[cov_to_id].insert(swap_p);
				p_sp_belong[swap_p] = cov_to_id;
								
				int z_ = swap_p / slice;
				int y_ = (swap_p - z_ * slice) / width;
				int x_ = swap_p - z_ * slice - y_ * width;
				Vector6d coor;
				coor << x_, y_, z_, data[3 * swap_p], data[3 * swap_p + 1], data[3 * swap_p + 2];
				CovDet C_init = CovDet(coor, 1.0);
				//update covariance of v1 and v2 
				from -= C_init;
				to += C_init;
			}
		}
	}
	

}

void Optimizer::check_label_contineous()
{
	int sum_ = 0;
	for (int i = 0; i < nClusters; i++)
	{
		set<int>& c_cluster = p_clusters[i];
		if (c_cluster.size() == 0)
		{
			cout << i << " disapear!"<<endl;
		}
		sum_ += c_cluster.size();
		for (set<int>::iterator it = p_clusters[i].begin(); it != p_clusters[i].end(); it++)
		{
			if (p_sp_belong[*it] != i)
			{
				cout << "p_sp_belong["<<*it<<"]="<< p_sp_belong[*it] <<",but in p_cluster["<<i<<"]" << endl;
			}
		}
	}
	if (sum_ != (width*height*depth))
	{
		cout << "sum=" << sum_ << endl;
		cout << "some pixel not in p_clusters!" << endl;
	}
	
	set<int> valid_cluster;
	int texel = width * height*depth;
	for (int i = 0; i < texel; i++)
	{
		valid_cluster.insert(p_sp_belong[i]);
	}
	cout << "valid_cluster end element:" << *(--valid_cluster.end()) << endl;
}