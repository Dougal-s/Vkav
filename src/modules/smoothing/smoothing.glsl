// simulates address mode VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
float wrapIndex(float index) {
	return 1.f - abs(mod(abs(index), 2.f)-1.f);
}

float texture(in samplerBuffer s, in float index) {
	return texelFetch(s, int(wrapIndex(index)*textureSize(s))).r;
}

float kernelSmoothTexture(in samplerBuffer s, in float smoothingAmount, in float index) {
	if (smoothingAmount == 0.f)
		return texture(s, index);

	const float pi = 3.14159265359;
	float val = texture(s, index);

	int size = textureSize(s);

	const float smoothingFactor = 0.5f/(smoothingAmount*smoothingAmount);
	const float radius = sqrt(-log(0.05f)/smoothingFactor);
	const float stepSize = 1.f/size;
	float sum = 0.f;
	// skip i = 0
	for (float i = stepSize; i < radius; i += stepSize) {
		const float weight = exp(-i*i*smoothingFactor);
		val += weight*(texture(s, index+i)+texture(s, index-i));
		sum += weight;
	}
	val /= sum;

	return val;
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
