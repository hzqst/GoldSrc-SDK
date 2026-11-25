#include "FontEffects.h"

#include <cmath>
#include <cstddef>
#include <vector>

float* CreateGaussianDistribution( int blur )
{
	if ( blur <= 1 )
		return nullptr;

	float* distribution = new float[static_cast<size_t>( blur * 2 + 1 )];
	const double sigma = static_cast<double>( blur ) * 0.683;
	const double scaleFactor = std::sqrt( 6.28 * sigma * sigma );
	const double normalization = 1.0 / scaleFactor;
	for ( int i = 0; i <= blur * 2; i++ )
	{
		const int x = i - blur;
		const double exponent = -static_cast<double>( x * x ) / ( ( sigma + sigma ) * sigma );
		distribution[i] = static_cast<float>( std::pow( 2.7, exponent ) * normalization );
	}
	return distribution;
}

void ApplyRotaryEffectToTexture( int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba, bool rotary )
{
	if ( !rotary )
		return;

	const int y = static_cast<int>( static_cast<double>( rgbaTall ) * 0.5 );
	unsigned char* dst = &rgba[4 * rgbaX + 4 * rgbaWide * ( rgbaY + y )];
	for ( int x = 0; x < rgbaWide; x++, dst += 4 )
	{
		dst[0] = 127;
		dst[1] = 127;
		dst[2] = 127;
		dst[3] = 255;
	}
}

void ApplyScanlineEffectToTexture(
	int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba, int scanLines )
{
	if ( scanLines <= 1 )
		return;

	for ( int y = 1; y < rgbaTall; y++ )
	{
		if ( !( y % scanLines ) )
			continue;

		unsigned char* dst = &rgba[4 * ( rgbaX + rgbaWide * ( rgbaY + y ) )];
		for ( int x = 0; x < rgbaWide; x++, dst += 4 )
		{
			dst[0] = static_cast<unsigned char>( dst[0] * 0.7f );
			dst[1] = static_cast<unsigned char>( dst[1] * 0.7f );
			dst[2] = static_cast<unsigned char>( dst[2] * 0.7f );
		}
	}
}

void ApplyDropShadowToTexture(
	int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba, int dropShadowOffset )
{
	if ( !dropShadowOffset || dropShadowOffset > rgbaTall - 1 )
		return;

	for ( int y = rgbaTall - 1; y >= dropShadowOffset; y-- )
	{
		for ( int x = rgbaWide - 1; x >= dropShadowOffset; x-- )
		{
			unsigned char* dst = &rgba[4 * ( rgbaX + x + rgbaWide * ( rgbaY + y ) )];
			if ( dst[3] )
				continue;

			const unsigned char* src =
				&rgba[4 * ( rgbaX + x - dropShadowOffset + rgbaWide * ( rgbaY + y - dropShadowOffset ) )];
			dst[0] = 0;
			dst[1] = 0;
			dst[2] = 0;
			dst[3] = src[3];
		}
	}
}

void ApplyOutlineToTexture(
	int rgbaX, int rgbaY, int rgbaWide, int charWide, int charTall, unsigned char* rgba, int outlineSize )
{
	if ( !outlineSize )
		return;

	for ( int y = 0; y < charTall; y++ )
	{
		unsigned char* src = &rgba[4 * rgbaX + 4 * rgbaWide * ( rgbaY + y )];
		for ( int x = 0; x < charWide; x++, src += 4 )
		{
			if ( src[3] )
				continue;

			for ( int shadowX = -outlineSize; shadowX <= outlineSize; shadowX++ )
			{
				const int testX = x + shadowX;
				if ( testX < 0 || testX >= charWide )
					continue;

				for ( int shadowY = -outlineSize; shadowY <= outlineSize; shadowY++ )
				{
					if ( !shadowX && !shadowY )
						continue;

					const int testY = y + shadowY;
					if ( testY < 0 || testY >= charTall )
						continue;

					const unsigned char* test = &rgba[4 * ( rgbaX + testX + rgbaWide * ( rgbaY + testY ) )];
					if ( test[0] && test[1] && test[2] && test[3] )
					{
						src[0] = 0;
						src[1] = 0;
						src[2] = 0;
						src[3] = 255;
					}
				}
			}
		}
	}
}

void ApplyGaussianBlurToTexture(
	int rgbaWide, int rgbaTall, unsigned char* rgba, int blur, const float* gaussianDistribution )
{
	if ( !gaussianDistribution || blur <= 0 || rgbaWide <= 0 || rgbaTall <= 0 )
		return;

	const size_t size = static_cast<size_t>( rgbaTall ) * static_cast<size_t>( rgbaWide ) * 4;
	std::vector<unsigned char> src( rgba, rgba + size );

	unsigned char* dst = rgba;
	for ( int y = 0; y < rgbaTall; y++ )
	{
		for ( int x = 0; x < rgbaWide; x++ )
		{
			int maxX = x + blur;
			if ( maxX > rgbaWide - 1 )
				maxX = rgbaWide - 1;
			int minX = x - blur;
			if ( minX < 0 )
				minX = 0;
			int minY = y - blur;
			if ( minY < 0 )
				minY = 0;
			int maxY = y + blur;
			if ( maxY > rgbaTall - 1 )
				maxY = rgbaTall - 1;

			double accum = 0.0;
			for ( int srcX = minX; srcX <= maxX; srcX++ )
			{
				const double weightX = gaussianDistribution[srcX - x + blur];
				for ( int srcY = minY; srcY <= maxY; srcY++ )
				{
					const double weightY = gaussianDistribution[srcY - y + blur];
					accum += static_cast<double>( src[static_cast<size_t>( srcX + srcY * rgbaWide ) * 4] )
						* weightX * weightY;
				}
			}

			int alpha = static_cast<int>( accum );
			if ( alpha > 255 )
				alpha = 255;

			dst[0] = 255;
			dst[1] = 255;
			dst[2] = 255;
			dst[3] = static_cast<unsigned char>( alpha );
			dst += 4;
		}
	}
}
