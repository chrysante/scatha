// NO-INCLUDE-GUARD

// clang-format off

// ===--------------------------------------------------------------------=== //
// === List of all entity types ------------------------------------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SC_SEMA_ENTITY_DEF
#   define SC_SEMA_ENTITY_DEF(...)
#endif

SC_SEMA_ENTITY_DEF(Entity,                 VoidParent,     Abstract)
SC_SEMA_ENTITY_DEF(Scope,                  Entity,         Abstract)
SC_SEMA_ENTITY_DEF(GlobalScope,            Scope,          Concrete)
SC_SEMA_ENTITY_DEF(FileScope,              Scope,          Concrete)
SC_SEMA_ENTITY_DEF(Library,                Scope,          Concrete)
SC_SEMA_ENTITY_DEF(NativeLibrary,          Library,        Concrete)
SC_SEMA_ENTITY_DEF(ForeignLibrary,         Library,        Concrete)
SC_SEMA_ENTITY_DEF(Function,               Scope,          Concrete)
SC_SEMA_ENTITY_DEF(Type,                   Scope,          Abstract)
SC_SEMA_ENTITY_DEF(FunctionType,           Type,           Concrete)
SC_SEMA_ENTITY_DEF(ObjectType,             Type,           Abstract)
SC_SEMA_ENTITY_DEF(BuiltinType,            ObjectType,     Abstract)
SC_SEMA_ENTITY_DEF(VoidType,               BuiltinType,    Concrete)
SC_SEMA_ENTITY_DEF(ArithmeticType,         BuiltinType,    Abstract)
SC_SEMA_ENTITY_DEF(BoolType,               ArithmeticType, Concrete)
SC_SEMA_ENTITY_DEF(ByteType,               ArithmeticType, Concrete)
SC_SEMA_ENTITY_DEF(IntType,                ArithmeticType, Concrete)
SC_SEMA_ENTITY_DEF(FloatType,              ArithmeticType, Concrete)
SC_SEMA_ENTITY_DEF(NullPtrType,            BuiltinType,    Concrete)
SC_SEMA_ENTITY_DEF(PointerType,            BuiltinType,    Abstract)
SC_SEMA_ENTITY_DEF(RawPtrType,             PointerType,    Concrete)
SC_SEMA_ENTITY_DEF(UniquePtrType,          PointerType,    Concrete)
SC_SEMA_ENTITY_DEF(ReferenceType,          Type,           Concrete)
SC_SEMA_ENTITY_DEF(CompoundType,           ObjectType,     Abstract)
SC_SEMA_ENTITY_DEF(RecordType,             CompoundType,   Abstract)
SC_SEMA_ENTITY_DEF(StructType,             RecordType,     Concrete)
SC_SEMA_ENTITY_DEF(ProtocolType,           RecordType,     Concrete)
SC_SEMA_ENTITY_DEF(ArrayType,              CompoundType,   Concrete)
SC_SEMA_ENTITY_DEF(AnonymousScope,         Scope,          Concrete)
SC_SEMA_ENTITY_DEF(OverloadSet,            Entity,         Concrete)
SC_SEMA_ENTITY_DEF(Generic,                Entity,         Concrete)
SC_SEMA_ENTITY_DEF(Object,                 Entity,         Abstract)
SC_SEMA_ENTITY_DEF(VarBase,                Object,         Abstract)
SC_SEMA_ENTITY_DEF(Variable,               VarBase,        Concrete)
SC_SEMA_ENTITY_DEF(Property,               VarBase,        Concrete)
SC_SEMA_ENTITY_DEF(BaseClassObject,        Object,         Concrete)
SC_SEMA_ENTITY_DEF(Temporary,              Object,         Concrete)
SC_SEMA_ENTITY_DEF(Alias,                  Entity,         Concrete)
SC_SEMA_ENTITY_DEF(TypeDeductionQualifier, Entity,         Concrete)
SC_SEMA_ENTITY_DEF(PoisonEntity,           Entity,         Concrete) // Update LAST definition in Fwd.h if this changes

#undef SC_SEMA_ENTITY_DEF

