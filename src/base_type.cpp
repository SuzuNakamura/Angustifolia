#include "base_type.h"

namespace kagami {
  //Array
  shared_ptr<void> CreateArrayCopy(shared_ptr<void> source) {
    auto &src_base = *static_pointer_cast<ArrayBase>(source);
    shared_ptr<ArrayBase> dest_base = make_shared<ArrayBase>();

    dest_base->reserve(src_base.size());
    for (auto &unit : src_base) {
      dest_base->emplace_back(Object(type::GetObjectCopy(unit), unit.GetTypeId(), 
        unit.GetMethods(), false).SetTokenType(unit.GetTokenType()));
    }

    return dest_base;
  }

  inline bool IsStringFamily(Object &obj) {
    return obj.GetTypeId() == kTypeIdRawString ||
      obj.GetTypeId() == kTypeIdString ||
      obj.GetTypeId() == kTypeIdWideString;
  }

  Message ArrayConstructor(ObjectMap &p) {
    OBJECT_ASSERT(p, "size", kTypeIdRawString);

    ArrayBase base;
    auto size = stoi(p.Get<string>("size"));
    Object obj = Object();

    if (p.Search("init_value")) {
      obj.Copy(p["init_value"]);
      obj.set_ro(false);
    }

    CONDITION_ASSERT(size > 0, "Illegal array size.");

    const auto type_id = obj.GetTypeId();
    const auto methods = obj.GetMethods();
    const auto token_type = obj.GetTokenType();
    base.reserve(size);

    for (auto count = 0; count < size; count++) {
      base.emplace_back(Object(type::GetObjectCopy(obj), type_id, methods, false)
        .SetTokenType(token_type));
    }

    return Message().SetObject(Object(make_shared<ArrayBase>(base),
      kTypeIdArrayBase,
      kArrayBaseMethods, false)
      .SetConstructorFlag());
  }

  Message ArrayGetElement(ObjectMap &p) {
    OBJECT_ASSERT(p, "index", kTypeIdRawString);

    ArrayBase &base = p.Get<ArrayBase>(kStrObject);
    int idx = stoi(p.Get<string>("index"));
    int size = int(base.size());

    CONDITION_ASSERT(idx < size, "Subscript is out of range.");

    return Message().SetObject(Object().Ref(base[idx]));
  }

  Message ArrayGetSize(ObjectMap &p) {
    auto &obj = p[kStrObject];
    return Message(to_string(GetObjectStuff<ArrayBase>(obj).size()));
  }

  Message ArrayPrint(ObjectMap &p) {
    Message result;
    ObjectMap obj_map;

    auto &base = p.Get<ArrayBase>(kStrObject);
    auto ent = entry::Order("print", kTypeIdNull, -1);

    for (auto &unit : base) {
      obj_map.Input(kStrObject, unit);
      result = ent.Start(obj_map);
      obj_map.clear();
    }

    return result;
  }
  
  //RawString
  Message RawStringGetElement(ObjectMap &p) {
    OBJECT_ASSERT(p, "index", kTypeIdRawString);

    int idx = stoi(p.Get<string>("index"));

    const auto makeStrToken = [](char target)->string {
      return string().append("'").append(1, target).append("'");
    };

    string data = RealString(p.Get<string>(kStrObject));
    size_t size = data.size();

    CONDITION_ASSERT(idx < int(size - 1), "Subscript is out of range.");

    return Message(makeStrToken(data.at(idx)));
  }

  Message RawStringGetSize(ObjectMap &p) {
    auto str = RealString(p.Get<string>(kStrObject));
    return Message(to_string(str.size()));
  }

  Message RawStringPrint(ObjectMap &p) {
    bool doNotWrap = (p.Search("not_wrap"));
    
    auto data = RealString(p.Get<string>(kStrObject));
    std::cout << data;
    if (!doNotWrap) std::cout << std::endl;
    
    return Message();
  }

  //String
  Message StringConstructor(ObjectMap &p) {
    Object &obj = p["raw_string"];
    Object base;

    CONDITION_ASSERT(IsStringFamily(obj),
      "String constructor can't accept this object.");

    if (obj.GetTypeId() == kTypeIdWideString) {
      wstring wstr = GetObjectStuff<wstring>(obj);
      string output = ws2s(wstr);

      base.Set(make_shared<string>(output),
        kTypeIdString, kStringMethods, false)
        .SetConstructorFlag();
    }
    else if (obj.GetTypeId() == kTypeIdString) {
      base.Set(obj.Get(), kTypeIdString)
        .SetConstructorFlag()
        .SetMethods(kStringMethods)
        .set_ro(false);
    }
    else {
      string output = RealString(GetObjectStuff<string>(obj));

      base.Set(make_shared<string>(output), kTypeIdString, 
        kStringMethods, false)
        .SetConstructorFlag();
    }

    return Message().SetObject(base);
  }

