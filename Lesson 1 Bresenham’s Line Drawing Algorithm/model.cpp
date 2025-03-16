#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "model.h"

Model::Model(const char *filename) : verts_(), faces_() {
    std::ifstream in;
    in.open (filename, std::ifstream::in);
    if (in.fail()) return;
    std::string line;
    while (!in.eof()) {
        std::getline(in, line);
        std::istringstream iss(line.c_str());
        char trash;
        if (!line.compare(0, 2, "v ")) {
            iss >> trash;
            Vec3f v;
            for (int i=0;i<3;i++) iss >> v.raw[i];
            verts_.push_back(v);
        } else if (!line.compare(0, 2, "f ")) {
            std::vector<int> f;
            std::vector<int> f_vt;
            int itrash, idx, idx_vt;
            iss >> trash;
            while (iss >> idx >> trash >> idx_vt >> trash >> itrash) {
                idx--; // in wavefront obj all indices start at 1, not zero
                f.push_back(idx);
                idx_vt--; // in wavefront obj all indices start at 1, not zero
                f_vt.push_back(idx_vt);
            }
            faces_.push_back(f);
            faces_uv_.push_back(f_vt);
        }
        else if (!line.compare(0, 3, "vt ")) {
            iss >> trash;
            iss >> trash;
            Vec2f vt;
            for (int i = 0; i < 2; i++) iss >> vt.raw[i];
            uvs_.push_back(vt);
        }
    }
    std::cerr << "# v# " << verts_.size() << " f# "  << faces_.size() << " vt# " << uvs_.size() << std::endl;
}

Model::~Model() {
}

int Model::nverts() {
    return (int)verts_.size();
}

int Model::nfaces() {
    return (int)faces_.size();
}

int Model::nfaces_uv() {
    return (int)faces_uv_.size();
}

int Model::nuvs() {
    return (int)uvs_.size();
}

std::vector<int> Model::face(int idx) {
    return faces_[idx];
}

Vec3f Model::vert(int i) {
    return verts_[i];
}

Vec2f Model::uv(int i) {
    return uvs_[i];
}

std::vector<int> Model::face_uv(int idx) {
    return faces_uv_[idx];
}

