
// bokeh blur
vec4 blurredTexture(in sampler2D image, in vec2 position, in float blur) {

	const float stepSize = 1.f/max(textureSize(image, 0).x, textureSize(image, 0).y);
	const float edgeWidth = 2*stepSize;
	const float edge0 = blur*blur-edgeWidth;
	const float edge1 = blur*blur+edgeWidth;
	const float radius = blur + edgeWidth;

	// centre pixel
	vec4 pixel = texture(image, position);
	int pixelCount = 1;

	// horizontal row of pixels along y = 0 (ignoring the center pixel)
	for (float off = stepSize; off <= radius; off += stepSize) {
		float distSq = off*off;
		float coef = smoothstep(edge0, edge1, distSq);
		pixel += coef*texture(image, position+vec2(off, 0)); // right
		pixel += coef*texture(image, position-vec2(off, 0)); // left
		pixel += coef*texture(image, position+vec2(0, off)); // right
		pixel += coef*texture(image, position-vec2(0, off)); // left
		pixelCount += 4;
	}

	for (float xOff = stepSize; xOff <= radius; xOff += stepSize) {
		for (float yOff = stepSize; xOff*xOff + yOff*yOff <= radius*radius; yOff += stepSize) {
			float distSq = xOff*xOff + yOff*yOff;
			float coef = smoothstep(edge0, edge1, distSq);
			pixel += coef * texture(image, position + vec2( xOff,  yOff));
			pixel += coef * texture(image, position + vec2( xOff, -yOff));
			pixel += coef * texture(image, position + vec2(-xOff,  yOff));
			pixel += coef * texture(image, position + vec2(-xOff, -yOff));
			pixelCount += 4;
		}
	}

	return pixel/pixelCount;
}
