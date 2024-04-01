#include <expected>
#include <unordered_map>
#include <optional>
#include <vector>
#include <ranges>
#include <algorithm>
#include <array>

#define DCLikely [[likely]]
#define DCUnlikely [[unlikely]]

namespace GOTHIC_ENGINE
{
	enum class eCallFuncError
	{
		WRONG_SYMBOL,
		WRONG_ARGS_SIZE,
		WRONG_ARG_TYPE,
		WRONG_RET_VAL
	};

	enum class eClearStack
	{
		CLEAR,
		NO
	};

	struct DaedalusVoid {};
	struct IgnoreReturn {};

	//use Zengin way to get upper string
	inline constexpr std::string StrViewToUpperZengin(const std::string_view t_name)
	{
		//same array is used in zSTRING::ToUpper
		constexpr std::array<unsigned char, 256> ToUpperArray =
		{
			 0,1,2,3,4,5,6,7,8,9,
			 10,11,12,13,14,15,16,17,18,19,
			 20,21,22,23,24,25,26,27,28,29,
			 30,31,32,33,34,35,36,37,38,39,
			 40,41,42,43,44,45,46,47,48,49,
			 50,51,52,53,54,55,56,57,58,59,
			 60, 61, 62, 63, 64,'A','B','C','D','E',
			 'F','G','H','I','J','K','L','M','N','O',
			 'P','Q','R','S','T','U','V','W','X','Y',
			 'Z', 91, 92, 93, 94, 95, 96,'A','B','C',
			 'D','E','F','G','H','I','J','K','L','M',
			 'N','O','P','Q','R','S','T','U','V','W',
			 'X','Y','Z',123,124,125,126,127,128,154,
			 144,182,142,183,143,128,210,211,212,216,
			 215,221,142,143,144,146,226,153,227,234,
			 235,190,152,153,154,157,156,157,158,159,
			 181,214,224,233,165,165,166,167,168,169,
			 170,171,172,173,174,175,176,177,178,179,
			 180,181,182,183,184,185,186,187,188,189,
			 190,191,192,193,194,195,196,197,198,198,
			 200,201,202,203,204,205,206,207,208,209,
			 210,211,212,213,214,215,216,217,218,219,
			 220,221,222,223,224,225,226,227,229,229,
			 230,232,232,233,208,235,237,237,238,239,
			 240,241,242,243,244,245,246,247,248,249,
			 250,251,252,253,254,255
		};

		constexpr auto CharToUpper = [](const char t_char)
			{
				return ToUpperArray[static_cast<unsigned char>(t_char)];
			};

		return t_name | std::views::transform(CharToUpper) | std::ranges::to<std::string>();
	}

	template<bool ToUpper = true>
	inline constexpr int ParserGetIndex(zCParser* const t_parser, std::string_view t_name)
	{	
		//TODO use better way
		[[maybe_unused]]
		const auto upperName = ToUpper ? StrViewToUpperZengin(t_name) : std::string{};

		if constexpr (ToUpper)
		{
			t_name = std::string_view{ upperName };
		}

		const auto& symbTab = t_parser->symtab.table;

		for (int i = 0; i < symbTab.GetNum(); i++)
		{
			if (std::string_view(symbTab[i]->name, symbTab[i]->name.Length()) == t_name)
			{
				return i;
			}
		}

		return -1;
	}

	inline constexpr zCPar_Symbol* ParserGetSymbol(zCParser* const t_parser, const int t_index)
	{
		if (t_index < 0 || t_index >= t_parser->symtab.table.GetNum())
		{
			return {};
		}

		return t_parser->symtab.table[t_index];
	}

	inline constexpr zCPar_Symbol* ParserGetSymbol(zCParser* const t_parser, const std::string_view t_name)
	{
		const auto index = ParserGetIndex(t_parser, t_name);

		if (index == -1)
		{
			return {};
		}

		return t_parser->symtab.table[index];
	}


	struct DaedalusFunction
	{
		explicit constexpr DaedalusFunction(const int t_index)
			: m_index(t_index)
		{
		};

