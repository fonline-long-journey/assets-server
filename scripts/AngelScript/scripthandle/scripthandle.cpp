#include "scripthandle.h"
#include <assert.h>
#include <fstream>
#include <sstream>
#include <iostream>

#include "../scriptarray.h"
#include "../scriptdictionary.h"
BEGIN_AS_NAMESPACE

int ArrayTypeId = 0;
int StringTypeId = 0;
int DictionaryTypeId = 0;

static void Construct(ScriptHandle *self) { new(self) ScriptHandle(); }
static void Construct(ScriptHandle *self, const ScriptHandle &o) { new(self) ScriptHandle(o); }
// This one is not static because it needs to be friend with the ScriptHandle class
void Construct(ScriptHandle *self, void *ref, int typeId) { new(self) ScriptHandle(ref, typeId); }
static void Destruct(ScriptHandle *self) { self->~ScriptHandle(); }

ScriptHandle::ScriptHandle()
{
	m_ref  = 0;
	m_type = 0;
	m_typeId = 0;
	RefCount = 1;
}

ScriptHandle::ScriptHandle(const ScriptHandle &other)
{
	m_ref  = other.m_ref;
	m_type = other.m_type;
	m_typeId = other.m_typeId;
	RefCount = 1;
	
	AddRefHandle();
}

ScriptHandle::ScriptHandle(void *ref, asIObjectType *type)
{
	m_ref  = ref;
	m_type = type;
	if( type )
		m_typeId = type->GetTypeId();
	else m_typeId = 0;
	RefCount = 1;
	AddRefHandle();
}

// This constructor shouldn't be called from the application 
// directly as it requires an active script context
ScriptHandle::ScriptHandle(void *ref, int typeId)
{
	m_ref  = 0;
	m_type = 0;
	m_typeId = 0;
	Assign(ref, typeId);
}

ScriptHandle::~ScriptHandle()
{
	ReleaseHandle();
}

void ScriptHandle::ReleaseHandle()
{
	if( m_ref && m_type )
	{
		ASEngine->ReleaseScriptObject(m_ref, m_type);

		m_ref  = 0;
		m_type = 0;
		m_typeId = 0;
	}
}

void ScriptHandle::AddRefHandle()
{
	if( m_ref && m_type )
		ASEngine->AddRefScriptObject(m_ref, m_type);
}

ScriptHandle &ScriptHandle::operator =(const ScriptHandle &other)
{
	// Don't do anything if it is the same reference
	if( m_ref == other.m_ref )
		return *this;

	ReleaseHandle();

	m_ref  = other.m_ref;
	m_type = other.m_type;

	AddRefHandle();

	return *this;
}

void ScriptHandle::Set(void *ref, asIObjectType *type, int typeId )
{
	if( m_ref == ref ) return;
	
	ReleaseHandle();

	m_ref  = ref;
	m_type = type;
	m_typeId = typeId;

	AddRefHandle();
}

asIObjectType *ScriptHandle::GetType()
{
	return m_type;
}

// This method shouldn't be called from the application 
// directly as it requires an active script context
ScriptHandle &ScriptHandle::Assign(void *ref, int typeId)
{
	// When receiving a null handle we just clear our memory
	if( typeId == 0 )
	{
		Set(0, 0, typeId);
		return *this;
	}

	// Dereference received handles to get the object
	if( typeId & asTYPEID_OBJHANDLE )
	{
		// Store the actual reference
		ref = *(void**)ref;
		typeId &= ~asTYPEID_OBJHANDLE;
		Set(ref, ASEngine->GetObjectTypeById(typeId), typeId);
	}
	else if( typeId == m_typeId && !(typeId & asTYPEID_MASK_OBJECT) )
	{
		int size = ASEngine->GetSizeOfPrimitiveType(typeId);
		memcpy( m_ref, ref, size);
	}
	/*
	refTypeId & asTYPEID_MASK_OBJECT
			int size = engine->GetSizeOfPrimitiveType(refTypeId);
			memcpy(ref, &value.valueInt, size);
	*/

	return *this;
}

