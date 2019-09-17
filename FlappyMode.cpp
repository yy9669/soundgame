#include "FlappyMode.hpp"
#include "Sprite.hpp"
#include "DrawSprites.hpp"
#include "Load.hpp"
#include "data_path.hpp"
#include "gl_errors.hpp"
#include "MenuMode.hpp"
#include "Sound.hpp"

//for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>

#include <random>

Load< Sound::Sample > music_air(LoadTagDefault, []() -> Sound::Sample * {
	return new Sound::Sample(data_path("advertising.opus"));
});
Load< Sound::Sample > music_mud(LoadTagDefault, []() -> Sound::Sample * {
	return new Sound::Sample(data_path("whistle.opus"));
});
Load< Sound::Sample > music_water(LoadTagDefault, []() -> Sound::Sample * {
	return new Sound::Sample(data_path("ins.opus"));
});
Load< Sound::Sample > music_ice(LoadTagDefault, []() -> Sound::Sample * {
	return new Sound::Sample(data_path("ukulele.opus"));
});

Load< Sound::Sample > music_warn(LoadTagDefault, []() -> Sound::Sample *{
	return new Sound::Sample(data_path("warn.opus"));
});

Load< Sound::Sample > music_die(LoadTagDefault, []() -> Sound::Sample *{
	return new Sound::Sample(data_path("death.opus"));
});

Load< Sound::Sample > music_up(LoadTagDefault, []() -> Sound::Sample *{
	std::vector< float > data(size_t(48000 * 0.2f), 0.0f);
	for (uint32_t i = 0; i < data.size(); ++i) {
		float t = i / float(48000);
		//phase-modulated sine wave (creates some metal-like sound):
		data[i] = std::sin(3.1415926f * 2.0f * 220.0f * t + std::sin(3.1415926f * 2.0f * 200.0f * t));
		//quadratic falloff:
		data[i] *= 0.3f * std::pow(std::max(0.0f, (1.0f - t / 0.2f)), 2.0f);
	}
	return new Sound::Sample(data);
});

Load< Sound::Sample > music_down(LoadTagDefault, []() -> Sound::Sample *{
	std::vector< float > data(size_t(48000 * 0.2f), 0.0f);
	for (uint32_t i = 0; i < data.size(); ++i) {
		float t = i / float(48000);
		//phase-modulated sine wave (creates some metal-like sound):
		data[i] = std::sin(-3.1415926f * 2.0f * 220.0f * t + std::sin(3.1415926f * 2.0f * 200.0f * t));
		//quadratic falloff:
		data[i] *= 0.3f * std::pow(std::max(0.0f, (1.0f - t / 0.2f)), 2.0f);
	}
	return new Sound::Sample(data);
});

