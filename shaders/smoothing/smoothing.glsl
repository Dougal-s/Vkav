// simulates address mode VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
float wrapIndex(float index) {
	return 1.f - abs(mod(abs(index), 2.f)-1.f);
}

float texture(in samplerBuffer s, in float index) {
	return texelFetch(s, int(wrapIndex(index)*audioSize)).r;
}

float textureExp(in samplerBuffer s, in float index) {
	return texelFetch(s, int(exp2(wrapIndex(index))*audioSize-audioSize)).r;
}

float kernelSmoothTexture(in samplerBuffer s, in float index) {
	// clamp input between 0 and 1
	index = wrapIndex(index);

	if (smoothingLevel == 0.f)
		return texture(s, index);

	const float pi = 3.14159265359;
	float val = 0.f;

	const float smoothingFactor = 0.5f/(smoothingLevel*smoothingLevel);
	const float radius = sqrt(-log(0.05f)/smoothingFactor)/audioSize;
	for (float i = index-radius; i < index+radius; i += 1.f/audioSize) {
		const float distance = audioSize*(index - i);
		const float weight = exp(-distance*distance*smoothingFactor);
		#ifdef exponential
			val += textureExp(s, i)*weight;
		#else
			val += texture(s, i)*weight;
		#endif
	}
	val *= sqrt(smoothingFactor/pi);

	return val;
}

float mcatSmoothTexture(in samplerBuffer s, in float index) {
	// clamp input between 0 and 1
	index = wrapIndex(index);

	if (smoothingLevel == 0.f)
		return texture(s, index);

	const float pi = 3.14159265359;
	float val = 0.f;

	const float smoothingFactor = 1.f+1.f/smoothingLevel;
	const float radius = log(30.f)/(log(smoothingFactor)*audioSize);
	for (float i = index-radius; i < index+radius; i += 1.f/audioSize) {
		const float distance = audioSize*abs(i - index);

		#ifdef exponential
			val = max(textureExp(s, i) * pow(smoothingFactor, -distance), val);
		#else
			val = max(texture(s, i) * pow(smoothingFactor, -distance), val);
		#endif
	}
	val *= 0.3;

	return val;
}
