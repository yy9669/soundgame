#include "ColorTextureProgram.hpp"

#include "Mode.hpp"
#include "GL.hpp"
#include "Sound.hpp"
#include <vector>
#include <deque>

/*
 * FlappyMode is a game mode that implements a single-player game of flappy.
 */

struct FlappyMode : Mode {
	FlappyMode();
	virtual ~FlappyMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	// flappy bird status
	glm::vec2 bird_radius = glm::vec2(0.2f, 0.2f);
	glm::vec2 bird = glm::vec2(-3.5f, 0.0f);
	glm::vec2 bird_velocity = glm::vec2(0.0f, 1.0f);	
	#define HEX_TO_U8VEC4( HX ) (glm::u8vec4( (HX >> 24) & 0xff, (HX >> 16) & 0xff, (HX >> 8) & 0xff, (HX) & 0xff ))
	glm::u8vec4 mud = HEX_TO_U8VEC4(0xff0000ff);
	glm::u8vec4 ice = HEX_TO_U8VEC4(0x888888ff);
	glm::u8vec4 water = HEX_TO_U8VEC4(0x0000ffff);
	glm::u8vec4 air = HEX_TO_U8VEC4(0xf3ffc6ff);
	glm::u8vec4 environ_color[4]={mud,ice,water,air};
	int environ=3;
	float last_change_time=0;
	float environ_time=0;
	int next_environ=-1;
	uint32_t left_score = 0;
	std::deque< glm::vec2 > bars;
	std::deque< glm::vec2 > bars_radius;

	glm::vec2 court_radius = glm::vec2(7.0f, 5.0f);

	//----- opengl assets / helpers ------

	//draw functions will work on vectors of vertices, defined as follows:
	struct Vertex {
		Vertex(glm::vec3 const &Position_, glm::u8vec4 const &Color_, glm::vec2 const &TexCoord_) :
			Position(Position_), Color(Color_), TexCoord(TexCoord_) { }
		glm::vec3 Position;
		glm::u8vec4 Color;
		glm::vec2 TexCoord;
	};
	static_assert(sizeof(Vertex) == 4*3 + 1*4 + 4*2, "FlappyMode::Vertex should be packed");

	//Shader program that draws transformed, vertices tinted with vertex colors:
	ColorTextureProgram color_texture_program;

	//Buffer used to hold vertex data during drawing:
	GLuint vertex_buffer = 0;

	//Vertex Array Object that maps buffer locations to color_texture_program attribute locations:
	GLuint vertex_buffer_for_color_texture_program = 0;

	//Solid white texture:
	GLuint white_tex = 0;

	//matrix that maps from clip coordinates to court-space coordinates:
	glm::mat3x2 clip_to_court = glm::mat3x2(1.0f);
	// computed in draw() as the inverse of OBJECT_TO_CLIP
	// (stored here so that the mouse handling code can use it to position the paddle)
	std::shared_ptr< Sound::PlayingSample > bgm;
};
