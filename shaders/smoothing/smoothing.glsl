float smoothTexture(in sampler1D s, in float index) {
	if (smoothingLevel != 0.f) {
		float val = 0.f;
		#if defined(kernel)
			const float smoothingFactor = 0.5f/(smoothingLevel*smoothingLevel);
			const float radius = sqrt(-log(0.05f)/smoothingFactor)/audioSize;
			float sum = 0.f;
			for (float i = index-radius; i < index+radius; i += 1.f/audioSize) {
				float distance = audioSize*(index - i);
				float weight = exp(-distance*distance*smoothingFactor);
				val += texture(s, i).r*weight;
				sum += weight;
			}
			val /= sum;
		#elif defined(monstercat)
			const float smoothingFactor = 1.f+1.f/smoothingLevel;
			float radius = log(20.f)/(log(smoothingFactor)*audioSize);
			for (float i = index-radius; i < index+radius; i += 1.f/audioSize) {
				float distance = audioSize*abs(i - index);
				val = max(texture(s, i).r / pow(smoothingFactor, distance), val);
			}
			val *= 0.4;
		#else
			val = texture(s, index).r
		#endif
		return val;
	} else {
		return texture(s, index).r;
	}
}
