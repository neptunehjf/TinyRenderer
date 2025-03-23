#include <vector>
#include <limits>
#include <iostream>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model* model = NULL;
float* shadowbuffer = NULL;

const int width = 3000;
const int height = 3000;

Vec3f light_dir(1, 1, 0);
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
    mat<4, 4, float> uniform_M;   // ԭ�����任����
    mat<4, 4, float> uniform_MIT; // (Projection*ModelView).invert_transpose()
    mat<4, 4, float> uniform_Mshadow; // transform framebuffer screen coordinates to shadowbuffer screen coordinates
    mat<2, 3, float> varying_uv;  // triangle uv coordinates, written by the vertex shader, read by the fragment shader
    mat<3, 3, float> varying_tri; // triangle coordinates before Viewport transform, written by VS, read by FS
    mat<3, 3, float> varying_nrm; // �����εķ���

    Shader(Matrix M, Matrix MIT, Matrix MS) : uniform_M(M), uniform_MIT(MIT), uniform_Mshadow(MS), varying_uv(), varying_tri() {}

    virtual Vec4f vertex(int iface, int nthvert) 
    {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = Viewport * Projection * ModelView * embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        varying_nrm.set_col(nthvert, proj<3>(uniform_MIT * embed<4>(model->normal(iface, nthvert), 0.f)));
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bc, TGAColor& color) 
    {
        // �����������㷨�ߵĲ�ֵ����������������һ�㣬����Ϊ����ģ�͹���Ķ����кܶ࣬
        // ������һ����ķ��߶���һ���ܴ�����ʵ�������εķ��߷������ȡ��������Ĳ�ֵ����ȡ�ñȽ�ƽ����Ч��
        Vec3f n = (varying_nrm * bc).normalize();

        Vec4f sb_p = uniform_Mshadow * embed<4>(varying_tri * bc); // ��Ƭ���ڹ�Դ�ӽ��µ�λ��
        sb_p = sb_p / sb_p[3]; // ������η��� =>ndc
        int idx = int(sb_p[0]) + int(sb_p[1]) * width; // ȷ������λ��
        const float bias = 1.0; // ����shadow acne
        const float ambient = 0.3; // ������
        float shadow = ambient + (1 - ambient) * (shadowbuffer[idx] < sb_p[2] + bias); // ������Ӱ
        Vec2f uv = varying_uv * bc;                 // ��ֵuv
        Vec3f l = proj<3>(uniform_M * embed<4>(light_dir)).normalize(); // ���շ���
        Vec3f r = (l - n * (n * l * 2.f)).normalize();   //����ⷽ�� ����Referrence/mirror reflection.jpg
        Vec3f v = (center - eye).normalize(); //�ӽǷ���
        const float shinness = 0.2; // �����
        float spec = pow(std::max(0.f, r * v), shinness); // �߹�ϵ��
        float diff = std::max(0.f, n * l); // diffuseϵ��
        TGAColor diff_color = model->diffuse(uv); // diffuse����
        TGAColor spec_color = model->specular(uv); // �߹����
        float light = 2; //����ǿ��
        for (int i = 0; i < 3; i++) 
            color[i] = std::min<float>(light * (diff * diff_color[i] + spec * spec_color[0]) * shadow, 255);
       
        return false;
    }
};

// ��Ⱦ��Դ�ӽǵ����ͼ��������Ⱦ��Ӱ
struct DepthShader : public IShader 
{
    mat<3, 3, float> varying_tri;

    DepthShader() : varying_tri() {}

    virtual Vec4f vertex(int iface, int nthvert) 
    {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert)); 
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3])); // NDC��
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bc, TGAColor& color) 
    {
        Vec3f p = varying_tri * bc; // ��ֵ
        color = TGAColor(255, 255, 255) * (p.z / depth); // ������� [0,1]
        return false;
    }
};


int main(int argc, char** argv) 
{
    if (2 > argc) 
    {
        std::cerr << "Usage: " << argv[0] << "obj/model.obj" << std::endl;
        return 1;
    }

    float* zbuffer = new float[width * height];  //ԭ������zbuffer
    shadowbuffer = new float[width * height];    //��Դ�ӽǵ�zbuffer

    // ��ʼ��Ϊ��Сֵ
    for (int i = width * height; --i; ) 
    {
        zbuffer[i] = shadowbuffer[i] = -std::numeric_limits<float>::max();
    }

    model = new Model(argv[1]);
    light_dir.normalize();

    { // rendering the shadow buffer
        TGAImage depth(width, height, TGAImage::RGB);
        lookat(light_dir, center, up);  //��Դ�ӽ�
        viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
        projection(0); // ƽ�й⣬����ͶӰ���ɣ����Բ�����Ϊ0

        DepthShader depthshader;
        Vec4f screen_coords[3];
        for (int i = 0; i < model->nfaces(); i++) 
        {
            for (int j = 0; j < 3; j++) 
            {
                screen_coords[j] = depthshader.vertex(i, j);
            }
            triangle(screen_coords, depthshader, depth, shadowbuffer);
        }
        depth.flip_vertically();
        depth.write_tga_file("depth.tga"); // ���ͼ
    }

    Matrix M = Viewport * Projection * ModelView;

    { // rendering the frame buffer
        TGAImage frame(width, height, TGAImage::RGB);
        lookat(eye, center, up);
        viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
        projection(-1.f / (eye - center).norm());

        Shader shader(ModelView, (Projection * ModelView).invert_transpose(), M * (Viewport * Projection * ModelView).invert());
        Vec4f screen_coords[3];
        for (int i = 0; i < model->nfaces(); i++) 
        {
            for (int j = 0; j < 3; j++) 
            {
                screen_coords[j] = shader.vertex(i, j);
            }
            triangle(screen_coords, shader, frame, zbuffer);
        }
        frame.flip_vertically(); // to place the origin in the bottom left corner of the image
        frame.write_tga_file("framebuffer.tga");
    }

    delete model;
    delete[] zbuffer;
    delete[] shadowbuffer;
    return 0;
}