//#include <iostream>
#include <ctime>
#ifndef _NO_CUI_
#include <iostream>
#endif
#include "parser.h"

namespace kagami {
  namespace trace {
    vector<log_t> &GetLogger() {
      static vector<log_t> base;
      return base;
    }

    void Log(Message msg) {
      auto now = time(nullptr);
#if defined(_WIN32)
      char nowtime[30] = { ' ' };
      ctime_s(nowtime, sizeof(nowtime), &now);
      GetLogger().emplace_back(log_t(string(nowtime), msg));
#else
      string nowtime(ctime(&now));
      GetLogger().emplace_back(log_t(nowtime, msg));
#endif
    }
  }

  namespace entry {
    BasicGenToken GetGenericToken(string src) {
      if (src == kStrNop)          return BasicGenToken::NOP;
      if (src == kStrDef)          return BasicGenToken::DEF;
      if (src == kStrRef)          return BasicGenToken::REF;
      if (src == kStrCodeSub)      return BasicGenToken::CODE_SUB;
      if (src == kStrSub)          return BasicGenToken::SUB;
      if (src == kStrBinOp)        return BasicGenToken::BINOP;
      if (src == kStrIf)           return BasicGenToken::IF;
      if (src == kStrElif)         return BasicGenToken::ELIF;
      if (src == kStrEnd)          return BasicGenToken::END;
      if (src == kStrElse)         return BasicGenToken::ELSE;
      if (src == kStrVar)          return BasicGenToken::VAR;
      if (src == kStrSet)          return BasicGenToken::SET;
      if (src == kStrWhile)        return BasicGenToken::WHILE;
      if (src == kStrFor)          return BasicGenToken::FOR;
      if (src == kStrLeftSelfInc)  return BasicGenToken::LSELF_INC;
      if (src == kStrLeftSelfDec)  return BasicGenToken::LSELF_DEC;
      if (src == kStrRightSelfInc) return BasicGenToken::RSELF_INC;
      if (src == kStrRightSelfDec) return BasicGenToken::RSELF_DEC;
      return BasicGenToken::NUL;
    }

    string GetGenTokenValue(BasicGenToken token) {
      string result;
      switch (token) {
      case BasicGenToken::NOP:result       = kStrNop;                 break;
      case BasicGenToken::DEF:result       = kStrDef;                 break;
      case BasicGenToken::REF:result       = kStrRef;                 break;
      case BasicGenToken::CODE_SUB:result  = kStrCodeSub;             break;
      case BasicGenToken::SUB:result       = kStrSub;                 break;
      case BasicGenToken::BINOP:result     = kStrBinOp;               break;
      case BasicGenToken::IF:result        = kStrIf;                  break;
      case BasicGenToken::ELIF:result      = kStrElif;                break;
      case BasicGenToken::END:result       = kStrEnd;                 break;
      case BasicGenToken::ELSE:result      = kStrElse;                break;
      case BasicGenToken::VAR:result       = kStrVar;                 break;
      case BasicGenToken::SET:result       = kStrSet;                 break;
      case BasicGenToken::WHILE:result     = kStrWhile;               break;
      case BasicGenToken::FOR:result       = kStrFor;                 break;
      case BasicGenToken::LSELF_INC:result = kStrLeftSelfInc;         break;
      case BasicGenToken::LSELF_DEC:result = kStrLeftSelfDec;         break;
      case BasicGenToken::RSELF_INC:result = kStrRightSelfInc;        break;
      case BasicGenToken::RSELF_DEC:result = kStrRightSelfDec;        break;
      }
      return result;
    }

    OperatorCode GetOperatorCode(string src) {
      if (src == "+")  return ADD;
      if (src == "-")  return SUB;
      if (src == "*")  return MUL;
      if (src == "/")  return DIV;
      if (src == "=")  return EQUAL;
      if (src == "==") return IS;
      if (src == "<=") return LESS_OR_EQUAL;
      if (src == ">=") return MORE_OR_EQUAL;
      if (src == "!=") return NOT_EQUAL;
      if (src == ">")  return MORE;
      if (src == "<")  return LESS;
      if (src == "++") return SELFINC;
      if (src == "--") return SELFDEC;
      return NUL;
    }

    vector<EntryProvider> &GetEntryBase() {
      static vector<EntryProvider> base;
      return base;
    }

    map<BasicGenToken, EntryProvider> &GetGenProviderBase() {
      static map<BasicGenToken, EntryProvider> base;
      return base;
    }

