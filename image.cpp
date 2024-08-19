#include "image.h"
#include "optimizer.h"
#include "partition.h"
#include "octree.h"
#define PI 3.141592653
extern Optimizer* optimizer;
extern AdaptiveOctree* tree;
extern Partition* tess;
extern map<int, int> valid_map;
Image::Image()
{
	width = 100;
	height = 100;
	depth = 0;
	gradient = NULL;
	gradient_sum = 0;
}
Image::~Image()
{
	if (render_color) delete[] render_color;
}

void Image::init_vaos()
{
	int medium_num = 2000;
	if (medium_num < contracted_num)
		medium_num = contracted_num;

	render_color = new int[medium_num * 3];
	for (int i = 0; i < medium_num * 3; i++)
	{
		render_color[i] = rand() % 256;
	}


	vector<short> all_texels_int;
	all_texels_int.reserve(texel_num * 3);
	for (int i = 0; i < depth; i++)
	{
		for (int j = 0; j < height; j++)
		{
			for (int t = 0; t < width; t++)
			{
				all_texels_int.push_back(short(t));
				all_texels_int.push_back(short(j));
				all_texels_int.push_back(short(i));
			}
		}
	}

	glGenVertexArrays(1, &VAO_all_t);
	glGenBuffers(1, &VBO_all_t);
	glBindVertexArray(VAO_all_t);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_all_t);
	glBufferData(GL_ARRAY_BUFFER, all_texels_int.size() * sizeof(short), &all_texels_int[0], GL_STATIC_DRAW);//sizeof(pos)只能当pos时数组时才能这样用。比如float pos[]={1,1,1};,这样的话sizeof(pos)=3*sizeof(float)
	glVertexAttribIPointer(0, 3, GL_SHORT, 3 * sizeof(short), (void*)0);
	glEnableVertexAttribArray(0);
}
bool Image::init_gpu(string& dbname_, unsigned char* data, int width_, int height_, int depth_, int contracted_num_)
{
	dbname = dbname_;
	width = width_;
	height = height_;
	depth = depth_;
	width_f = float(width);
	height_f = float(height);
	depth_f = float(depth);
	width_ratio = 1.0 / width_f;
	height_ratio = 1.0 / height_f;
	depth_ratio = 1.0 / depth_f;
	slice_num = width * height;
	texel_num = slice_num * depth;
	contracted_num = contracted_num_;
	//cout << width << "," << height << "," << depth << endl;
	init_image_texture(data);
	init_flood_texture();
	init_cluster_texture();
	init_vaos();
	glEnable(GL_TEXTURE_3D);
	glViewport(0, 0, width, height);
	glDisable(GL_DEPTH_TEST);

	
	return true;
}


bool Image::init_cluster_texture()
{
	int cov_bits = 7;
	frameBuffer_cluster = 0;
	buffers_cluster[0] = GL_COLOR_ATTACHMENT0;
	glGenFramebuffers(1, &frameBuffer_cluster);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer_cluster);

	glGenTextures(1, texture_cluster);
	glBindTexture(GL_TEXTURE_3D, texture_cluster[0]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, cov_bits, contracted_num, 1, 0, GL_RGB,
		GL_FLOAT, NULL);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_cluster[0], 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		printf("something wrong about framebuffer!");
		return false;
	}
}

bool Image::init_flood_texture()
{
	frameBuffer_flood = 0;
	buffers_flood[0] = GL_COLOR_ATTACHMENT0;
	buffers_flood[1] = GL_COLOR_ATTACHMENT1;

	glGenFramebuffers(1, &frameBuffer_flood);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer_flood);


	for (unsigned int i = 0; i < 1; i++)
	{
		glGenTextures(1, texture_flood + i);
		glBindTexture(GL_TEXTURE_3D, texture_flood[i]);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, width, height, depth, 0, GL_RGBA,
			GL_FLOAT, NULL);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, texture_flood[i], 0);
	}




	glGenTextures(1, texture_flood + 1);
	glBindTexture(GL_TEXTURE_3D, texture_flood[1]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, width, height, depth, 0, GL_RED,
		GL_FLOAT, NULL);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1, texture_flood[1], 0);


	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		printf("something wrong about framebuffer!");
		return false;
	}
}



