// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <iostream>

// Include GLEW
#include <GL/glew.h>

// Include OpenCL
#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif

// Include GLFW
#include <glfw3.h>
GLFWwindow* window;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext.hpp>
using namespace glm;

#include <common/shader.hpp>
#include <common/texture.hpp>
#include <common/controls.hpp>
#include <common/objloader.hpp>
#include <common/vboindexer.hpp>
#include <common/matrix.hpp>

// Hard-coded number of values to test, for convenience.
#define NUM_VALUES 1024

// A utility function that checks that our kernel execution performs the
// requested work over the entire range of data.
static int validate(cl_float* input, cl_float* output) {
	int i;
	for (i = 0; i < NUM_VALUES; i++) {

		// The kernel was supposed to square each value.
		if ( output[i] != (input[i] * input[i]) ) {
			fprintf(stdout,
					"Error: Element %d did not match expected output.\n", i);
			fprintf(stdout,
					"       Saw %1.4f, expected %1.4f\n", output[i],
							input[i] * input[i]);
			fflush(stdout);
			return 0;
		}
	}
	return 1;
}

int main (int argc, const char * argv[]) {
	int i;
	char name[128];

	// First, try to obtain a dispatch queue that can send work to the
	// GPU in our system.                                             // 2
	dispatch_queue_t queue =
			   gcl_create_dispatch_queue(CL_DEVICE_TYPE_GPU, NULL);

	// In the event that our system does NOT have an OpenCL-compatible GPU,
	// we can use the OpenCL CPU compute device instead.
	if (queue == NULL) {
		queue = gcl_create_dispatch_queue(CL_DEVICE_TYPE_CPU, NULL);
	}

	// This is not required, but let's print out the name of the device
	// we are using to do work.  We could use the same function,
	// clGetDeviceInfo, to obtain all manner of information about the device.
	cl_device_id gpu = gcl_get_device_id_with_dispatch_queue(queue);
	clGetDeviceInfo(gpu, CL_DEVICE_NAME, 128, name, NULL);
	fprintf(stdout, "Created a dispatch queue using the %s\n", name);

	// Here we hardcode some test data.
	// Normally, when this application is running for real, data would come from
	// some REAL source, such as a camera, a sensor, or some compiled collection
	// of statistics—it just depends on the problem you want to solve.
	float* test_in = (float*)malloc(sizeof(cl_float) * NUM_VALUES);
	for (i = 0; i < NUM_VALUES; i++) {
		test_in[i] = (cl_float)i;
	}

	// Once the computation using CL is done, will have to read the results
	// back into our application's memory space.  Allocate some space for that.
	float* test_out = (float*)malloc(sizeof(cl_float) * NUM_VALUES);

	// The test kernel takes two parameters: an input float array and an
	// output float array.  We can't send the application's buffers above, since
	// our CL device operates on its own memory space.  Therefore, we allocate
	// OpenCL memory for doing the work.  Notice that for the input array,
	// we specify CL_MEM_COPY_HOST_PTR and provide the fake input data we
	// created above.  This tells OpenCL to copy the data into its memory
	// space before it executes the kernel.                               // 3
	void* mem_in  = gcl_malloc(sizeof(cl_float) * NUM_VALUES, test_in,
							   CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);

	// The output array is not initalized; we're going to fill it up when
	// we execute our kernel.                                             // 4
	void* mem_out =
		   gcl_malloc(sizeof(cl_float) * NUM_VALUES, NULL, CL_MEM_WRITE_ONLY);

	// Dispatch the kernel block using one of the dispatch_ commands and the
	// queue created earlier.                                            // 5

	dispatch_sync(queue, ^{
		// Although we could pass NULL as the workgroup size, which would tell
		// OpenCL to pick the one it thinks is best, we can also ask
		// OpenCL for the suggested size, and pass it ourselves.
		size_t wgs;
		gcl_get_kernel_block_workgroup_info(square_kernel,
											CL_KERNEL_WORK_GROUP_SIZE,
											sizeof(wgs), &wgs, NULL);

		// The N-Dimensional Range over which we'd like to execute our
		// kernel.  In this case, we're operating on a 1D buffer, so
		// it makes sense that the range is 1D.
		cl_ndrange range = {                                              // 6
			1,                     // The number of dimensions to use.

			{0, 0, 0},             // The offset in each dimension.  To specify
								   // that all the data is processed, this is 0
								   // in the test case.                   // 7

			{NUM_VALUES, 0, 0},    // The global range—this is how many items
								   // IN TOTAL in each dimension you want to
								   // process.

			{wgs, 0, 0}            // The local size of each workgroup.  This
								   // determines the number of work items per
								   // workgroup.  It indirectly affects the
								   // number of workgroups, since the global
								   // size / local size yields the number of
								   // workgroups.  In this test case, there are
								   // NUM_VALUE / wgs workgroups.
		};
		// Calling the kernel is easy; simply call it like a function,
		// passing the ndrange as the first parameter, followed by the expected
		// kernel parameters.  Note that we case the 'void*' here to the
		// expected OpenCL types.  Remember, a 'float' in the
		// kernel, is a 'cl_float' from the application's perspective.   // 8

		square_kernel(&range,(cl_float*)mem_in, (cl_float*)mem_out);

		// Getting data out of the device's memory space is also easy;
		// use gcl_memcpy.  In this case, gcl_memcpy takes the output
		// computed by the kernel and copies it over to the
		// application's memory space.                                   // 9

		gcl_memcpy(test_out, mem_out, sizeof(cl_float) * NUM_VALUES);

	});


	// Check to see if the kernel did what it was supposed to:
	if ( validate(test_in, test_out)) {
		fprintf(stdout, "All values were properly squared.\n");
	}

	// Don't forget to free up the CL device's memory when you're done. // 10
	gcl_free(mem_in);
	gcl_free(mem_out);

	// And the same goes for system memory, as usual.
	free(test_in);
	free(test_out);

	// Finally, release your queue just as you would any GCD queue.    // 11
	dispatch_release(queue);
}

