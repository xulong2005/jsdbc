/**
The MIT License (MIT)

Copyright (c) 2013 Moritz Schulze 

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/
#ifndef _RESULT_H
#define _RESULT_H

#include <vector>
#include <string>
#include <v8.h>

#include "result_set.h"

using namespace v8;

namespace nodezdb {
struct Row {
  std::vector<char*> fieldValues;
  ~Row() {
    for(unsigned int i = 0; i < fieldValues.size(); i++) {
      delete[] fieldValues[i];
    }
  }
};

class Result {
public:
  Result(const char* _errorText) : errorText(_errorText) {};
  Result() {};
  virtual Handle<Value> getResultObject() = 0;
  std::string errorText;
  virtual ~Result() {};
};

class EmptyResult : public Result {
public:
  EmptyResult(const char* errorText) : Result(errorText) {};
  Handle<Value> getResultObject() {
    HandleScope scope;
    return scope.Close(Object::New());
  }
};

class SelectResult : public Result {
public:
  SelectResult() : rows(), fieldNames() {};
  ~SelectResult() {
    for(unsigned int i = 0; i < rows.size(); i++) {
      delete rows[i];
    }
  }
  std::vector<Row*> rows;
  std::vector<std::string> fieldNames;
  Handle<Value> getResultObject() {
    HandleScope scope;
    Handle<Array> array = Array::New();

    for(unsigned int i = 0; i < rows.size(); i++) {
      Handle<Object> obj = Object::New();
      for(unsigned int j = 0; j < rows[i]->fieldValues.size(); j++) {
        obj->Set(String::New(fieldNames[j].c_str()),
          String::New(rows[i]->fieldValues[j]));
      }
      array->Set(Number::New(i), obj);
    }
    return scope.Close(array);
  }
};

class UpdateResult : public Result {
public:
  Handle<Value> getResultObject() {
    HandleScope scope;
    Handle<Object> obj = Object::New();
    obj->Set(String::NewSymbol("affectedRows"), Number::New(affectedRows));
    return scope.Close(obj);
  }
  int affectedRows;
};

class StreamingResult : public Result {
public:
  StreamingResult(ResultSet_T _resultSet_T, Connection_T _connection)
    : resultSet_T(_resultSet_T), connection(_connection) {};
  ~StreamingResult() {};
  Handle<Value> getResultObject() {
    HandleScope scope;
    ResultSet* resultSet = new ResultSet(resultSet_T, connection);
    return scope.Close(resultSet->v8Object());
  };
private:
  ResultSet_T resultSet_T;
  Connection_T connection;
};
}
#endif