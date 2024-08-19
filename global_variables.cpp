#include "partition.h"
#include "Octree.h"
#include "optimizer.h"
#include "partition.h"
#include <opencv2/opencv.hpp>
#include "opencv2/imgproc/imgproc_c.h"
using namespace std;

cv::Mat video_mat;
AdaptiveOctree *tree;

Optimizer * optimizer = NULL;

//cluster information
Partition* tess = NULL;
//map
int * valid_mapping = NULL;
map<int, int> valid_map;
float location_ratio;