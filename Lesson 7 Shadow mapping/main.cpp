#include <vector>
#include <iostream>

#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model* model = NULL;
const int width = 800;
const int height = 800;

Vec3f light_dir(1, 1, 1);
Vec3f       eye(0, -1, 3);
Vec3f    center(0, 0, 0);
Vec3f        up(0, 1, 0);

// Gouraud Shading���ȼ���ø��������ɫ���ٶ���ɫ��ֵ
//struct GouraudShader : public IShader
//{
//    // vertex shader�����, fragment shader������
//    Vec3f varying_intensity;
//
//    // vertex shader
//    virtual Vec4f vertex(int iface, int nthvert)
//    {
//        // ��ȡģ�͵Ķ�������
//        //  embed<4> 3d����=>4d�������
//        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
//
//        // local=>view=>projection=>screen
//        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
//
//        // diffuse���գ��뷨�ߺ͹��ߵļн��й�
//        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert) * light_dir);
//
//        return gl_Vertex;
//    }
//
//    // fragment shader
//    // bc����������ϵ�������ڲ�ֵ
//    virtual bool fragment(Vec3f bc, TGAColor& color)
//    {
//        // ���ݸ��������ɫ���в�ֵ
//        float intensity = varying_intensity * bc;
//
//        color = TGAColor(255, 255, 255) * intensity;
//
//        // �Ƿ�������ǰƬ��
//        return false;
//    }
//};

// Phong Shading���Ȳ�ֵ���㣬�ټ�����ɫ
struct Shader : public IShader
{
    mat<2, 3, float> varying_uv;  // �����ε�uv����  
    mat<4, 3, float> varying_tri; // �����εĲü�(clip)����
    mat<3, 3, float> varying_nrm; // �����εķ���
    mat<3, 3, float> ndc_tri;     // �����ε�NDC����

    // vertex shader
    virtual Vec4f vertex(int iface, int nthvert)
    {
        // ���ø�fragment shader��Ҫ�Ĳ���
        // ע���openGLһ���ǰ��д洢
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        varying_nrm.set_col(nthvert, proj<3>((Projection * ModelView).invert_transpose() * embed<4>(model->normal(iface, nthvert), 0.f)));
        Vec4f gl_Vertex = Projection * ModelView * embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, gl_Vertex);
        ndc_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3])); // ͸�ӳ�����clip=>NDC
        return gl_Vertex;
    }

    // fragment shader
    // bc����������ϵ�������ڲ�ֵ
    virtual bool fragment(Vec3f bc, TGAColor& color)
    {
        // �����������㷨�ߵĲ�ֵ����������������һ�㣬����Ϊ����ģ�͹���Ķ����кܶ࣬
        // ������һ����ķ��߶���һ���ܴ�����ʵ�������εķ��߷������ȡ��������Ĳ�ֵ����ȡ�ñȽϽӽ���Ч��
        Vec3f bn = (varying_nrm * bc).normalize();

        Vec2f uv = varying_uv * bc;

        // ����tangent space������Referrence/tangent space.png
        mat<3, 3, float> A;
        A[0] = ndc_tri.col(1) - ndc_tri.col(0);
        A[1] = ndc_tri.col(2) - ndc_tri.col(0);
        A[2] = bn;

        mat<3, 3, float> AI = A.invert();

        Vec3f i = AI * Vec3f(varying_uv[0][1] - varying_uv[0][0], varying_uv[0][2] - varying_uv[0][0], 0);
        Vec3f j = AI * Vec3f(varying_uv[1][1] - varying_uv[1][0], varying_uv[1][2] - varying_uv[1][0], 0);

        mat<3, 3, float> B;
        B.set_col(0, i.normalize());
        B.set_col(1, j.normalize());
        B.set_col(2, bn);

        // ����tangent space��normalmap �����Ƭ�εķ���
        Vec3f n = (B * model->normal(uv)).normalize();

        // diffuse����
        float diff = std::max(0.f, n * light_dir);
        color = model->diffuse(uv) * diff;

        // �Ƿ�������ǰƬ��
        return false;
    }
};

int main(int argc, char** argv) 
{
    if (2 > argc) 
    {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    float* zbuffer = new float[width * height];
    for (int i = width * height; i--; zbuffer[i] = -std::numeric_limits<float>::max());

    TGAImage frame(width, height, TGAImage::RGB);
    lookat(eye, center, up);
    viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
    projection(-1.f / (eye - center).norm());
    light_dir = proj<3>((Projection * ModelView * embed<4>(light_dir, 0.f))).normalize();

    for (int m = 1; m < argc; m++) 
    {
        model = new Model(argv[m]);
        Shader shader;
        for (int i = 0; i < model->nfaces(); i++) {
            for (int j = 0; j < 3; j++) {
                shader.vertex(i, j);
            }
            triangle(shader.varying_tri, shader, frame, zbuffer);
        }
        delete model;
    }
    frame.flip_vertically(); // to place the origin in the bottom left corner of the image
    frame.write_tga_file("framebuffer.tga");

    delete[] zbuffer;
    return 0;
}