bool Image::init_image_texture(unsigned char* data)
{
	frameBuffer_image = 0;
	buffers_image[0] = GL_COLOR_ATTACHMENT0;
	buffers_image[1] = GL_COLOR_ATTACHMENT1;
	/*buffers_image[2] = GL_COLOR_ATTACHMENT2;
	buffers_image[3] = GL_COLOR_ATTACHMENT3;*/
	glGenFramebuffers(1, &frameBuffer_image);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer_image);
	glGenTextures(1, texture_image + 0);
	glBindTexture(GL_TEXTURE_3D, texture_image[0]);
	if (data)
	{
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, width, height, depth, 0, GL_RGB,
			GL_UNSIGNED_BYTE, data);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_image[0], 0);
	}
	else
	{
		std::cout << "Failed to load 3d texture!" << std::endl;
	}

	/*glBindTexture(GL_TEXTURE_3D, texture_image[0]);
	print_buffer3(0);*/


	for (unsigned int i = 1; i < 2; ++i)
	{
		glGenTextures(1, texture_image + i);
		glBindTexture(GL_TEXTURE_3D, texture_image[i]);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, width, height, depth, 0, GL_RGB,
			GL_FLOAT, NULL);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, texture_image[i], 0);
	}


	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		printf("something wrong about image framebuffer!");
		return false;
	}


}

void Image::gradient_info(Shader& derivative_t, Shader& derivative_sum)
{
	derivative_t.use();
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer_image);
	glDrawBuffers(1, buffers_image + 1);
	glClearColor(3.0f, -1.0f, -1.0f, -1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, texture_image[0]);
	derivative_t.setInt("texels", 0);
	derivative_t.setFloat("width", width_f);
	derivative_t.setFloat("height", height_f);
	derivative_t.setFloat("depth", depth_f);
	glBindVertexArray(VAO_all_t);
	glDrawArrays(GL_POINTS, 0, texel_num);

	glBindTexture(GL_TEXTURE_3D, texture_image[1]);
	//print_buffer3(1);
	float* res1 = new float[texel_num * 3];//GL_RGB
	glGetTexImage(GL_TEXTURE_3D, 0, GL_RGB, GL_FLOAT, res1);
	gradient = new double[texel_num];
	int id = 0;
	for (int i = 0; i < texel_num; i++)
	{
		gradient[i] = res1[id];
		id += 3;
	}
	




	//calculate sum of gradient
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glBlendEquation(GL_FUNC_ADD);
	derivative_sum.use();
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer_flood);
	glDrawBuffers(1, buffers_flood);
	glClearColor(3.0f, 0.0f, -1.0f, -1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, texture_image[1]);
	derivative_sum.setInt("gradient", 0);
	derivative_sum.setFloat("width", width_f);
	derivative_sum.setFloat("height", height_f);
	derivative_sum.setFloat("depth", depth_f);
	glBindVertexArray(VAO_all_t);
	glDrawArrays(GL_POINTS, 0, texel_num);
	glDisable(GL_BLEND);

	glBindTexture(GL_TEXTURE_3D, texture_flood[0]);
	//print_buffer(0);

	int read_height = 1;
	float* res2 = new float[4 * read_height*width];
	GLuint fboId = 0;
	glGenFramebuffers(1, &fboId);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fboId);
	glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		texture_flood[0], 0, 0);
	glReadPixels(0, 0, width, read_height, GL_RGBA, GL_FLOAT, res2);

	gradient_sum = res2[0];
	cout <<"gradient_sum:"<< gradient_sum << endl;
	//int pixel_num = int(res2[1]);
	//cout<< "pixel_num:" << pixel_num << endl;
	//assert(pixel_num == texel_num);

	if (res1) delete[] res1;
	if (res2) delete[] res2;
}

void Image::build_octree(int leaf_size)
{
	if (tree != NULL)
		delete tree;
	if (gradient == NULL)
	{
		cout << "error,calculate gradient first!" << endl;
	}
	else
	{
		double ratio = 0.8;
		double max_gradient = (gradient_sum / (width*height*depth) * leaf_size*leaf_size*leaf_size*ratio);
		cout << "max_gradient:" << max_gradient << endl;
		tree = new AdaptiveOctree();
		tree->init(&gradient[0], width, height, depth, leaf_size, 512, max_gradient);
		tree->set_leaf_size(leaf_size);
		tree->build_tree();
		
		if (gradient) delete[] gradient;
	}

	
}



void Image::init_cluster()
{
	if (optimizer != NULL)
		delete optimizer;

	optimizer = new Optimizer();
	optimizer->init_cov(contracted_num);
	
}

