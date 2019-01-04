#ifndef SHADER_HXX
#define SHADER_HXX

#include <vector>
#include <string>

class CShader
{
public:
    enum EShaderType {
        SHADER_TYPE_UNKNOWN,
        SHADER_TYPE_VERTEX_SHADER,
        SHADER_TYPE_FRAGMENT_SHADER,
        SHADER_TYPE_GEOMETRY_SHADER,
        SHADER_TYPE_TESSELATION_CONTROL_SHADER,
        SHADER_TYPE_TESSELATION_EVAL_SHADER,
        SHADER_TYPE_COMPUTE_SHADER,
        SHADER_TYPE_RAY_GENERATION_SHADER,
        SHADER_TYPE_CLOSEST_HIT_SHADER,
        SHADER_TYPE_MISS_SHADER,
        SHADER_TYPE_ANY_HIT_SHADER,
        SHADER_TYPE_INTERSECTION_SHADER
    };

public:
    CShader(std::string const& name, EShaderType type, std::string const& source);

    std::vector<uint32_t> compileFile(bool optimize = false);
    std::string compileFileToAssembly(bool optimize = false);

    static uint32_t shaderType2Internal(EShaderType type);
private:
    std::string m_name;
    EShaderType m_type;
    std::string m_source;
};

#endif // SHADER_HXX