  //InStream
  Message InStreamConsturctor(ObjectMap &p) {
    CONDITION_ASSERT(IsStringObject(p["path"]), 
      "Illegal path.");

    string path = RealString(p.Get<string>("path"));

    shared_ptr<ifstream> ifs = 
      make_shared<ifstream>(ifstream(path.c_str(), std::ios::in));

    return Message().SetObject(Object(ifs, kTypeIdInStream, kInStreamMethods, false));
  }

  Message InStreamGet(ObjectMap &p) {
    ifstream &ifs = p.Get<ifstream>(kStrObject);
    Message msg;

    CUSTOM_ASSERT(ifs.good(), kCodeBadStream, "InStream is not working.");

    if (ifs.eof()) return Message("");

    string str;
    std::getline(ifs, str);

    return Message()
      .SetObject(Object(make_shared<string>(str), kTypeIdString, 
        kStringMethods, false));
  }

  Message InStreamEOF(ObjectMap &p) {
    ifstream &ifs = p.Get<ifstream>(kStrObject);
    return Message(ifs.eof() ? kStrTrue : kStrFalse);
  }

  //OutStream
  Message OutStreamConstructor(ObjectMap &p) {
    CONDITION_ASSERT(IsStringObject(p["path"]), 
      "Illegal path.");
    CONDITION_ASSERT(IsStringObject(p["mode"]), 
      "Illegal mode option.");

    string path = RealString(p.Get<string>("path"));
    string mode = RealString(p.Get<string>("mode"));

    shared_ptr<ofstream> ofs;
    bool append = (mode == "append");
    bool truncate = (mode == "truncate");

    if (!append && truncate) {
      ofs = make_shared<ofstream>(ofstream(path.c_str(), 
        std::ios::out | std::ios::trunc));
    }
    else if (append && !truncate) {
      ofs = make_shared<ofstream>(ofstream(path.c_str(),
        std::ios::out | std::ios::app));
    }

    return Message()
      .SetObject(Object(ofs, kTypeIdOutStream, kOutStreamMethods, false));
  }

  Message OutStreamWrite(ObjectMap &p) {
    ofstream &ofs = p.Get<ofstream>(kStrObject);
    Message msg = Message(kStrTrue);

    ASSERT_RETURN(!ofs.good(), kStrFalse);

    if (p.CheckTypeId("str",kTypeIdRawString)) {
      string output = RealString(p.Get<string>("str"));
      ofs << output;
    }
    else if (p.CheckTypeId("str",kTypeIdString)) {
      string origin = p.Get<string>("str");
      ofs << origin;
    }
    else {
      msg = Message(kStrFalse);
    }

    return msg;
  }

  //regex
  Message RegexConstructor(ObjectMap &p) {
    CONDITION_ASSERT(IsStringObject(p["regex"]), 
      "Illegal pattern string.");

    string pattern_string = RealString(p.Get<string>("regex"));
    shared_ptr<regex> reg = make_shared<regex>(regex(pattern_string));

    return Message().SetObject(Object(reg, kTypeIdRegex, kRegexMethods, false));
  }

  Message RegexMatch(ObjectMap &p) {
    CONDITION_ASSERT(IsStringObject(p["str"]), 
      "Illegal target string.");

    string str = RealString(p.Get<string>("str"));
    auto &pat = p.Get<regex>(kStrObject);

    return Message(regex_match(str, pat) ? kStrTrue : kStrFalse);
  }

  //wstring
  Message WideStringContructor(ObjectMap &p) {
    Object obj = p["raw_string"];

    CONDITION_ASSERT(IsStringObject(obj), 
      "String constructor can't accept this object.");

    string output = RealString(GetObjectStuff<string>(obj));
    wstring wstr = s2ws(output);

    return Message()
      .SetObject(Object(make_shared<wstring>(wstr), 
        kTypeIdWideString, kWideStringMethods, false));
  }

  //Function
  Message FunctionGetId(ObjectMap &p) {
    auto &ent = p.Get<Entry>(kStrObject);
    return Message(ent.GetId());
  }