    void Inject(ActivityTemplate temp) { 
      GetEntryBase().emplace_back(EntryProvider(temp));
    }

    void LoadGenProvider(BasicGenToken token, ActivityTemplate temp) {
      GetGenProviderBase().insert(pair<BasicGenToken, EntryProvider>(
        token, EntryProvider(temp)));
    }

    EntryProvider GetGenericProvider(BasicGenToken token) {
      auto &base = GetGenProviderBase();
      map<BasicGenToken, EntryProvider>::iterator it = base.find(token);
      if (it != base.end()) return it->second;
      return EntryProvider();
    }

    EntryProvider Order(string id,string type = kTypeIdNull,int size = -1) {
      BasicGenToken basicOpCode = GetGenericToken(id);
      if (basicOpCode != BasicGenToken::NUL) {
        return GetGenericProvider(basicOpCode);
      }

      vector<EntryProvider> &base = GetEntryBase();
      OperatorCode opCode = GetOperatorCode(id);

      if (opCode == EQUAL)    return Order(kStrSet); 
      else if (opCode != NUL) return Order(kStrBinOp); 

      EntryProvider result;
      for (auto &unit : base) {
        if (id == unit.GetId() && type == unit.GetSpecificType()
          && (size == -1 || size == unit.GetParameterSIze())) {
          result = unit;
          break;
        }
      }
      return result;
    }

    size_t GetRequiredCount(string id) {
      OperatorCode opCode = GetOperatorCode(id);
      if (opCode == EQUAL) return Order(kStrSet).GetParameterSIze();
      if (opCode != NUL)   return Order(kStrBinOp).GetParameterSIze();
      auto provider = Order(id);
      if (provider.Good()) return provider.GetParameterSIze();
      
      return 0;
    }

    void RemoveByTemplate(ActivityTemplate temp) {
      auto &base = GetEntryBase();
      vector<EntryProvider>::iterator it;
      for (it = base.begin(); it != base.end(); ++it) if (*it == temp) break;
      if (it != base.end()) base.erase(it);
    }
  }

  BasicTokenValue GetBasicToken(string src) {
    if (src == "=")   return TOKEN_EQUAL;
    if (src == ",")   return TOKEN_COMMA;
    if (src == "[")   return TOKEN_LEFT_SQRBRACKET;
    if (src == ".")   return TOKEN_DOT;
    if (src == ":")   return TOKEN_COLON;
    if (src == "(")   return TOKEN_LEFT_BRACKET;
    if (src == "]")   return TOKEN_RIGHT_SQRBRACKET;
    if (src == ")")   return TOKEN_RIGHT_BRACKET;
    if (src == "++")  return TOKEN_SELFOP;
    if (src == "--")  return TOKEN_SELFOP;
    return TOKEN_OTHERS;
  }

  //TODO:processing for integer
  bool GetBooleanValue(string src) {
    if (src == kStrTrue) return true;
    if (src == kStrFalse) return false;
    if (src == "0" || src.empty()) return false;
    return true;
  }

  ScriptMachine::ScriptMachine(const char *target) {
    string temp;
    end = false;
    isTerminal = false;
    current = 0;
    size_t subscript = 0;

    stream.open(target, std::ios::in);
    if (stream.good()) {
      while (!stream.eof()) {
        std::getline(stream, temp);
        if (!IsBlankStr(temp) && temp.front() != '#') {
          storage.push_back(Processor().Build(temp).SetIndex(subscript));
        }
        subscript++;
      }
    }
    stream.close();
  }

  void ScriptMachine::ConditionRoot(bool value) {
    modeStack.push(currentMode);
    switch (value) {
    case true:
      entry::CreateManager();
      currentMode = kModeCondition;
      conditionStack.push(true);
      break;
    case false:
      currentMode = kModeNextCondition;
      conditionStack.push(false);
      break;
    }
  }

  void ScriptMachine::ConditionBranch(bool value) {
    if (!conditionStack.empty()) {
      if (conditionStack.top() == false && currentMode == kModeNextCondition
        && value == true) {
        entry::CreateManager();
        currentMode = kModeCondition;
        conditionStack.top() = true;
      }
    }
    else {
      //msg.combo
      health = false;
    }
  }

  void ScriptMachine::ConditionLeaf() {
    if (!conditionStack.empty()) {
      switch (conditionStack.top()) {
      case true:
        currentMode = kModeNextCondition;
        break;
      case false:
        entry::CreateManager();
        conditionStack.top() = true;
        currentMode = kModeCondition;
        break;
      }
    }
    else {
      //msg.combo
      health = false;
    }
  }

