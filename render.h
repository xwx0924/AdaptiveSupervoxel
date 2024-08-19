#ifndef RENDER_H
#define RENDER_H
#include<io.h>
#include <iostream>
#include <vector>
#include <opencv2/core/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/cudaobjdetect.hpp>
#include <opencv2/cudaoptflow.hpp>
#include <opencv2/core/opengl.hpp>
#include "Image.h"
using namespace std;
using namespace cv;


class Render
{
private:
	GLFWwindow * window;

	int screenWidth, screenHeight;
	int width, height, depth;
	string get_imgname(string filename_txt);
	void getFiles(string path, vector<string>& files, vector<string>& file_names);
	//void print_node_render(int * node_label, int node_num);
public:
	bool next_step;
	bool Lloyd_ends;
	int Lloyd_curr_iter;
	int Lloyd_max_iter;
	int cluster_num;


	Render();
	~Render();

	int run(string dbname_, string type, string vdname_, int seeds_num_);
	void cvMat2TexInput(Mat& img);
	/*void output_video(unsigned char* data, int frame_width, int frame_height);
	void output_frame(unsigned char* data, int index, int width, int height);*/
};
#endif