  Message FunctionGetParameters(ObjectMap &p) {
    auto &ent = p.Get<Entry>(kStrObject);
    shared_ptr<ArrayBase> dest_base = make_shared<ArrayBase>();
    auto origin_vector = ent.GetArguments();

    for (auto it = origin_vector.begin(); it != origin_vector.end(); ++it) {
      dest_base->emplace_back(Object(make_shared<string>(*it), kTypeIdString, 
        kStringMethods, false));
    }

    return Message()
      .SetObject(Object(dest_base, kTypeIdArrayBase, kArrayBaseMethods, false));
  }

  bool AssemblingForAutosized(Entry &ent, ObjectMap &p, ObjectMap &target_map, int size) {
    auto ent_args = ent.GetArguments();
    auto va_arg_head = ent_args.back();
    int idx = 0;
    int va_arg_size = 0;
    int count = 0;
    auto is_method = (ent.GetFlag() == kFlagMethod);

    while (idx < int(ent_args.size() - 1)) {
      target_map.Input(ent_args[idx], p("arg", idx));
      idx += 1;
    }

    is_method ?
      va_arg_size = size - 1 :
      va_arg_size = size;

    while (idx < va_arg_size) {
      target_map.Input(va_arg_head + to_string(count), p["arg" + to_string(idx)]);
      count += 1;
      idx += 1;
    }

    target_map.Input(kStrVaSize, Object(to_string(count), T_INTEGER));
    if (is_method) target_map.Input(kStrObject, p["arg" + to_string(size - 1)]);
    return true;
  }

  bool AssemblingForAutoFilling(Entry &ent, ObjectMap &p, ObjectMap &target_map, int size) {
    auto ent_args = ent.GetArguments();
    int idx = 0;
    auto is_method = (ent.GetFlag() == kFlagMethod);

    while (idx < ent_args.size()) {
      if (idx >= size) break;
      if (idx >= size - 1 && is_method) break;
      target_map.Input(ent_args[idx], p["arg" + to_string(idx)]);
      idx += 1;
    }

    if (is_method) target_map.Input(kStrObject, p["arg" + to_string(size - 1)]);
    return true;
  }

  bool AssemblingForNormal(Entry &ent, ObjectMap &p, ObjectMap &target_map, int size) {
    auto ent_args = ent.GetArguments();
    int idx = 0;
    auto is_method = (ent.GetFlag() == kFlagMethod);
    bool state = true;

    while (idx < ent_args.size()) {
      if (idx >= size || (idx >= size - 1 && is_method)) {
        state = false;
        break;
      }

      target_map.Input(ent_args[idx], p["arg" + to_string(idx)]);
      idx += 1;
    }

    if (is_method && state) 
      target_map.Input(kStrObject, p["arg" + to_string(size - 1)]);

    return state;
  }

  Message FunctionCall(ObjectMap &p) {
    auto &ent = p.Get<Entry>(kStrObject);
    int size = stoi(p.Get<string>(kStrVaSize));
    int count = 0;
    ObjectMap target_map;
    bool state;
    
    CALL_ASSERT(ent.Good(), "Bad entry - " + ent.GetId() + ".");

    switch (ent.GetArgumentMode()) {
    case kCodeAutoSize:
      state = AssemblingForAutosized(ent, p, target_map, size);
      break;
    case kCodeAutoFill:
      state = AssemblingForAutoFilling(ent, p, target_map, size);
    default:
      state = AssemblingForNormal(ent, p, target_map, size);
      break;
    }

    CONDITION_ASSERT(state, "Argument error.");

    return ent.Start(target_map);
  }