		zCPar_Symbol* GetSymbol(zCParser* const t_parser) const
		{
			auto const symb = ParserGetSymbol(t_parser, m_index);

			if (!symb)
			{
				return nullptr;
			}

			if (symb->type != zPAR_TYPE_FUNC)
				//|| !(symb->flags & zPAR_FLAG_CONST))
			{
				return nullptr;
			}

			return symb;
		}

		constexpr std::strong_ordering operator<=>(const DaedalusFunction& t_function) const = default;


		int m_index{ -1 };
	};

	template<typename T>
	concept DaedalusData =
		std::is_same_v<std::decay_t<T>, int>
		|| std::is_same_v<std::decay_t<T>, DaedalusFunction>
		|| std::is_same_v<std::decay_t<T>, float>
		|| std::is_same_v<std::decay_t<T>, zSTRING>
		|| (std::is_pointer_v<std::decay_t<T>> && !std::is_pointer_v<std::remove_pointer_t<T>>);

	template<typename T>
	concept DaedalusReturn =
		std::is_same_v<T, int>
		|| std::is_same_v<T, DaedalusFunction>
		|| std::is_same_v<T, float>
		|| std::is_same_v<T, zSTRING>
		|| std::is_same_v<T, DaedalusVoid>
		|| std::is_same_v<T, IgnoreReturn>
		|| (std::is_pointer_v<std::decay_t<T>> && !std::is_pointer_v<std::remove_pointer_t<T>>);

	struct CallFuncContext
	{
		CallFuncContext(zCParser* const t_par, const DaedalusFunction t_function)
			: m_parser(t_par),
			m_function(t_function)
		{
			m_symbol = ParserGetSymbol(m_parser, t_function.m_index);
		}

		template<DaedalusReturn T>
		static inline constexpr int TypeToEnum()
		{
			if constexpr (std::is_same_v<T, int>)
			{
				return zPAR_TYPE_INT;
			}
			else if constexpr (std::is_same_v<T, DaedalusFunction>)
			{
				return zPAR_TYPE_FUNC;
			}
			else if constexpr (std::is_same_v<T, zSTRING>)
			{
				return zPAR_TYPE_STRING;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				return zPAR_TYPE_FLOAT;
			}
			else if constexpr (std::is_pointer_v<T>)
			{
				return zPAR_TYPE_INSTANCE;
			}
			else if constexpr (std::is_same_v<T, DaedalusVoid>)
			{
				return zPAR_TYPE_VOID;
			}
		}

		template<DaedalusReturn T>
		static inline constexpr int ShouldReturn()
		{
			return !(std::is_same_v<T, DaedalusVoid> || std::is_same_v<T, IgnoreReturn>);
		}

		template<DaedalusData... Args>
		inline bool CheckAllTypes()
		{
			size_t counter{};
			bool valid{ true };
			(((!CheckType<Args>(counter++)
				? (valid = false, false) : true)
				&& ...));

			return valid;
		}

		inline void PushOne(DaedalusData auto&& t_argument, [[maybe_unused]] const size_t t_index)
		{
			using ArgType = std::decay_t<decltype(t_argument)>;

			if constexpr (std::is_pointer_v<ArgType>)
			{
				auto const argumentSymbol = ParserGetSymbol(m_parser, t_index);

				argumentSymbol->offset = reinterpret_cast<int>(t_argument);
				m_parser->datastack.Push(t_index);
			}
			else if constexpr (std::is_same_v<ArgType, int>)
			{
				m_parser->datastack.Push(t_argument);
				m_parser->datastack.Push(zPAR_TOK_PUSHINT);
			}
			else if constexpr (std::is_same_v<ArgType, DaedalusFunction>)
			{
				m_parser->datastack.Push(t_argument.m_index);
				m_parser->datastack.Push(zPAR_TOK_PUSHINT);
			}
			else if constexpr (std::is_same_v<ArgType, float>)
			{
				m_parser->datastack.Push(std::bit_cast<int>(t_argument));
				m_parser->datastack.Push(zPAR_TOK_PUSHINT);
			}
			else if constexpr (std::is_same_v<ArgType, zSTRING>)
			{
				m_parser->datastack.Push(reinterpret_cast<int>(&t_argument));
				m_parser->datastack.Push(zPAR_TOK_PUSHSTR);
			}
		}

