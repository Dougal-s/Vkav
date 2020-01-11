// simulates address mode VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
float wrapIndex(float index) {
	return 1.f - abs(mod(abs(index), 2.f)-1.f);
}

float texture(in samplerBuffer s, in float index) {
	return texelFetch(s, int(wrapIndex(index)*audioSize)).r;
}

float kernelSmoothTexture(in samplerBuffer s, in float smoothingAmount, in float index) {
	if (smoothingAmount == 0.f)
		return texture(s, index);

	const float pi = 3.14159265359;
	float val = 0.f;

	const float smoothingFactor = 0.5f/(smoothingAmount*smoothingAmount);
	const float radius = sqrt(-log(0.05f)/smoothingFactor)/audioSize;
	for (float i = -radius; i < radius; i += 1.f/audioSize) {
		const float distance = audioSize*i;
		const float weight = exp(-distance*distance*smoothingFactor);
		val += texture(s, index+i)*weight;
	}
	val *= sqrt(smoothingFactor/3.14159265359);

	return val;
}

float mcatSmoothTexture(in samplerBuffer s, in float smoothingAmount, in float index) {
	if (smoothingAmount == 0.f)
		return texture(s, index);

	float val = 0.f;

	const float smoothingFactor = 1.f+1.f/smoothingAmount;
	const float radius = log(30.f)/(log(smoothingFactor)*audioSize);
	for (float i = -radius; i < radius; i += 1.f/audioSize) {
		val = max(texture(s, index+i) * pow(smoothingFactor, -audioSize*abs(i)), val);
	}
	val *= 0.3;

	return val;
}
