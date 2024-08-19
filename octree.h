#ifndef _OCTREE_H
#define _OCTREE_H
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <set>
#include <cassert>
using namespace std;

class OctreeNode
{
private:
	double			  _length;
	double            _center[3];
	std::vector<int>  _members;

public:
	OctreeNode()
		: _length(1)
	{
		_center[0] = 0;
		_center[1] = 0;
		_center[2] = 0;
	}

	OctreeNode(const double *nodeCent, double nodeLength)
		: _length(nodeLength)
	{
		_center[0] = nodeCent[0];
		_center[1] = nodeCent[1];
		_center[2] = nodeCent[2];
	}

	OctreeNode(const OctreeNode &rhs)
		: _length(rhs._length)
	{
		_center[0] = rhs._center[0];
		_center[1] = rhs._center[1];
		_center[2] = rhs._center[2];
		_members = rhs._members;
	}

	~OctreeNode()
	{

	}

	OctreeNode& operator= (const OctreeNode &rhs)
	{
		if (this != &rhs)
		{
			_center[0] = rhs._center[0];
			_center[1] = rhs._center[1];
			_center[2] = rhs._center[2];
			_length = rhs._length;
			_members = rhs._members;
		}

		return *this;
	}

	void add_member(int i)
	{
		_members.push_back(i);
	}

	size_t members_number() const
	{
		return _members.size();
	}

	const std::vector<int>* members_pointer() const
	{
		return &_members;
	}

	double length() const
	{
		return _length;
	}

	void set_length(double len)
	{
		_length = len;
	}

	const double* center() const
	{
		return _center;
	}

	void set_center(const double *c)
	{
		_center[0] = c[0];
		_center[1] = c[1];
		_center[2] = c[2];
	}
};



class AdaptiveOctree
{

private:
	int  					_leafSize;
	int                     _maxNumber;
	double                  _maxGradient;
	vector<OctreeNode*>     _leaves;
	int*                    _pixel_leaf_label;
	vector<set<int>>        _leaf_neighbors;
	// input
	const double            *_gradients;
	int                      _pnum;
	int                      _videoWidth;
	int                      _videoHeight;
	int                      _videoDepth;
	double					 _slice;
	

public:
	
	AdaptiveOctree()
	{

	}
	void init(const double *gradients, int width, int height, int depth, int _leafS = 4, int _maxN = 512, double _maxG = 30)
	{
		_videoWidth = width;
		_videoHeight = height;
		_videoDepth = depth;
		_leafSize = _leafS;
		_maxNumber = _maxN;
		_maxGradient = _maxG;
		_gradients = gradients;
		_pnum = width * height*depth;
		_slice = width * height;
		_pixel_leaf_label = NULL;
	}
	OctreeNode* get_leaf(int i)
	{
		return _leaves[i];
	}

	~AdaptiveOctree()
	{
		clear();
		cout << "destroy tree" << endl;
	}

	void clear()
	{
		int nb = (int)_leaves.size();
		for (int i = 0; i < nb; ++i)
		{
			if (_leaves[i])
			{
				delete _leaves[i];
				_leaves[i] = NULL;
			}
		}
		_leaves.clear();

		if (_pixel_leaf_label) delete[] _pixel_leaf_label;
	}

	void set_max_number(int nb)
	{
		_maxNumber = nb;
	}

	void set_max_gradient(double grad)
	{
		_maxGradient = grad;
	}

	void set_leaf_size(int l_s)
	{
		_leafSize = l_s;
	}
	int get_video_width()
	{
		return _videoWidth;
	}
	int get_video_height()
	{
		return _videoHeight;
	}
	int get_video_depth()
	{
		return _videoDepth;
	}
	size_t nodes_number() const
	{
		return _leaves.size();
	}

	vector<set<int>>& get_leaf_neighbors()
	{
		return _leaf_neighbors;
	}


	void pixel_leaf()
	{
		_pixel_leaf_label = new int[_videoWidth*_videoHeight*_videoDepth];
		int nb = (int)_leaves.size();
		cout << "number of node:" << nb << endl;
		
		for (int i = 0; i < nb; ++i)
		{
			OctreeNode *currentNode = _leaves[i];
			for (int k1 = 0; k1 < (int)currentNode->members_number(); ++k1)
			{
				int v1 = (*currentNode->members_pointer())[k1];
				_pixel_leaf_label[v1] = i;
			}

			/*if (currentNode->length() == 1)
				cout << i << endl;*/
		}

		
	}
	