bool ScriptHandle::operator==(const ScriptHandle &o) const
{
	if( m_ref  == o.m_ref &&
		m_type == o.m_type )
		return true;

	// TODO: If type is not the same, we should attempt to do a dynamic cast,
	//       which may change the pointer for application registered classes

	return false;
}

bool ScriptHandle::operator!=(const ScriptHandle &o) const
{
	return !(*this == o);
}

bool ScriptHandle::Equals(void *ref, int typeId) const
{
	return ( ref == m_ref && typeId == m_typeId );
}

// AngelScript: used as '@obj = cast<obj>(ref);'
void ScriptHandle::Cast(void **outRef, int typeId)
{
	// If we hold a null handle, then just return null
	if( m_type == 0 )
	{
		*outRef = 0;
		return;
	}
	
	// It is expected that the outRef is always a handle
	assert( typeId & asTYPEID_OBJHANDLE );

	// Compare the type id of the actual object
	typeId &= ~asTYPEID_OBJHANDLE;
	asIObjectType    *type   = ASEngine->GetObjectTypeById(typeId);

	*outRef = 0;

	if( type == m_type )
	{
		// Must increase the ref count as we're returning a new reference to the object
		AddRefHandle();
		*outRef = m_ref;
	}
	else if( m_type->GetFlags() & asOBJ_SCRIPT_OBJECT )
	{
		// Attempt a dynamic cast of the stored handle to the requested handle type
		if( ASEngine->IsHandleCompatibleWithObject(m_ref, m_type->GetTypeId(), typeId) )
		{
			// The script type is compatible so we can simply return the same pointer
			AddRefHandle();
			*outRef = m_ref;
		}
	}
	else
	{
		// TODO: Check for the existance of a reference cast behaviour. 
		//       Both implicit and explicit casts may be used
		//       Calling the reference cast behaviour may change the actual 
		//       pointer so the AddRef must be called on the new pointer
	}
}

ScriptString* ScriptHandle::script_TypeName()
{
	if( m_ref )
	{
		if( m_type )
			return &ScriptString::Create( m_type->GetName( ) );
		else if( m_typeId )
		{
			switch( m_typeId )
			{
				case asTYPEID_VOID           : // 0,
					break;
				case asTYPEID_BOOL           : // 1,
					return &ScriptString::Create( "bool" );
				case asTYPEID_INT8           : // 2,
					return &ScriptString::Create( "int8" );
				case asTYPEID_INT16          : // 3,
					return &ScriptString::Create( "int16" );
				case asTYPEID_INT32          : // 4,
					return &ScriptString::Create( "int" );
				case asTYPEID_INT64          : // 5,
					return &ScriptString::Create( "int64" );
				case asTYPEID_UINT8          : // 6,
					return &ScriptString::Create( "uint8" );
				case asTYPEID_UINT16         : // 7,
					return &ScriptString::Create( "uint16" );
				case asTYPEID_UINT32         : // 8,
					return &ScriptString::Create( "uint" );
				case asTYPEID_UINT64         : // 9,
					return &ScriptString::Create( "uint64" );
				case asTYPEID_FLOAT          : // 10,
					return &ScriptString::Create( "float" );
				case asTYPEID_DOUBLE         : // 11,
					return &ScriptString::Create( "double" );
				default: break;
			}
		}
	}
	return &ScriptString::Create( "null" );
}

int ScriptHandle::script_TypeId()
{
	if( m_ref && m_type )
		return m_type->GetTypeId();
	else return -1;
}

ScriptString* ScriptHandle::script_TypeNamespace()
{
	if( m_ref && m_type )
		return &ScriptString::Create( m_type->GetNamespace( ) );
	else return &ScriptString::Create( "" );
}

