#include "math.h"

Mat4 Mat4Translate(Vec2 vec)
{
	Mat4 result = MAT4_IDENTITY;
	MAT4_INDEX(result,3,0) = vec.x;
	MAT4_INDEX(result,3,1) = vec.y;
	return result;
}

Mat4 Mat4Scale(Vec2 vec)
{
	Mat4 result = MAT4_IDENTITY;
	MAT4_INDEX(result,0,0) = vec.x;
	MAT4_INDEX(result,1,1) = vec.y;
	return result;
}

Mat4 Mat4Orthographic(float left,float right,float bottom,float top,float near,float far)
{
	Mat4 result = MAT4_IDENTITY;
	MAT4_INDEX(result,0,0) = 2.0f / (right - left);
	MAT4_INDEX(result,1,1) = 2.0f / (top - bottom);
	MAT4_INDEX(result,2,2) = 1.0f / (far - near);
	MAT4_INDEX(result,3,0) = - (right + left) / (right - left);
	MAT4_INDEX(result,3,1) = - (top + bottom) / (top - bottom);
	MAT4_INDEX(result,3,2) = - near / (far - near);
	return result;
}

Mat4 Mat4Mul(Mat4 a,Mat4 b)
{
	Mat4 result = {0};
	for(unsigned y = 0;y < 4;++y)
	{
		for(unsigned x = 0;x < 4;++x)
		{
			MAT4_INDEX(result,x,y) = MAT4_INDEX(a,0,y) * MAT4_INDEX(b,x,0) +
									 MAT4_INDEX(a,1,y) * MAT4_INDEX(b,x,1) +
									 MAT4_INDEX(a,2,y) * MAT4_INDEX(b,x,2) +
									 MAT4_INDEX(a,3,y) * MAT4_INDEX(b,x,3);
		}
	}
	return result;
}