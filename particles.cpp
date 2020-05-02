/*
 * particles.cpp
 *
 *  Created on: Apr 29, 2019
 *      Author: little
 */
#include <iostream>

#include "particles.h"

#include <stdlib.h>

#ifdef _OGLES3
#include "OpenGLES/FrmGLES3.h"
//#include <GLES3/gl3.h>
//#include <GLES2/gl2ext.h>
#else
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#endif



#include <glm/glm.hpp>
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
//#include "glm/detail/func_geometric.hpp"
#include "glm/geometric.hpp"


extern GLuint       g_hShaderProgram;


#ifndef NDEBUG

#define __GL_LOG(get_iv, get_log, obj, v) \
	do { \
		GLsizei len; \
		get_iv(obj, GL_INFO_LOG_LENGTH, &len); \
		LOG_INFO("Length:", len); \
		if (len > 0) \
		{ \
			GLchar *log = new GLchar[len + 1]; \
			get_log(obj, len, &len, log); \
			std::cerr << "[ERROR][@" << __LINE__ << "] " << v << ": " << log << std::endl; \
			delete [] log; \
		} \
	} while(0)

#define SHADER_LOG(_shader, v)		__GL_LOG(glGetShaderiv, glGetShaderInfoLog, _shader, v)

#define PROGRAM_LOG(_program, v)	__GL_LOG(glGetProgramiv, glGetProgramInfoLog, _program, v)

#else

#define SHADER_LOG(_shader, v)
#define PROGRAM_LOG(_program, v)

#endif //_DEBUG


#define GL_CHECK_ERROR()	LOG_INFO("GL Error String:", gluErrorString(glGetError()))



#define P_TEX_WIDTH		8
#define P_TEX_HEIGHT	8


static const unsigned char particle_texture[ P_TEX_WIDTH * P_TEX_HEIGHT ] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x11, 0x22, 0x22, 0x11, 0x00, 0x00,
    0x00, 0x11, 0x33, 0x99, 0x77, 0x33, 0x11, 0x00,
    0x00, 0x22, 0x99, 0xff, 0xef, 0x77, 0x22, 0x00,
    0x00, 0x22, 0x77, 0xef, 0xff, 0x99, 0x22, 0x00,
    0x00, 0x11, 0x33, 0x77, 0x99, 0x33, 0x11, 0x00,
    0x00, 0x00, 0x11, 0x22, 0x22, 0x11, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


/*
#define STRINGIZE(version, shader)	"#version " #version " core\n" #shader


#define ENABLE_VERTEX_OFFSET

// Vertex Shader
static const GLchar *vs_source = STRINGIZE(300,
layout (location = 0) in vec4 vertex; // <vec2 position, vec2 texCoords>

out vec2 TexCoords;
out vec4 ParticleColor;

uniform mat4 projection;
uniform vec2 offset;
uniform vec4 color;
uniform float scale;

void main()
{
    TexCoords = vertex.zw;
    ParticleColor = color; //vec4(0.999995, 0.999995, 0.5, 0.0);

#ifdef ENABLE_VERTEX_OFFSET
    gl_Position = projection * vec4(((vertex.xy - 0.5) * scale) + offset, 0.0, 1.0);
#else //!ENABLE_VERTEX_OFFSET
    gl_Position = projection * vec4((vertex.xy * scale) + offset, 0.0, 1.0);
#endif //ENABLE_VERTEX_OFFSET
}
);


// Fragment Shader
static const char *fs_source = STRINGIZE(300,
in vec2 TexCoords;
in vec4 ParticleColor;

out vec4 color;

uniform sampler2D sprite;

void main()
{
#if 1
	color = (texture(sprite, TexCoords) * ParticleColor);
#else
    color = ParticleColor;
#endif
}
);
*/



#define RAND()	rand()