		template<DaedalusReturn T>
		inline T ReturnScriptValue()
		{
			if constexpr (std::is_same_v<T, DaedalusVoid>)
			{
				return {};
			}
			else if constexpr (std::is_pointer_v<T>)
			{
				//TODO check index?
				return static_cast<T>(m_parser->GetInstance());
			}
			else if constexpr (std::is_same_v<T, zSTRING>)
			{
				return *m_parser->PopString();
			}
			else if constexpr (std::is_same_v<T, int>)
			{
				return m_parser->PopDataValue();
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				return m_parser->PopFloatValue();
			}
		}

		inline void PopReturnValue()
		{
			switch (m_symbol->offset)
			{
			case zPAR_TYPE_INT:
				(void)ReturnScriptValue<int>();
				break;
			case zPAR_TYPE_STRING:
				(void)ReturnScriptValue<zSTRING>();
				break;
			case zPAR_TYPE_FLOAT:
				(void)ReturnScriptValue<float>();
				break;
			case zPAR_TYPE_INSTANCE:
				(void)ReturnScriptValue<void*>();
				break;
			case zPAR_TYPE_VOID:
				break;
			default:
				break;
			}
		}

		template<DaedalusReturn T>
		inline bool CheckType(const size_t t_offset)
		{
			return m_parser->symtab.table[m_function.m_index + t_offset]->type == static_cast<unsigned int>(TypeToEnum<T>());
		}

		template<DaedalusReturn T, DaedalusData... Args>
		inline std::optional<eCallFuncError> CheckDaedalusCallError()
		{
			if (!m_symbol)
			{
				return eCallFuncError::WRONG_SYMBOL;
			}

			if (sizeof...(Args) != m_symbol->ele)
			{
				return eCallFuncError::WRONG_ARGS_SIZE;
			}

			const bool hasReturn = (m_symbol->flags & zPAR_FLAG_RETURN) != 0;

			if (!hasReturn)
			{
				if constexpr (ShouldReturn<T>())
				{
					return eCallFuncError::WRONG_RET_VAL;
				}
			}

			if constexpr (!std::is_same_v<T, IgnoreReturn>)
			{
				if (m_symbol->offset != TypeToEnum<std::decay_t<T>>())
				{
					return eCallFuncError::WRONG_RET_VAL;
				}
			}

			if constexpr (sizeof...(Args))
			{
				if (!CheckAllTypes<Args...>())
				{
					return eCallFuncError::WRONG_ARG_TYPE;
				}
			}

			return {};
		}

		zCParser* m_parser;
		zCPar_Symbol* m_symbol;
		DaedalusFunction m_function;
	};

	struct string_hash
	{
		using is_transparent = void;
		[[nodiscard]] size_t operator()(const char* txt) const
		{
			return std::hash<std::string_view>{}(txt);
		}
		[[nodiscard]] size_t operator()(std::string_view txt) const
		{
			return std::hash<std::string_view>{}(txt);
		}
		[[nodiscard]] size_t operator()(const std::string& txt) const
		{
			return std::hash<std::string>{}(txt);
		}
	};


	class CallFuncStringCache
	{

	public:

		CallFuncStringCache(const zCParser* t_parser)
			: m_parser(t_parser)
		{

		}

		__declspec(noinline) void Add(std::string t_functionName, const DaedalusFunction t_function)
		{
			m_cache.emplace(std::move(t_functionName), t_function);
		}

		[[nodiscard]] std::optional<DaedalusFunction> FindCache(const std::string_view t_name) const noexcept
		{
			if (const auto it = m_cache.find(t_name);
				it != std::end(m_cache))
			{
				return it->second;
			}

			return {};
		}