  void InitPlanners() {
    using type::AddTemplate;
    using entry::AddEntry;

    AddTemplate(kTypeIdFunction, ObjectPlanner(SimpleSharedPtrCopy<Entry>, kFunctionMethods));
    AddEntry(Entry(FunctionGetId, kCodeNormalParm, "", "id", kTypeIdFunction, kFlagMethod));
    AddEntry(Entry(FunctionCall, kCodeAutoSize, "arg", "call", kTypeIdFunction, kFlagMethod));
    AddEntry(Entry(FunctionGetParameters, kCodeNormalParm, "", "parms", kTypeIdFunction, kFlagMethod));

    AddTemplate(kTypeIdRawString, ObjectPlanner(SimpleSharedPtrCopy<string>, kRawStringMethods));
    AddEntry(Entry(RawStringPrint, kCodeNormalParm, "", "__print", kTypeIdRawString, kFlagMethod));
    AddEntry(Entry(RawStringGetElement, kCodeNormalParm, "subscript", "__at", kTypeIdRawString, kFlagMethod));
    AddEntry(Entry(RawStringGetSize, kCodeNormalParm, "", "size", kTypeIdRawString, kFlagMethod));

    AddTemplate(kTypeIdArrayBase, ObjectPlanner(CreateArrayCopy, kArrayBaseMethods));
    AddEntry(Entry(ArrayConstructor, kCodeAutoFill, "size|init_value", "array"));
    AddEntry(Entry(ArrayGetElement, kCodeNormalParm, "index", "__at", kTypeIdArrayBase, kFlagMethod));
    AddEntry(Entry(ArrayPrint, kCodeNormalParm, "", "__print", kTypeIdArrayBase, kFlagMethod));
    AddEntry(Entry(ArrayGetSize, kCodeNormalParm, "", "size", kTypeIdArrayBase, kFlagMethod));

    AddTemplate(kTypeIdString, ObjectPlanner(SimpleSharedPtrCopy<string>, kStringMethods));
    AddEntry(Entry(StringConstructor, kCodeNormalParm, "raw_string", "string"));
    AddEntry(Entry(StringFamilyGetElement<string>, kCodeNormalParm, "index", "__at", kTypeIdString, kFlagMethod));
    AddEntry(Entry(StringFamilyPrint<string, std::ostream>, kCodeNormalParm, "", "__print", kTypeIdString, kFlagMethod));
    AddEntry(Entry(StringFamilySubStr<string>, kCodeNormalParm, "start|size", "substr", kTypeIdString, kFlagMethod));
    AddEntry(Entry(GetStringFamilySize<string>, kCodeNormalParm, "", "size", kTypeIdString, kFlagMethod));
    AddEntry(Entry(StringFamilyConverting<wstring, string>, kCodeNormalParm, "", "to_wide", kTypeIdString, kFlagMethod));

    AddTemplate(kTypeIdInStream, ObjectPlanner(FakeCopy, kInStreamMethods));
    AddEntry(Entry(InStreamConsturctor, kCodeNormalParm, "path", "instream"));
    AddEntry(Entry(InStreamGet, kCodeNormalParm, "", "get", kTypeIdInStream, kFlagMethod));
    AddEntry(Entry(StreamFamilyState<ifstream>, kCodeNormalParm, "", "good", kTypeIdInStream, kFlagMethod));
    AddEntry(Entry(InStreamEOF, kCodeNormalParm, "", "eof", kTypeIdInStream, kFlagMethod));
    AddEntry(Entry(StreamFamilyClose<ifstream>, kCodeNormalParm, "", "close", kTypeIdInStream, kFlagMethod));

    AddTemplate(kTypeIdOutStream, ObjectPlanner(FakeCopy, kOutStreamMethods));
    AddEntry(Entry(OutStreamConstructor, kCodeNormalParm, "path|mode", "outstream"));
    AddEntry(Entry(OutStreamWrite, kCodeNormalParm, "str", "write", kTypeIdOutStream, kFlagMethod));
    AddEntry(Entry(StreamFamilyState<ofstream>, kCodeNormalParm, "", "good", kTypeIdOutStream, kFlagMethod));
    AddEntry(Entry(StreamFamilyClose<ofstream>, kCodeNormalParm, "", "close", kTypeIdOutStream, kFlagMethod));

    AddTemplate(kTypeIdRegex, ObjectPlanner(FakeCopy, kTypeIdRegex));
    AddEntry(Entry(RegexConstructor, kCodeNormalParm, "regex", "regex"));
    AddEntry(Entry(RegexMatch, kCodeNormalParm, "str", "match", kTypeIdRegex, kFlagMethod));

    AddTemplate(kTypeIdWideString, ObjectPlanner(SimpleSharedPtrCopy<wstring>, kWideStringMethods));
    AddEntry(Entry(WideStringContructor, kCodeNormalParm, "raw_string", "wstring"));
    AddEntry(Entry(GetStringFamilySize<wstring>, kCodeNormalParm, "", "size", kTypeIdWideString, kFlagMethod));
    AddEntry(Entry(StringFamilyGetElement<wstring>, kCodeNormalParm, "index", "__at", kTypeIdWideString, kFlagMethod));
    AddEntry(Entry(StringFamilyPrint<wstring, std::wostream>, kCodeNormalParm, "", "__print", kTypeIdWideString, kFlagMethod));
    AddEntry(Entry(StringFamilySubStr<wstring>, kCodeNormalParm, "start|size", "substr", kTypeIdWideString, kFlagMethod));
    AddEntry(Entry(StringFamilyConverting<string, wstring>, kCodeNormalParm, "", "to_byte", kTypeIdWideString, kFlagMethod));

    AddTemplate(kTypeIdNull, ObjectPlanner(NullCopy, kStrEmpty));
  }
}