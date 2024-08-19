#pragma once

#include "Render.h"
extern float location_ratio;

string get_imgname(string filename_txt)
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

void getFileNames(string path, vector<string>& file_names)
{
	intptr_t hFile = 0;
	struct _finddata_t fileinfo;
	string p;
	if ((hFile = _findfirst(p.assign(path).append("\\*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			if ((fileinfo.attrib&_A_SUBDIR))
			{
				//files.push_back(p.assign(path).append("\\").append(fileinfo.name));
				if (fileinfo.name[0] != '.')
					file_names.push_back(fileinfo.name);
				//cout << fileinfo.name << endl;
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}
}
void getFiles(string path, vector<string>& files, vector<string>& file_names)
{
	//long hFile = 0;//win32
	intptr_t hFile = 0;//x64
	struct _finddata_t fileinfo;
	string p;
	if ((hFile = _findfirst(p.assign(path).append("\\*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			if (!(fileinfo.attrib&_A_SUBDIR))
			{
				files.push_back(p.assign(path).append("\\").append(fileinfo.name));
				file_names.push_back(fileinfo.name);
				//cout << fileinfo.name << endl;
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}

	string image_name;
	for (unsigned int i = 0; i < files.size(); ++i)
	{
		image_name = get_imgname(file_names[i]);
		file_names[i] = image_name;
	}
}



int main()
{
	string dbname = "SegTrackv2";//BuffaloXiph,SegTrackv2
	vector<string> files_B;//BVDS
	vector<string> file_names_B;
	string type = "\\PNGImages";
	string datasetPath = "C:\\Workspace\\VSWorkspace\\datasets\\" + dbname + type;
	getFileNames(datasetPath, file_names_B);
	vector<int> svnum;


	//svnum.push_back(200); svnum.push_back(300); svnum.push_back(500); svnum.push_back(1000); svnum.push_back(2000); svnum.push_back(3000); svnum.push_back(4000);



	svnum.push_back(1000);
	location_ratio = 1;//for swapping


	int video_number = file_names_B.size();

	for (int i = 0; i < video_number; i++)
	{
		string vdname = file_names_B[i];
		for (int j = 0; j < 1; j++)
		{
			int seed_num = svnum[j];
			Render render;
			render.run(dbname,type, vdname, seed_num);
		}
	}
	system("pause");
	return 0;

}