ScriptHandle* CreateScriptHandle( ScriptString* typeName, ScriptString* moduleName )
{
	if( !typeName )
		return NULL;
	
	{
		int typeId = 0;
		if( moduleName )
		{
			asIScriptModule *module = ASEngine->GetModule( moduleName->c_str() );
			if( module )
				typeId = module->GetTypeIdByDecl( typeName->c_str() );
		}
		else
			typeId = ASEngine->GetTypeIdByDecl( typeName->c_str() );
		switch( typeId )
		{
			case asTYPEID_BOOL           : // 1,
			{
				bool value = false;
				ScriptHandle* ret = new ScriptHandle( &value, typeId );
				return ret;
			}
			case asTYPEID_INT8           : // 2,
			{
				int8 value = 0;
				return new ScriptHandle( &value, typeId );
			}
			case asTYPEID_INT16          : // 3,
			{
				int16 value = 0;
				return new ScriptHandle( &value, typeId );
			}
			case asTYPEID_INT32          : // 4,
			{
				int value = 0;
				return new ScriptHandle( &value, typeId );
			}
			case asTYPEID_INT64          : // 5,
			{
				int64 value = 0;
				return new ScriptHandle( &value, typeId );
			}
			case asTYPEID_UINT8          : // 6,
			{
				uint8 value = 0;
				return new ScriptHandle( &value, typeId );
			}
			case asTYPEID_UINT16         : // 7,
			{
				uint16 value = 0;
				return new ScriptHandle( &value, typeId );
			}
			case asTYPEID_UINT32         : // 8,
			{
				uint value = 0;
				return new ScriptHandle( &value, typeId );
			}
			case asTYPEID_UINT64         : // 9,
			{
				uint64 value = 0;
				return new ScriptHandle( &value, typeId );
			}
			case asTYPEID_FLOAT          : // 10,
			{
				float value = 0.0f;
				return new ScriptHandle( &value, typeId );
			}
			case asTYPEID_DOUBLE         : // 11,
			{
				double value = 0.0;
				return new ScriptHandle( &value, typeId );
			}
			default:
			{
				asIObjectType *type = ASEngine->GetObjectTypeById( typeId );
				if( type )
				{
					string factoryName = typeName->c_str();
					factoryName += " @";
					factoryName += typeName->c_str();
					factoryName += "()";
					
					asIScriptObject *obj = reinterpret_cast<asIScriptObject*>(ASEngine->CreateScriptObject(typeId));
					if( obj )
					{
						ScriptHandle* ref = new ScriptHandle( );
						if( ref )
							ref->Assign( obj, typeId );
						obj->Release();
						return ref;
					}
				}
			}
			break;
		}
	}
	return NULL;
}

void ScriptHandle::AddRef(){ RefCount++; }
void ScriptHandle::Release(){ if( --RefCount == 0 ) Destruct( this ); }

ScriptHandle* ScriptHandle::script_Property( ScriptString* propName )
{
	if( m_ref && m_type && propName )
	{
		asIScriptObject *object = reinterpret_cast<asIScriptObject*>(m_ref);
		if( object )
			for( uint i = 0, iEnd = object->GetPropertyCount(); i < iEnd; i++ )
				if( strcmp( object->GetPropertyName(i), propName->c_str() ) == 0 )
					return FormatCScriptHandle( object->GetAddressOfProperty(i), object->GetPropertyTypeId(i) );
			/*
			GetPropertyCount() const = 0;
			virtual int         GetPropertyTypeId(asUINT prop) const = 0;
			virtual const char *GetPropertyName(asUINT prop) const = 0;
			virtual void       *GetAddressOfProperty(asUINT prop) = 0; */
	}
	return NULL;
}

bool ScriptHandle::script_Primitive( )
{
	return ( m_typeId > 0 && !(m_typeId & asTYPEID_MASK_OBJECT) );
}

bool ScriptHandle::Get( void *ref, int typeId )
{
	if( typeId == m_typeId && !(typeId & asTYPEID_MASK_OBJECT) )
	{
		int size = ASEngine->GetSizeOfPrimitiveType(typeId);
		memcpy( ref, m_ref, size);
		return true;
	}
	return false;
}

bool ScriptHandle::GetBool( bool& value )
{
	if( m_typeId == asTYPEID_BOOL )
		return Get( &value, asTYPEID_BOOL );
	return false;
}

bool ScriptHandle::GetUint64( uint64& value )
{
	if( m_typeId == asTYPEID_UINT64 )
		return Get( &value, asTYPEID_UINT64 );
	return false;
}

bool ScriptHandle::GetUint( uint& value )
{
	if( m_typeId == asTYPEID_UINT32 )
		return Get( &value, asTYPEID_UINT32 );	
	return false;
}