  void ScriptMachine::HeadSign(bool value, bool selfObjectManagement) {
    if (cycleNestStack.empty()) {
      modeStack.push(currentMode);
      if (!selfObjectManagement) entry::CreateManager();
    }
    else {
      if (cycleNestStack.top() != current - 1) {
        modeStack.push(currentMode);
        if (!selfObjectManagement) entry::CreateManager();
      }
    }
    if (value == true) {
      currentMode = kModeCycle;
      if (cycleNestStack.empty()) {
        cycleNestStack.push(current - 1);
      }
      else if (cycleNestStack.top() != current - 1) {
        cycleNestStack.push(current - 1);
      }
    }
    else if (value == false) {
      currentMode = kModeCycleJump;
      if (!cycleTailStack.empty()) {
        current = cycleTailStack.top();
      }
    }
    else {
      health = false;
    }
  }

  void ScriptMachine::TailSign() {
    if (currentMode == kModeCondition || currentMode == kModeNextCondition) {
      conditionStack.pop();
      currentMode = modeStack.top();
      modeStack.pop();
      entry::DisposeManager();
    }
    if (currentMode == kModeCycle || currentMode == kModeCycleJump) {
      switch (currentMode) {
      case kModeCycle:
        if (cycleTailStack.empty() || cycleTailStack.top() != current - 1) {
          cycleTailStack.push(current - 1);
        }
        current = cycleNestStack.top();
        entry::GetCurrentManager().clear();
        break;
      case kModeCycleJump:
        currentMode = modeStack.top();
        modeStack.pop();
        cycleNestStack.pop();
        cycleTailStack.pop();
        entry::DisposeManager();
        break;
      default:break;
      }
    }
  }

  Message ScriptMachine::Run() {
    Message result;

    currentMode = kModeNormal;
    nestHeadCount = 0;
    current = 0;
    health = true;

    if (storage.empty()) return result;

    entry::CreateManager();

    //Main state machine
    while (current < storage.size()) {
      if (!health) break;

      result = storage[current].Start(currentMode);
      const auto value = result.GetValue();
      const auto code  = result.GetCode();
      const auto selfObjectManagement = storage[current].IsSelfObjectManagement();

      if (value == kStrFatalError) {
        trace::Log(result.SetIndex(storage[current].GetIndex()));
        break;
      }

      if (value == kStrWarning) {
        trace::Log(result.SetIndex(storage[current].GetIndex()));
      }
      //TODO:return

      switch (code) {
      case kCodeConditionRoot:
        ConditionRoot(GetBooleanValue(value)); 
        break;
      case kCodeConditionBranch:
        if (nestHeadCount > 0) break;
        ConditionBranch(GetBooleanValue(value));
        break;
      case kCodeConditionLeaf:
        if (nestHeadCount > 0) break;
        ConditionLeaf();
        break;
      case kCodeHeadSign:
        HeadSign(GetBooleanValue(value), selfObjectManagement);
        break;
      case kCodeFillingSign:
        nestHeadCount++;
        break;
      case kCodeTailSign:
        if (nestHeadCount > 0) {
          nestHeadCount--;
          break;
        }
        TailSign();
        break;
      default:break;
      }
      ++current;
    }

    entry::DisposeManager();

    return result;
  }

  void ScriptMachine::Terminal() {
    Message msg;
    string buf;
    string head = kStrNormalArrow;
    Processor processor;
    bool subProcess = false;

    entry::CreateManager();

    while (msg.GetCode() != kCodeQuit) {
      std::cout << head;
      std::getline(std::cin, buf);
      processor.Build(buf);
      auto tokenValue = entry::GetGenericToken(processor.GetFirstToken().first);
      switch (tokenValue) {
      case BasicGenToken::IF:
      case BasicGenToken::WHILE:
      case BasicGenToken::FOR:
        subProcess = true;
        head = kStrDotGroup;
        break;
      case BasicGenToken::END:
        subProcess = false;
        storage.emplace_back(processor);
        head = kStrNormalArrow;
        msg = this->Run();
        storage.clear();
        current = 0;
        break;
      case BasicGenToken::DEF:
        //
        break;
      default:
        break;
      }
      if (subProcess) {
        storage.emplace_back(processor);
      }
      else {
        msg = processor.Start();
      }

      if (msg.GetCode() < kCodeSuccess) {
        std::cout << msg.GetDetail() << std::endl;
        trace::Log(msg);
      }
    }

    entry::DisposeManager();
  }