float (*centered_random)(int, int) = [](int v, int range) -> float	{ return v % range - range / 2; };
float (*positive_random)(int, int) = [](int v, int range) -> float	{ return v % range; };

float (*v_ellipse_y)(float, float, float) = [](float a, float b, float x) -> float
{
	return a * sqrt(1 - (x * x)/(b * b));
};



typedef struct
{
	glm::vec4 First, Second, Third;
} ParticleColors;

static ParticleColors color_palettes[][3] =
{
		{ glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0.9f, 1.0f, 0.0f, 1.0f), glm::vec4(0.95f, 0.0f, 0.0f, 1.0f) },
		{ glm::vec4(0.1f, 0.6f, 0.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), glm::vec4(0.1f, 0.1f, 0.9f, 1.0f) }
};



/*static ShaderEntry shaders[] =
{
		{GL_VERTEX_SHADER,		vs_source},
		{GL_FRAGMENT_SHADER,	fs_source},
		{GL_NONE, NULL}
};*/



static const GLchar *UNIFORM_VAR_NAME_PROJECTION	= "projection";
static const GLchar *UNIFORM_VAR_NAME_OFFSET		= "offset";
static const GLchar *UNIFORM_VAR_NAME_COLOR			= "color";
static const GLchar *UNIFORM_VAR_NAME_SCALE			= "scale";
static const GLchar *UNIFORM_VAR_NAME_SPRITE		= "sprite";





//ParticleGenerator::ParticleGenerator(GLuint width, GLuint height, GLuint amount)
//    : width(width), height(height), program(0), amount(amount)
ParticleGenerator::ParticleGenerator()
{
	width = 800;
	height = 800;
	//program = 0;
	program = g_hShaderProgram;
	amount = 1000;
    this->init();
}


void ParticleGenerator::Update(GLfloat dt)
{
    // Update all particles
    for (GLuint i = 0; i < this->amount; ++i)
    {
        Particle	&p = this->particles[i];

        GLfloat base_value = this->emission_center_distance;
        GLfloat life_distance_threshold = base_value * 5 + centered_random(RAND(), base_value);

        GLint x_range;
        GLint y_range;

        GLfloat coff = positive_random(RAND(), 10);
        GLfloat to_emission_point_d = distance(p.Position, this->emission_point);

        p.Life -= dt * coff; // reduce life
        if (p.Life > 0.0f && to_emission_point_d < life_distance_threshold)
        {
        }
        else
        {
//        	LOG_INFO("RespawnParticle", "");
//        	LOG_INFO("ToEmissionPointDistance:", to_emission_point_d);
//        	LOG_INFO("LifeDistanceThreshold:", life_distance_threshold);

        	p.Position = glm::vec2(0.0f, 0.0f);
        	p.Life = 1.0f;
        }


        GLfloat abs_x = glm::abs(p.Position.x);
        GLfloat abs_y = glm::abs(p.Position.y);
		GLfloat scale = abs_x > 1 ? abs_y / abs_x : 1;

		if (p.Position.y <= this->central_point.y)
        {
			// The 3rd and 4th quadrant
        	if (abs_x <= abs_y)
        	{
            	x_range = base_value * 2;
            	y_range = 10;
        	}
        	else
        	{
        		if (scale < 1.5f)
				{
					x_range = base_value * 0.5;//(0.8f + scale);
					y_range = 5;
				}
				else
				{
					x_range = base_value * 0.1;
					y_range = 5;
				}
        	}
        }
        else
        {
        	// The 1st and 2nd quadrant
        	if (abs_x <= abs_y)
        	{
        		x_range = 20;//base_value * 0.2;//
            	y_range = 10;//6;//
        	}
        	else
        	{
        		x_range = 20;//base_value * (1.1 + scale);//base_value * 0.2;//
            	y_range = 5;
        	}
        }

       	//LOG_INFO("Alpha:", p.Color.a);

    	GLfloat v_x = centered_random(RAND(), x_range);
    	GLfloat v_y = positive_random(RAND(), y_range);

        p.Velocity = glm::vec2(v_x, v_y);
        p.Position += p.Velocity;

        GLfloat width_limit = this->width / 2;
        if (p.Position.x > 0)
        {
        	p.Position.x = glm::min(p.Position.x, width_limit);
        }
        else if (p.Position.x < 0)
		{
        	p.Position.x = glm::max(p.Position.x, -width_limit);
		}

        this->updateColor(p, base_value);

       	p.Color.a = p.Life;
    }
}