		[[nodiscard]] static CallFuncStringCache& Get(const zCParser* t_parser)
		{
			const auto SameKey = [t_parser](const auto& t_object)
				{
					return t_object.m_parser == t_parser;
				};

			auto& value = [&]() -> CallFuncStringCache&
				{
					if (const auto it = std::ranges::find_if(s_cache, SameKey);
						it != std::end(s_cache)) DCLikely
					{
						return *it;
					}
					else DCUnlikely
					{
						return s_cache.emplace_back(CallFuncStringCache{t_parser});
					}

				}();

			return value;
		}

	private:

		const zCParser* m_parser;
		std::unordered_map<std::string, DaedalusFunction, string_hash, std::equal_to<>> m_cache;

		inline static std::vector<CallFuncStringCache> s_cache;
	};

	//TODO do not copy zSTRINGS
	template<DaedalusReturn T = IgnoreReturn, bool SafeCall = true>
	std::expected<T, eCallFuncError> DaedalusCall(zCParser* const t_par, const DaedalusFunction t_function, const eClearStack t_clearStack, DaedalusData auto...  t_args)
	{
		CallFuncContext contex{ t_par,t_function };

		if constexpr (SafeCall)
		{
			if (const auto error = contex.CheckDaedalusCallError<T, std::decay_t<decltype(t_args)>...>();
				error.has_value())
			{
				return std::unexpected{ *error };
			}
		}

		if (t_clearStack == eClearStack::CLEAR)
		{
			t_par->datastack.Clear();
		}

		auto PushFunc = [&, counter = 0]() mutable
			{
				((contex.PushOne(std::move(t_args), t_function.m_index + counter++)), ...);
			};

		PushFunc();

		if (contex.m_symbol->flags & zPAR_FLAG_EXTERNAL) DCUnlikely
		{
			auto const Func = reinterpret_cast<int(*)()>(contex.m_symbol->single_intdata);
			Func();
		}
		else DCLikely
		{
			t_par->DoStack(contex.m_symbol->single_intdata);
		}

		if constexpr (std::is_same_v<T, IgnoreReturn>)
		{
			contex.PopReturnValue();
			return{};
		}
		else
		{
			return contex.ReturnScriptValue<T>();
		}
	}

	template<DaedalusReturn T = IgnoreReturn>
	std::expected<T, eCallFuncError> DaedalusCall(zCParser* const t_par, const std::string_view t_name, const eClearStack t_clearStack, DaedalusData auto...  t_args)
	{
		const std::string upper = StrViewToUpperZengin(t_name);
		
		auto& cache = CallFuncStringCache::Get(t_par);

		auto index = cache.FindCache(upper).value_or(DaedalusFunction{ -1 });
	
		if (index == DaedalusFunction{ -1 })
		{
			auto callError = [&]() -> std::optional<eCallFuncError>
			{
				index = DaedalusFunction{ ParserGetIndex<false>(t_par, upper) };

				if (const auto error = CallFuncContext{ t_par,index }.CheckDaedalusCallError<T, std::decay_t<decltype(t_args)>...>();
					error.has_value())
				{
					return error;
				}

				cache.Add(std::move(upper), index);

				return{};
			}();

			if (callError.has_value())
			{
				return std::unexpected{ *callError };
			}

		}

		return DaedalusCall<T, false>(t_par, index, t_clearStack, std::move(t_args)...);
	}

	template<DaedalusReturn T = IgnoreReturn, typename ZSTR = zSTRING>
		//hack for implicit zSTRING conversion
		requires(std::same_as<ZSTR, zSTRING>)
	std::expected<T, eCallFuncError> DaedalusCall(zCParser* const t_par, const ZSTR& t_name, const eClearStack t_clearStack, DaedalusData auto...  t_args)
	{
		return DaedalusCall<T>(t_par, std::string_view{ t_name.ToChar(), static_cast<size_t>(t_name.Length()) }, t_clearStack, std::move(t_args)...);
	}

}

#undef DCLikely 
#undef DCUnlikely