  Processor &Processor::Build(string target) {
    Kit kit;
    string current, data, forward, next;
    auto exemptBlankChar = true;
    auto stringProcessing = false;
    auto appendingOnce = false;
    char currentChar;
    auto forwardChar = ' ';
    vector<string> origin, output;
    size_t head = 0, tail = 0, nest = 0;
    auto toString = [](char t) ->string {return string().append(1, t); };
    health = true;

    //PreProcessing
    this->origin.clear();
    item.clear();
    symbol.clear();
    for (size_t count = 0; count < target.size(); ++count) {
      currentChar = target[count];
      if (kit.GetDataType(toString(currentChar)) != kTypeBlank
        && exemptBlankChar) {
        head = count;
        exemptBlankChar = false;
      }
      if (currentChar == '\'' && forwardChar != '\\') stringProcessing = !stringProcessing;
      if (!stringProcessing && currentChar == '#') {
        tail = count;
        break;
      }
      forwardChar = target[count];
    }

    if (tail > head) data = target.substr(head, tail - head);
    else data = target.substr(head, target.size() - head);

    while (kit.GetDataType(toString(data.back())) == kTypeBlank) data.pop_back();
    
    //Spilt
    forwardChar = 0;
    for (size_t count = 0; count < data.size(); ++count) {
      currentChar = data[count];
      if (currentChar == '\'' && forwardChar != '\\') {
        if (kit.GetDataType(current) == kTypeBlank) {
          current.clear();
        }
        stringProcessing = !stringProcessing;
      }
      current.append(1, currentChar);
      if (kit.GetDataType(current) != kTypeNull) {
        forwardChar = data[count];
        continue;
      }
      else {
        current = current.substr(0, current.size() - 1);
        if (kit.GetDataType(current) == kTypeBlank) {
          if (stringProcessing) origin.emplace_back(current);
          current.clear();
          current.append(1, currentChar);
        }
        else {
          origin.emplace_back(current);
          current.clear();
          current.append(1, currentChar);
        }
      }
      forwardChar = data[count];
    }

    const auto type = kit.GetDataType(current);
    if (type != kTypeNull && type != kTypeBlank) origin.emplace_back(current);
    current.clear();

    //third cycle
    stringProcessing = false;
    for (size_t count = 0; count < origin.size(); ++count) {
      if (count + 1 < origin.size()) next = origin[count + 1];

      current = origin[count];
      if (!stringProcessing) {
        if (current == ".") {
          if (kit.IsInteger(forward) && kit.IsInteger(next)) {
            output.back().append(current);
            appendingOnce = true;
            continue;
          }
        }
        if (current == "(" || current == "[") nest++;
        if (current == ")" || current == "]") nest--;
        if (current == "[") {
          if (kit.GetDataType(forward) != kGenericToken && forward != "]" && forward != ")") {
            errorString = "Illegal subscript operation.";
            nest = 0;
            health = false;
            break;
          }
        }
        if (current == ",") {
          if (kit.GetDataType(forward) == kTypeSymbol &&
            forward != "]" && forward != ")" &&
            forward != "++" && forward != "--" &&
            forward != "'") {
            errorString = "Illegal comma location.";
            health = false;
            break;
          }
        }
      }
      if (current == "'" && forward != "\\") {
        if (stringProcessing) {
          output.back().append(current);
        }
        else {
          output.emplace_back(kStrEmpty);
          output.back().append(current);
        }
        stringProcessing = !stringProcessing;
      }
      else {
        if (stringProcessing) {
          output.back().append(current);
        }
        else if (appendingOnce) {
          output.back().append(current);
          appendingOnce = false;
        }
        else {
          if ((current == "+" || current == "-") && !output.empty()) {
            if (output.back() == current) {
              output.back().append(current);
            }
            else {
              output.emplace_back(current);
            }
          }
          else if (current == "=") {
            if (output.back() == "<" || output.back() == ">" || output.back() == "=" || output.back() == "!") {
              output.back().append(current);
            }
            else {
              output.emplace_back(current);
            }
          }
          else {
            output.emplace_back(current);
          }
        }
      }

      forward = origin[count];
    }

    if (stringProcessing == true) {
      errorString = "Quotation mark is missing.";
      health = false;
    }
    if (nest > 0) {
      errorString = "Bracket/Square Bracket is missing";
      health = false;
    }

    if (health) {
      origin.reserve(output.size());
      for (auto &unit : output) {
        Token token;
        token.first = unit;
        token.second = kit.GetDataType(unit);
        this->origin.push_back(token);
      }
    }

    kit.CleanupVector(origin).CleanupVector(output);

    return *this;
  }