// ===--------------------------------------------------------------------=== //
// === List of property objects ------------------------------------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SC_SEMA_PROPERTY_KIND
#   define SC_SEMA_PROPERTY_KIND(Name, SourceName)
#endif

SC_SEMA_PROPERTY_KIND(ArraySize, "count")
SC_SEMA_PROPERTY_KIND(ArrayEmpty, "empty")
SC_SEMA_PROPERTY_KIND(ArrayFront, "front")
SC_SEMA_PROPERTY_KIND(ArrayBack, "back")
SC_SEMA_PROPERTY_KIND(This, "this")

#undef SC_SEMA_PROPERTY_KIND

// ===--------------------------------------------------------------------=== //
// === List of all special member functions ------------------------------=== //
// ===--------------------------------------------------------------------=== //

// Actually special member functions are the default constructor, the copy
// constructor, the move constructor and the destructor. We will fix this later

#ifndef SC_SEMA_SPECIAL_MEMBER_FUNCTION_DEF
#   define SC_SEMA_SPECIAL_MEMBER_FUNCTION_DEF(Name, str)
#endif

SC_SEMA_SPECIAL_MEMBER_FUNCTION_DEF(New,    "new")
SC_SEMA_SPECIAL_MEMBER_FUNCTION_DEF(Move,   "move")
SC_SEMA_SPECIAL_MEMBER_FUNCTION_DEF(Delete, "delete")

#undef SC_SEMA_SPECIAL_MEMBER_FUNCTION_DEF

// ===--------------------------------------------------------------------=== //
// === List of all special lifetime functions ----------------------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SC_SEMA_SPECIAL_LIFETIME_FUNCTION_DEF
#   define SC_SEMA_SPECIAL_LIFETIME_FUNCTION_DEF(Name)
#endif

SC_SEMA_SPECIAL_LIFETIME_FUNCTION_DEF(DefaultConstructor)
SC_SEMA_SPECIAL_LIFETIME_FUNCTION_DEF(CopyConstructor)
SC_SEMA_SPECIAL_LIFETIME_FUNCTION_DEF(MoveConstructor)
SC_SEMA_SPECIAL_LIFETIME_FUNCTION_DEF(Destructor)

#undef SC_SEMA_SPECIAL_LIFETIME_FUNCTION_DEF

// ===--------------------------------------------------------------------=== //
// === List of all special member functions ------------------------------=== //
// ===--------------------------------------------------------------------=== //

// NEW special member functions

#ifndef SC_SEMA_SMF_DEF
#   define SC_SEMA_SMF_DEF(Name, Spelling)
#endif

SC_SEMA_SMF_DEF(DefaultConstructor, "new")
SC_SEMA_SMF_DEF(CopyConstructor,    "new")
SC_SEMA_SMF_DEF(MoveConstructor,    "move")
SC_SEMA_SMF_DEF(Destructor,         "delete")

#undef SC_SEMA_SMF_DEF

// ===--------------------------------------------------------------------=== //
// === List of all access control kinds ----------------------------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SC_SEMA_ACCESS_CONTROL_DEF
#   define SC_SEMA_ACCESS_CONTROL_DEF(Kind, Spelling)
#endif

/// Must be in order of ascending strength (or descending accessibility)!
SC_SEMA_ACCESS_CONTROL_DEF(Public,   "public")
SC_SEMA_ACCESS_CONTROL_DEF(Internal, "internal")
SC_SEMA_ACCESS_CONTROL_DEF(Private,  "private")

#undef SC_SEMA_ACCESS_CONTROL_DEF

// ===--------------------------------------------------------------------=== //
// === List of all constant kinds ----------------------------------------=== //
// ===--------------------------------------------------------------------=== //

#ifndef SC_SEMA_CONSTKIND_DEF
#   define SC_SEMA_CONSTKIND_DEF(...)
#endif

SC_SEMA_CONSTKIND_DEF(Value,        VoidParent, Abstract)
SC_SEMA_CONSTKIND_DEF(IntValue,     Value,      Concrete)
SC_SEMA_CONSTKIND_DEF(FloatValue,   Value,      Concrete)
SC_SEMA_CONSTKIND_DEF(PointerValue, Value,      Concrete)

#undef SC_SEMA_CONSTKIND_DEF