FlappyMode::FlappyMode() {

	//set up bars and bars_radius
	bars.clear();
	bars_radius.clear();

	//----- allocate OpenGL resources -----
	{ //vertex buffer:
		glGenBuffers(1, &vertex_buffer);
		//for now, buffer will be un-filled.

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //vertex array mapping buffer for color_texture_program:
		//ask OpenGL to fill vertex_buffer_for_color_texture_program with the name of an unused vertex array object:
		glGenVertexArrays(1, &vertex_buffer_for_color_texture_program);

		//set vertex_buffer_for_color_texture_program as the current vertex array object:
		glBindVertexArray(vertex_buffer_for_color_texture_program);

		//set vertex_buffer as the source of glVertexAttribPointer() commands:
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

		//set up the vertex array object to describe arrays of FlappyMode::Vertex:
		glVertexAttribPointer(
			color_texture_program.Position_vec4, //attribute
			3, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 0 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Position_vec4);
		//[Note that it is okay to bind a vec3 input to a vec4 attribute -- the w component will be filled with 1.0 automatically]

		glVertexAttribPointer(
			color_texture_program.Color_vec4, //attribute
			4, //size
			GL_UNSIGNED_BYTE, //type
			GL_TRUE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Color_vec4);

		glVertexAttribPointer(
			color_texture_program.TexCoord_vec2, //attribute
			2, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 + 4*1 //offset
		);
		glEnableVertexAttribArray(color_texture_program.TexCoord_vec2);

		//done referring to vertex_buffer, so unbind it:
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//done setting up vertex array object, so unbind it:
		glBindVertexArray(0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //solid white texture:
		//ask OpenGL to fill white_tex with the name of an unused texture object:
		glGenTextures(1, &white_tex);

		//bind that texture object as a GL_TEXTURE_2D-type texture:
		glBindTexture(GL_TEXTURE_2D, white_tex);

		//upload a 1x1 image of solid white to the texture:
		glm::uvec2 size = glm::uvec2(1,1);
		std::vector< glm::u8vec4 > data(size.x*size.y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

		//set filtering and wrapping parameters:
		//(it's a bit silly to mipmap a 1x1 texture, but I'm doing it because you may want to use this code to load different sizes of texture)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		//since texture uses a mipmap and we haven't uploaded one, instruct opengl to make one for us:
		glGenerateMipmap(GL_TEXTURE_2D);

		//Okay, texture uploaded, can unbind it:
		glBindTexture(GL_TEXTURE_2D, 0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}
}

FlappyMode::~FlappyMode() {

	//----- free OpenGL resources -----
	glDeleteBuffers(1, &vertex_buffer);
	vertex_buffer = 0;

	glDeleteVertexArrays(1, &vertex_buffer_for_color_texture_program);
	vertex_buffer_for_color_texture_program = 0;

	glDeleteTextures(1, &white_tex);
	white_tex = 0;
}

bool FlappyMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.button.button == SDL_BUTTON_LEFT) {
		bird_velocity = glm::vec2(bird_velocity.x,bird_velocity.y+0.5);
		Sound::play(*music_up);
	}else if 	(evt.button.button == SDL_BUTTON_RIGHT) {
		bird_velocity = glm::vec2(bird_velocity.x,bird_velocity.y-0.5);
		Sound::play(*music_down);
	}
	return false;
}

void FlappyMode::update(float elapsed) {

	static std::mt19937 mt; //mersenne twister pseudo-random number generator
	//----- flappy enrionment update
	if (!bgm) {
		bgm = Sound::play(*music_air, 1.0f);
	}
	environ_time+=elapsed;
	if(environ_time>10){
		environ=next_environ;
		next_environ=-1;
		environ_time=0;
		bgm->stop(0);
		if(environ==3){
			bgm = Sound::play(*music_air, 1.0f);		
		}else if (environ==0){
			bgm = Sound::play(*music_mud, 1.0f);		
		}else if (environ==1){
			bgm = Sound::play(*music_ice, 1.0f);		
		}else{
			bgm = Sound::play(*music_water, 1.0f);					
		}

		left_score+=1;
	}
	if(environ_time<10 && environ_time>8 && next_environ==-1){
		next_environ=int((mt() / float(mt.max())) * 3.99f);
		Sound::play(*music_warn, 1.0f);
	}

	//----- bird update -----
	if (environ==3){
		bird += glm::vec2(0.0f, elapsed*bird_velocity.y-1.5*elapsed*elapsed);
		bird_velocity = glm::vec2(bird_velocity.x,  bird_velocity.y-3*elapsed);		
	}else if (environ==2){
		bird += glm::vec2(0.0f, elapsed*bird_velocity.y+1.5*elapsed*elapsed);
		bird_velocity = glm::vec2(bird_velocity.x,  bird_velocity.y+3*elapsed);
	}else if (environ==1){
		bird += glm::vec2(0.0f, elapsed*bird_velocity.y);
	}else if (environ==0){
		float acc=0;
		if(bird_velocity.y>0){
			acc=-3;
		}else{
			acc=3;
		}
		bird += glm::vec2(0.0f, elapsed*bird_velocity.y+acc/2*elapsed*elapsed);
		bird_velocity = glm::vec2(bird_velocity.x,  bird_velocity.y+acc*elapsed);
	}

	for (auto &t : bars) {
		t.x -= elapsed;
	}

	while(bars.size()>0 &&bars[0].x<-7.0f){
		bars.pop_front();
		bars_radius.pop_front();
	}

	glm::vec2  new_bar=glm::vec2(7.0f, (mt() / float(mt.max()))*10.0f-5.0f);
	glm::vec2  new_radius=glm::vec2((mt() / float(mt.max()))*1.0f+0.2f,(mt() / float(mt.max()))*2.0f+0.5f);	
	float upper_side=std::min(5.0f,new_bar.y+new_radius.y);
	float lower_side=std::max(-5.0f,new_bar.y-new_radius.y);
	new_bar.y=(lower_side+upper_side)/2;
	new_radius.y=(upper_side-lower_side)/2;

	if((bars.size()>0 && bars[bars.size()-1].x<0.0f) || bars.size()==0){
		bars.emplace_back(new_bar);
		bars_radius.emplace_back(new_radius);
	}else if(bars.size()>0 && bars[bars.size()-1].x<2.0f){
		bool add_new=mt() / float(mt.max())>0.5f;
		if(add_new==true){
			bars.emplace_back(new_bar);
			bars_radius.emplace_back(new_radius);		
		}
	}

	//---- collision handling ----

	//bars:
	bool collision=false;
	for(unsigned int i=0;i<bars.size();i++){
		if(bird.x+bird_radius.x>bars[i].x-bars_radius[i].x && bird.x-bird_radius.x<bars[i].x+bars_radius[i].x){
			if(bird.y>=bars[i].y+bars_radius[i].y || bird.y<=bars[i].y-bars_radius[i].y){
				collision=true;
				break;
			}
		}
	}

	//walls:
	if(bird.y>4.8 || bird.y<-4.8){
		collision=true;
	}

	if(collision==true){
		Sound::play(*music_die, 1.0f);
		left_score=0;
		bird = glm::vec2(-3.5f, 0.0f);
		bird_velocity = glm::vec2(0.0f, 1.0f);
		bars.clear();
		bars_radius.clear();
	}

}

void FlappyMode::draw(glm::uvec2 const &drawable_size) {
	//some nice colors from the course web page:
	#define HEX_TO_U8VEC4( HX ) (glm::u8vec4( (HX >> 24) & 0xff, (HX >> 16) & 0xff, (HX >> 8) & 0xff, (HX) & 0xff ))
	glm::u8vec4 bg_color = environ_color[environ];
	const glm::u8vec4 fg_color = HEX_TO_U8VEC4(0x000000ff);
	const std::vector< glm::u8vec4 > rainbow_colors = {
		HEX_TO_U8VEC4(0xe2ff70ff), HEX_TO_U8VEC4(0xcbff70ff), HEX_TO_U8VEC4(0xaeff5dff),
		HEX_TO_U8VEC4(0x88ff52ff), HEX_TO_U8VEC4(0x6cff47ff), HEX_TO_U8VEC4(0x3aff37ff),
		HEX_TO_U8VEC4(0x2eff94ff), HEX_TO_U8VEC4(0x2effa5ff), HEX_TO_U8VEC4(0x17ffc1ff),
		HEX_TO_U8VEC4(0x00f4e7ff), HEX_TO_U8VEC4(0x00cbe4ff), HEX_TO_U8VEC4(0x00b0d8ff),
		HEX_TO_U8VEC4(0x00a5d1ff), HEX_TO_U8VEC4(0x0098cfd8), HEX_TO_U8VEC4(0x0098cf54),
		HEX_TO_U8VEC4(0x0098cf54), HEX_TO_U8VEC4(0x0098cf54), HEX_TO_U8VEC4(0x0098cf54),
		HEX_TO_U8VEC4(0x0098cf54), HEX_TO_U8VEC4(0x0098cf54), HEX_TO_U8VEC4(0x0098cf54),
		HEX_TO_U8VEC4(0x0098cf54)
	};
	#undef HEX_TO_U8VEC4

	//other useful drawing constants:
	const float wall_radius = 0.05f;
	const float padding = 0.14f; //padding between outside of walls and edge of window

	//---- compute vertices to draw ----

	//vertices will be accumulated into this list and then uploaded+drawn at the end of this function:
	std::vector< Vertex > vertices;

	//inline helper function for rectangle drawing:
	auto draw_rectangle = [&vertices](glm::vec2 const &center, glm::vec2 const &radius, glm::u8vec4 const &color) {
		//split rectangle into two CCW-oriented triangles:
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));

		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
	};

	auto draw_bird = [&vertices](glm::vec2 const &center, glm::vec2 const &radius, glm::u8vec4 const &color) {
		//split bird into two triangles:
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x, center.y, 0.0f), color, glm::vec2(0.5f, 0.5f));

		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x, center.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
	};

	// bars
	for(unsigned int i=0;i<bars.size();i++){
		float upper_radius= (5.0f-(bars[i].y+bars_radius[i].y))/2;
		float upper_y=5.0f-upper_radius;
		float lower_radius= ((bars[i].y-bars_radius[i].y)+5.0f)/2;
		float lower_y=-5.0f+lower_radius;
		draw_rectangle(glm::vec2(bars[i].x,upper_y), glm::vec2(bars_radius[i].x,upper_radius), fg_color);
		draw_rectangle(glm::vec2(bars[i].x,lower_y), glm::vec2(bars_radius[i].x,lower_radius), fg_color);
	}

	//solid objects:

	//bird
	draw_bird(bird, bird_radius, fg_color);

	//walls:
	draw_rectangle(glm::vec2(-court_radius.x-wall_radius, 0.0f), glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), fg_color);
	draw_rectangle(glm::vec2( court_radius.x+wall_radius, 0.0f), glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), fg_color);
	draw_rectangle(glm::vec2( 0.0f,-court_radius.y-wall_radius), glm::vec2(court_radius.x, wall_radius), fg_color);
	draw_rectangle(glm::vec2( 0.0f, court_radius.y+wall_radius), glm::vec2(court_radius.x, wall_radius), fg_color);

	//scores:
	glm::vec2 score_radius = glm::vec2(0.1f, 0.1f);
	for (uint32_t i = 0; i < left_score; ++i) {
		draw_rectangle(glm::vec2( -court_radius.x + (2.0f + 3.0f * i) * score_radius.x, court_radius.y + 2.0f * wall_radius + 2.0f * score_radius.y), score_radius, fg_color);
	}

	//------ compute court-to-window transform ------

	//compute area that should be visible:
	glm::vec2 scene_min = glm::vec2(
		-court_radius.x - 2.0f * wall_radius - padding,
		-court_radius.y - 2.0f * wall_radius - padding
	);
	glm::vec2 scene_max = glm::vec2(
		court_radius.x + 2.0f * wall_radius + padding,
		court_radius.y + 2.0f * wall_radius + 3.0f * score_radius.y + padding
	);

	//compute window aspect ratio:
	float aspect = drawable_size.x / float(drawable_size.y);
	//we'll scale the x coordinate by 1.0 / aspect to make sure things stay square.

	//compute scale factor for court given that...
	float scale = std::min(
		(2.0f * aspect) / (scene_max.x - scene_min.x), //... x must fit in [-aspect,aspect] ...
		(2.0f) / (scene_max.y - scene_min.y) //... y must fit in [-1,1].
	);

	glm::vec2 center = 0.5f * (scene_max + scene_min);

	//build matrix that scales and translates appropriately:
	glm::mat4 court_to_clip = glm::mat4(
		glm::vec4(scale / aspect, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(-center.x * (scale / aspect), -center.y * scale, 0.0f, 1.0f)
	);
	//NOTE: glm matrices are specified in *Column-Major* order,
	// so this matrix is actually transposed from how it appears.

	//also build the matrix that takes clip coordinates to court coordinates (used for mouse handling):
	clip_to_court = glm::mat3x2(
		glm::vec2(aspect / scale, 0.0f),
		glm::vec2(0.0f, 1.0f / scale),
		glm::vec2(center.x, center.y)
	);

	//---- actual drawing ----

	//clear the color buffer:
	glClearColor(bg_color.r / 255.0f, bg_color.g / 255.0f, bg_color.b / 255.0f, bg_color.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	//use alpha blending:
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//don't use the depth test:
	glDisable(GL_DEPTH_TEST);

	//upload vertices to vertex_buffer:
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer); //set vertex_buffer as current
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STREAM_DRAW); //upload vertices array
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//set color_texture_program as current program:
	glUseProgram(color_texture_program.program);

	//upload OBJECT_TO_CLIP to the proper uniform location:
	glUniformMatrix4fv(color_texture_program.OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(court_to_clip));

	//use the mapping vertex_buffer_for_color_texture_program to fetch vertex data:
	glBindVertexArray(vertex_buffer_for_color_texture_program);

	//bind the solid white texture to location zero:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, white_tex);

	//run the OpenGL pipeline:
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));

	//unbind the solid white texture:
	glBindTexture(GL_TEXTURE_2D, 0);

	//reset vertex array to none:
	glBindVertexArray(0);

	//reset current program to none:
	glUseProgram(0);
	

	GL_ERRORS(); //PARANOIA: print errors just in case we did something wrong.
}
