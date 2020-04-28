#extension GL_GOOGLE_include_directive : enable

struct SomeStruct {
    vec3 test;
};

SomeStruct some_function(vec2 abc) {
    SomeStruct result;
    result.test = vec3(abc, 0.0);
    return result;
}