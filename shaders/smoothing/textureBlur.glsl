
// bokeh blur
vec4 blurredTexture(in sampler2D image, in vec2 position, float volume) {
	vec4 pixel = vec4(0.f, 0.f, 0.f, 0.f);

	const float stepSize = 1.f/max(textureSize(image, 0).x, textureSize(image, 0).y);
	const float radius = volume;

	float sum = 0;

	// start at centre pixel and go outwards.
	for (float yOff = 0; yOff <= radius; yOff += stepSize) {
		float y1 = position.y+yOff;
		float y2 = position.y-yOff;
		for (float xOff = 0; xOff <= radius; xOff += stepSize) {
			float x1 = position.x+xOff;
			float x2 = position.x-xOff;

			if (distance(position, vec2(x1, y1)) <= volume) {
				pixel += texture(image, vec2(x1, y1));
				pixel += texture(image, vec2(x1, y2));
				pixel += texture(image, vec2(x2, y1));
				pixel += texture(image, vec2(x2, y2));

				sum += 4;
			}
		}
	}

	pixel /= sum;
	return pixel;
}
