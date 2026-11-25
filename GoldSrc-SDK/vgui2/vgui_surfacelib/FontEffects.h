#pragma once

float* CreateGaussianDistribution( int blur );

void ApplyRotaryEffectToTexture( int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba, bool rotary );
void ApplyScanlineEffectToTexture(
	int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba, int scanLines );
void ApplyDropShadowToTexture(
	int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba, int dropShadowOffset );
void ApplyOutlineToTexture(
	int rgbaX, int rgbaY, int rgbaWide, int charWide, int charTall, unsigned char* rgba, int outlineSize );
void ApplyGaussianBlurToTexture(
	int rgbaWide, int rgbaTall, unsigned char* rgba, int blur, const float* gaussianDistribution );
