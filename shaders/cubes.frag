#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in flat uint fragTexIndex;

layout(set = 0, binding = 1) uniform sampler2D textures[7];

float factor = 8.0f;

void main()
{
    switch(fragTexIndex)
    {
        case 0:
            factor = 8.0f;
            break;
        case 1:
            factor = 4.0f;
            break;
        case 2:
            factor = 2.0f;
            break;
    }

    vec2 flipped = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y);

    // outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0);
    outColor = texture(textures[fragTexIndex], flipped * factor);
    // outColor = vec4(fragTexCoord, 0.0,1.0);
}