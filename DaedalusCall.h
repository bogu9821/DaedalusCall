#include <expected>
#include <unordered_map>
#include <optional>
#include <vector>

#define DCLikely [[likely]]
#define DCUnlikely [[unlikely]]

namespace GOTHIC_ENGINE
{
	enum class eCallFuncError
	{
		NONE,
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

	struct DaedalusFunction
	{
		explicit DaedalusFunction(const int t_index)
			: m_index(t_index)
		{
		};

		zCPar_Symbol* GetSymbol(zCParser* const t_parser) const
		{
			auto const symb = t_parser->GetSymbol(m_index);

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
			m_symbol = parser->GetSymbol(t_function.m_index);
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
				auto const argumentSymbol = m_parser->GetSymbol(t_index);

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
				ReturnScriptValue<int>();
				break;
			case zPAR_TYPE_STRING:
				m_parser->PopString();
				break;
			case zPAR_TYPE_FLOAT:
				ReturnScriptValue<float>();
				break;
			case zPAR_TYPE_INSTANCE:
				ReturnScriptValue<void*>();
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
		inline eCallFuncError CheckDaedalusCallError()
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
				error != eCallFuncError::NONE)
			{
				return std::unexpected{ error };
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
		auto& cache = CallFuncStringCache::Get(t_par);

		auto index = cache.FindCache(t_name).value_or(DaedalusFunction{ -1 });

		if (index == DaedalusFunction{ -1 })
		{
			index = DaedalusFunction{ t_par->GetIndex(t_name.data()) };

			if (const auto error = CallFuncContext{ t_par,index }.CheckDaedalusCallError<T, std::decay_t<decltype(t_args)>...>();
				error != eCallFuncError::NONE)
			{
				return std::unexpected{ error };
			}

			cache.Add(std::string{ t_name }, index);

		}

		return DaedalusCall<T, false>(t_par, index, t_clearStack, std::move(t_args)...);
	}

}

#undef DCLikely 
#undef DCUnlikely