bool ScriptHandle::GetUint16( uint16& value )
{
	if( m_typeId == asTYPEID_UINT16 )
		return Get( &value, asTYPEID_UINT16 );	
	return false;
}

bool ScriptHandle::GetUint8( uint8& value )
{
	if( m_typeId == asTYPEID_UINT8 )
		return Get( &value, asTYPEID_UINT8 );	
	return false;
}

bool ScriptHandle::GetInt64( int64& value )
{
	if( m_typeId == asTYPEID_INT64 )
		return Get( &value, asTYPEID_INT64 );	
	return false;
}

bool ScriptHandle::GetInt( int& value )
{
	if( m_typeId == asTYPEID_INT32 )
		return Get( &value, asTYPEID_INT32 );	
	return false;
}

bool ScriptHandle::GetInt16( int16& value )
{
	if( m_typeId == asTYPEID_INT16 )
		return Get( &value, asTYPEID_INT16 );	
	return false;
}

bool ScriptHandle::GetInt8( int8& value )
{
	if( m_typeId == asTYPEID_INT8 )
		return Get( &value, asTYPEID_INT8 );	
	return false;
}

bool ScriptHandle::GetFloat( float& value )
{
	if( m_typeId == asTYPEID_FLOAT )
		return Get( &value, asTYPEID_FLOAT );	
	return false;
}

bool ScriptHandle::GetDouble( double& value )
{
	if( m_typeId == asTYPEID_DOUBLE )
		return Get( &value, asTYPEID_DOUBLE );	
	return false;
}


ScriptHandle* CreateCScriptHandle() 						{ return new ScriptHandle(); }
ScriptHandle* CopyScriptHandle(const ScriptHandle &o) 		{ return new ScriptHandle(o); }
ScriptHandle* FormatCScriptHandle(void *ref, int typeId) 	{ return new ScriptHandle(ref, typeId); }

class ScriptStream
{
public:

	struct DumpReference
	{
		int typeId;
		void* ref;
		string name;
		int count;
	};
	
	map<int, DumpReference*> References;

	ScriptStream( const string& _path )
	{
		stream.open( _path.c_str() );
		prefix = "";
		fullname = "";
	}
	
	~ScriptStream( )
	{
		stream.close( );
		for( auto it = References.begin(); it != References.end(); it++ )
		{
			delete it->second;
			it->second = nullptr;
		}
		References.clear();
	}
	
	void write( void *ref, int typeId );
	
	void write_full( void *ref, int typeId, const string& name )
	{
		// fullname
		stream << prefix.c_str() << name.c_str() <<  "\n" << prefix.c_str() << "{\n" ;
		string old_prefix = prefix;
		string old_fullname = fullname;
		if( fullname != "" )
			fullname += "." + name;
		else fullname = name;
		prefix += "\t";
		
		if( typeId & asTYPEID_OBJHANDLE )
		{
			typeId &= ~asTYPEID_OBJHANDLE;
			ref = *(void**)ref;
			
			if( References[(int)ref] == nullptr )
			{
				DumpReference* dump = new DumpReference;
				dump->typeId = typeId;
				dump->ref = ref;
				dump->name = fullname;
				dump->count = 0;
				References[(int)ref] = dump;
				write( ref, typeId );
			}
			else stream << prefix.c_str() << References[(int)ref]->name.c_str() <<  "\n";
		}
		else write( ref, typeId );
		
		prefix = old_prefix;
		fullname = old_fullname;
		stream << prefix.c_str() << "}\n\n" ;
	}
	
	void begin_write( void *ref, int typeId, const string& name )
	{
		write_full( ref, typeId, name );
	}
	
	string prefix;
	string fullname;
	ofstream stream;
};

