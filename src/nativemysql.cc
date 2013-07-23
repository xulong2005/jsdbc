#include <node.h>

#include <mysql.h>
#include <vector>

#include <iostream>
using namespace v8;

MYSQL mysql;

struct Row {
  char** fieldValues;
};

struct QueryBaton {
  QueryBaton(Persistent<Function> _callback, char* _query) :
    request(), callback(_callback), query(_query), rows(0), numRows(0), fieldNames(0), numFields(0),
    errorText(0) {};
  ~QueryBaton() {
    if(rows != 0) {
      for(int i = 0; i < numRows; i++) {
        for(int j = 0; j < numFields; j++) {
          delete[] rows[i]->fieldValues[j];
        }
        delete[] rows[i]->fieldValues;
        delete rows[i];
      }
      delete[] rows;
    }
    if(fieldNames != 0) {
      for(int i = 0; i < numFields; i++) {
        delete[] fieldNames[i];
      }
      delete[] fieldNames;
    }
  }
  uv_work_t request;
  Persistent<Function> callback;
  char* query;
  Row** rows;
  int numRows;
  char** fieldNames;
  int numFields;
  const char* errorText;
};

static void query(uv_work_t* req) {
  QueryBaton* baton = static_cast<QueryBaton*>(req->data);
  int error = mysql_query(&mysql, baton->query);
  
  baton->errorText = 0;
  if(error) {
    baton->rows = 0;
    //TODO: copy this
    baton->errorText = mysql_error(&mysql);
  } else {
    MYSQL_RES* result;
    result = mysql_store_result(&mysql);
    if(result) {
      baton->numRows = mysql_num_rows(result);
      baton->rows = new Row*[baton->numRows];
      baton->numFields = mysql_num_fields(result);
      baton->fieldNames = new char*[baton->numFields];

      MYSQL_FIELD* mysqlField;
      unsigned int fieldCounter = 0;
      while((mysqlField = mysql_fetch_field(result))) {
        baton->fieldNames[fieldCounter] = new char[mysqlField->name_length + 1];
        memcpy(baton->fieldNames[fieldCounter], mysqlField->name, mysqlField->name_length + 1 );
        fieldCounter++;
      }

      MYSQL_ROW mysqlRow;
      unsigned int rowCounter = 0;
      unsigned long* lengths;
      while((mysqlRow = mysql_fetch_row(result))) {
        lengths = mysql_fetch_lengths(result);
        Row* row = new Row();
        row->fieldValues = new char*[mysql_num_fields(result)];
        for(unsigned int i = 0; i < mysql_num_fields(result); i++) {
          row->fieldValues[i] = new char[lengths[i]];
          memcpy ( row->fieldValues[i], mysqlRow[i], lengths[i] );
        }
        baton->rows[rowCounter] = row;
        rowCounter++;
      }
      mysql_free_result(result);
    }
  }
}

static void afterQuery(uv_work_t* req, int bla) {
  QueryBaton* baton = static_cast<QueryBaton*>(req->data);
  Handle<Array> array = Array::New();

  for(int i = 0; i < baton->numRows; i++) {
    Handle<Object> obj = Object::New();
    for(int j = 0; j < baton->numFields; j++) {
      obj->Set(String::New(baton->fieldNames[j]), String::New(baton->rows[i]->fieldValues[j]));
    }
    array->Set(Number::New(i), obj);
  }

  Handle<Value> error;
  if(baton->errorText != 0) {
    error = String::New(baton->errorText);
  } else {
    error = Undefined();
  }
  Handle<Value> argv[] = { array, error };
  baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);
  baton->callback.Dispose();
  delete baton;
}

Handle<Value> query(const Arguments& args) {
  HandleScope scope;
  String::Utf8Value _query(args[0]->ToString());
  Handle<Function> callback = Handle<Function>::Cast(args[1]);
  QueryBaton* baton = new QueryBaton(Persistent<Function>::New(callback), *_query);
  baton->request.data = baton;
  uv_queue_work(uv_default_loop(), &baton->request, query, afterQuery);
  return scope.Close(Undefined());
}

Handle<Value> connect(const Arguments& args) {
  HandleScope scope;
  Handle<Object> params = Handle<Object>::Cast(args[0]);
  String::Utf8Value _host(params->Get(String::New("host")));
  String::Utf8Value _user(params->Get(String::New("user")));
  String::Utf8Value _password(params->Get(String::New("password")));
  Number _port(**params->Get(String::New("port"))->ToNumber());
  String::Utf8Value _database(params->Get(String::New("database")));
  
  mysql_init(&mysql);
  if(!mysql_real_connect(&mysql, *_host, *_user, *_password, *_database, _port.IntegerValue(), 0, 0)) {
    return ThrowException(Exception::Error(String::New(mysql_error(&mysql))));
  }
  return scope.Close(Undefined());
}

void init(Handle<Object> target) {
  target->Set(String::NewSymbol("query"),
              FunctionTemplate::New(query)->GetFunction());
  target->Set(String::NewSymbol("connect"),
              FunctionTemplate::New(connect)->GetFunction());
}
NODE_MODULE(nativemysql, init)