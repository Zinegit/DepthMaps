#include <QGLViewer/qglviewer.h>

#include <glm/glm.hpp>

class Viewer : public QGLViewer
{
	protected :
	virtual void draw();
	virtual void init();
	virtual QString helpString() const;

	virtual void decompose();
	virtual void reconstruct();

	virtual void keyPressEvent(QKeyEvent* event);

	private :
	GLuint m_render_programID;
	GLuint m_compute_programID;
	GLuint m_compute_lifting_programID;
	std::vector<GLuint> m_vertex_arrays;
	std::vector<GLuint> m_index_buffers;
	std::vector<GLuint> m_textures;
	std::vector<int> m_nb_points_buffers;
	std::vector<glm::mat4> m_mvp_matrices;

	int m_width;
	int m_height;
	int m_level;
};