void Image::decimate_s()
{
	if (optimizer != NULL)
	{
		//double decimate_t = get_cpu_time();
		optimizer->decimate_s();
		//decimate_t = get_cpu_time() - decimate_t;
		//cerr << "decimate  use time "<< decimate_t << " sec" << endl;
	}
}



void Image::decimate()
{
	int adjust_num = 1500;
	if (optimizer != NULL)
	{
		if (contracted_num >= adjust_num)
		{
			optimizer->decimate(contracted_num);

		}
		else
		{
			optimizer->decimate(adjust_num);


			optimizer->decimate_s();

		}
		
	}
}


void Image::draw_octree()
{
	int* leaf_label = tree->get_pixel_leaf_label();
	print_node_render(leaf_label, tree->nodes_number());
}

void Image::draw_decimate()
{
	int* leaf_label = tree->get_pixel_leaf_label();//pixel->leaf
	int* leaf_belong = tess->cluster_belong;//leaf->cluster
	print_decimate_render(contracted_num, leaf_label, leaf_belong);
}

void Image::init_pixel_swap()
{

	optimizer->relabel_cluster_label();
	//optimizer->print_label();
	optimizer->init_pixel_swap();
}

void Image::swap_once(int swap_iter)
{
	cout << "swap iter " << swap_iter << endl;
	optimizer->swap_pixel_once();
	float* p_sp_belong = optimizer->get_cluster_label();
	string a = "cpu_swap";
	print_swap_txt(0, a);
}


void Image::cluster_boundary_swaps(Shader& boundary_swap,int swap_iter)
{
	boundary_swap.use();
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer_flood);
	glDrawBuffers(1, buffers_flood);
	glClearColor(3.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, texture_image[0]);
	boundary_swap.setInt("image", 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, texture_flood[1]);
	boundary_swap.setInt("seg", 1);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_3D, texture_cluster[0]);
	boundary_swap.setInt("clusterCov", 2);
	boundary_swap.setFloat("width", width_f);
	boundary_swap.setFloat("height", height_f);
	boundary_swap.setFloat("depth", depth_f);
	boundary_swap.setFloat("ratio", CovDet::ratio);
	boundary_swap.setFloat("contracted_num", contracted_num);
	glBindVertexArray(VAO_all_t);
	glDrawArrays(GL_POINTS, 0, texel_num);

	glBindTexture(GL_TEXTURE_3D, texture_flood[0]);
	//print_buffer(0);
	float* res1 = new float[texel_num * 4];//GL_RGBA
	glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_FLOAT, res1);

	
	optimizer->execute_cluster_boundary_swap(res1);

	
	if (res1) delete[] res1;
}
void Image::check_label_contineous()
{
	optimizer->check_label_contineous();
}


void Image::print_swap_supervoxel(int swap_iter,string dirName)
{
	float* p_sp_belong = optimizer->get_cluster_label();
	print_swap_render(p_sp_belong, swap_iter, dirName);
}
void Image::print_swap_seg(unsigned char* data, string dirName)
{
	float* p_sp_belong = optimizer->get_cluster_label();
	
	
	int index = 0;
	int index_r = 0, index_d = 0;
	int index3 = 0;
	for (int k = 0; k < depth; k++)
	{
		for (int i = 0; i < height - 1; i++)
		{
			for (int j = 0; j < width - 1; j++)
			{
				index = k * slice_num + i * width + j;
				index_r = index + 1;
				index_d = index + width;

				if ((p_sp_belong[index] != p_sp_belong[index_r]) || (p_sp_belong[index] != p_sp_belong[index_d]))
				{
					data[3 * index] = 255;
					data[3 * index + 1] = 0;
					data[3 * index + 2] = 0;
				}
			}
		}
	}

	unsigned char* slice_start = NULL;
	//namedWindow("Display window", cv::WINDOW_AUTOSIZE);
	for (int i = 0; i < depth; i++)
	{
		if (i == 0)
			slice_start = data;
		else
			slice_start = slice_start + 3 * slice_num;
		cv::Mat seg(cv::Size(width, height), CV_8UC3, slice_start);
		cvtColor(seg, seg, cv::COLOR_BGR2RGB);
		//flip(seg, seg, 0);//rows-i-1,j
		char savepath[1024];
		string a = dirName + "%05d.png";
		sprintf_s(savepath, a.c_str(), i + 1);
		imwrite(savepath, seg);

	}
}

