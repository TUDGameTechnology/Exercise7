#version 450

uniform sampler2D tex;

uniform vec3 light;
uniform vec3 eye;

uniform float spec;
uniform float roughness;
uniform int mode;

in vec3 position;
in vec2 texCoord;
in vec3 normal;

out vec4 FragColor;

// Schlick's Fresnel approximation
float F(vec3 l, vec3 h) {
	// Exercise 6 - Implement here:
	return 1.0;
}

// Trowbridge-Reitz normal distribution term
float D(vec3 n, vec3 h) {
	// Exercise 6 - Implement here:
	return 1.0;
}

// Cook and Torrance's geometry factor
float G(vec3 l, vec3 v, vec3 h, vec3 n) {
	// Exercise 6 - Implement here:
	return 1.0;
}

void main() {
	// determine the normal vector
	vec3 n = normalize(normal);
	// determine the light vector
	vec3 l = light - position;
	l = normalize(l);
	// determine the view vector
	vec3 v = eye - position;
	v = normalize(v);
	// determine the half vector
	vec3 h = l + v;
	h = normalize(h);
	// determine the BRDF
	// Exercise 6: Calculate the correct value here
	float f = 1.0;
	// determine texel
	vec3 t = pow(texture(tex, texCoord).rgb, vec3(2.2));
	
	// determine view dependend on mode
	vec3 rgb;
	if (mode == 1) {
		rgb = F(l, h) * vec3(1);
	}
	else if (mode == 2) {
		rgb = D(n, h) * vec3(1);
	}
	else if (mode == 3) {
		rgb = G(l, v, h, n) * vec3(1);
	}
	else {
		rgb = f * dot(l, n) * vec3(1) + dot(l, n) * t;
	}
	
	// other views
	//float fpure = 1.0 / (4.0 * dot(n, l) * dot(n, v));
	//vec3 rgb = fpure * texture2D(tex, texCoord).rgb;
	//vec3 rgb = t * dot(l, n);
	//vec3 rgb = (n + 1.0) / 2.0;

	FragColor = vec4(pow(rgb, vec3(1.0 / 2.2)), 1);
}
