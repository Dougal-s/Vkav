// simulates address mode VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
float texture(in samplerBuffer s, in float index) {
	return texelFetch(s, int(( 1.f - abs(mod(abs(index), 2.f)-1.f) )*audioSize)).r;
}


float smoothTexture(in samplerBuffer s, in float index) {
	const float pi = 3.14159265359;
	if (smoothingLevel != 0.f) {
		float val = 0.f;
		
		#if defined(kernel)
			const float smoothingFactor = 0.5f/(smoothingLevel*smoothingLevel);
			const float radius = sqrt(-log(0.05f)/smoothingFactor)/audioSize;
			for (float i = index-radius; i < index+radius; i += 1.f/audioSize) {
				const float distance = audioSize*(index - i);
				const float weight = exp(-distance*distance*smoothingFactor);
				val += texture(s, i)*weight;
			}
			val /= sqrt(pi/smoothingFactor);
		#elif defined(monstercat)
			const float smoothingFactor = 1.f+1.f/smoothingLevel;
			const float radius = log(30.f)/(log(smoothingFactor)*audioSize);
			for (float i = index-radius; i < index+radius; i += 1.f/audioSize) {
				const float distance = audioSize*abs(i - index);
				val = max(texture(s, i) * pow(smoothingFactor, -distance), val);
			}
			val *= 0.3;
		#else
			val = texture(s, index);
		#endif
		
		return val;
		
	} else {
		return texture(s, index);
	}
}
