# DaedalusCall
Templated, compile time specialized universal way of calling daedalus functions from C++ with error checking.
Requires C++23 and is designed to work with [Gothic API](https://gitlab.com/union-framework/gothic-api).

# Example
```cpp
bool IsPlayerFakeBandit() noexcept(false)
{
  const auto playerIsFakeBanditCall = DaedalusCall<int>(parser, "C_PLAYERISFAKEBANDIT", {}/*eClearStack::CLEAR*/, GetFocus(), player);
  if (!playerIsFakeBanditCall.has_value())
  {
    throw std::runtime_error(std::format("CallFuncError: {}", std::to_underlying(f.error())));
  }

  return *playerIsFakeBanditCall;
}

void CallPrintFunction(const DaedalusFunction t_functionIndex, const zSTRING& t_string)
{
  (void)DaedalusCall(parser, t_functionIndex, eClearStack::NO, t_string);
}

CallPrintFunction(DaedalusFunction{parser->GetIndex("MyPrint")}, zSTRING{"Hello world!"});
```
