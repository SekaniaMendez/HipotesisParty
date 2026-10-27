#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;

out vec4 finalColor;

void main()
{
    vec4 base = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);
    vec3 eye = normalize(viewPos - fragPosition);

    // A warm key and broad cool sky keep stone and faces readable from above.
    vec3 key = normalize(vec3(-0.55, 0.90, 0.35));
    vec3 fill = normalize(vec3(0.75, 0.35, -0.65));
    float sky = normal.y * 0.5 + 0.5;
    vec3 ambient = mix(vec3(0.28, 0.29, 0.32), vec3(0.48, 0.55, 0.62), sky);
    float diffuse = max(dot(normal, key), 0.0);
    float fillDiffuse = max(dot(normal, fill), 0.0);
    vec3 light = ambient + vec3(0.70, 0.62, 0.47) * diffuse
                         + vec3(0.13, 0.20, 0.24) * fillDiffuse;

    vec3 halfway = normalize(key + eye);
    float specular = pow(max(dot(normal, halfway), 0.0), 36.0) * diffuse;
    float rim = pow(1.0 - max(dot(normal, eye), 0.0), 3.0);
    vec3 color = base.rgb * light + vec3(1.0, 0.85, 0.62) * specular * 0.12
                                + vec3(0.08, 0.15, 0.17) * rim * sky;
    color = pow(max(color, vec3(0.0)), vec3(0.96));

    // The near field stays clear; gentle haze softens only the far perimeter.
    float distanceToEye = length(viewPos - fragPosition);
    float fog = smoothstep(45.0, 110.0, distanceToEye) * 0.58;
    color = mix(color, vec3(13.0, 24.0, 39.0) / 255.0, fog);
    finalColor = vec4(color, base.a);
}
