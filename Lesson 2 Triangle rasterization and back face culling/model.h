#ifndef __MODEL_H__
#define __MODEL_H__

#include <vector>
#include "geometry.h"

class Model {
private:
	std::vector<Vec3f> verts_;
	std::vector<std::vector<int> > faces_;
	std::vector<std::vector<int> > faces_uv_;
	std::vector<Vec2f> uvs_;
public:
	Model(const char *filename);
	~Model();
	int nverts();
	int nfaces();
	int nuvs();
	int nfaces_uv();
	Vec3f vert(int i);
	std::vector<int> face(int idx);
	Vec2f uv(int i);
	std::vector<int> face_uv(int idx);
};

#endif //__MODEL_H__
