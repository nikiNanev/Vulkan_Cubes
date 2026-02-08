
#include "mouse.h"
#include "camera.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

struct Mouse mouse;
Camera *camera = new Camera();

class Callbacks
{
    
public:

	static void mouse_input(GLFWwindow *window, double xpos, double ypos)
	{
		if (mouse.firstMouse)
		{
			mouse.lastX = xpos;
			mouse.lastY = ypos;
			mouse.firstMouse = false;
		}

		float xoffset = xpos - mouse.lastX;
		float yoffset = mouse.lastY - ypos;
		mouse.lastX = xpos;
		mouse.lastY = ypos;

		xoffset *= mouse.sensitivity;
		yoffset *= mouse.sensitivity;

		mouse.yaw += xoffset;
		mouse.pitch += yoffset;

		if (mouse.pitch > 89.0f)
			mouse.pitch = 89.0f;
		if (mouse.pitch < -89.0f)
			mouse.pitch = -89.0f;

		glm::vec3 direction;
		direction.x = cos(glm::radians(mouse.yaw)) *
					  cos(glm::radians(mouse.pitch));
		direction.y = sin(glm::radians(mouse.pitch));
		direction.z = sin(glm::radians(mouse.yaw)) *
					  cos(glm::radians(mouse.pitch));
		camera->Front = glm::normalize(direction);
	}

	static void mouse_scroll(GLFWwindow *window, double xoffset,
							 double yoffset)
	{
		mouse.fov -= (float)yoffset;
		if (mouse.fov < 1.0f)
			mouse.fov = 1.0f;
		if (mouse.fov > 45.0f)
			mouse.fov = 45.0f;
	}
};