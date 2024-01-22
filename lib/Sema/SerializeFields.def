// No include guards

// ===----------------------------------------------------------------------===
// === List of fields in serialized symbol tables --------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_SEMA_FIELD_DEF
#   define SC_SEMA_FIELD_DEF(Type, Name, Spelling)
#endif

/// We prepend underscores to entity type and name to make them the first
/// entries in the json objects so serialized files are easier to read
SC_SEMA_FIELD_DEF(EntityType, EntityType, "_entity_type")
SC_SEMA_FIELD_DEF(std::string, Name, "_name")
SC_SEMA_FIELD_DEF(json, Entities, "entities")
SC_SEMA_FIELD_DEF(json, Children, "children")
SC_SEMA_FIELD_DEF(json, NativeDependencies, "native_dependencies")
SC_SEMA_FIELD_DEF(json, ForeignDependencies, "foreign_dependencies")
SC_SEMA_FIELD_DEF(std::string, ReturnType, "return_type")
SC_SEMA_FIELD_DEF(json, ArgumentTypes, "argument_types")
SC_SEMA_FIELD_DEF(SpecialMemberFunctionDepr, SMFKind, "smf_kind")
SC_SEMA_FIELD_DEF(SpecialLifetimeFunctionDepr, SLFKind, "slf_kind")
SC_SEMA_FIELD_DEF(FunctionKind, FunctionKind, "function_kind")
SC_SEMA_FIELD_DEF(size_t, Size, "size")
SC_SEMA_FIELD_DEF(size_t, Align, "align")
SC_SEMA_FIELD_DEF(bool, DefaultConstructible, "default_constructible")
SC_SEMA_FIELD_DEF(bool, TrivialLifetime, "trivial_lifetime")
SC_SEMA_FIELD_DEF(std::string, Type, "type")
SC_SEMA_FIELD_DEF(bool, Mutable, "mutable")
SC_SEMA_FIELD_DEF(size_t, Index, "index")
SC_SEMA_FIELD_DEF(AccessControl, AccessControl, "access_control")

#undef SC_SEMA_FIELD_DEF