// Render all particles
void ParticleGenerator::Draw(GLfloat particle_scale)
{
    //static const float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    //glClearBufferfv(GL_COLOR, 0, black);
    glClearColor(0.1f, 0.1f, 0.1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, this->particle_tex_id);

    // Use additive blending to give it a 'glow' effect
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glUseProgram(this->program);

    for (Particle particle : this->particles)
    {
//        GLfloat d = distance(particle.Position, glm::vec2(0.0f, 0.0f));
//        LOG_INFO("X:", particle.Position.x);
//        LOG_INFO("Y:", particle.Position.y);
//        LOG_INFO("R:", particle.Color.r);
//        LOG_INFO("G:", particle.Color.g);
//        LOG_INFO("B:", particle.Color.b);
//        LOG_INFO("A:", particle.Color.a);
//        LOG_INFO("Distance:", d);

        if (particle.Life > 0.0f)
        {
            glUniform2fv(this->offset, 1, glm::value_ptr(particle.Position));
            glUniform4fv(this->color, 1, glm::value_ptr(particle.Color));
            glUniform1f(this->scale, particle_scale);

        	glBindVertexArray(this->VAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
        }
        else
        {
        	LOG_INFO("ParticleLife:", particle.Life);
        }
    }

    // Don't forget to reset to default blending mode
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

void ParticleGenerator::init()
{
    // Load shaders
    glUseProgram(this->program);

	GL_CHECK_ERROR();
    //DUMP_PROG_PARAMS(program);

    this->getUniformVarsLocation();

    GLfloat x_radius = static_cast<GLfloat>(this->width);
    GLfloat y_radius = static_cast<GLfloat>(this->height);

    glm::mat4 projection = glm::ortho(-x_radius, x_radius, -y_radius * 1 / 3, y_radius * (1 + 2 / 3), 1.0f, -1.0f);

    this->emission_point	= glm::vec2(0.0f, 0.0f);
    this->central_point		= glm::vec2(0.0f, glm::min(this->width, this->height) / 10);
    this->emission_center_distance = distance(this->emission_point, this->central_point);
/*
    printf("Width: %d", this->width);
    printf("Height: %d", this->height);
    printf("EmissionPoint.x: %d", this->emission_point.x);
    printf("EmissionPoint.y: %d", this->emission_point.y);
    printf("CentralPoint.x: %d", this->central_point.x);
    printf("CentralPoint.y: %d", this->central_point.y);
    printf("EmissionCenterDistance: %d", this->emission_center_distance);
*/
    glUniformMatrix4fv(this->projection, 1, GL_FALSE, glm::value_ptr(projection));

    glGenVertexArrays(1, &this->VAO);
    glBindVertexArray(this->VAO);

#ifdef ENABLE_VERTEX_OFFSET
    GLfloat particle_quads[] = {
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,

        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f
    };
#else //!ENABLE_VERTEX_OFFSET
    GLfloat particle_quads[] = {
        -0.5f,  0.5f, 0.0f, 1.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
        -0.5f, -0.5f, 0.0f, 0.0f,

        -0.5f,  0.5f, 0.0f, 1.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
         0.5f, -0.5f, 1.0f, 0.0f
    };
#endif //ENABLE_VERTEX_OFFSET

	// Set up mesh and attribute properties
    GLuint VBO;

    // Generates mesh buffer, then populating it with data
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(particle_quads), particle_quads, GL_STATIC_DRAW);

    // Set mesh attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);
    glBindVertexArray(0);

    // Upload particle texture
    glGenTextures(1, &this->particle_tex_id);
    glBindTexture(GL_TEXTURE_2D, this->particle_tex_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#if 1
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, P_TEX_WIDTH, P_TEX_HEIGHT, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, particle_texture);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, P_TEX_WIDTH, P_TEX_HEIGHT, 0, GL_ALPHA, GL_UNSIGNED_BYTE, particle_texture);
#endif

    GLuint count = this->amount * 0.8;

    printf("Count: %d", count);

    // Create default particle instances
    for (GLuint i = 0; i < this->amount; ++i)
    {
        this->particles.push_back(Particle());

    	if (i <= count)
    	{
        	Particle &p = this->particles[i];

        	p.Position.x = positive_random(RAND(), 1.5 * this->emission_center_distance);
    		p.Position.y = positive_random(RAND(), 2 * this->emission_center_distance);

//    		LOG_INFO("Position.x:", p.Position.x);
//    		LOG_INFO("Position.y:", p.Position.y);
    	}

    }
}