	int* get_pixel_leaf_label()
	{
		if(_pixel_leaf_label==NULL)
			pixel_leaf();
		
		return _pixel_leaf_label;
	}

	void build_tree()
	{
		clear();

		int n_width = int(_videoWidth / _leafSize + 1);
		int n_height = int(_videoHeight / _leafSize + 1);
		int n_depth = int(_videoDepth / _leafSize + 1);

		_leaves.resize(n_width * n_height * n_depth, NULL);
		int slice = _videoWidth * _videoHeight;
		int n_i=0, n_j=0, n_k=0;
		int node_id = 0;
		int p_id = 0;
		for (int k = 0; k < _videoDepth; k++)
		{
			for (int j = 0; j < _videoHeight; j++)
			{
				for (int i = 0; i < _videoWidth; i++)
				{
					n_i = i / _leafSize;
					n_j = j / _leafSize;
					n_k = k / _leafSize;
					node_id = n_k * n_width*n_height + n_j * n_width + n_i;
					if (!_leaves[node_id])
					{
						double nodeCent[3] = {
							 double(n_i + 0.5) * _leafSize,
					         double(n_j + 0.5) * _leafSize,
							 double(n_k + 0.5) * _leafSize };
						_leaves[node_id] = new OctreeNode(nodeCent, _leafSize);
					}
					_leaves[node_id]->add_member(p_id);

					p_id++;
				}
			}
		}

	
		cout << "node_num:" << _leaves.size() << endl;
		//subdivide_tree_test();
		subdivide_tree();
		cout << "node_num after subdivision:" << _leaves.size()<<endl;
		cout << "collect pixel-leaf label" << endl;
		pixel_leaf();

		leaf_neighbors();
	}

	OctreeNode* node(int index)
	{
		if (_leaves.empty())
			return NULL;

		return _leaves[index];
	}

protected:
	
	void leaf_neighbors_test()
	{
		/*_leaf_neighbors = vector<set<int>>(_leaves.size(), set<int>());*/
		set<int> l_n_set;

		/*
		  calculate the neighbors of pixels on six planes of a node. */
		int mo = 0;
		OctreeNode *currentNode = _leaves[mo];
		const double* center = currentNode->center();
		double length = currentNode->length();
		cout << "leaf " << 0 << "center:(" << center[0] << "," << center[1] << "," << center[2] << "), elements:";
		
		
		int fix = 0, s1 = 0, s2 = 0, f_v=0,s1_v=0,s2_v=0;
		int fix_axis = 0;
		int video_box[3] = { _videoWidth,_videoHeight,_videoDepth };
		int last_n_label = -1, curr_n_label=0;
		for (int dd = 0; dd < 3; dd++)
		{
			if (dd == 0)
			{
				fix = 0; s1 = 1; s2 = 2; //left and right
				f_v = 1; s1_v = _videoWidth; s2_v = _slice;
			}
			else if (dd == 1)
			{
				fix = 1; s1 = 0; s2 = 2; //up and down
				f_v = _videoWidth; s1_v = 1; s2_v = _slice;
			}
			else
			{
				fix = 2; s1 = 0; s2 = 1; //front and back
				f_v = _slice; s1_v = 1; s2_v = _videoWidth;
			}

			for (int d = 0; d < 2; d++)
			{
				if (d == 0)
					fix_axis = center[fix] - (0.5*length) - 1;
				else
					fix_axis = center[fix] + (0.5*length);
				int start_1 = center[s1] - (0.5*length);
				int start_2 = center[s2] - (0.5*length);
				int n_index = 0;
				if (fix_axis >= 0 && fix_axis < video_box[fix])
				{
					int end_1 = start_1 + length;
					int end_2 = start_2 + length;
					end_1 = end_1 < video_box[s1] ? end_1 : video_box[s1];
					end_2 = end_2 < video_box[s2] ? end_2 : video_box[s2];
					for (int j = start_2; j < end_2; j++)
					{
						for (int i = start_1; i < end_1; i++)
						{
							n_index = fix_axis * f_v + j * s2_v + i * s1_v;
							//cout << n_index << ",";
							curr_n_label = _pixel_leaf_label[n_index];
							if (curr_n_label != last_n_label)
							{
								last_n_label = curr_n_label;
								l_n_set.insert(curr_n_label);
								//cout << "k";
							}
							
						}
					}
				}
				//cout<<endl;
			}

		}
		
		
		set<int> l_n_temp;
		int n_off[6]= {-1,1,-_videoWidth,_videoWidth,-_slice,_slice};
		int v_ng = 0,v_ng_label=0;
		int max_index = _slice * _videoDepth;
		for (int k1 = 0; k1 < (int)currentNode->members_number(); ++k1)
		{
			int v = (*currentNode->members_pointer())[k1];
			int v_d = v / _slice;
			int v_h = (v - v_d * _slice) / _videoWidth;
			int v_w = v - v_d * _slice - v_h * _videoWidth;
			if (v_w > 0)
			{
				v_ng = v - 1;
				v_ng_label = _pixel_leaf_label[v_ng];
				if (v_ng_label != mo)
					l_n_temp.insert(v_ng_label);
			}
			if (v_w < _videoWidth - 1)
			{
				v_ng = v + 1;
				v_ng_label = _pixel_leaf_label[v_ng];
				if (v_ng_label != mo)
					l_n_temp.insert(v_ng_label);
			}
			if (v_h > 0)
			{
				v_ng = v - _videoWidth;
				v_ng_label = _pixel_leaf_label[v_ng];
				if (v_ng_label != mo)
					l_n_temp.insert(v_ng_label);
			}
			if (v_h < _videoHeight-1)
			{
				v_ng = v + _videoWidth;
				v_ng_label = _pixel_leaf_label[v_ng];
				if (v_ng_label != mo)
					l_n_temp.insert(v_ng_label);
			}
			if (v_d > 0)
			{
				v_ng = v - _slice;
				v_ng_label = _pixel_leaf_label[v_ng];
				if (v_ng_label != mo)
					l_n_temp.insert(v_ng_label);
			}
			if (v_d < _videoDepth-1)
			{
				v_ng = v + _slice;
				v_ng_label = _pixel_leaf_label[v_ng];
				if (v_ng_label != mo)
					l_n_temp.insert(v_ng_label);
			}
			
		}

		//compare l_n_set and l_n_temp
		
		for (int ele : l_n_set)
		{
			cout << ele << ",";
		}
		cout << endl;
		for (int ele : l_n_temp)
		{
			cout << ele << ",";
		}
		cout << endl;
	}



