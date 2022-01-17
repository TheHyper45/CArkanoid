#ifndef MATH_H
#define MATH_H

#define PI 3.1415927f
#define RADIANS(x) ((x) * PI / 180.0f)

typedef struct Vec2
{
	float x;
	float y;
} Vec2;

typedef struct Mat4
{
	float matrix[16];
} Mat4;

#define MAT4_IDENTITY (Mat4){.matrix = {[0] = 1.0f,[5] = 1.0f,[10] = 1.0f,[15] = 1.0f}}
#define MAT4_INDEX(mat,x,y) mat.matrix[(x) * 4 + (y)]

Mat4 Mat4Translate(Vec2 vec);
Mat4 Mat4Scale(Vec2 vec);
Mat4 Mat4Orthographic(float left,float right,float bottom,float top,float near,float far);
Mat4 Mat4Mul(Mat4 a,Mat4 b);

#endif