void Image::print_swap_txt(int swap_iter,string dirName)
{
	float* p_sp_belong = optimizer->get_cluster_label();
	string save_name = dirName + ".txt";
	ofstream f_debug1(save_name);
	int p_index = 0;
	//p_index = width * height * 66;
	for (int k = 0; k < depth; k++)
	{
		for (int j = 0; j < height; j++)
		{
			for (int i = 0; i < width; i++)
			{
				f_debug1 << std::left << std::setw(4) << p_sp_belong[p_index++] << " ";
			}
			f_debug1 << endl;
		}
		f_debug1 << endl;
		f_debug1 << endl;
	}
	f_debug1.close();
}

void Image::update_cluster_info()
{
	float* p_sp_belong = optimizer->get_cluster_label();
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer_flood);
	glBindTexture(GL_TEXTURE_3D, texture_flood[1]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, width, height, depth, 0, GL_RED,
		GL_FLOAT, &p_sp_belong[0]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1, texture_flood[1], 0);
	//print_buffer1(1);

	vector<float> cluster_cov_info = vector<float>(contracted_num * 21, -1);//7*3
	optimizer->collect_cov_info(cluster_cov_info);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer_cluster);
	glBindTexture(GL_TEXTURE_3D, texture_cluster[0]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, 7, contracted_num, 1, 0, GL_RGB,
		GL_FLOAT, &cluster_cov_info[0]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_cluster[0], 0);
	//print_cov(0);
}
void Image::test()
{

	optimizer->test();
}
void Image::organize_cluster_info()
{
	/*
	1. pixel belongs to which cluster(0-200)；2. cluste's neighbors，cov
	*/
	optimizer->relabel_cluster_label();
	float* p_sp_belong = optimizer->get_cluster_label();

	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer_flood);
	glBindTexture(GL_TEXTURE_3D, texture_flood[1]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, width, height, depth, 0, GL_RED,
		GL_FLOAT, &p_sp_belong[0]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1, texture_flood[1], 0);
	//print_buffer1(1);



	//optimizer->update_energy_ratio();
	CovDet::ratio = 0.1;

	vector<float> cluster_cov_info = vector<float>(contracted_num*21,-1);//7*3
	optimizer->collect_cov_info(cluster_cov_info);

	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer_cluster);
	glBindTexture(GL_TEXTURE_3D, texture_cluster[0]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, 7, contracted_num, 1, 0, GL_RGB,
		GL_FLOAT, &cluster_cov_info[0]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_cluster[0], 0);
	//print_cov(0);



	

}

void Image::print_swap_render(float * pixel_label,int swap_iter,string dirName)
{
	cv::Mat render(cv::Size(width, height*depth), CV_8UC3, cv::Scalar(0, 0, 0));
	int label_index = 0;
	int label_va = 0;
	int p2l2c = 0;
	uchar* ptr = NULL;
	for (int i = 0; i < (depth*height); i++)
	{
		ptr = render.ptr<uchar>(i);
		for (int j = 0; j < width; j++)
		{
			p2l2c = int(pixel_label[label_index++]);
			label_va = 3 * p2l2c;
			if (label_va < 0)
			{
				*(ptr++) = 0;
				*(ptr++) = 0;
				*(ptr++) = 0;
			}
			else
			{
				*(ptr++) = render_color[label_va];
				*(ptr++) = render_color[label_va + 1];
				*(ptr++) = render_color[label_va + 2];
			}

		}
	}

	unsigned char* slice_start = NULL;
	for (int i = 0; i < depth; i++)
	{
		if (i == 0)
			slice_start = render.data;
		else
			slice_start = slice_start + 3 * (width*height);


		cv::Mat seg(cv::Size(width, height), CV_8UC3, slice_start);
		cv::cvtColor(seg, seg, cv::COLOR_BGR2RGB);

		char savepath[1024];

		//sprintf_s(savepath, "swap\\parachute_gpu\\%05d_%d.png", i + 1, swap_iter);
		string a = dirName + "%05d.png";
		sprintf_s(savepath, a.c_str(), i+1);
		imwrite(savepath, seg);

	}

}