  int Processor::GetPriority(const string target) {
    int priority;
    if (target == "=" || target == "var") {
      priority = 0;
    }
    else if (target == "==" || target == ">=" || target == "<="
      || target == "!=" || target == "<" || target == ">") {
      priority = 1;
    }
    else if (target == "+" || target == "-") {
      priority = 2;
    }
    else if (target == "*" || target == "/" || target == "\\") {
      priority = 3;
    }
    else {
      priority = 4;
    }
    return priority;
  }

  Object *Processor::GetObj(string name) {
    Object *result = nullptr;
    if (name.substr(0, 2) == "__") {
      const auto it = lambdamap.find(name);
      if (it != lambdamap.end()) result = &(it->second);
    }
    else {
      const auto ptr = entry::FindObject(name);
      if (ptr != nullptr) result = ptr;
    }
    return result;
  }

  vector<string> Processor::Spilt(string target) {
    string temp;
    auto start = false;
    for (auto &unit : target) {
      if (unit == ':') {
        start = true;
        continue;
      }
      if (start) {
        temp.append(1, unit);
      }
    }
    auto result = Kit().BuildStringVector(temp);
    return result;
  }

  string Processor::GetHead(string target) {
    string result;
    for (auto &unit : target) {
      if (unit == ':') break;
      else result.append(1, unit);
    }
    return result;
  }

