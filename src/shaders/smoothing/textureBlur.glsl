
// bokeh blur
vec4 blurredTexture(in sampler2D image, in vec2 position, in float blur) {
	if (blur == 0)
		return texture(image, position);

	vec4 pixel = vec4(0);
	const float stepSize = 1.f/min(textureSize(image, 0).x, textureSize(image, 0).y);
	const float radius = blur;

	int pixelCount = 0;

	// start at centre pixel and go outwards.
	for (vec2 off = {0,0}; off.y <= radius; off.y += stepSize) {
		for (off.x = 0; off.x <= radius; off.x += stepSize) {
			vec2 pos1 = position+off;
			vec2 pos2 = position-off;

			if (dot(off, off) <= radius*radius) {
				pixel += texture(image, pos1);
				pixel += texture(image, vec2(pos1.x, pos2.y));
				pixel += texture(image, vec2(pos2.x, pos1.y));
				pixel += texture(image, pos2);

				pixelCount += 4;
			}
		}
	}

	return pixel/pixelCount;
}
