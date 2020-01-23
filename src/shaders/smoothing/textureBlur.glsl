
// bokeh blur
vec4 blurredTexture(in sampler2D image, in vec2 position, in float blur) {
	if (blur == 0)
		return texture(image, position);

	vec4 pixel = vec4(0);
	const float stepSize = 1.f/min(textureSize(image, 0).x, textureSize(image, 0).y);
	const float radius = blur;

	int pixelCount = 0;

	// Go through the octant from 0 rad to pi/4 rad
	for (vec2 off = {0,0}; off.y <= radius; off.y += stepSize) {
		for (off.x = off.y; dot(off, off) <= radius*radius; off.x += stepSize) {
			vec4 offset = vec4(off, -off);
			pixel += texture(image, position+offset.xy); // top right
			pixel += texture(image, position+offset.xw); // bottom right
			pixel += texture(image, position+offset.zy); // top left
			pixel += texture(image, position+offset.zw); // bottom left

			// flip x and y axis
			pixel += texture(image, position+offset.yx);
			pixel += texture(image, position+offset.wx);
			pixel += texture(image, position+offset.yz);
			pixel += texture(image, position+offset.wz);

			pixelCount += 8;
		}
	}

	return pixel/pixelCount;
}