  bool Processor::Assemble(Message &msg) {
    Kit           kit;
    Object        temp, *origin;
    size_t        count;
    deque<Token>  tokens;
    ObjectMap     map;
    auto          providerSize = -1;
    auto          health       = true;
    auto          providerType = kTypeIdNull;
    auto          tags         = Spilt(symbol.back().first);
    const auto    id           = GetHead(symbol.back().first);
    EntryProvider provider;

    auto getName = [](string target) ->string {
      if (target.front() == '&' || target.front() == '%') {
        return target.substr(1, target.size() - 1);
      }
      else return target;
    };

    if (!tags.empty()) {
      providerType = tags.front();
      tags.size() > 1 ? providerSize = stoi(tags[1]) : providerSize = -1;
    }

    if (id != kStrNop) {
      provider = entry::Order(id, providerType, providerSize);
      if (!provider.Good()) {
        msg.combo(kStrFatalError, kCodeIllegalCall, "Activity not found - " + id);
        symbol.pop_back();
        return false;
      }
    }
    
    const auto size     = provider.GetParameterSIze();
    const auto parmMode = provider.GetArgumentMode();
    const auto priority = provider.GetPriority();
    auto args           = provider.GetArguments();

    if (id == kStrNop) {
      auto res = item.back();
      while (!item.empty() && item.back().first != "(") {
        item.pop_back();
      }
      if (!item.empty() && item.back().first == "(") item.pop_back();
      item.push_back(tokens.back());
      symbol.pop_back();
      kit.CleanupDeque(tokens);
      return true;
    }

    if (!disableSetEntry) {
      count = size;
      while (count > 0 && !item.empty() && item.back().first != "(") {
        tokens.push_front(item.back());
        item.pop_back();
        count--;
      }
      if (!item.empty() && item.back().first == "(") item.pop_back();
    }
    else {
      while (item.back().first != "," && !item.empty()) {
        tokens.push_front(item.back());
        item.pop_back();
      }
    }

    if (parmMode == kCodeNormalParm && (tokens.size() < size || tokens.size() > size)) {
      msg.combo(kStrFatalError, kCodeIllegalParm, "Parameter doesn't match expected count.(01)");
      health = false;
    }
    else if (parmMode == kCodeAutoFill && tokens.size() < 1) {
      msg.combo(kStrFatalError, kCodeIllegalParm, "You should provide at least one parameter.(02)");
      health = false;
    }
    else {
      for (count = 0; count < size; count++) {
        if (tokens.size() - 1 < count) {
          map.insert(Parameter(getName(args[count]), Object()));
          continue;
        }
        
        switch (tokens[count].second) {
        case kGenericToken:
          if (args[count].front() == '&') {
            origin = GetObj(tokens[count].first);
            temp.Ref(*origin);
          }
          else if (args[count].front() == '%') {
            temp.Manage(tokens[count].first)
                .SetMethods(type::GetTemplate(kTypeIdRawString)->GetMethods())
                .SetTokenType(kGenericToken);
          }
          else {
            auto ptr = GetObj(tokens[count].first);
            if (ptr == nullptr) {
              msg.combo(kStrFatalError, kCodeIllegalCall, "Object not found - " + tokens[count].first);
              health = false;
            }
            else {
              temp = *GetObj(tokens[count].first);
            }
          }
          break;
        default:
          temp.Manage(tokens[count].first)
              .SetMethods(type::GetTemplate(kTypeIdRawString)->GetMethods())
              .SetTokenType(tokens[count].second);
          break;
        }
        map.insert(pair<string,Object>(getName(args[count]), temp));
        temp.Clear();
        
      }

      if (id == kStrFor) {
        const auto sub = to_string(this->index);
        map.insert(Parameter(kStrCodeSub,Object()
           .Manage(sub)
           .SetMethods(type::GetTemplate(kTypeIdRawString)->GetMethods())
           .SetTokenType(kTypeInteger)));
      }

      switch (priority) {
      case kFlagOperatorEntry:
        map.insert(Parameter(kStrOperator, Object()
           .Manage(symbol.back().first)
           .SetMethods(type::GetTemplate(kTypeIdRawString)->GetMethods())
           .SetTokenType(kTypeSymbol)));
        break;
      case kFlagMethod:
        origin = GetObj(item.back().first);
        map.insert(Parameter(kStrObject, Object()
           .Ref(*origin)));
        item.pop_back();
        break;
      default:
        break;
      }
    }
    
    if (health) {
      switch (mode) {
      case kModeCycleJump:
        if (id == kStrEnd) msg = provider.Start(map);
        else if (symbol.front().first == kStrIf || symbol.front().first == kStrWhile) {
          msg.combo(kStrRedirect, kCodeFillingSign, kStrTrue);
        }
        break;
      case kModeNextCondition:
        if (symbol.front().first == kStrIf || symbol.front().first == kStrWhile) {
          msg.combo(kStrRedirect, kCodeFillingSign, kStrTrue);
        }
        else if (id == kStrElse || id == kStrEnd) msg = provider.Start(map);
        else if (symbol.front().first == kStrElif) msg = provider.Start(map);
        else msg.combo(kStrEmpty, kCodeSuccess, kStrEmpty);
        break;
      case kModeNormal:
      case kModeCondition:
      default:
        msg = provider.Start(map);
        break;
      }

      const auto code   = msg.GetCode();
      const auto value  = msg.GetValue();
      const auto detail = msg.GetDetail();

      if (code == kCodeObject) {
        temp = msg.GetObj();
        auto tempId = detail + to_string(lambdaObjectCount); //detail start with "__"
        item.emplace_back(Token(tempId, kGenericToken));
        lambdamap.insert(Parameter(tempId, temp));
        ++lambdaObjectCount;
      }
      else if (value == kStrRedirect && (code == kCodeSuccess || code == kCodeFillingSign)) {
        item.emplace_back(Token(detail, kit.GetDataType(detail)));
      }

      health = (value != kStrFatalError && value != kStrWarning);
      symbol.pop_back();
    }
    return health;
  }

  void Processor::EqualMark() {
    switch (symbol.empty()) {
    case true:
      symbol.emplace_back(currentToken); 
      break;
    case false:
      switch (defineLine) {
      case true: defineLine = false; break;
      case false:symbol.emplace_back(currentToken); break;
      }
      break;
    }
  }

  void Processor::Comma() {
    if (symbol.back().first == kStrVar) {
      disableSetEntry = true;
    }
    if (disableSetEntry) {
      symbol.emplace_back(Token(kStrVar,kGenericToken));
      item.emplace_back(currentToken);
    }
  }

  bool Processor::LeftBracket(Message &msg) {
    auto result = true;
    if (forwardToken.first == kStrVar) {
      msg.combo(kStrFatalError, kCodeIllegalCall, "Illegal pattern of definition.");
      result = false;
    }
    else {
      if(forwardToken.second != kGenericToken) {
        symbol.emplace_back(Token(kStrNop,kGenericToken));
      }
      symbol.emplace_back(currentToken);
      item.emplace_back(currentToken);
    }
    return result;
  }

  bool Processor::RightBracket(Message &msg) {
    auto result = true;
    while (symbol.back().first != "(" && !symbol.empty()) {
      result = Assemble(msg);
      if (!result) break;
    }

    if (result) {
      if (symbol.back().first == "(") symbol.pop_back();
      result = Assemble(msg);
    }

    return result;
  }