//int main( void )
//{
//	// Initialise GLFW
//	if( !glfwInit() )
//	{
//		fprintf( stderr, "Failed to initialize GLFW\n" );
//		return -1;
//	}

//	glfwWindowHint(GLFW_SAMPLES, 4);
//	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
//	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
//	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

//	// Open a window and create its OpenGL context
//	window = glfwCreateWindow( 1024, 768, "Tutorial 08 - Basic Shading", NULL, NULL);
//	if( window == NULL ){
//		fprintf( stderr, "Failed to open GLFW window. "
//						 "If you have an Intel GPU, they are not 3.3 compatible. "
//						 "Try the 2.1 version of the tutorials.\n" );
//		glfwTerminate();
//		return -1;
//	}
//	glfwMakeContextCurrent(window);

//	// Initialize GLEW
//	glewExperimental = true; // Needed for core profile
//	if (glewInit() != GLEW_OK) {
//		fprintf(stderr, "Failed to initialize GLEW\n");
//		return -1;
//	}

//	// Ensure we can capture the escape key being pressed below
//	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
//	glfwSetCursorPos(window, 1024/2, 768/2);

//	// Dark blue background
//	glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

//	// Enable depth test
//	glEnable(GL_DEPTH_TEST);
//	// Accept fragment if it closer to the camera than the former one
//	glDepthFunc(GL_LESS);

//	// Cull triangles which normal is not towards the camera
//	glEnable(GL_CULL_FACE);

//	GLuint VertexArrayID;
//	glGenVertexArrays(1, &VertexArrayID);
//	glBindVertexArray(VertexArrayID);

//	// Create and compile our GLSL program from the shaders
//	ShaderProgram shader_program;
//	shader_program.loadShader(GL_VERTEX_SHADER, "StandardShading.vert");
//	shader_program.loadShader(GL_FRAGMENT_SHADER, "StandardShading.frag");
////	shader_program.loadShader(GL_COMPUTE_SHADER, "ComputeShading.comp");

