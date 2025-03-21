#include <cmath>
#include <limits>
#include <cstdlib>
#include "our_gl.h"

Matrix ModelView;
Matrix Viewport;
Matrix Projection;

IShader::~IShader() {}

// 视口变换，裁剪坐标=>屏幕坐标
// 2D坐标 [-1,1] => [x+w, y+h]
// 深度   [-1,1] => [0, 255]
void viewport(int x, int y, int w, int h)
{
    Viewport = Matrix::identity();
    Viewport[0][3] = x + w / 2.f;
    Viewport[1][3] = y + h / 2.f;
    Viewport[2][3] = 255.f / 2.f;
    Viewport[0][0] = w / 2.f;
    Viewport[1][1] = h / 2.f;
    Viewport[2][2] = 255.f / 2.f;
}

// 投影
// 参照 Referrence/projection1.png
// 参照 Referrence/projection2.png
void projection(float coeff)
{
    Projection = Matrix::identity();
    Projection[3][2] = coeff;
}

// 视角
void lookat(Vec3f eye, Vec3f center, Vec3f up)
{
    // 相机坐标系
    Vec3f z = (eye - center).normalize();
    Vec3f x = cross(up, z).normalize();
    Vec3f y = cross(z, x).normalize();

    ModelView = Matrix::identity();
    for (int i = 0; i < 3; i++)
    {
        // 转到相机坐标系(view空间)
        ModelView[0][i] = x[i];
        ModelView[1][i] = y[i];
        ModelView[2][i] = z[i];
        // 以相机为原点，物体位置相对于相机是相反的
        ModelView[i][3] = -center[i];
    }
}

// 求重心坐标系数
// 参考Referrence/barycentric coordinates.jpg
Vec3f barycentric(Vec2f A, Vec2f B, Vec2f C, Vec2f P)
{
    Vec3f s[2];
    for (int i = 2; i--; )
    {
        s[i][0] = C[i] - A[i];
        s[i][1] = B[i] - A[i];
        s[i][2] = A[i] - P[i];
    }
    // 重心坐标向量u
    Vec3f u = cross(s[0], s[1]);

    // 因为t.z = 2 * 三角形面积，小于等于0说明是退化三角形。
    // 又因为精度原因，取一个阈值1
    // 如果是退化三角形，则返回带负值的重心坐标系数，因为重心坐标系数小于0的时候说明点不在三角形内部，绘制时舍弃
    if (abs(u[2]) > 1e-2)
        return Vec3f(1.f - (u.x + u.y) / u.z, u.y / u.z, u.x / u.z);
    return Vec3f(-1, 1, 1);
}

void triangle(mat<4,3,float> &clipc, IShader &shader, TGAImage &image, float *zbuffer) 
{
    // 因为存储方式是和opengl一样按列存储，所以需要转置方便计算
    // 用viewport转为屏幕坐标，因为光栅化是基于屏幕像素进行插值
    mat<3,4,float> pts  = (Viewport*clipc).transpose();

    // 屏幕坐标标准化
    // 除以齐次分量w(位移和透视视角都会影响w)，4D=>3D
    // 用proj<2> 3D=>2D
    mat<3,2,float> pts2;
    for (int i=0; i<3; i++) pts2[i] = proj<2>(pts[i]/pts[i][3]);

    // 初始bbox为空
    Vec2f bboxmin( std::numeric_limits<float>::max(),  std::numeric_limits<float>::max());
    Vec2f bboxmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());

    // // 计算bbox
    Vec2f clamp(image.get_width() - 1, image.get_height() - 1);
    for (int i=0; i<3; i++) 
    {
        for (int j=0; j<2; j++) 
        {
            bboxmin[j] = std::max(0.f,      std::min(bboxmin[j], pts2[i][j]));
            bboxmax[j] = std::min(clamp[j], std::max(bboxmax[j], pts2[i][j]));
        }
    }
    Vec2i P;
    TGAColor color;
    // 遍历bbox内的所有像素
    for (P.x=bboxmin.x; P.x<=bboxmax.x; P.x++) 
    {
        for (P.y=bboxmin.y; P.y<=bboxmax.y; P.y++) 
        {
            // 求重心坐标系数bc，要用标准化的屏幕坐标计算
            Vec3f bc = barycentric(pts2[0], pts2[1], pts2[2], P);

            // 根据重心坐标法，用插值算出三角形任意一个像素的深度z和齐次分量w，最终深度是z/w
            float z = pts[0][2] * bc.x + pts[1][2] * bc.y + pts[2][2] * bc.z;
            float w = pts[0][3] * bc.x + pts[1][3] * bc.y + pts[2][3] * bc.z;
            // 深度限制在[0,255]，注意四舍五入
            int frag_depth = std::max(0, std::min(255, int(z / w + .5)));

            // 如果点不在三角形内部就不渲染
            // 如果当前像素的深度值小于zbuffer中的，则不渲染（注意和OPENGL深度值判断是相反的）
            if (bc.x<0 || bc.y<0 || bc.z<0 || zbuffer[P.x+P.y*image.get_width()]>frag_depth) continue;

            bool discard = shader.fragment(bc, color);
            if (!discard) 
            {
                // 更新zbuffer并绘制
                zbuffer[P.x+P.y*image.get_width()] = frag_depth;
                image.set(P.x, P.y, color);
            }
        }
    }
}

