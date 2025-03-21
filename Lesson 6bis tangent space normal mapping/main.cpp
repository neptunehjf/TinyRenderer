#include <vector>
#include <iostream>

#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model *model     = NULL;
const int width  = 800;
const int height = 800;

Vec3f light_dir(1,1,1);
Vec3f       eye(0,-1,3);
Vec3f    center(0,0,0);
Vec3f        up(0,1,0);

// Gouraud Shading���ȼ���ø��������ɫ���ٶ���ɫ��ֵ
struct GouraudShader : public IShader 
{
    // vertex shader�����, fragment shader������
    Vec3f varying_intensity; 

    // vertex shader
    virtual Vec4f vertex(int iface, int nthvert) 
    {
        // ��ȡģ�͵Ķ�������
        //  embed<4> 3d����=>4d�������
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert)); 

        // local=>view=>projection=>screen
        gl_Vertex = Viewport*Projection*ModelView*gl_Vertex;

        // diffuse���գ��뷨�ߺ͹��ߵļн��й�
        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert)*light_dir);
        
        return gl_Vertex;
    }

    // fragment shader
    // bar����������ϵ�������ڲ�ֵ
    virtual bool fragment(Vec3f bar, TGAColor &color) 
    {
        // ���ݸ��������ɫ���в�ֵ
        float intensity = varying_intensity * bar;

        color = TGAColor(255, 255, 255)*intensity;

        // �Ƿ�������ǰƬ��
        return false;                           
    }
};

int main(int argc, char** argv) 
{
    if (2==argc) 
    {
        model = new Model(argv[1]);
    } 
    else 
    {
        model = new Model("obj/african_head.obj");
    }

    lookat(eye, center, up);
    viewport(width/8, height/8, width*3/4, height*3/4);
    projection(-1.f/(eye-center).norm());

    light_dir.normalize();

    TGAImage image  (width, height, TGAImage::RGB);
    TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);

    GouraudShader shader;
    for (int i=0; i<model->nfaces(); i++) 
    {
        Vec4f screen_coords[3];
        // ͼԪװ��
        for (int j=0; j<3; j++) 
        {
            screen_coords[j] = shader.vertex(i, j);
        }
        triangle(screen_coords, shader, image, zbuffer);
    }

    image.  flip_vertically();
    zbuffer.flip_vertically();
    image.  write_tga_file("output.tga");
    zbuffer.write_tga_file("zbuffer.tga");

    delete model;
    return 0;
}
