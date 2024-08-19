#ifndef DOMAIN_H
#define DOMAIN_H
#include<vector>
#include <set>
#include <ctime>
#include <iostream>
typedef int PixelID;
using namespace std;
class Partition
{
public:
	set<PixelID> * clusters;
	int * cluster_belong;
	int size;
	

public:
	Partition(int leaf_num)
	{
		size = leaf_num;
		
		clusters = new set<PixelID>[leaf_num];//nodes in cluster
		cluster_belong = new int[leaf_num];//which cluster a node belongs to 


		for (int i = 0; i < leaf_num; i++)
		{
			clusters[i].insert(i);
			cluster_belong[i] = i;
		}
	}
	

	~Partition()
	{
		delete[] clusters;
		delete[] cluster_belong;
		std::cout << "destroy partition" << std::endl;
	}

	void mergeDomain(int v1, int v2)
	{
		//update containing and belonging information
		for (set<PixelID>::iterator it = clusters[v2].begin(); it != clusters[v2].end(); it++)
		{
			clusters[v1].insert(*it);
			cluster_belong[*it] = v1;
		}
		clusters[v2].clear();
	}
};

#endif