void ParticleGenerator::getUniformVarsLocation()
{
	this->projection	= glGetUniformLocation(program, UNIFORM_VAR_NAME_PROJECTION);
	this->offset		= glGetUniformLocation(program, UNIFORM_VAR_NAME_OFFSET);
	this->color			= glGetUniformLocation(program, UNIFORM_VAR_NAME_COLOR);
	this->scale			= glGetUniformLocation(program, UNIFORM_VAR_NAME_SCALE);
	this->sprite		= glGetUniformLocation(program, UNIFORM_VAR_NAME_SPRITE);

#ifndef NDEBUG
	PRINT_SEPARATOR();
	LOG_INFO("Location of uniform variables:", "");
	LOG_INFO(UNIFORM_VAR_NAME_PROJECTION,	this->projection);
	LOG_INFO(UNIFORM_VAR_NAME_OFFSET,		this->offset);
	LOG_INFO(UNIFORM_VAR_NAME_COLOR,		this->color);
	LOG_INFO(UNIFORM_VAR_NAME_SCALE,		this->scale);
	LOG_INFO(UNIFORM_VAR_NAME_SPRITE,		this->sprite);
	PRINT_SEPARATOR();
#endif
}

void ParticleGenerator::updateColor(Particle &particle, GLfloat base_value)
{
    GLfloat a1 = base_value * 0.5f;
    GLfloat b1 = a1 * 0.8f;
    GLfloat a2 = a1 * 2.0f;
    GLfloat b2 = a2 * 0.9f;

    glm::vec2 offset1(0, a1 + 30), offset2(0, a2 + 10);

    GLfloat y;

    ParticleColors *colors = color_palettes[0];

    if (abs(particle.Position.x) <= abs(b1))
    {
		glm::vec2 pos = particle.Position - offset1;
		y = v_ellipse_y(a1, b1, pos.x);
/*
		printf("Y: %f", y);
		printf("OriginalP.Y: %d", particle.Position.y);
		printf("P.Y: %d", pos.y);
*/
		if (abs(pos.y) <= abs(y))
		{
			particle.Color = colors->First;

			//LOG_INFO("White", "");

			return;
		}
    }

    if (abs(particle.Position.x) <= abs(b2))
    {
		glm::vec2 pos = particle.Position - offset2;
		y = v_ellipse_y(a2, b2, pos.x);
/*
		printf("Y: %f", y);
		printf("OriginalP.Y: %d", particle.Position.y);
		printf("P.Y: %d", pos.y);
*/
		if (abs(pos.y) <= abs(y))
		{
			particle.Color = colors->Second;

			//LOG_INFO("Yellow", "");

			return;
		}
    }

    particle.Color = colors->Third;
}