//	GLuint programID = shader_program.getProgramId();

//	// Get a handle for our "MVP" uniform
//	GLuint MatrixID = glGetUniformLocation(programID, "MVP");

//	// Load the texture
//	GLuint Texture = loadDDS("uvmap.DDS");

//	// Get a handle for our "myTextureSampler" uniform
//	GLuint TextureID  = glGetUniformLocation(programID, "myTextureSampler");

//	// Read our .obj file
//	std::vector<glm::vec3> vertices;
//	std::vector<glm::vec2> uvs;
//	std::vector<glm::vec3> normals;
//	loadOBJ("suzanne.obj", vertices, uvs, normals);


//	std::vector<glm::vec3> vertices_out(vertices);

//	// Load it into a VBO
//	GLuint vertexbuffer;
//	glGenBuffers(1, &vertexbuffer);
//	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
//	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);

//	GLuint vertexbuffer_out;
//	glGenBuffers(1, &vertexbuffer_out);
//	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer_out);
//	glBufferData(GL_ARRAY_BUFFER, vertices_out.size() * sizeof(glm::vec3), &vertices_out[0], GL_STATIC_DRAW);

//	GLuint uvbuffer;
//	glGenBuffers(1, &uvbuffer);
//	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
//	glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);

//	GLuint normalbuffer;
//	glGenBuffers(1, &normalbuffer);
//	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
//	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);

//	// Get a handle for our "LightPosition" uniform
//	glUseProgram(programID);

//	do{

//		// Clear the screen
//		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//		// Use our shader
//		glUseProgram(programID);

//		// Compute the MVP matrix from keyboard and mouse input
//		computeMatricesFromInputs();
//		glm::mat4 ProjectionMatrix = getProjectionMatrix();
//		glm::mat4 ViewMatrix = getViewMatrix();
//		glm::mat4 ModelMatrix = glm::mat4(1.0);
//		glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

//		// Send our transformation to the currently bound shader,
//		// in the "MVP" uniform
//		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

//		// Bind our texture in Texture Unit 0
//		glActiveTexture(GL_TEXTURE0);
//		glBindTexture(GL_TEXTURE_2D, Texture);
//		// Set our "myTextureSampler" sampler to user Texture Unit 0
//		glUniform1i(TextureID, 0);

//		// 1rst attribute buffer : vertices
//		glEnableVertexAttribArray(0);
//		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
//		glVertexAttribPointer(
//			0,                  // attribute
//			3,                  // size
//			GL_FLOAT,           // type
//			GL_FALSE,           // normalized?
//			0,                  // stride
//			(void*)0            // array buffer offset
//		);

//		// 2nd attribute buffer : UVs
//		glEnableVertexAttribArray(1);
//		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
//		glVertexAttribPointer(
//			1,                                // attribute
//			2,                                // size
//			GL_FLOAT,                         // type
//			GL_FALSE,                         // normalized?
//			0,                                // stride
//			(void*)0                          // array buffer offset
//		);

//		// Draw the triangles !
//		glDrawArrays(GL_TRIANGLES, 0, vertices.size() );

//		glDisableVertexAttribArray(0);
//		glDisableVertexAttribArray(1);

//		// Swap buffers
//		glfwSwapBuffers(window);
//		glfwPollEvents();

//	} // Check if the ESC key was pressed or the window was closed
//	while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
//		   glfwWindowShouldClose(window) == 0 );

//	// Cleanup VBO and shader
//	glDeleteBuffers(1, &vertexbuffer);
//	glDeleteBuffers(1, &uvbuffer);
//	glDeleteProgram(programID);
//	glDeleteTextures(1, &Texture);
//	glDeleteVertexArrays(1, &VertexArrayID);

//	// Close OpenGL window and terminate GLFW
//	glfwTerminate();

//	return 0;
//}