  void Processor::LeftSquareBracket() {
    if (item.back().first.substr(0, 2) == "__") {
      operatorTargetType = lambdamap.find(item.back().first)->second.GetTypeId();
    }
    else {
      operatorTargetType = entry::FindObject(item.back().first)->GetTypeId();
    }
    item.emplace_back(currentToken);
    symbol.emplace_back(currentToken);
    subscriptProcessing = true;
  }

  bool Processor::RightSquareBracket(Message &msg) {
    bool result;
    deque<Token> container;
    if (!subscriptProcessing) {
      //msg.combo
      result = false;
    }
    else {
      subscriptProcessing = false;
      while (symbol.back().first != "[" && !symbol.empty()) {
        result = Assemble(msg);
        if (!result) break;
      }
      if (symbol.back().first == "[") symbol.pop_back();
      while (item.back().first != "[" && !item.empty()) {
        container.emplace_back(item.back());
        item.pop_back();
      }
      if (item.back().first == "[") item.pop_back();
      if (!container.empty()) {
        switch (container.size()) {
        case 1:symbol.emplace_back(Token("at:" + operatorTargetType + "|1",kGenericToken)); break;
        case 2:symbol.emplace_back(Token("at:" + operatorTargetType + "|2", kGenericToken)); break;
        default:break;
        }

        while (!container.empty()) {
          item.emplace_back(container.back());
          container.pop_back();
        }

        result = Assemble(msg);
      }
      else {
        //msg.combo
        result = false;
      }
    }

    Kit().CleanupDeque(container);
    return result;
  }

  void Processor::OtherSymbols() {
    if (symbol.empty()) symbol.emplace_back(currentToken);
    else if (GetPriority(currentToken.first) < GetPriority(symbol.back().first) && 
      symbol.back().first != "(" && symbol.back().first != "[") {
      auto j = symbol.size() - 1;
      auto k = item.size();
      while (symbol[j].first != "(" && symbol.back().first != "[" && 
        GetPriority(currentToken.first) < GetPriority(symbol[j].first)) {
        k == item.size()?
          k -= entry::GetRequiredCount(symbol[j].first):
          k -= entry::GetRequiredCount(symbol[j].first) - 1;
        --j;
      }
      symbol.insert(symbol.begin() + j + 1, currentToken);
      nextInsertSubscript = k;
      insertBtnSymbols = true;
    }
    else {
      symbol.emplace_back(currentToken);
    }
  }

  bool Processor::FunctionAndObject(Message &msg) {
    Kit kit;
    auto result = true, function = false;

    if (currentToken.first == kStrVar) defineLine = true;
    if (currentToken.first == kStrDef) functionLine = true;

    if (dotOperator) {
      const auto id = entry::GetTypeId(item.back().first);
      if (kit.FindInStringGroup(currentToken.first, type::GetTemplate(id)->GetMethods())) {
        auto provider = entry::Order(currentToken.first, id);
        if (provider.Good()) {
          symbol.emplace_back(Token(currentToken.first + ':' + id, kGenericToken));
          function = true;
        }
        else {
          //TODO:???
        }
        
        dotOperator = false;
      }
      else {
        msg.combo(kStrFatalError, kCodeIllegalCall, "No such method/member in " + id + ".");
        result = false;
      }
    }
    else {
      switch (functionLine) {
      case true:
        item.emplace_back(currentToken);
        break;
      case false:
        if (entry::Order(currentToken.first).Good()) {
          symbol.emplace_back(currentToken); 
          function = true;
        }
        else {
          if (nextToken.first == "(") {
            symbol.emplace_back(currentToken);
          }
          else {
            item.emplace_back(currentToken);
          }
        }
        break;
      }
    }

    if (!defineLine && function 
      && nextToken.first    != "(" 
      && currentToken.first != kStrElse
      && currentToken.first != kStrEnd) {
      errorString = "Bracket after function is missing.";
      this->health = false;
      result = false;
    }
    if (functionLine 
      && forwardToken.first == kStrDef 
      && nextToken.first != "(" 
      && currentToken.first != kStrEnd) {
      errorString = "Illegal declaration of function.";
      this->health = false;
      result = false;
    }

    return result;
  }

  void Processor::OtherTokens() {
    switch (insertBtnSymbols) {
    case true:
      item.insert(item.begin() + nextInsertSubscript, currentToken);
      insertBtnSymbols = false;
      break;
    case false:
      item.emplace_back(currentToken);
      break;
    }
  }