void ScriptStream::write( void *ref, int typeId )
{
	if( typeId == 0 || ref == 0 ) stream << prefix.c_str() << "nullptr\n";
	else
	{
		switch( typeId )
		{
			case asTYPEID_BOOL:
			{
				bool value;
				int size = ASEngine->GetSizeOfPrimitiveType(typeId);
				memcpy( &value, ref, size);
				stream << prefix.c_str() << ( value ? "true" : "false" ) << "\n";
			} break;
			case asTYPEID_UINT64:
			{
				uint64 value;
				int size = ASEngine->GetSizeOfPrimitiveType(typeId);
				memcpy( &value, ref, size);
				stream << prefix.c_str() << value << "\n";
			} break;
			case asTYPEID_UINT32:
			{
				uint value;
				int size = ASEngine->GetSizeOfPrimitiveType(typeId);
				memcpy( &value, ref, size);
				stream << prefix.c_str() << value << "\n";
			} break;
			case asTYPEID_UINT16:
			{
				uint16 value;
				int size = ASEngine->GetSizeOfPrimitiveType(typeId);
				memcpy( &value, ref, size);
				stream << prefix.c_str() << value << "\n";
			} break;
			case asTYPEID_UINT8:
			{
				uint8 value;
				int size = ASEngine->GetSizeOfPrimitiveType(typeId);
				memcpy( &value, ref, size);
				stream << prefix.c_str() << value << "\n";
			} break;
			case asTYPEID_INT64:
			{
				int64 value;
				int size = ASEngine->GetSizeOfPrimitiveType(typeId);
				memcpy( &value, ref, size);
				stream << prefix.c_str() << value << "\n";
			} break;
			case asTYPEID_INT32:
			{
				int value;
				int size = ASEngine->GetSizeOfPrimitiveType(typeId);
				memcpy( &value, ref, size);
				stream << prefix.c_str() << value << "\n";
			} break;
			case asTYPEID_INT16:
			{
				int16 value;
				int size = ASEngine->GetSizeOfPrimitiveType(typeId);
				memcpy( &value, ref, size);
				stream << prefix.c_str() << value << "\n";
			} break;
			case asTYPEID_INT8:
			{
				int8 value;
				int size = ASEngine->GetSizeOfPrimitiveType(typeId);
				memcpy( &value, ref, size);
				stream << prefix.c_str() << value << "\n";
			} break;
			case asTYPEID_FLOAT:
			{
				float value;
				int size = ASEngine->GetSizeOfPrimitiveType(typeId);
				memcpy( &value, ref, size);
				stream << prefix.c_str() << value << "\n";
			} break;
			case asTYPEID_DOUBLE:
			{
				double value;
				int size = ASEngine->GetSizeOfPrimitiveType(typeId);
				memcpy( &value, ref, size);
				stream << prefix.c_str() << value << "\n";
			} break;
			default: 
			{
				if( typeId & asTYPEID_SCRIPTOBJECT )
				{
					asIObjectType *type = ASEngine->GetObjectTypeById(typeId);
					if( type )
					{
						asIScriptObject *object = (asIScriptObject*)ref;
						if( object )
						{
							asUINT countProp = type->GetPropertyCount();
							for( asUINT i = 0; i < countProp; i++ )
								write_full( object->GetAddressOfProperty(i), object->GetPropertyTypeId(i), string(object->GetPropertyName(i)) );
						}
						else stream << prefix.c_str() << "//error object typeId " << typeId << " " << ASEngine->GetTypeDeclaration( typeId ) << "\n"; 
					}
					else stream << prefix.c_str() << "//error type typeId " << typeId << " " << ASEngine->GetTypeDeclaration( typeId ) << "\n"; 
				}
				else if( typeId == StringTypeId )
				{
					ScriptString* str = (ScriptString*)ref;
					if( str )
						stream << prefix.c_str() << "\"" << str->c_str() << "\"\n";
				}
				else if( typeId == DictionaryTypeId )
				{
					ScriptDictionary* dictionary = (ScriptDictionary*)ref;
					if( dictionary )
					{
						ScriptArray& arr = ScriptArray::Create( "string@" );
						auto count_keys = dictionary->Keys( &arr );
						stream << prefix.c_str();
						for( uint i = 0; i < count_keys; i++ )
						{
							ScriptString* key = *(ScriptString**) arr.At( i );
							//int keytypeId = dictionary->GetTypeId( *key );
							//string name = ASEngine->GetTypeDeclaration( keytypeId );
							//name += " ";
							//name += key.c_str();
							//void* value = NULL;
							//dictionary->Get( key, value, keytypeId );
							//write_full( value, keytypeId, name );
							stream << key->c_str() << " ";
						}
						stream << "\n";
						arr.Release();
					}
				}
				else 
				{
					string declaration = ASEngine->GetTypeDeclaration( typeId );
					if( declaration.find("[]") != string::npos ) // array
					{
						ScriptArray* arr = (ScriptArray*)ref;
						if( arr )
						{
							int elementTypeId = arr->GetElementTypeId(); 
							stringstream iname;
							for( asUINT i = 0, iEnd = arr->GetSize(); i < iEnd; i++ )
							{
								iname.str("element");
								iname << i;
								write_full( arr->At(i), elementTypeId, iname.str() );
							}
						}
					}
					else
					{
						stream << prefix.c_str() << "//no parse object< " << typeId << " " << declaration << " flags: ";
						asIObjectType *type = ASEngine->GetObjectTypeById(typeId);
						if( type )
						{
							asDWORD flags = type->GetFlags();
							if( flags & asOBJ_REF )
								stream << "ref ";
							if( flags & asOBJ_VALUE )
								stream << "value ";
							if( flags & asOBJ_GC )
								stream << "gc ";
							if( flags & asOBJ_POD )
								stream << "pod ";
							if( flags & asOBJ_NOHANDLE )
								stream << "nohandle ";
							if( flags & asOBJ_SCOPED )
								stream << "scoped ";
							if( flags & asOBJ_TEMPLATE )
								stream << "template ";
							if( flags & asOBJ_ASHANDLE )
								stream << "ashandle ";
							if( flags & asOBJ_APP_CLASS )
								stream << "app_class ";
							if( flags & asOBJ_APP_CLASS_CONSTRUCTOR )
								stream << "app_class_constructor ";
							if( flags & asOBJ_APP_CLASS_DESTRUCTOR )
								stream << "app_class_destructor ";
							if( flags & asOBJ_APP_CLASS_ASSIGNMENT )
								stream << "app_class_assignment ";
							if( flags & asOBJ_APP_CLASS_COPY_CONSTRUCTOR )
								stream << "app_class_copy_constructor ";
							if( flags & asOBJ_APP_PRIMITIVE )
								stream << "app_primitive ";
							if( flags & asOBJ_APP_FLOAT )
								stream << "app_float ";
							if( flags & asOBJ_APP_CLASS_ALLINTS )
								stream << "app_class_allints ";
							if( flags & asOBJ_APP_CLASS_ALLFLOATS )
								stream << "app_class_allfloat ";
							if( flags & asOBJ_NOCOUNT )
								stream << "nocount ";
							if( flags & asOBJ_MASK_VALID_FLAGS )
								stream << "mask_valid ";
							if( flags & asOBJ_SCRIPT_OBJECT )
								stream << "script_object ";
							if( flags & asOBJ_SHARED )
								stream << "shared ";
							if( flags & asOBJ_NOINHERIT )
								stream << "noinherit ";
						}
						stream << "> ";
						if( typeId & asTYPEID_OBJHANDLE )
							stream << "objhandle ";
						if( typeId & asTYPEID_HANDLETOCONST )
							stream << "handletoconst ";
						if( typeId & asTYPEID_MASK_OBJECT )
							stream << "mask_object ";
						if( typeId & asTYPEID_APPOBJECT )
							stream << "appobject ";
						if( typeId & asTYPEID_SCRIPTOBJECT )
							stream << "scriptobject ";
						if( typeId & asTYPEID_TEMPLATE )
							stream << "template ";
						if( typeId & asTYPEID_MASK_SEQNBR )
							stream << "mask_seqnbr ";
						stream << "\n";
					}
				}
			}	break;
		}
	}
}

