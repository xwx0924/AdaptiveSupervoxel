#ifndef _CVIMAGE_H
#define _CVIMAGE_H
#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <shader.h>
#include <GLFW/glfw3.h>
#include <iomanip>
#include <map>
#include <opencv2/core/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
//#include <opencv2/cudaobjdetect.hpp>
//#include <opencv2/cudaoptflow.hpp>
#include <opencv2/core/opengl.hpp>
#include <Eigen/Dense>
#include "Octree.h"
using namespace std;
using namespace Eigen;

class Image
{
private:

	//float *texels;

	int texel_num;
	int slice_num;
	int width, height, depth;

	float width_f, height_f, depth_f;
	float width_ratio, height_ratio, depth_ratio;
	int contracted_num;
	int final_label_num;
	int current_buffer;
	float compactness;
	float ratio;
	string dbname;


	GLuint frameBuffer_image;
	GLuint texture_image[2];
	GLenum buffers_image[2];

	GLuint frameBuffer_flood;
	GLuint texture_flood[2];
	GLenum buffers_flood[2];
	GLuint frameBuffer_cluster;
	GLuint texture_cluster[1];
	GLenum buffers_cluster[1];
	int seed_color_index;//3
	unsigned int VBO_s, VAO_s;
	unsigned int VBO_all_t, VAO_all_t;
	unsigned int VBO_s_index, VAO_s_index;
	//unsigned int VBO_test, VAO_test;
	
	// for gradient
	//--------------------
	double * gradient;
	double gradient_sum;
	
	int* render_color;
	//--------------------

	
	



public:

	Image();
	~Image();
	bool init_gpu(string& dbname, unsigned char* data, int width, int height, int depth, int seeds_num);
	void RGB2LAB(Shader& rgb2lab);
	bool init_image_texture(unsigned char* data);
	bool init_flood_texture();
	bool init_cluster_texture();
	void init_vaos();
	void gradient_info(Shader& derivative_t, Shader& derivative_sum);
	void build_octree(int leaf_size);
	void draw_octree();
	void print_node_render(int * node_label, int node_num);
	void init_cluster();
	void decimate();
	void decimate_s();
	void organize_cluster_info();
	void draw_decimate();
	void print_decimate_render(int contracted_num,int * leaf_label, int* leaf_belong);
	void print_swap_render(float * pixel_label,int swap_iter, string dirName);
	void cluster_boundary_swaps(Shader& boundary_swap, int swap_iter);
	void check_label_contineous();
	void update_cluster_info();
	void init_pixel_swap();
	void swap_once(int swap_iter);
	void test();
	void print_swap_supervoxel(int swap_iter, string dirName);
	void print_swap_seg(unsigned char* data, string dirName);
	void print_swap_txt(int swap_iter, string dirName);
private:
	void print_buffer(int index);
	void print_buffer3(int index);
	void print_buffer1(int index); 
	void print_cov(int index);

};
#endif