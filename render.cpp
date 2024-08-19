#include "Render.h"
extern cv::Mat video_mat;
// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}

void Render::getFiles(string path, vector<string>& files, vector<string>& file_names)
{
	//读取目录
	//long hFile = 0;//win32下
	intptr_t hFile = 0;//x64
	struct _finddata_t fileinfo;
	string p;
	if ((hFile = _findfirst(p.assign(path).append("\\*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			//如果不是目录
			if (!(fileinfo.attrib&_A_SUBDIR))
			{
				files.push_back(p.assign(path).append("\\").append(fileinfo.name));
				file_names.push_back(fileinfo.name);
				//cout << fileinfo.name << endl;
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}

	//得到file_names
	string image_name;
	for (unsigned int i = 0; i < files.size(); ++i)
	{
		image_name = get_imgname(file_names[i]);
		file_names[i] = image_name;
	}
}



string Render::get_imgname(string filename_txt)
{
	string::size_type pos, _pos2;

	pos = filename_txt.find('.');
	while (string::npos != pos)
	{
		_pos2 = pos + 1;
		pos = filename_txt.find('.', _pos2);
	}
	_pos2 -= 1;
	return filename_txt.substr(0, _pos2);
}

Render::Render()
{
}

Render::~Render()
{

}

int Render::run(string dbname_, string type_,string vdname_, int seeds_num_)
{

	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	/*glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);*/

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
#endif


	string dbname = dbname_;
	int seeds_num = seeds_num_;
	cluster_num = seeds_num_;
	string vdname = vdname_;//SegTrackv2:bird_of_paradise;birdfall;bmx;cheetah;drift;frog_1;frog_2;frog_3;girl;hummingbird;monkey;monkeydog;parachute;penguin;soldier;worm_1;worm_2;worm_3

	width = 146;
	height = 120;
	depth = 25;
	bool output_label = false;
	bool split_flag = false;
	string dirName = "C:\\Workspace\\VSWorkspace\\Octree_Merge_Projects\\result_txt\\";
	dirName = dirName + dbname + "\\OM_two-1\\" + vdname + "\\";
	string command = "md " + dirName;
	if (_access(dirName.c_str(), 0) == -1)
	{
		system(command.c_str());
	}
	dirName += to_string(seeds_num);

	string dirName2 = "C:\\Workspace\\VSWorkspace\\Octree_Merge_Projects\\result_supervoxel\\";
	dirName2 = dirName2 + dbname + "\\OM_two-1\\" + vdname + "\\"+ to_string(seeds_num)+"\\";
	command = "md " + dirName2;
	if (_access(dirName2.c_str(), 0) == -1)
	{
		system(command.c_str());
	}


	string videoPath = "C:\\Workspace\\VSWorkspace\\datasets\\";
	videoPath = videoPath + dbname + type_+"\\" + vdname;

	cout << vdname << "," << seeds_num << ":" << endl;
	
	vector<string> files;
	vector<string> file_names;
	getFiles(videoPath, files, file_names);
	Mat first_frame = imread(files[0], 1);
	width = first_frame.cols;
	height = first_frame.rows;
	depth = file_names.size();

	screenWidth = width;
	screenHeight = height;
	window = glfwCreateWindow(screenWidth, screenHeight, "JFA_Test", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwHideWindow(window);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}
	const GLubyte* OpenGLVersion = glGetString(GL_VERSION);
	cout << "OpenGL version：" << OpenGLVersion << endl;
	//printf("OpenGL version：%s\n", OpenGLVersion);
	int attri_n = 0;
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attri_n);
	cout << "GL_MAX_VERTEX_ATTRIBS:" << attri_n << endl;

	
	Mat curr_frame;
	//Mat video_mat;
	Size video_size(width, height*depth);
	video_mat.create(video_size, CV_MAKETYPE(CV_8U, 3));
	video_mat = Scalar::all(0);
	cvMat2TexInput(first_frame);
	curr_frame = video_mat(Rect(0, 0, width, height));
	first_frame.copyTo(curr_frame);

	for (int i = 1; i < depth; i++)//start from frame 2
	{
		Mat frame = imread(files[i], 1);
		cvMat2TexInput(frame);
		curr_frame = video_mat(Rect(0, i*height, width, height));
		frame.copyTo(curr_frame);
	}
	//output_video(video_mat.data, width, height);
	glm::mat4 trans = glm::mat4(1.0f);

	time_t start, end;
	double time_ms;
	start = clock();
	cout << "load shaders ..." << endl;
	Shader derivative_t = Shader("derivative_t.vs", "derivative_t.fs", "derivative_t.gs");
	Shader derivative_sum = Shader("derivative_sum.vs", "derivative_sum.fs", "derivative_sum.gs");
	Shader boundary_swap = Shader("bd_swap.vs", "bd_swap.fs", "bd_swap.gs");
	cout << "init GPU ..." << endl;
	Image image;
	image.init_gpu(dbname, video_mat.data, width, height, depth, seeds_num);
	
	cout << "calculate gradient info ..." << endl;
	image.gradient_info(derivative_t, derivative_sum);
	
	cout << "build octree ..." << endl;
	int leaf_size = 4;
	image.build_octree(leaf_size);
	//image.draw_octree();

	cout << "decimate ..." << endl;
	image.init_cluster();
	image.decimate();
	end = clock();
	time_ms = (double)(end - start) / CLOCKS_PER_SEC;
	cout << "decimate time:" << time_ms << " s" << endl;

	//image.draw_decimate();
	////GPU swapping。
	cout << "swap boundary pixels ..." << endl;
	for (int i = 0; i < 3; i++)
	{
		if (i == 0)
		{
			image.organize_cluster_info();
		}
		else
		{
			image.update_cluster_info();
		}
		image.cluster_boundary_swaps(boundary_swap, i);
		//image.print_swap_supervoxel(i, dirName2);
		//image.print_swap_txt(2, save_name);
	}
	cout << "done!" << endl;
	end = clock();
	time_ms = (double)(end - start) / CLOCKS_PER_SEC;
	cout << "process time:" << time_ms << " s" << endl;
	image.check_label_contineous();
	image.print_swap_supervoxel(2, dirName2);
	//image.print_swap_seg(video_mat.data, dirName2);
	image.print_swap_txt(2, dirName);


	
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}



void  Render::cvMat2TexInput(Mat& img)
{
	GaussianBlur(img, img, Size(3, 3), 0);
	cvtColor(img, img, COLOR_BGR2RGB);//COLOR_BGR2RGB;COLOR_BGR2Lab
}