void SaveToFile( void *ref, int typeId, const ScriptString* fileName, const ScriptString* objectName )
{
	ScriptStream( fileName->c_std_str()).begin_write( ref, typeId, objectName->c_std_str() );
}

void RegisterScriptHandle()
{
	int r;
	ArrayTypeId = ASEngine->GetDefaultArrayTypeId();
	StringTypeId = ASEngine->GetTypeIdByDecl( "string" );
	DictionaryTypeId = ASEngine->GetTypeIdByDecl( "dictionary" );
	
	r = ASEngine->RegisterObjectType( "Handle", sizeof( ScriptHandle ), asOBJ_REF ); assert( r >= 0 );
	r = ASEngine->RegisterObjectBehaviour( "Handle", asBEHAVE_ADDREF, "void f()", asMETHOD( ScriptHandle, AddRef ), asCALL_THISCALL ); assert( r >= 0 );
    r = ASEngine->RegisterObjectBehaviour( "Handle", asBEHAVE_RELEASE, "void f()", asMETHOD( ScriptHandle, Release ), asCALL_THISCALL ); assert( r >= 0 );
	r = ASEngine->RegisterObjectBehaviour( "Handle", asBEHAVE_REF_CAST, "void f(?&out)", asMETHODPR(ScriptHandle, Cast, (void **, int), void), asCALL_THISCALL); assert( r >= 0 );
	
	r = ASEngine->RegisterObjectMethod( "Handle", "Handle &opAssign(const Handle &in)", asMETHOD(ScriptHandle, operator=), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "Handle &opAssign(const ?&in)", asMETHOD(ScriptHandle, Assign), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool opEquals(const Handle &in) const", asMETHODPR(ScriptHandle, operator==, (const ScriptHandle &) const, bool), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool opEquals(const ?&in) const", asMETHODPR(ScriptHandle, Equals, (void*, int) const, bool), asCALL_THISCALL); assert( r >= 0 );
	
	// Type information
	r = ASEngine->RegisterObjectMethod( "Handle", "string@ get_TypeName( ) const", asMETHOD(ScriptHandle, script_TypeName), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "int get_TypeId( ) const", asMETHOD(ScriptHandle, script_TypeId), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "string@ get_TypeNamespace( ) const", asMETHOD(ScriptHandle, script_TypeNamespace), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "Handle@+ get_Property( string@ name ) const", asMETHOD(ScriptHandle, script_Property), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool Get( bool&out value ) const", asMETHOD(ScriptHandle, GetBool), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool Get( uint8&out value ) const", asMETHOD(ScriptHandle, GetUint8), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool Get( uint16&out value ) const", asMETHOD(ScriptHandle, GetUint16), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool Get( uint&out value ) const", asMETHOD(ScriptHandle, GetUint), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool Get( uint64&out value ) const", asMETHOD(ScriptHandle, GetUint64), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool Get( int64&out value ) const", asMETHOD(ScriptHandle, GetInt64), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool Get( int&out value ) const", asMETHOD(ScriptHandle, GetInt), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool Get( int16&out value ) const", asMETHOD(ScriptHandle, GetInt16), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool Get( int8&out value ) const", asMETHOD(ScriptHandle, GetInt8), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool Get( float&out value ) const", asMETHOD(ScriptHandle, GetFloat), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool Get( double&out value ) const", asMETHOD(ScriptHandle, GetDouble), asCALL_THISCALL); assert( r >= 0 );
	r = ASEngine->RegisterObjectMethod( "Handle", "bool get_IsPrimitive( ) const", asMETHOD(ScriptHandle, script_Primitive), asCALL_THISCALL); assert( r >= 0 );
	
    ASEngine->SetDefaultNamespace( "Handle" );
	r = ASEngine->RegisterGlobalFunction( "::Handle@+ Create( )", asFUNCTION( CreateCScriptHandle ), asCALL_CDECL ); assert( r >= 0 );
	r = ASEngine->RegisterGlobalFunction( "::Handle@+ Create( const ::Handle &in )", asFUNCTION( CopyScriptHandle ), asCALL_CDECL ); assert( r >= 0 );
	r = ASEngine->RegisterGlobalFunction( "::Handle@+ Create( const ?&in )", asFUNCTION( FormatCScriptHandle ), asCALL_CDECL ); assert( r >= 0 );
	r = ASEngine->RegisterGlobalFunction( "::Handle@+ Create( ::string@ type, ::string@ module )", asFUNCTION( CreateScriptHandle ), asCALL_CDECL ); assert( r >= 0 );
    ASEngine->SetDefaultNamespace( "" );
	r = ASEngine->RegisterGlobalFunction( "void Dump( ?& object, const string@ path, const string@ name )", asFUNCTION(SaveToFile), asCALL_CDECL); assert( r >= 0 );
}

END_AS_NAMESPACE