  void Processor::FinalProcessing(Message &msg) {
    if (symbol.empty() && !item.empty()) {
      if (item.back().second == kGenericToken) {
        msg.combo(kStrFatalError, kCodeIllegalCall, "Unrecognized token.");
      }
    }
    while (symbol.empty() != true) {
      if (symbol.back().first == "(" || symbol.back().first == ")") {
        msg.combo(kStrFatalError, kCodeIllegalSymbol, "Another bracket is expected.");
        break;
      }
      Assemble(msg);
    }
  }

  bool Processor::SelfOperator(Message &msg) {
    using entry::OperatorCode;
    auto OPValue = entry::GetOperatorCode(currentToken.first);
    if (forwardToken.second != kGenericToken) {
      switch (OPValue) {
      case OperatorCode::SELFINC:symbol.emplace_back(kStrLeftSelfInc, kGenericToken); break;
      case OperatorCode::SELFDEC:symbol.emplace_back(kStrLeftSelfDec, kGenericToken); break;
      default:break;
      }
    }
    else {
      switch (OPValue) {
      case OperatorCode::SELFINC:symbol.emplace_back(kStrRightSelfInc, kGenericToken); break;
      case OperatorCode::SELFDEC:symbol.emplace_back(kStrRightSelfDec, kGenericToken); break;
      }
    }
    return true;
  }

  bool Processor::Colon() {
    if (symbol.front().first != kStrFor) {
      errorString = "Illegal colon location.";
      return false;
    }
    return true;
  }

  Message Processor::Start(size_t mode) {
    using namespace entry;
    Kit kit;
    Message result;
    auto state = true;
    const auto size = origin.size();

    if (health == false) {
      return Message(kStrFatalError, kCodeBadExpression, errorString);
    }

    this->mode          = mode;
    lambdaObjectCount   = 0;
    nextInsertSubscript = 0;
    operatorTargetType  = kTypeIdNull;
    commaExpFunc        = false,
    insertBtnSymbols    = false,
    disableSetEntry     = false,
    dotOperator         = false,
    subscriptProcessing = false;
    functionLine        = false;
    defineLine          = false;

    for (size_t i = 0; i < size; ++i) {
      if (!health) break;
      if (!state)  break;

      currentToken = origin[i];
      i < size - 1 ? nextToken = origin[i + 1] : nextToken = Token(kStrNull, kTypeNull);

      result.combo(kStrEmpty, kCodeSuccess, kStrEmpty);
      const auto tokenType  = currentToken.second;
      const auto tokenValue = currentToken.first;
      if (tokenType == kTypeSymbol) {
        BasicTokenValue value = GetBasicToken(tokenValue);
        switch (value) {
        case TOKEN_EQUAL:           EqualMark(); break;
        case TOKEN_COMMA:           Comma(); break;
        case TOKEN_LEFT_SQRBRACKET: LeftSquareBracket(); break;
        case TOKEN_DOT:             dotOperator = true; break;
        case TOKEN_COLON:           state = Colon(); break;
        case TOKEN_LEFT_BRACKET:    state = LeftBracket(result); break;
        case TOKEN_RIGHT_SQRBRACKET:state = RightSquareBracket(result); break;
        case TOKEN_RIGHT_BRACKET:   state = RightBracket(result); break;
        case TOKEN_SELFOP:          state = SelfOperator(result); break;
        case TOKEN_OTHERS:          OtherSymbols(); break;
        default:break;
        }
      }
      else if (tokenType == kGenericToken) state = FunctionAndObject(result);
      else if (tokenType == kTypeNull) {
        result.combo(kStrFatalError, kCodeIllegalParm, "Illegal token.");
        state = false;
      }
      else OtherTokens();
      forwardToken = currentToken;
    }

    if (state == true) FinalProcessing(result);
    if (!health) result.combo(kStrFatalError, kCodeBadExpression, errorString);
    
    kit.CleanupDeque(item).CleanupDeque(symbol);
    nextToken = Token();
    forwardToken = Token();
    currentToken = Token();

    lambdamap.clear();
    return result;
  }

  Message EntryProvider::Start(ObjectMap &map) const {
    Message result;
    switch (this->Good()) {
    case true: result = activity(map); break;
    case false:result.combo(kStrFatalError, kCodeIllegalCall, "Illegal entry."); break;
    }
    return result;
  }
}