void Image::print_decimate_render(int contracted_num,int * leaf_label, int* leaf_belong)
{
	cv::Mat render(cv::Size(width, height*depth), CV_8UC3, cv::Scalar(0, 0, 0));
	/*int* render_color = new int[contracted_num * 3];
	for (int i = 0; i < contracted_num * 3; i++)
	{
		render_color[i] = rand() % 256;
	}*/

	int label_index = 0;
	int label_va = 0;
	int p2l2c = 0;
	uchar* ptr = NULL;
	for (int i = 0; i < (depth*height); i++)
	{
		ptr = render.ptr<uchar>(i);
		for (int j = 0; j < width; j++)
		{
			p2l2c = valid_map[leaf_belong[leaf_label[label_index++]]];
			label_va = 3 * p2l2c;
			if (label_va < 0)
			{
				*(ptr++) = 0;
				*(ptr++) = 0;
				*(ptr++) = 0;
			}
			else
			{
				*(ptr++) = render_color[label_va];
				*(ptr++) = render_color[label_va + 1];
				*(ptr++) = render_color[label_va + 2];
			}

		}
	}

	unsigned char* slice_start = NULL;
	for (int i = 0; i < depth; i++)
	{
		if (i == 0)
			slice_start = render.data;
		else
			slice_start = slice_start + 3 * (width*height);


		cv::Mat seg(cv::Size(width, height), CV_8UC3, slice_start);
		cv::cvtColor(seg, seg, cv::COLOR_BGR2RGB);

		char savepath[1024];
		sprintf_s(savepath, "decimate\\girl\\%05d.png", i + 1);
		imwrite(savepath, seg);

	}

	/*if (render_color) delete[] render_color;*/
}

void Image::print_node_render(int * leaf_label, int node_num)
{
	/*string txtname = "octree.txt";
	ofstream f_debug1(txtname);
	int p_index = 0;
	for (int k = 0; k < 1; k++)
	{
		for (int j = 0; j < height; j++)
		{
			for (int i = 0; i < width; i++)
			{
				f_debug1 << std::left << std::setw(4) << leaf_label[p_index++] << " ";
			}
			f_debug1 << endl;
		}
		f_debug1 << endl;
		f_debug1 << endl;
	}

	f_debug1.close();*/


	cv::Mat render(cv::Size(width, height*depth), CV_8UC3, cv::Scalar(0, 0, 0));
	int* render_color = new int[node_num * 3];
	for (int i = 0; i < node_num * 3; i++)
	{
		render_color[i] = rand() % 256;
	}

	int label_index = 0;
	int label_va = 0;
	uchar* ptr = NULL;
	for (int i = 0; i < (depth*height); i++)
	{
		ptr = render.ptr<uchar>(i);
		for (int j = 0; j < width; j++)
		{
			label_va = 3 * leaf_label[label_index++];
			if (label_va < 0)
			{
				*(ptr++) = 0;
				*(ptr++) = 0;
				*(ptr++) = 0;
			}
			else
			{
				*(ptr++) = render_color[label_va];
				*(ptr++) = render_color[label_va + 1];
				*(ptr++) = render_color[label_va + 2];
			}

		}
	}


	unsigned char* slice_start = NULL;
	for (int i = 0; i < depth; i++)
	{
		if (i == 0)
			slice_start = render.data;
		else
			slice_start = slice_start + 3 * (width*height);
		

		cv::Mat seg(cv::Size(width, height), CV_8UC3, slice_start);
		cv::cvtColor(seg, seg, cv::COLOR_BGR2RGB);
		//flip(seg, seg, 0);//rows-i-1,j
		/*imshow("JFAInitMatrix", seg);
		waitKey();*/

		char savepath[1024];
		sprintf_s(savepath, "octree\\%05d.png", i + 1);
		imwrite(savepath, seg);

		//writer.write(seg);
	}
	//writer.release();


	if (render_color) delete[] render_color;
}


void Image::RGB2LAB(Shader& rgb2lab)
{
	rgb2lab.use();
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer_image);
	glDrawBuffers(1, buffers_image + 1);
	glClearColor(3.0f, -1.0f, -1.0f, -1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, texture_image[0]);
	rgb2lab.setInt("texels", 0);
	rgb2lab.setFloat("width_ratio", width_ratio);
	rgb2lab.setFloat("height_ratio", height_ratio);
	rgb2lab.setFloat("depth_ratio", depth_ratio);

	glBindVertexArray(VAO_all_t);
	glDrawArrays(GL_POINTS, 0, texel_num);

	//glBindTexture(GL_TEXTURE_3D, texture_image[1]);
	//print_buffer3(1);

}

