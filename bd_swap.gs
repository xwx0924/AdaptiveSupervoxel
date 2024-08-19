#version 430
layout(points )in;
layout(points ,max_vertices = 1 )out;

uniform sampler3D image;
uniform sampler3D seg;
uniform sampler3D clusterCov;
uniform float width;
uniform float height;
uniform float depth;
uniform float ratio;
uniform float contracted_num;


in VS_OUT {
	vec3 gPos;
	vec3 gTex;
} gs_in[];

out vec4 f_color;


void main(){
	gl_Position=gl_in[0].gl_Position;//这里是gl_in[0]
	gl_Layer = int(gs_in[0].gPos.z);//z为0.5，gl_Layer=0
	vec3 curr_xyz = gs_in[0].gPos-0.5;
	float pId = curr_xyz.z*(width*height)+curr_xyz.y*width+curr_xyz.x;

	//8邻居
	float off_set[18] = 
	{-1.0/width,-1.0/height,
	0,-1.0/height,
	1.0/width,-1.0/height,
	-1.0/width,0,
	0,0,
	1.0/width,0,
	-1.0/width,1.0/height,
	0,1.0/height,
	1.0/width,1.0/height
	};
	float off_z[3]={-1.0/depth,0,1.0/depth};

	vec3 c_whd_ratio = vec3(1.0/7.0,1.0/contracted_num,1.0);//1.0/7.0
	vec3 this_tex = gs_in[0].gTex;//texture坐标
	float curr_label = texture(seg,this_tex).r;
	float neighbor_label=-1;
	vec3 neighbor_tex;
	//flag=-1, it is not a boundary pixel; flag=0, it is a boundary pixel but no need to swap; flag=1, it is a boundary pixel and need swap
	int flag=-1;
	vec3 cc0,cc1,cc2,cc3,cc4,cc5,cc6;
	vec3 curr_color;
	
	int neighbor_number=0;
	float energy_before=0,energy_after=0,energy_from=0,energy_to=0;
	float energy_saved=0, energy_saved_closest=0;
	float cluster_closest = curr_label;
	int index=0;
	curr_color = floor(255.0*(texture(image,this_tex).rgb)+0.5);//从byte texture中取出来的值默认在0-1之间
	// <cId, area,energy>,<x,y,z>,<lab>,<cov1_f3>,<cov1_l3>,<cov2_f3>，<cov2_l3>
	cc0 = texture(clusterCov,vec3(0.5,curr_label+0.5,0.5)*c_whd_ratio).rgb;//位置处，纵坐标不加0.5取得是上一行
	cc1 = texture(clusterCov,vec3(1.5,curr_label+0.5,0.5)*c_whd_ratio).rgb;
	cc2 = texture(clusterCov,vec3(2.5,curr_label+0.5,0.5)*c_whd_ratio).rgb;
	cc3 = texture(clusterCov,vec3(3.5,curr_label+0.5,0.5)*c_whd_ratio).rgb;
	cc4 = texture(clusterCov,vec3(4.5,curr_label+0.5,0.5)*c_whd_ratio).rgb;
	cc5 = texture(clusterCov,vec3(5.5,curr_label+0.5,0.5)*c_whd_ratio).rgb;
	cc6 = texture(clusterCov,vec3(6.5,curr_label+0.5,0.5)*c_whd_ratio).rgb;
	vec3 t_cc0,t_cc1,t_cc2,t_cc3,t_cc4,t_cc5,t_cc6;	
	//测试：计算跟左上角的邻居cluster交换时的能量变化，与CPU版本的结果做对比！！！
	
	for(int k=0;k<3;k++)
	{
		index=0;//这里不要忘了！
		for(int i=0;i<9;i++)
		{
			neighbor_tex = this_tex + vec3(off_set[index],off_set[index+1],off_z[k]);
			neighbor_label = texture(seg,neighbor_tex).r;
			if(neighbor_label!=curr_label)
			{
				// <cId, area,energy>,<x,y,z>,<lab>,<cov1_f3>,<cov1_l3>,<cov2_f3>，<cov2_l3>
				t_cc0 = texture(clusterCov,vec3(0.5,neighbor_label+0.5,0.5)*c_whd_ratio).rgb;
				t_cc1 = texture(clusterCov,vec3(1.5,neighbor_label+0.5,0.5)*c_whd_ratio).rgb;
				t_cc2 = texture(clusterCov,vec3(2.5,neighbor_label+0.5,0.5)*c_whd_ratio).rgb;
				t_cc3 = texture(clusterCov,vec3(3.5,neighbor_label+0.5,0.5)*c_whd_ratio).rgb;
				t_cc4 = texture(clusterCov,vec3(4.5,neighbor_label+0.5,0.5)*c_whd_ratio).rgb;
				t_cc5 = texture(clusterCov,vec3(5.5,neighbor_label+0.5,0.5)*c_whd_ratio).rgb;
				t_cc6 = texture(clusterCov,vec3(6.5,neighbor_label+0.5,0.5)*c_whd_ratio).rgb;
				energy_before = cc0.z+t_cc0.z;
				
				float area_n=0; mat3 cov; vec3 centroid_n; mat3 cov_n1,cov_n2; vec3 i1,i2;
				//subtract p from cluster i
				//-------------------------
				area_n = cc0.y-1;
				cov = mat3(cc3.x,cc3.y,cc3.z,cc3.y,cc4.x,cc4.y,cc3.z,cc4.y,cc4.z);
				centroid_n = (cc1*cc0.y - curr_xyz)/area_n;
				i1 = cc1-centroid_n;//big-small
				i2 = cc1-curr_xyz;
				cov_n1 = cov-area_n*mat3(i1.x*i1.x,i1.x*i1.y,i1.x*i1.z,i1.y*i1.x,i1.y*i1.y,i1.y*i1.z,i1.z*i1.x,i1.z*i1.y,i1.z*i1.z)-mat3(i2.x*i2.x,i2.x*i2.y,i2.x*i2.z,i2.y*i2.x,i2.y*i2.y,i2.y*i2.z,i2.z*i2.x,i2.z*i2.y,i2.z*i2.z);
				cov = mat3(cc5.x,cc5.y,cc5.z,cc5.y,cc6.x,cc6.y,cc5.z,cc6.y,cc6.z);
				centroid_n = (cc2*cc0.y - curr_color)/area_n;
				i1 = cc2-centroid_n;
				i2 = cc2-curr_color;
				cov_n2 = cov-area_n*mat3(i1.x*i1.x,i1.x*i1.y,i1.x*i1.z,i1.y*i1.x,i1.y*i1.y,i1.y*i1.z,i1.z*i1.x,i1.z*i1.y,i1.z*i1.z)-mat3(i2.x*i2.x,i2.x*i2.y,i2.x*i2.z,i2.y*i2.x,i2.y*i2.y,i2.y*i2.z,i2.z*i2.x,i2.z*i2.y,i2.z*i2.z);
				energy_from = ratio*(cov_n1[0][0]+cov_n1[1][1]+cov_n1[2][2])+(cov_n2[0][0]+cov_n2[1][1]+cov_n2[2][2]);
				
				//add p to cluster j
				//-------------------------
				area_n = t_cc0.y+1;
				cov = mat3(t_cc3.x,t_cc3.y,t_cc3.z,t_cc3.y,t_cc4.x,t_cc4.y,t_cc3.z,t_cc4.y,t_cc4.z);
				centroid_n = (t_cc1*t_cc0.y + curr_xyz)/area_n;
				i1 = centroid_n-t_cc1;
				i2 = centroid_n-curr_xyz;
				cov_n1 = cov+ t_cc0.y*mat3(i1.x*i1.x,i1.x*i1.y,i1.x*i1.z,i1.y*i1.x,i1.y*i1.y,i1.y*i1.z,i1.z*i1.x,i1.z*i1.y,i1.z*i1.z)+mat3(i2.x*i2.x,i2.x*i2.y,i2.x*i2.z,i2.y*i2.x,i2.y*i2.y,i2.y*i2.z,i2.z*i2.x,i2.z*i2.y,i2.z*i2.z);		
				cov = mat3(t_cc5.x,t_cc5.y,t_cc5.z,t_cc5.y,t_cc6.x,t_cc6.y,t_cc5.z,t_cc6.y,t_cc6.z);
				centroid_n = (t_cc2*t_cc0.y + curr_color)/area_n;
				i1 = centroid_n-t_cc2;
				i2 = centroid_n-curr_color;
				cov_n2 = cov+ t_cc0.y*mat3(i1.x*i1.x,i1.x*i1.y,i1.x*i1.z,i1.y*i1.x,i1.y*i1.y,i1.y*i1.z,i1.z*i1.x,i1.z*i1.y,i1.z*i1.z)+mat3(i2.x*i2.x,i2.x*i2.y,i2.x*i2.z,i2.y*i2.x,i2.y*i2.y,i2.y*i2.z,i2.z*i2.x,i2.z*i2.y,i2.z*i2.z);
				energy_to = ratio*(cov_n1[0][0]+cov_n1[1][1]+cov_n1[2][2])+(cov_n2[0][0]+cov_n2[1][1]+cov_n2[2][2]);
				//-------------------------
				energy_after = energy_from+energy_to;
				energy_saved = energy_before-energy_after;
				if(energy_saved>energy_saved_closest)
				{
					cluster_closest = neighbor_label;
					energy_saved_closest = energy_saved;
				}
			}
			index+=2;
		}
	}
	
	if(energy_saved_closest>0)
	{
		f_color = vec4(pId,curr_label,cluster_closest,energy_saved_closest);//cluster_closest默认为curr_label
	}else
	{
		f_color = vec4(pId,curr_label,cluster_closest,energy_saved_closest);
	}

	


	
	EmitVertex();
	EndPrimitive();
	
}