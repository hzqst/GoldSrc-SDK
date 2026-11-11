//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Implements math functions specific to Worldcraft.
//
// $NoKeywords: $
//=============================================================================

#include "worldcraft_mathlib.h"
#include <assert.h>
#include <string.h>
#include <Windows.h>

// provide implementation for mathlib Sys_Error()
extern void Error(char* fmt, ...);
extern "C" void Sys_Error( char *error, ... )
{
	Error( "%s", error );
}

void polyMake( float x1, float y1, float x2, float y2, int npoints, float start_ang, Vector* pmPoints )
{
	double angle = start_ang;
	double angleDelta = 360.0 / npoints;
	double xRadius = ( (double)x2 - x1 ) / 2;
	double yRadius = ( (double)y2 - y1 ) / 2;
	// make centerpoint for polygon:
	float xCenter = x1 + xRadius;
	float yCenter = y1 + yRadius;

	int point;
	for ( point = 0; point < npoints; point++, angle += angleDelta )
	{
		if ( angle > 360 )
			angle -= 360;

		double x = xCenter + sin( DEG2RAD( angle ) ) * (float)xRadius;
		double y = yCenter + cos( DEG2RAD( angle ) ) * (float)yRadius;
		pmPoints[point][0] = x > 0 ? floor( x + 0.5 ) : x < 0 ? ceil( x - 0.5 ) : 0;
		pmPoints[point][1] = y > 0 ? floor( y + 0.5 ) : y < 0 ? ceil( y - 0.5 ) : 0;
	}

	pmPoints[point][0] = pmPoints[0][0];
	pmPoints[point][1] = pmPoints[0][1];
}

float fixang(float a)
{
	if(a < 0.0)
		return a+360.0;
	if(a > 359.9)
		return a-360.0;

	return a;
}

float lineangle(float x1, float y1, float x2, float y2)
{
    float x, y;
	float rvl;

    x = x2 - x1;
    y = y2 - y1;

    if(!x && !y)
        return 0.0;

    rvl = RAD2DEG(atan2( y, x ));

    return (rvl);
}

float WorldcraftRint( float f )
{
	if ( f > 0 )
		return (float)floor( (double)f + 0.5 );
	if ( f < 0 )
		return (float)ceil( (double)f - 0.5 );
	return 0;
}

void rotate_coords(float *dx, float *dy, float sx, float sy, double angle)
{
    double r, theta;

    if( !sx && !sy )
    {
        *dx = 0.0;
        *dy = 0.0;
        return;
    }

	// special cases
	if(angle == 0.0 || angle == 360.0)
	{
		dx[0] = sx;
		dy[0] = sy;
		return;
	}
	else if(angle == 90.0)
	{
		dx[0] = -sy;
		dy[0] = sx;
		return;
	}
	else if(angle == 180.0)
	{
		dx[0] = -sx;
		dy[0] = -sy;
		return;
	}
	else if(angle == 270.0)
	{
		dx[0] = sy;
		dy[0] = -sx;
		return;
	}

    angle *= 0.0174533;

    r = hypot( sx, sy );
    theta = atan2( sy, sx );
    *dx = r * cos( theta + angle );
    *dy = r * sin( theta + angle );
}

#define FPU_HALF_PI_1	1.5707963276654482
#define FPU_HALF_PI_2	-8.705515692000731e-10
#define FPU_HALF_PI_3	-3.5236570605778894e-19

static double FpuReduce(double flRadians, int &nQuadrant)
{
	double flMultiple = floor(flRadians / FPU_HALF_PI_1 + 0.5);
	nQuadrant = (int)flMultiple & 3;

	return ((flRadians - flMultiple * FPU_HALF_PI_1) - flMultiple * FPU_HALF_PI_2) - flMultiple * FPU_HALF_PI_3;
}

double FpuSin(double flRadians)
{
	int nQuadrant;
	double flRemainder = FpuReduce(flRadians, nQuadrant);

	switch (nQuadrant)
	{
		case 0: return sin(flRemainder);
		case 1: return cos(flRemainder);
		case 2: return -sin(flRemainder);
		default: return -cos(flRemainder);
	}
}