	void leaf_neighbors()
	{
		int nb = _leaves.size();
		_leaf_neighbors.reserve(nb);
		int fix = 0, s1 = 0, s2 = 0, f_v = 0, s1_v = 0, s2_v = 0;
		int fix_axis = 0;
		int video_box[3] = { _videoWidth,_videoHeight,_videoDepth };
		int last_n_label = -1, curr_n_label = 0;

		for (int mo = 0; mo < nb; ++mo)
		{
			set<int> l_n_set;


			OctreeNode *currentNode = _leaves[mo];
			const double* center = currentNode->center();
			double length = currentNode->length();


			last_n_label = -1;
			for (int dd = 0; dd < 3; dd++)
			{
				if (dd == 0)
				{
					fix = 0; s1 = 1; s2 = 2; //left and right
					f_v = 1; s1_v = _videoWidth; s2_v = _slice;
				}
				else if (dd == 1)
				{
					fix = 1; s1 = 0; s2 = 2; //up and down
					f_v = _videoWidth; s1_v = 1; s2_v = _slice;
				}
				else
				{
					fix = 2; s1 = 0; s2 = 1; //front and back
					f_v = _slice; s1_v = 1; s2_v = _videoWidth;
				}

				for (int d = 0; d < 2; d++)
				{
					if (d == 0)
						fix_axis = center[fix] - (0.5*length) - 1;
					else
						fix_axis = center[fix] + (0.5*length);
					int start_1 = center[s1] - (0.5*length);
					int start_2 = center[s2] - (0.5*length);
					int n_index = 0;
					if (fix_axis >= 0 && fix_axis < video_box[fix])
					{
						int end_1 = start_1 + length;
						int end_2 = start_2 + length;
						end_1 = end_1 < video_box[s1] ? end_1 : video_box[s1];
						end_2 = end_2 < video_box[s2] ? end_2 : video_box[s2];
						for (int j = start_2; j < end_2; j++)
						{
							for (int i = start_1; i < end_1; i++)
							{
								n_index = fix_axis * f_v + j * s2_v + i * s1_v;
								curr_n_label = _pixel_leaf_label[n_index];
								if (curr_n_label != last_n_label)
								{
									last_n_label = curr_n_label;
									l_n_set.insert(curr_n_label);
								}

							}
						}
					}
				}

			}

			_leaf_neighbors.push_back(l_n_set);

		}
	}



