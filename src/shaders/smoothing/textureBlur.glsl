
// bokeh blur
vec4 blurredTexture(in sampler2D image, in vec2 position, in float blur) {

	const float stepSize = 1.f/min(textureSize(image, 0).x, textureSize(image, 0).y);
	const float radius = blur;

	// go through overlapped regions

	// centre pixel
	vec4 pixel = texture(image, position);
	int pixelCount = 1;

	// horizontal row of pixels along off.y = 0 (ignoring the center pixel)
	for (vec2 off = {stepSize, 0}; off.x <= radius; off.x += stepSize) {
		pixel += texture(image, position+off); // right
		pixel += texture(image, position-off); // left
		pixel += texture(image, position+off.yx); // top
		pixel += texture(image, position-off.yx); // bottom
	}
	pixelCount += 4*int(radius/stepSize);

	// Go through the octant from 0 rad to pi/4 rad
	for (vec2 off = {0,stepSize}; sqrt(2)*off.y <= radius; off.y += stepSize) {
		// diagonals
		const vec2 negate = vec2(1, -1);
		pixel += texture(image, position+off.yy); // top right
		pixel += texture(image, position-off.yy); // bottom left
		pixel += texture(image, position+negate*off.yy); // bottom right
		pixel += texture(image, position-negate*off.yy); // top left
		for (off.x = off.y+stepSize; dot(off, off) <= radius*radius; off.x += stepSize) {
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
	pixelCount += 4*int(radius/(sqrt(2)*stepSize));

	return pixel/pixelCount;
}