void Image::print_buffer3(int index)
{
	float* res1 = new float[texel_num * 3];//GL_RGB
	glGetTexImage(GL_TEXTURE_3D, 0, GL_RGB, GL_FLOAT, res1);
	string txtname = "img_lab" + std::to_string(index) + ".txt";
	ofstream f_debug1(txtname);
	int slice_index = 0;
	for (int t = 0; t < 10; t++)//depth
	{
		for (int i = 0; i < height; i++) {
			//slice_index = (t * slice_num + i * width) * 3;
			for (int j = 0; j < width; j++) {
				f_debug1 << std::left << std::setw(4) << res1[slice_index++] << " ";
				f_debug1 << std::left << std::setw(4) << res1[slice_index++] << " ";
				f_debug1 << std::left << std::setw(4) << res1[slice_index++] << " ";
			}
			f_debug1 << endl;
		}
		f_debug1 << endl;
		f_debug1 << endl;
	}

	f_debug1.close();
	if (res1) delete[] res1;
}

void Image::print_cov(int index)
{
	int bits = 7;
	float* res1 = new float[contracted_num*bits *3];//GL_RED
	glGetTexImage(GL_TEXTURE_3D, 0, GL_RGB, GL_FLOAT, res1);
	string txtname = "cluster_cov" + std::to_string(index) + ".txt";
	ofstream f_debug1(txtname);
	int slice_index = 0;
	for (int t = 0; t < 1; t++)//depth
	{
		for (int i = 0; i < contracted_num; i++) {
			//slice_index = (t * slice_num + i * width) * 3;
			for (int j = 0; j < bits; j++) {
				f_debug1 << std::left << std::setw(4) << res1[slice_index++] << " ";
				f_debug1 << std::left << std::setw(4) << res1[slice_index++] << " ";
				f_debug1 << std::left << std::setw(4) << res1[slice_index++] << " ";
				//f_debug1 << std::left << std::setw(4) << res1[slice_index++] << " ";
			}
			f_debug1 << endl;
		}
		f_debug1 << endl;
		f_debug1 << endl;
	}

	f_debug1.close();
	if (res1) delete[] res1;
}

void Image::print_buffer1(int index)
{
	float* res1 = new float[texel_num];//GL_RED
	glGetTexImage(GL_TEXTURE_3D, 0, GL_RED, GL_FLOAT, res1);
	string txtname = "flood_label_" + std::to_string(index) + ".txt";
	ofstream f_debug1(txtname);
	int slice_index = 0;
	for (int t = 0; t < 10; t++)//depth
	{
		for (int i = 0; i < height; i++) {
			//slice_index = (t * slice_num + i * width) * 3;
			for (int j = 0; j < width; j++) {
				f_debug1 << std::left << std::setw(4) << res1[slice_index++] << " ";
			}
			f_debug1 << endl;
		}
		f_debug1 << endl;
		f_debug1 << endl;
	}

	f_debug1.close();
	if (res1) delete[] res1;
}


void Image::print_buffer(int index)
{
	/*float* res1 = new float[width*height * 3];
	glReadBuffer(buffers_image);
	glReadPixels(0, 0, width, height, GL_RGB, GL_FLOAT, res1);
	*/

	float* res1 = new float[texel_num * 4];//GL_RGBA
	glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_FLOAT, res1);
	string txtname = "rgb" + std::to_string(index) + ".txt";
	ofstream f_debug1(txtname);
	int slice_index = 0;
	/*f_debug1.width(5);
	f_debug1.precision(3);
	f_debug1.setf(ios::left);*/
	//slice_index = width * height * 66*4;
	for (int t = 0; t < 3; t++)
	{
		//slice_index = t * width*height * 3;
		for (int i = 0 ; i <height; i++) {
			//slice_index = (t * slice_num + i * width) * 4;
			for (int j = 0; j < width; j++) {
				f_debug1 << std::left << std::setw(4) << res1[slice_index++] << " ";
				f_debug1 << std::left << std::setw(4) << res1[slice_index++] << " ";
				f_debug1 << std::left << std::setw(4) << res1[slice_index++] << " ";
				f_debug1 << std::left << std::setw(4) << res1[slice_index++] << " ";

			}
			f_debug1 << endl;
		}
		f_debug1 << endl;
		f_debug1 << endl;
	}

	f_debug1.close();
	if (res1) delete[] res1;
}
