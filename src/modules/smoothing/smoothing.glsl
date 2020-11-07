// simulates address mode VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
float wrapIndex(float index) {
	return 1.f - abs(mod(abs(index), 2.f)-1.f);
}

float texture(in samplerBuffer s, in float index) {
	return texelFetch(s, int(wrapIndex(index)*textureSize(s))).r;
}

float kernelSmoothTexture(in samplerBuffer s, in float stdDeviation, in float index) {
	if (stdDeviation == 0.f)
		return texture(s, index);

	const float coef = 0.5f/(stdDeviation*stdDeviation);
	const float stepSize = 1.f/textureSize(s);

	float val = texture(s, index);
	for (float i = stepSize; i < 3*stdDeviation; i += stepSize) {
		const float weight = exp(-coef*i*i);
		val += weight*(texture(s, index+i)+texture(s, index-i));
	}

	return stepSize*val / (stdDeviation*sqrt(2*3.14159265359));
}

float mcatSmoothTexture(in samplerBuffer s, in float smoothingAmount, in float index) {
	if (smoothingAmount == 0.f)
		return texture(s, index);

	float val = texture(s, index);

	int size = textureSize(s);

	const float smoothingFactor = 1.f+1.f/smoothingAmount;
	const float radius = log(30.f)/(log(smoothingFactor)*size);
	const float stepSize = 1.f/size;
	// skip i = 0
	for (float i = stepSize; i < radius; i += stepSize) {
		float coeff = pow(smoothingFactor, -size*i);
		val = max(texture(s, index+i) * coeff, val);
		val = max(texture(s, index-i) * coeff, val);
	}
	val *= 0.3;

	return val;
}
