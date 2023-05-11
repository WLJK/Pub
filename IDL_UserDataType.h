// Don't modify this file as it will be overwritten.
//
#include "CDR/CDR.h"
#include "ReturnCode.h"
#include "BasicTypes.h"

#ifndef IDL_UserDataType_hh
#define IDL_UserDataType_hh

#ifndef UserDataType_defined
#define UserDataType_defined

struct UserDataType {
  UserDataType()
	{
		a = new char[255];
		a[0]= '\0';
		MD5 = new char[255];
		MD5[0]= '\0';
	}

  UserDataType(const UserDataType  &IDL_s);

  ~UserDataType(){
		delete a;
		a = NULL;
		delete MD5;
		MD5 = NULL;
	}

  	int StructSize()
	{
		int strSize = 0;
		strSize += sizeof(UserDataType);
		strSize += strlen(a);
		strSize += strlen(MD5);
		return strSize;
	}
  UserDataType& operator= (const UserDataType &IDL_s);

  void Marshal(CDR *cdr) const;
  void UnMarshal(CDR *cdr);

  char* a;
char* MD5;
  
};

typedef sequence<UserDataType> UserDataTypeSeq;

#endif

#ifndef demo_defined
#define demo_defined

struct demo {
  demo()
	{
		a = new char[255];
		a[0]= '\0';
		MD5 = new char[255];
		MD5[0]= '\0';
	}

  demo(const demo  &IDL_s);

  ~demo(){
		delete a;
		a = NULL;
		delete MD5;
		MD5 = NULL;
	}

  	int StructSize()
	{
		int strSize = 0;
		strSize += sizeof(demo);
		strSize += strlen(a);
		strSize += strlen(MD5);
		return strSize;
	}
  demo& operator= (const demo &IDL_s);

  void Marshal(CDR *cdr) const;
  void UnMarshal(CDR *cdr);

  char* a;
char* MD5;
  
};

typedef sequence<demo> demoSeq;

#endif




#endif /*IDL_UserDataType_hh */
