#ifndef SCRIPTHANDLE_H
#define SCRIPTHANDLE_H

#ifndef ANGELSCRIPT_H 
// Avoid having to inform include path if header is already include before
#include "../angelscript.h"
#endif


BEGIN_AS_NAMESPACE

class ScriptHandle 
{
public:
	// Constructors
	ScriptHandle();
	ScriptHandle(const ScriptHandle &other);
	ScriptHandle(void *ref, asIObjectType *type);
	~ScriptHandle();

	// Copy the stored value from another any object
	ScriptHandle &operator=(const ScriptHandle &other);

	// Set the reference
	void Set(void *ref, asIObjectType *type, int typeId);
	
	bool Get( void *ref, int typeId );
	bool GetBool( bool& value );
	bool GetUint64( uint64& value );
	bool GetUint( uint& value );
	bool GetUint16( uint16& value );
	bool GetUint8( uint8& value );
	bool GetInt64( int64& value );
	bool GetInt( int& value );
	bool GetInt16( int16& value );
	bool GetInt8( int8& value );
	bool GetFloat( float& value );
	bool GetDouble( double& value );
	
	bool script_Primitive();

	// Compare equalness
	bool operator==(const ScriptHandle &o) const;
	bool operator!=(const ScriptHandle &o) const;
	bool Equals(void *ref, int typeId) const;

	// Dynamic cast to desired handle type
	void Cast(void **outRef, int typeId);

	// Returns the type of the reference held
	asIObjectType *GetType();
	
	ScriptString* script_TypeName();
	ScriptString* script_TypeNamespace();
	ScriptHandle* script_Property( ScriptString* propName );
	
	void AddRef();
	void Release();
protected:
	// These functions need to have access to protected
	// members in order to call them from the script engine
	friend void Construct(ScriptHandle *self, void *ref, int typeId);
	friend void RegisterScriptHandle( );
	friend ScriptHandle* CreateScriptHandle( ScriptString* moduleName, ScriptString* typeName );
	friend class ScriptHandleSaver;
	
	friend ScriptHandle* FormatCScriptHandle(void *ref, int typeId);

	void ReleaseHandle();
	void AddRefHandle();

	// These shouldn't be called directly by the 
	// application as they requires an active context
	ScriptHandle(void *ref, int typeId);
	ScriptHandle &Assign(void *ref, int typeId);
	ScriptHandle &HandleAssign(void *ref, int typeId);
	int script_TypeId();

	void          *m_ref;
	asIObjectType *m_type;
	int			   m_typeId;
	
	int 		   RefCount;
};

void RegisterScriptHandle( );

END_AS_NAMESPACE

#endif
