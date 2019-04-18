
// bokeh blur
vec4 blurredTexture(in sampler2D image, in vec2 position, in float volume) {
	vec4 pixel = vec4(0.f, 0.f, 0.f, 0.f);

	const float radius = volume;
	const float stepSize = min(1.f/textureSize(image, 0).x, 1.f/textureSize(image, 0).y);

	float sum = 0;
	for (float y = position.y-radius; y <= position.y+radius; y += stepSize) {
		for (float x = position.x-radius; x <= position.x+radius; x += stepSize) {
			if (distance(position, vec2(x,y)) <= distance(position, vec2(volume, volume))) {
				pixel += texture(image, vec2(x, y));
				++sum;
			}
		}
	}

	pixel /= sum;
	return pixel;
}
