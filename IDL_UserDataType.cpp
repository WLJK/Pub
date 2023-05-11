// Don't modify this file as it will be overwritten.
//
#include "IDL_UserDataType.h"

UserDataType::UserDataType(const UserDataType &IDL_s){
  a = IDL_s.a;
  MD5 = IDL_s.MD5;
}

UserDataType& UserDataType::operator= (const UserDataType &IDL_s){
  if (this == &IDL_s) return *this;
  a = IDL_s.a;
  MD5 = IDL_s.MD5;
  return *this;
}

void UserDataType::Marshal(CDR *cdr) const {
  cdr->PutString(a);
  cdr->PutString(MD5);
}

void UserDataType::UnMarshal(CDR *cdr){
  {
    char *IDL_str;
    cdr->GetString(IDL_str);
    if(a != NULL )
    {
        delete a;
        a = NULL;
    }
    a = IDL_str;
  }
  {
    char *IDL_str;
    cdr->GetString(IDL_str);
    if(MD5 != NULL )
    {
        delete MD5;
        MD5 = NULL;
    }
    MD5 = IDL_str;
  }
}

demo::demo(const demo &IDL_s){
  a = IDL_s.a;
  MD5 = IDL_s.MD5;
}

demo& demo::operator= (const demo &IDL_s){
  if (this == &IDL_s) return *this;
  a = IDL_s.a;
  MD5 = IDL_s.MD5;
  return *this;
}

void demo::Marshal(CDR *cdr) const {
  cdr->PutString(a);
  cdr->PutString(MD5);
}

void demo::UnMarshal(CDR *cdr){
  {
    char *IDL_str;
    cdr->GetString(IDL_str);
    if(a != NULL )
    {
        delete a;
        a = NULL;
    }
    a = IDL_str;
  }
  {
    char *IDL_str;
    cdr->GetString(IDL_str);
    if(MD5 != NULL )
    {
        delete MD5;
        MD5 = NULL;
    }
    MD5 = IDL_str;
  }
}

