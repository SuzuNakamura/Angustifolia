#include "machine.h"

namespace kagami {
#if defined(_WIN32) && defined(_MSC_VER)
  //from MSDN
  std::wstring s2ws(const std::string& s) {
    auto slength = static_cast<int>(s.length()) + 1;
    auto len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    auto *buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
  }

  std::string ws2s(const std::wstring& s) {
    int len;
    int slength = (int)s.length() + 1;
    len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, 0, 0);
    std::string r(len, '\0');
    WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, &r[0], len, 0, 0);
    return r;
  }
#else
  //from https://www.yasuhisay.info/entry/20090722/1248245439
  std::wstring s2ws(const std::string& s) {
    if (s.empty()) return wstring();
    size_t length = s.size();
    wchar_t *wc = (wchar_t*)malloc(sizeof(wchar_t)* (length + 2));
    mbstowcs(wc, s.c_str(), s.length() + 1);
    std::wstring str(wc);
    free(wc);
    return str;
  }

  std::string ws2s(const std::wstring& s) {
    if (s.empty()) return string();
    size_t length = s.size();
    char *c = (char*)malloc(sizeof(char)* length * 2);
    wcstombs(c, s.c_str(), s.length() + 1);
    std::string result(c);
    free(c);
    return result;
  }
#endif

  string IndentationAndCommentProc(string target) {
    if (target == "") return "";
    string data;
    char current, last;
    size_t head = 0, tail = 0;
    bool exempt_blank_char = true;
    bool string_processing = false;
    auto toString = [](char t) ->string {return string().append(1, t); };

    for (size_t count = 0; count < target.size(); ++count) {
      current = target[count];
      auto type = kagami::Kit::GetTokenType(toString(current));
      if (type != TokenTypeEnum::T_BLANK && exempt_blank_char) {
        head = count;
        exempt_blank_char = false;
      }
      if (current == '\'' && last != '\\') string_processing = !string_processing;
      if (!string_processing && current == '#') {
        tail = count;
        break;
      }
      last = target[count];
    }
    if (tail > head) data = target.substr(head, tail - head);
    else data = target.substr(head, target.size() - head);
    if (data.front() == '#') return "";

    while (!data.empty() &&
      Kit::GetTokenType(toString(data.back())) == TokenTypeEnum::T_BLANK) {
      data.pop_back();
    }
    return data;
  }

  vector<StringUnit> MultilineProcessing(vector<string> &src) {
    vector<StringUnit> output;
    string buf;
    size_t idx = 0, line_idx = 0;

    auto isInString = [](string src) {
      bool inString = false;
      bool escape = false;
      for (size_t strIdx = 0; strIdx < src.size(); ++strIdx) {
        if (inString && escape) {
          escape = false;
          continue;
        }
        if (src[strIdx] == '\'') inString = !inString;
        if (inString && !escape && src[strIdx] == '\\') {
          escape = true;
          continue;
        }
      }
      return inString;
    };

    while (idx < src.size()) {
      buf = IndentationAndCommentProc(src[idx]);
      if (buf == "") {
        idx += 1;
        continue;
      }
      line_idx = idx;
      while (buf.back() == '_') {
        bool inString = isInString(buf);
        if (!inString) {
          idx += 1;
          buf.pop_back();
          buf = buf + IndentationAndCommentProc(src[idx]);
        }
      }
      output.push_back(StringUnit(line_idx, buf));
      idx += 1;
    }

    return output;
  }

  map<string, Machine> &GetFunctionBase() {
    static map<string, Machine> base;
    return base;
  }

  inline void AddFunction(string id, vector<Meta> proc, vector<string> parms) {
    auto &base = GetFunctionBase();
    base[id] = Machine(proc).SetParameters(parms).SetFunc();
    entry::AddEntry(Entry(FunctionTunnel, id, parms));
  }

  inline Machine *GetFunction(string id) {
    Machine *machine = nullptr;
    auto &base = GetFunctionBase();
    auto it = base.find(id);
    if (it != base.end()) machine = &(it->second);
    return machine;
  }

  Message FunctionTunnel(ObjectMap &p) {
    Message msg;
    Object &func_id = p[kStrUserFunc];
    string id = *static_pointer_cast<string>(func_id.Get());
    Machine *mach = GetFunction(id);
    if (mach != nullptr) {
      msg = mach->RunAsFunction(p);
    }
    return msg;
  }

  inline bool GetBooleanValue(string src) {
    if (src == kStrTrue) return true;
    if (src == kStrFalse) return false;
    if (src == "0" || src.empty()) return false;
    return true;
  }

  Message Calling(Activity activity, string parmStr, vector<Object> objects) {
    vector<string> parms = Kit::BuildStringVector(parmStr);
    ObjectMap obj_map;
    for (size_t i = 0; i < parms.size(); i++) {
      obj_map.insert(NamedObject(parms[i], objects[i]));
    }
    return activity(obj_map);
  }

  void Machine::MakeFunction(size_t start,size_t end, vector<string> &def_head) {
    if (start > end) return;
    string id = def_head[0];
    vector<string> parms;
    vector<Meta> proc;

    for (size_t i = 1; i < def_head.size(); ++i) {
      parms.push_back(def_head[i]);
    }
    for (size_t j = start; j <= end; ++j) {
      proc.push_back(storage_[j]);
    }
    AddFunction(id, proc, parms);
  }

  void Machine::Reset(MachCtlBlk *blk) {
    Kit().CleanupVector(storage_);
    while (!blk->cycle_nest.empty()) blk->cycle_nest.pop();
    while (!blk->cycle_tail.empty()) blk->cycle_tail.pop();
    while (!blk->mode_stack.empty()) blk->mode_stack.pop();
    while (!blk->condition_stack.empty()) blk->condition_stack.pop();
    delete blk;
  }

  Machine::Machine(const char *target, bool is_main) {
    std::wifstream stream;
    wstring buf;
    health_ = true;
    size_t subscript = 0;
    vector<string> script_buf;

    is_main_ = is_main;

    stream.open(target, std::ios::in);
    if (stream.good()) {
      while (!stream.eof()) {
        std::getline(stream, buf);
        string temp = ws2s(buf);
        if (!temp.empty() && temp.back() == '\0') temp.pop_back();
        script_buf.emplace_back(temp);
      }
    }
    stream.close();

    vector<StringUnit> string_units = MultilineProcessing(script_buf);
    Analyzer analyzer;
    Message msg;
    for (auto it = string_units.begin(); it != string_units.end(); ++it) {
      if (it->second == "") continue;
      msg = analyzer.Make(it->second, it->first);
      if (msg.GetValue() == kStrFatalError) {
        trace::Log(msg.SetIndex(subscript));
        health_ = false;
        break;
      }
      if (msg.GetValue() == kStrWarning) {
        trace::Log(msg.SetIndex(subscript));
      }
      storage_.emplace_back(Meta(
        analyzer.GetOutput(),
        analyzer.get_index(),
        analyzer.GetMainToken()));
      analyzer.Clear();
    }

    msg = PreProcessing();
    if (msg.GetValue() == kStrFatalError) {
      health_ = false;
      trace::Log(msg);
    }
  }

  void Machine::CaseHead(Message &msg, MachCtlBlk *blk) {
    entry::CreateContainer();
    blk->mode_stack.push(blk->mode);
    blk->mode = kModeCaseJump;
    blk->condition_stack.push(false);
  }

  void Machine::WhenHead(bool value, MachCtlBlk *blk) {
    if (!blk->condition_stack.empty()) {
      if (blk->mode == kModeCase && blk->condition_stack.top() == true) {
        blk->mode = kModeCaseJump;
      }
      else if (value == true && blk->condition_stack.top() == false) {
        blk->mode = kModeCase;
        blk->condition_stack.top() = true;
      }
    }
  }

  void Machine::ConditionRoot(bool value, MachCtlBlk *blk) {
    blk->mode_stack.push(blk->mode);
    entry::CreateContainer();
    if (value == true) {
      blk->mode = kModeCondition;
      blk->condition_stack.push(true);
    }
    else {
      blk->mode = kModeNextCondition;
      blk->condition_stack.push(false);
    }
  }

  void Machine::ConditionBranch(bool value, MachCtlBlk *blk) {
    if (!blk->condition_stack.empty()) {
      if (blk->condition_stack.top() == false && blk->mode == kModeNextCondition
        && value == true) {
        entry::CreateContainer();
        blk->mode = kModeCondition;
        blk->condition_stack.top() = true;
      }
      else if (blk->condition_stack.top() == true && blk->mode == kModeCondition) {
        blk->mode = kModeNextCondition;
      }
    }
    else {
      //msg = Message
      health_ = false;
    }
  }

  void Machine::ConditionLeaf(MachCtlBlk *blk) {
    if (!blk->condition_stack.empty()) {
      if (blk->condition_stack.top() == true) {
        switch (blk->mode) {
        case kModeCondition:
        case kModeNextCondition:
          blk->mode = kModeNextCondition;
          break;
        case kModeCase:
        case kModeCaseJump:
          blk->mode = kModeCaseJump;
          break;
        }
      }
      else {
        entry::CreateContainer();
        blk->condition_stack.top() = true;
        switch (blk->mode) {
        case kModeNextCondition:
          blk->mode = kModeCondition;
          break;
        case kModeCaseJump:
          blk->mode = kModeCase;
          break;
        }
      }
    }
    else {
      //msg = Message
      health_ = false;
    }
  }

  void Machine::HeadSign(bool value, MachCtlBlk *blk) {
    if (blk->cycle_nest.empty()) {
      blk->mode_stack.push(blk->mode);
      entry::CreateContainer();
    }
    else {
      if (blk->cycle_nest.top() != blk->current - 1) {
        blk->mode_stack.push(blk->mode);
        entry::CreateContainer();
      }
    }
    if (value == true) {
      blk->mode = kModeCycle;
      if (blk->cycle_nest.empty()) {
        blk->cycle_nest.push(blk->current - 1);
      }
      else if (blk->cycle_nest.top() != blk->current - 1) {
        blk->cycle_nest.push(blk->current - 1);
      }
    }
    else if (value == false) {
      blk->mode = kModeCycleJump;
      if (!blk->cycle_tail.empty()) {
        blk->current = blk->cycle_tail.top();
      }
    }
    else {
      health_ = false;
    }
  }

  void Machine::TailSign(MachCtlBlk *blk) {
    if (blk->mode == kModeCondition || blk->mode == kModeNextCondition) {
      blk->condition_stack.pop();
      blk->mode = blk->mode_stack.top();
      blk->mode_stack.pop();
      entry::DisposeManager();
    }
    else if (blk->mode == kModeCycle || blk->mode == kModeCycleJump) {
      switch (blk->mode) {
      case kModeCycle:
        if (blk->cycle_tail.empty() || blk->cycle_tail.top() != blk->current - 1) {
          blk->cycle_tail.push(blk->current - 1);
        }
        blk->current = blk->cycle_nest.top();
        entry::GetCurrentContainer().clear();
        break;
      case kModeCycleJump:
        if (blk->s_continue) {
          if (blk->cycle_tail.empty() || blk->cycle_tail.top() != blk->current - 1) {
            blk->cycle_tail.push(blk->current - 1);
          }
          blk->current = blk->cycle_nest.top();
          blk->mode = kModeCycle;
          blk->mode_stack.top() = blk->mode;
          blk->s_continue = false;
          entry::GetCurrentContainer().clear();
        }
        else {
          if (blk->s_break) blk->s_break = false;
          blk->mode = blk->mode_stack.top();
          blk->mode_stack.pop();
          if (!blk->cycle_nest.empty()) blk->cycle_nest.pop();
          if (!blk->cycle_tail.empty()) blk->cycle_tail.pop();
          entry::DisposeManager();
        }
        break;
      default:break;
      }
    }
    else if (blk->mode == kModeCase || blk->mode == kModeCaseJump) {
      blk->condition_stack.pop();
      blk->mode = blk->mode_stack.top();
      blk->mode_stack.pop();
      entry::DisposeManager();
    }
  }

  void Machine::Continue(MachCtlBlk *blk) {
    while (!blk->mode_stack.empty() && blk->mode != kModeCycle) {
      if (blk->mode == kModeCondition) {
        blk->condition_stack.pop();
        blk->nest_head_count++;
      }
      blk->mode = blk->mode_stack.top();
      blk->mode_stack.pop();
    }
    blk->mode = kModeCycleJump;
    blk->s_continue = true;
  }

  void Machine::Break(MachCtlBlk *blk) {
    while (!blk->mode_stack.empty() && blk->mode != kModeCycle) {
      if (blk->mode == kModeCondition) {
        blk->condition_stack.pop();
        blk->nest_head_count++;
      }
      blk->mode = blk->mode_stack.top();
      blk->mode_stack.pop();
    }
    if (blk->mode == kModeCycle) {
      blk->mode = kModeCycleJump;
      blk->s_break = true;
    }
  }

  bool Machine::IsBlankStr(string target) {
    if (target.empty() || target.size() == 0) return true;
    for (const auto unit : target) {
      if (unit != '\n' && unit != ' ' && unit != '\t' && unit != '\r') {
        return false;
      }
    }
    return true;
  }

  Message Machine::MetaProcessing(Meta &meta, string name, MachCtlBlk *blk) {
    Kit kit;
    int mode, flag;
    string error_string, id, type_id, va_arg_head;
    Message msg;
    ObjectMap obj_map;
    deque<Object> returning_base;
    vector<string> args;
    size_t sub = 0;
    size_t count = 0;
    size_t va_arg_size = 0;
    vector<Instruction> &action_base = meta.GetContains();
    bool error_returning = false, error_obj_checking = false;
    bool tail_recursion = false, preprocessing = (blk == nullptr);

    auto makeObject = [&](Argument &parm) -> Object {
      Object obj;
      ObjectPointer ptr;
      switch (parm.type) {
      case PT_NORMAL:
        obj.Manage(parm.data, parm.tokenType);
        break;
      case PT_OBJ:
        ptr = entry::FindObject(parm.data);
        if (ptr != nullptr) {
          obj.Ref(*ptr);
        }
        else {
          error_string = "Object is not found - " + parm.data;
          error_obj_checking = true;
        }
        break;
      case PT_RET:
        if (!returning_base.empty()) {
          obj = returning_base.front();
          returning_base.pop_front();
        }
        else {
          error_string = "Return base error.";
          error_returning = true;
        }
        break;
      default:
        break;
      }

      return obj;
    };

    for (size_t idx = 0; idx < action_base.size(); idx += 1) {
      kit.CleanupMap(obj_map).CleanupVector(args);
      id.clear();
      va_arg_head.clear();
      type_id.clear();
      sub = 0;
      count = 0;

      auto &ent = action_base[idx].first;
      auto &parms = action_base[idx].second;
      auto entParmSize = ent.GetParmSize();


      (ent.GetId() == name && idx == action_base.size() - 2
        && action_base.back().first.GetTokenEnum() == GT_RETURN) ?
        tail_recursion = true :
        tail_recursion = false;

      if (!preprocessing && !tail_recursion) {
        (ent.GetId() == name && idx == action_base.size() - 1
          && blk->last_index && name != "") ?
          tail_recursion = true :
          tail_recursion = false;
      }

      if (ent.NeedRecheck()) {
        id = ent.GetId();
        ent.IsMethod() ?
          type_id = makeObject(parms.back()).GetTypeId() :
          type_id = kTypeIdNull;

        ent = entry::Order(id, type_id);

        if (!ent.Good()) {
          msg = Message(kStrFatalError, kCodeIllegalCall, "Function not found - " + id);
          break;
        }
      }

      args = ent.GetArguments();
      mode = ent.GetArgumentMode();
      flag = ent.GetFlag();

      if (mode == kCodeAutoSize) {
        while (sub < entParmSize - 1) {
          obj_map.insert(NamedObject(args[sub], makeObject(parms[sub])));
          sub += 1;
        }

        va_arg_head = args.back();
        flag == kFlagMethod ?
          va_arg_size = parms.size() - 1 :
          va_arg_size = parms.size();

        while (sub < va_arg_size) {
          obj_map.insert(NamedObject(va_arg_head + to_string(count), makeObject(parms[sub])));
          count += 1;
          sub += 1;
        }

        obj_map.insert(NamedObject("__size",
          Object().Manage(to_string(count), T_INTEGER)));
      }
      else {
        while (sub < args.size()) {
          if (sub >= parms.size() && mode == kCodeAutoFill) break;
          //error
          obj_map.insert(NamedObject(args[sub], makeObject(parms[sub])));
          sub += 1;
        }
      }

      if (flag == kFlagMethod) {
        obj_map.insert(NamedObject(kStrObject, makeObject(parms.back())));
      }

      if (tail_recursion) {
        blk->recursion_map = obj_map;
        break;
      }

      if (error_returning || error_obj_checking) break;
      msg = ent.Start(obj_map);
      const auto code = msg.GetCode();
      const auto value = msg.GetValue();
      const auto detail = msg.GetDetail();

      if (value == kStrFatalError) break;

      if (code == kCodeObject) {
        auto object = msg.GetObj();
        returning_base.emplace_back(object);
      }
      else if ((value == kStrRedirect && (code == kCodeSuccess || code == kCodeHeadPlaceholder))
        && ent.GetTokenEnum() != GT_TYPE_ASSERT) {
        Object obj;
        obj.Manage(detail, Kit::GetTokenType(detail));

        if (entry::IsOperatorToken(ent.GetTokenEnum())
          && idx + 1 < action_base.size()) {
          auto token = action_base[idx + 1].first.GetTokenEnum();
          if (entry::IsOperatorToken(token)) {
            returning_base.emplace_front(obj);
          }
          else {
            returning_base.emplace_back(obj);
          }
        }
        else {
          returning_base.emplace_back(obj);
        }
      }
    }

    if (error_returning || error_obj_checking) {
      msg = Message(kStrFatalError, kCodeIllegalSymbol, error_string);
    }

    if(!preprocessing && tail_recursion) blk->tail_recursion = tail_recursion;

    return msg;
  }

  Message Machine::PreProcessing() {
    Meta *meta = nullptr;
    GenericTokenEnum token;
    Message result;
    bool flag = false;
    map<size_t, size_t> skipped_idx;
    using IndexPair = pair<size_t, size_t>;
    size_t nest_head_count = 0;
    size_t def_start;
    vector<string> def_head;

    for (size_t idx = 0; idx < storage_.size(); ++idx) {
      if (!health_) break;
      meta = &storage_[idx];
      token = entry::GetGenericToken(meta->GetMainToken().first);
      if (token == GT_WHILE || token == GT_IF || token == GT_CASE) {
        nest_head_count++;
      }
      else if (token == GT_DEF) {
        if (flag == true) {
          result = Message(kStrFatalError, kCodeBadExpression, "Define function in function is not supported.").SetIndex(idx);
          break;
        }
        result = MetaProcessing(*meta, "", nullptr);
        def_head = Kit().BuildStringVector(result.GetDetail());
        def_start = idx + 1;
        flag = true;
      }
      else if (token == GT_END) {
        if (nest_head_count > 0) {
          nest_head_count--;
        }
        else {
          skipped_idx.insert(IndexPair(def_start - 1, idx));
          MakeFunction(def_start, idx - 1, def_head);
          Kit().CleanupVector(def_head);
          def_start = 0;
          flag = false;
        }
      }
    }

    vector<Meta> *otherMeta = new vector<Meta>();
    bool filter = false;
    for (size_t idx = 0; idx < storage_.size(); ++idx) {
      auto it = skipped_idx.find(idx);
      if (it != skipped_idx.end()) {
        idx = it->second;
        continue;
      }
      otherMeta->emplace_back(storage_[idx]);
    }

    storage_.swap(*otherMeta);
    delete otherMeta;

    return result;
  }

  void Machine::InitGlobalObject(bool create_container, string name) {
    if (create_container) entry::CreateContainer();

    auto create = [&](string id, string value)->void {
      entry::CreateObject(id, Object()
        .Manage("'" + value + "'",T_STRING));
    };

    if (is_main_) {
      create("__name__", "__main__");
    }
    else {
      if (name != kStrEmpty) {
        create("__name__", name);
      }
      else {
        create("__name__", "");
      }
    }
  }

  void Machine::ResetBlock(MachCtlBlk *blk) {
    blk->mode = kModeNormal;
    blk->nest_head_count = 0;
    blk->current = 0;
    blk->mode = kModeNormal;
    blk->def_start = 0;
    blk->s_continue = false;
    blk->s_break = false;
    blk->last_index = false;
    blk->tail_recursion = false;
    blk->tail_call = false;
  }

  void Machine::ResetContainer(string func_id) {
    Object *func_sign = entry::GetCurrentContainer().Find(kStrUserFunc);

    while (func_sign == nullptr) {
      entry::DisposeManager();
      func_sign = entry::GetCurrentContainer().Find(kStrUserFunc);
      if (func_sign != nullptr) {
        string value = GetObjectStuff<string>(*func_sign);
        if (value == func_id) break;
      }
    }
  }

  Message Machine::Run(bool create_container, string name) {
    Message result;
    MachCtlBlk *blk = new MachCtlBlk();
    Meta *meta = nullptr;
    GenericTokenEnum token;
    bool judged = false;

    ResetBlock(blk);
    health_ = true;

    if (storage_.empty()) return result;

    InitGlobalObject(create_container, name);
    if (result.GetCode() < kCodeSuccess) return result;

    //Main state machine
    while (blk->current < storage_.size()) {
      if (!health_) break;
      blk->last_index = (blk->current == storage_.size() - 1);
      meta = &storage_[blk->current];

      token = entry::GetGenericToken(meta->GetMainToken().first);
      switch (blk->mode) {
      case kModeNextCondition:
        if (entry::HasTailTokenRequest(token)) {
          result = Message(kStrRedirect, kCodeHeadPlaceholder, kStrTrue);
          judged = true;
        }
        else if (token != GT_ELSE && token != GT_END && token != GT_ELIF) {
          result = Message(kStrRedirect, kCodeSuccess, kStrPlaceHolder);
          judged = true;
        }
        break;
      case kModeCycleJump:
        if (token != GT_END && token != GT_IF && token != GT_WHILE) {
          result = Message(kStrRedirect, kCodeSuccess, kStrPlaceHolder);
          judged = true;
        }
        break;
      case kModeCaseJump:
        if (entry::HasTailTokenRequest(token)) {
          result = Message(kStrRedirect, kCodeHeadPlaceholder, kStrTrue);
          judged = true;
        }
        else if (token != GT_WHEN && token != GT_END) {
          result = Message(kStrRedirect, kCodeSuccess, kStrPlaceHolder);
          judged = true;
        }
        break;
      default:
        break;
      }
      
      if (!judged) result = MetaProcessing(*meta, name, blk);

      const auto value = result.GetValue();
      const auto code  = result.GetCode();
      const auto detail = result.GetDetail();

      if (blk->tail_recursion) {
        Object obj = *entry::FindObject(kStrUserFunc);
        auto &base = entry::GetCurrentContainer();
        base.clear();
        base.Add(kStrUserFunc, obj);
        for (auto &unit : blk->recursion_map) {
          base.Add(unit.first, unit.second);
        }
        blk->recursion_map.clear();
        ResetBlock(blk);
        ResetContainer(name);
        continue;
      }

      if (value == kStrFatalError) {
        trace::Log(result.SetIndex(storage_[blk->current].GetIndex()));
        break;
      }

      if (value == kStrWarning) {
        trace::Log(result.SetIndex(storage_[blk->current].GetIndex()));
      }

      if (value == kStrStopSign) {
        break;
      }

      switch (code) {
      case kCodeContinue:
        Continue(blk);
        break;
      case kCodeBreak:
        Break(blk);
        break;
      case kCodeConditionRoot:
        ConditionRoot(GetBooleanValue(value), blk); 
        break;
      case kCodeConditionBranch:
        if (blk->nest_head_count > 0) break;
        ConditionBranch(GetBooleanValue(value), blk);
        break;
      case kCodeConditionLeaf:
        if (blk->nest_head_count > 0) break;
        ConditionLeaf(blk);
        break;
      case kCodeCase:
        CaseHead(result, blk);
        break;
      case kCodeWhen:
        WhenHead(GetBooleanValue(value), blk);
        break;
      case kCodeHeadSign:
        HeadSign(GetBooleanValue(value), blk);
        break;
      case kCodeHeadPlaceholder:
        blk->nest_head_count++;
        break;
      case kCodeTailSign:
        if (blk->nest_head_count > 0) {
          blk->nest_head_count--;
          break;
        }
        TailSign(blk);
        break;
      default:break;
      }
      ++blk->current;
      if (judged) judged = false;
    }

    if (create_container) entry::DisposeManager();
    delete blk;
    return result;
  }

  Message Machine::RunAsFunction(ObjectMap &p) {
    Message msg;
    auto &base = entry::CreateContainer();
    string func_id = p.Get<string>(kStrUserFunc);

    for (auto &unit : p) {
      base.Add(unit.first, unit.second);
    }

    msg = Run(false, func_id);

    if (msg.GetCode() >= kCodeSuccess) {
      msg = Message();
      auto &currentBase = entry::GetCurrentContainer();
      Object *ret = currentBase.Find(kStrRetValue);
      if (ret != nullptr) {
        Object obj;
        obj.Copy(*ret);
        msg.SetObject(obj);
      }
    }

    Object *func_sign = entry::GetCurrentContainer().Find(kStrUserFunc);

    ResetContainer(func_id);

    entry::DisposeManager();
    return msg;
  }
}