	void subdivide_tree_test()
	{

		bool hasSplit = false;

		vector<OctreeNode*> _leaves_test;
		_leaves_test.push_back(_leaves[0]);
		std::vector<OctreeNode*> tempLeaves;
		

		OctreeNode *currentNode = _leaves_test[0];
		for (int k1 = 0; k1 < (int)currentNode->members_number(); ++k1)
		{
			int v1 = (*currentNode->members_pointer())[k1];
			cout << v1 << ",";
		}
		cout << endl;
		cout << endl;

		hasSplit = true;
		std::vector<OctreeNode*> newNodes = subdivide_node(currentNode);

		for (int k = 0; k < newNodes.size(); ++k)
		{
			if (newNodes[k]->members_number() > 0)
				tempLeaves.push_back(newNodes[k]);
			else
			{
				delete newNodes[k];
				newNodes[k] = NULL;
			}
		}
		newNodes.clear();
		
		for (int k = 0; k < tempLeaves.size(); ++k)
		{
			OctreeNode *temp = tempLeaves[k];
			for (int k1 = 0; k1 < (int)temp->members_number(); ++k1)
			{
				int v1 = (*temp->members_pointer())[k1];
				cout << v1 << ",";
			}
			cout << endl;
		}
	
	}

	void subdivide_tree()
	{
		while (1)
		{
			bool hasSplit = false;

			std::vector<OctreeNode*> tempLeaves;

			int nb = (int)_leaves.size();
			for (int i = 0; i < nb; ++i)
			{
				OctreeNode *currentNode = _leaves[i];
				if (currentNode == NULL)
					continue;

				if (need_split(currentNode))
				{
					hasSplit = true;
					std::vector<OctreeNode*> newNodes = subdivide_node(currentNode);

					for (int k = 0; k < newNodes.size(); ++k)
					{
						if (newNodes[k]->members_number() > 0)
							tempLeaves.push_back(newNodes[k]);
						else
						{
							delete newNodes[k];
							newNodes[k] = NULL;
						}
					}
					newNodes.clear();

					delete currentNode;
					currentNode = NULL;
					_leaves[i] = NULL;
				}
				else
					tempLeaves.push_back(currentNode);
			}

			_leaves.swap(tempLeaves);

			if (!hasSplit)
				break;
		}
	}

	bool need_split(OctreeNode *node)
	{
		if (node->members_number() > _maxNumber)
			return true;

		if (!_gradients)
			return false;
		if (node->length() < 1.1)
			return false;
		double gradient_sum = 0;
		for (int k1 = 0; k1 < (int)node->members_number(); ++k1)
		{
			int v1 = (*node->members_pointer())[k1];
			gradient_sum += _gradients[v1];
			if (gradient_sum > _maxGradient)
				return true;
		}
		return false;
	}

	std::vector<OctreeNode*> subdivide_node(OctreeNode *node)
	{
		double nodeLength = node->length();
		const double *nodeCenter = node->center();

		std::vector<OctreeNode*> newNodes(8, NULL);

		for (int i = 0; i < 8; ++i)
		{
			int xi = i % 2;
			int yi = (i / 2) % 2;
			int zi = (i / 4) % 2;

			double newNodeCenter[3] =
			{ nodeCenter[0] + std::pow(-1, xi + 1) * nodeLength / 4,
				nodeCenter[1] + std::pow(-1, yi + 1) * nodeLength / 4,
				nodeCenter[2] + std::pow(-1, zi + 1) * nodeLength / 4 };
			newNodes[i] = new OctreeNode(newNodeCenter, nodeLength / 2);
		}

		for (int k = 0; k < (int)node->members_number(); ++k)
		{
			int v = (*node->members_pointer())[k];
			double v_d = floor(v / _slice);
			double v_h = floor((v - v_d * _slice) / _videoWidth);
			double v_w = v - v_d * _slice - v_h * _videoWidth;
			int loc = 0;
			if (v_w >= nodeCenter[0])//>=
				loc += 1;
			if (v_h >= nodeCenter[1])
				loc += 2;
			if (v_d >= nodeCenter[2])
				loc += 4;

			newNodes[loc]->add_member(v);
		}

		return newNodes;
	}
};
#endif