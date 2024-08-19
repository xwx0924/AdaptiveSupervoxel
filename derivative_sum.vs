#version 430
layout(location = 0) in ivec3 aPos;


uniform sampler3D gradient;
uniform float width;
uniform float height;
uniform float depth;


out VS_OUT {
	int gLayer;
	float p_grad;
} vs_out;

void main()
{
	vec3 curr_tex = vec3(aPos.x+0.5,aPos.y+0.5,aPos.z+0.5);
	curr_tex.x = curr_tex.x/width;
	curr_tex.y = curr_tex.y/height;
	curr_tex.z = curr_tex.z/depth;
	float targetId = 0;//求和，写在第0位
	vs_out.p_grad = texture(gradient,curr_tex).r;
	vec3 target;
	target.z=0;
	target.y = 0;
	target.x = 0;
	target+=0.5;

	target.x = target.x/width;
	target.y = target.y/height;
	vec2 opengl_xy = 2*(target.xy)-1;
	
	
	//vec2 opengl_xy = 2*(curr_tex.xy)-1;
	
	gl_Position = vec4(opengl_xy,0.0,1.0);
	//vs_out.gLayer = aPos.z;
	
	vs_out.gLayer = int(target.z);
	
}