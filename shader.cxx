#include "shader.hxx"

#define NV_EXTENSIONS
#include <shaderc/shaderc.hpp>

CShader::CShader(std::string const& name, EShaderType type, std::string const& source)
    : m_name(name)
    , m_type(type)
    , m_source(source)
{

}

uint32_t CShader::shaderType2Internal(CShader::EShaderType type) {

    switch (type) {
    case SHADER_TYPE_VERTEX_SHADER:
        return shaderc_vertex_shader;
    case SHADER_TYPE_FRAGMENT_SHADER:
        return shaderc_fragment_shader;
    case SHADER_TYPE_GEOMETRY_SHADER:
        return shaderc_geometry_shader;
    case SHADER_TYPE_TESSELATION_CONTROL_SHADER:
        return shaderc_tess_control_shader;
    case SHADER_TYPE_TESSELATION_EVAL_SHADER:
        return shaderc_tess_evaluation_shader;
    case SHADER_TYPE_COMPUTE_SHADER:
        return shaderc_compute_shader;
    case SHADER_TYPE_RAY_GENERATION_SHADER:
        return shaderc_raygen_shader;
    case SHADER_TYPE_CLOSEST_HIT_SHADER:
        return shaderc_closesthit_shader;
    case SHADER_TYPE_MISS_SHADER:
        return shaderc_miss_shader;
    case SHADER_TYPE_ANY_HIT_SHADER:
        return shaderc_anyhit_shader;
    case SHADER_TYPE_INTERSECTION_SHADER:
        return shaderc_intersection_shader;
    default:
        break;
    }

    return shaderc_vertex_shader;
}

std::vector<uint32_t> CShader::compileFile(bool optimize) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_size);

    shaderc_shader_kind kind = static_cast<shaderc_shader_kind>(CShader::shaderType2Internal(m_type));

    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(m_source.c_str(), m_source.size(), kind, m_name.c_str(), "main", options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        printf("Shader compilation failed, reason: %s\n", module.GetErrorMessage().c_str());
        return std::vector<uint32_t>();
    }

    return {module.cbegin(), module.cend()};
}

std::string CShader::compileFileToAssembly(bool optimize) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_size);

    shaderc_shader_kind kind = static_cast<shaderc_shader_kind>(CShader::shaderType2Internal(m_type));

    shaderc::AssemblyCompilationResult module = compiler.CompileGlslToSpvAssembly(m_source.c_str(), m_source.size(), kind, m_name.c_str(), "main", options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        printf("Shader compilation failed, reason: %s\n", module.GetErrorMessage().c_str());
        return "";
    }

    return {module.cbegin(), module.cend()};
}