double FpuCos(double flRadians)
{
	int nQuadrant;
	double flRemainder = FpuReduce(flRadians, nQuadrant);

	switch (nQuadrant)
	{
		case 0: return cos(flRemainder);
		case 1: return -sin(flRemainder);
		case 2: return -cos(flRemainder);
		default: return sin(flRemainder);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Builds the matrix for a counterclockwise rotation about an arbitrary axis.
// Input  : Matrix -
//			Axis -
//-----------------------------------------------------------------------------
void AxisAngleMatrix(matrix4_t& Matrix, const Vector& Axis, float fAngle)
{
	double flRadians = fAngle * 0.017453292519943295;
	double flSin = FpuSin(flRadians);
	float flCos = (float)FpuCos(flRadians);

	double flAxisXSquared = (double)Axis[0] * Axis[0];
	double flAxisYSquared = (double)Axis[1] * Axis[1];
	float flAxisZSquared = Axis[2] * Axis[2];

	Matrix[0][0] = (float)((1.0 - flAxisXSquared) * flCos + flAxisXSquared);
	float flOneMinusCos = (float)(1.0 - flCos);
	Matrix[0][1] = (float)((double)Axis[1] * Axis[0] * flOneMinusCos + flSin * Axis[2]);
	Matrix[0][2] = (float)((double)Axis[2] * Axis[0] * flOneMinusCos - flSin * Axis[1]);
	Matrix[0][3] = 0;

	Matrix[1][0] = (float)((double)Axis[1] * Axis[0] * flOneMinusCos - flSin * Axis[2]);
	Matrix[1][1] = (float)((1.0 - flAxisYSquared) * flCos + flAxisYSquared);
	Matrix[1][2] = (float)((double)Axis[2] * Axis[1] * flOneMinusCos + flSin * Axis[0]);
	Matrix[1][3] = 0;

	Matrix[2][0] = (float)((double)Axis[2] * Axis[0] * flOneMinusCos + flSin * Axis[1]);
	Matrix[2][1] = (float)((double)Axis[2] * Axis[1] * flOneMinusCos - flSin * Axis[0]);
	Matrix[2][2] = (float)((1.0 - flAxisZSquared) * flCos + flAxisZSquared);
	Matrix[2][3] = 0;

	// Column 3:
	Matrix[3][0] = 0;
	Matrix[3][1] = 0;
	Matrix[3][2] = 0;
	Matrix[3][3] = 1;
}

//-----------------------------------------------------------------------------
// Invert a matrix
//-----------------------------------------------------------------------------

int  LU_Decompose  (matrix4_t& matrix, int index[4])
{
    int    i, j, k;		/* Loop Indices */
	int    imax;		/* Maximum i Index */
	float big;		/* Non-Zero Largest Element */
	float sum;		/* Accumulator */
	float temp;		/* Scratch Variable */
	float vv[4];		/* Implicit Scaling of Each Row */

	for (i=0;  i < 4;  ++i)
	{
		big = 0;
		for (j=0;  j < 4;  ++j)
		{
			if ((temp = fabs (matrix[i][j])) > big)
				big = temp;
		}
		if (big == 0.0)
			return 0;
		vv[i] = 1 / big;
	}

	for (j=0;  j < 4;  ++j)
	{
		for (i=0;  i < j;  ++i)
		{
			sum = matrix[i][j];
			for (k=0;  k < i;  ++k)
				sum -= matrix[i][k] * matrix[k][j];
			matrix[i][j] = sum;
		}

		big = 0;
		for (i=j;  i < 4;  ++i)
		{
			sum = matrix[i][j];
			for (k=0;  k < j;  ++k)
				sum -= matrix[i][k] * matrix[k][j];
			matrix[i][j] = sum;
			temp = vv[i] * fabs(sum);
			if (temp >= big)
			{
				big = temp;
				imax = i;
			}
		}

		if (j != imax)
		{
			for (k=0;  k < 4;  ++k)
			{
				temp = matrix[imax][k];
				matrix[imax][k] = matrix[j][k];
				matrix[j][k] = temp;
			}
			vv[imax] = vv[j];
		}

		index[j] = imax;
		if (matrix[j][j] == 0.0)
			return 0;

		if (j != (4-1))
		{
			temp = 1 / matrix[j][j];
			for (i=j+1;  i < 4;  ++i)
				matrix[i][j] *= temp;
		}
	}

	return 1;
}

/****************************************************************************/

void  LU_Backsub  (matrix4_t& matrix, int index[4], float B[4])
{
	int    i,j;
	int    ii;
	int    ip;
	float sum;

	ii = -1;

	for (i=0;  i < 4;  ++i)
	{
		ip = index[i];
		sum = B[ip];
		B[ip] = B[i];
		if (ii >= 0)
		{
			for (j=ii;  j <= i-1;  ++j)
				sum -= matrix[i][j] * B[j];
		}
		else if (sum)
			ii = i;
		B[i] = sum;
	}

	for (i=(4-1);  i >= 0;  --i)
	{
		sum = B[i];
		for (j=i+1;  j < 4;  ++j)
			sum -= matrix[i][j] * B[j];
		B[i] = sum / matrix[i][i];
	}
}

// General, 4x4 matrix inversion
void  MatrixInvert(matrix4_t& matrix)
{
	int         i,j;
	int         index[4];
	float      column[4];
	matrix4_t inverse;

	if (!LU_Decompose (matrix, index))
    {
		// singular matrix
		assert(0);
		return;
    }

	for (j=0;  j < 4;  ++j)
	{
		for (i=0;  i < 4;  ++i)
		{
			column[i] = 0;
		}
		column[j] = 1;
		LU_Backsub (matrix, index, column);
		for (i=0;  i < 4;  ++i)
		{
			inverse[i][j] = column[i];
		}
	}

	memcpy (matrix.Base(), inverse.Base(), sizeof(matrix4_t));
}

//
// Multiplies a vector by a matrix.
//
void MatrixMultiply(Vector &Dest, const Vector &Source, const matrix4_t& Matrix)
{
//	assert( Dest != Source );
	Dest[0] = Source[0] * Matrix[0][0] + Source[1] * Matrix[0][1] + Source[2] * Matrix[0][2];
	Dest[1] = Source[0] * Matrix[1][0] + Source[1] * Matrix[1][1] + Source[2] * Matrix[1][2];
	Dest[2] = Source[0] * Matrix[2][0] + Source[1] * Matrix[2][1] + Source[2] * Matrix[2][2];
}

void MatrixMultiplyPoint(Vector& Dest, const Vector& Source, const matrix4_t& Matrix)
{
//	assert( &Dest != &Source );
	Dest[0] = Source[0] * Matrix[0][0] + Source[1] * Matrix[0][1] + Source[2] * Matrix[0][2] + Matrix[0][3];
	Dest[1] = Source[0] * Matrix[1][0] + Source[1] * Matrix[1][1] + Source[2] * Matrix[1][2] + Matrix[1][3];
	Dest[2] = Source[0] * Matrix[2][0] + Source[1] * Matrix[2][1] + Source[2] * Matrix[2][2] + Matrix[2][3];
}

void ProjectPoint( Vector& Dest, const Vector& Source, const matrix4_t& Matrix )
{
	float w;

	Dest[0] = Source[0] * Matrix[0][0] + Source[1] * Matrix[0][1] + Source[2] * Matrix[0][2] + Matrix[0][3];
	Dest[1] = Source[0] * Matrix[1][0] + Source[1] * Matrix[1][1] + Source[2] * Matrix[1][2] + Matrix[1][3];
	Dest[2] = Source[0] * Matrix[2][0] + Source[1] * Matrix[2][1] + Source[2] * Matrix[2][2] + Matrix[2][3];
	w		= Source[0] * Matrix[3][0] + Source[1] * Matrix[3][1] + Source[2] * Matrix[3][2] + Matrix[3][3];
	if (w != 0)
	{
		w = 1.0 / w;
		Dest[0] *= w;
		Dest[1] *= w;
		Dest[2] *= w;
	}
}

void RotateX(matrix4_t& Matrix, float fDegrees)
{
	float Temp01, Temp11, Temp21, Temp31;
	float Temp02, Temp12, Temp22, Temp32;

	float fRadians = DEG2RAD(fDegrees);

	float fSin = (float)sin(fRadians);
	float fCos = (float)cos(fRadians);

	Temp01 = Matrix[0][1] * fCos + Matrix[0][2] * fSin;
	Temp11 = Matrix[1][1] * fCos + Matrix[1][2] * fSin;
	Temp21 = Matrix[2][1] * fCos + Matrix[2][2] * fSin;
	Temp31 = Matrix[3][1] * fCos + Matrix[3][2] * fSin;

	Temp02 = Matrix[0][1] * -fSin + Matrix[0][2] * fCos;
	Temp12 = Matrix[1][1] * -fSin + Matrix[1][2] * fCos;
	Temp22 = Matrix[2][1] * -fSin + Matrix[2][2] * fCos;
	Temp32 = Matrix[3][1] * -fSin + Matrix[3][2] * fCos;

	Matrix[0][1] = Temp01;
	Matrix[1][1] = Temp11;
	Matrix[2][1] = Temp21;
	Matrix[3][1] = Temp31;

	Matrix[0][2] = Temp02;
	Matrix[1][2] = Temp12;
	Matrix[2][2] = Temp22;
	Matrix[3][2] = Temp32;
}

void RotateY( matrix4_t& Matrix, float fDegrees )
{
	float Temp00, Temp10, Temp20, Temp30;
	float Temp02, Temp12, Temp22, Temp32;

	float fRadians = DEG2RAD(fDegrees);

	float fSin = (float)sin(fRadians);
	float fCos = (float)cos(fRadians);

	Temp00 = Matrix[0][0] * fCos + Matrix[0][2] * -fSin;
	Temp10 = Matrix[1][0] * fCos + Matrix[1][2] * -fSin;
	Temp20 = Matrix[2][0] * fCos + Matrix[2][2] * -fSin;
	Temp30 = Matrix[3][0] * fCos + Matrix[3][2] * -fSin;

	Temp02 = Matrix[0][0] * fSin + Matrix[0][2] * fCos;
	Temp12 = Matrix[1][0] * fSin + Matrix[1][2] * fCos;
	Temp22 = Matrix[2][0] * fSin + Matrix[2][2] * fCos;
	Temp32 = Matrix[3][0] * fSin + Matrix[3][2] * fCos;

	Matrix[0][0] = Temp00;
	Matrix[1][0] = Temp10;
	Matrix[2][0] = Temp20;
	Matrix[3][0] = Temp30;

	Matrix[0][2] = Temp02;
	Matrix[1][2] = Temp12;
	Matrix[2][2] = Temp22;
	Matrix[3][2] = Temp32;
}

void RotateZ(matrix4_t& Matrix, float fDegrees)
{
	float Temp00, Temp10, Temp20, Temp30;
	float Temp01, Temp11, Temp21, Temp31;

	float fRadians = DEG2RAD(fDegrees);

	float fSin = (float)sin(fRadians);
	float fCos = (float)cos(fRadians);

	Temp00 = Matrix[0][0] * fCos + Matrix[0][1] * fSin;
	Temp10 = Matrix[1][0] * fCos + Matrix[1][1] * fSin;
	Temp20 = Matrix[2][0] * fCos + Matrix[2][1] * fSin;
	Temp30 = Matrix[3][0] * fCos + Matrix[3][1] * fSin;

	Temp01 = Matrix[0][0] * -fSin + Matrix[0][1] * fCos;
	Temp11 = Matrix[1][0] * -fSin + Matrix[1][1] * fCos;
	Temp21 = Matrix[2][0] * -fSin + Matrix[2][1] * fCos;
	Temp31 = Matrix[3][0] * -fSin + Matrix[3][1] * fCos;

	Matrix[0][0] = Temp00;
	Matrix[1][0] = Temp10;
	Matrix[2][0] = Temp20;
	Matrix[3][0] = Temp30;

	Matrix[0][1] = Temp01;
	Matrix[1][1] = Temp11;
	Matrix[2][1] = Temp21;
	Matrix[3][1] = Temp31;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : Matrix -
//-----------------------------------------------------------------------------
void ZeroMatrix(matrix4_t& Matrix)
{
	Matrix[0][0] = 0;
	Matrix[1][0] = 0;
	Matrix[2][0] = 0;
	Matrix[3][0] = 0;

	Matrix[0][1] = 0;
	Matrix[1][1] = 0;
	Matrix[2][1] = 0;
	Matrix[3][1] = 0;

	Matrix[0][2] = 0;
	Matrix[1][2] = 0;
	Matrix[2][2] = 0;
	Matrix[3][2] = 0;

	Matrix[0][3] = 0;
	Matrix[1][3] = 0;
	Matrix[2][3] = 0;
	Matrix[3][3] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Loads the given matrix with an identity matrix.
// Input  : Matrix - 4 x 4 matrix to be loaded with the identity matrix.
//-----------------------------------------------------------------------------
void IdentityMatrix(matrix4_t& Matrix)
{
	Matrix[0][0] = 1;
	Matrix[1][0] = 0;
	Matrix[2][0] = 0;
	Matrix[3][0] = 0;

	Matrix[0][1] = 0;
	Matrix[1][1] = 1;
	Matrix[2][1] = 0;
	Matrix[3][1] = 0;

	Matrix[0][2] = 0;
	Matrix[1][2] = 0;
	Matrix[2][2] = 1;
	Matrix[3][2] = 0;

	Matrix[0][3] = 0;
	Matrix[1][3] = 0;
	Matrix[2][3] = 0;
	Matrix[3][3] = 1;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : Matrix -
//-----------------------------------------------------------------------------
void TransposeMatrix(matrix4_t& Matrix)
{
	float fTemp;

	fTemp = Matrix[0][1];
	Matrix[0][1] = Matrix[1][0];
	Matrix[1][0] = fTemp;

	fTemp = Matrix[0][2];
	Matrix[0][2] = Matrix[2][0];
	Matrix[2][0] = fTemp;

	fTemp = Matrix[0][3];
	Matrix[0][3] = Matrix[3][0];
	Matrix[3][0] = fTemp;

	fTemp = Matrix[1][2];
	Matrix[1][2] = Matrix[2][1];
	Matrix[2][1] = fTemp;

	fTemp = Matrix[1][3];
	Matrix[1][3] = Matrix[3][1];
	Matrix[3][1] = fTemp;

	fTemp = Matrix[2][3];
	Matrix[2][3] = Matrix[3][2];
	Matrix[3][2] = fTemp;
}

//-----------------------------------------------------------------------------
// Purpose: Computes MatrixIn1 * MatrixIn2, placing the result in MatrixOut.
// Input  : MatrixIn1 -
//			MatrixIn2 -
//			MatrixOut -
//-----------------------------------------------------------------------------
void MultiplyMatrix(const matrix4_t& MatrixIn1, const matrix4_t& MatrixIn2, matrix4_t& MatrixOut)
{
	int nRow;
	int nCol;
	int i;

	for (nRow = 0; nRow < 4; nRow++)
	{
		for (nCol = 0; nCol < 4; nCol++)
		{
			MatrixOut[nRow][nCol] = 0;

			for (i = 0; i < 4; i++)
			{
				MatrixOut[nRow][nCol] = (float)(MatrixOut[nRow][nCol] + (double)MatrixIn1[nRow][i] * MatrixIn2[i][nCol]);
			}
		}
	}
}
