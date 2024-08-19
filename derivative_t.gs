#version 430
layout(points )in;
layout(points ,max_vertices = 1 )out;

uniform sampler3D texels;
uniform float width;
uniform float height;
uniform float depth;

in VS_OUT {
	int  gLayer;
	vec3 gTex;//texture坐标，texture()函数使用
} gs_in[];

//out vec3 f_color;
out float gradient_t;


void main(){
	
	gl_Position=gl_in[0].gl_Position;//这里是gl_in[0]
	gl_Layer = gs_in[0].gLayer;//z为0.5，gl_Layer=0
	vec3 curr_tex = gs_in[0].gTex;
	vec3 f_color = floor(255.0*(texture(texels,curr_tex).rgb)+0.5);
	
	vec3 last_tex;
	vec3 last_color;
	vec3 delta_color;
	gradient_t = 0;
	float aver_pa = 0.33;//x,y方向比例
	
	//********
	vec3 aver_delta_color = vec3(0,0,0);
	vec3 neighbor_tex, neighbor_color;	
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
	for(int i=0;i<9;i++)
	{
		neighbor_tex = curr_tex + vec3(off_set[i],off_set[i+1],0);
		neighbor_color = floor(255.0*(texture(texels,neighbor_tex).rgb)+0.5);
		last_tex = vec3(neighbor_tex.x,neighbor_tex.y,neighbor_tex.z-1.0/depth);
		last_color = floor(255.0*(texture(texels,last_tex).rgb)+0.5);
		delta_color = abs(last_color-neighbor_color);
		aver_delta_color += delta_color;
	
	}
	aver_delta_color = aver_delta_color/9;
	gradient_t = aver_delta_color.x+aver_delta_color.y+aver_delta_color.z;
	//********
	
	
	
	
	EmitVertex();
	EndPrimitive();
}