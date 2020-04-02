
#define EXPORT extern "C" __declspec(dllexport)
EXPORT Renderer::ExportedFunctions exportedFunctions;
Renderer::ExportedFunctions exportedFunctions;
struct ExportedFunctionAdder {
	ExportedFunctionAdder(char const* name, u32 offset) { exportedFunctions.push_back({name, offset}); }
};

#define R_OFFSET(name) ((size_t*)&((Renderer*)0)->RNAME(name) - (size_t*)&((Renderer*)0)->functions)

#undef R_DECORATE
#define R_DECORATE(type, name, args, params) ExportedFunctionAdder _efa_##name(#name, (u32)(R_OFFSET(name)))

R_ALLFUNS;
#undef R_DECORATE
#undef R_OFFSET
