
// gaussian blur
vec4 blurredTexture(in sampler2D image, in vec2 position, in float volume) {
	vec4 pixel = vec4(0.f, 0.f, 0.f, 0.f);

	float radius = volume;

	float sum = 0;
	for (float y = position.y-radius; y <= position.y+radius; y += 1.f/textureSize(image, 0).y) {
		for (float x = position.x-radius; x <= position.x+radius; x += 1.f/textureSize(image, 0).x) {
			float weight = exp( ((x-position.x)*(x-position.x)/(2*volume)) * ((y-position.y)*(y-position.y)/(2*volume)) );
			pixel += texture(image, vec2(x, y))*weight;
			sum += weight;
		}
	}

	pixel /= sum;
	return pixel;
}
