#pragma once
#include "Types.h"

#pragma region TypeList

template<typename... T>
struct TypeList;


template<typename T, typename  U>
struct TypeList<T, U>
{
	using Head = T;
	using Tail = U;
};


template <typename T, typename... U>
struct TypeList<T, U...>
{
	using Head = T;
	using Tail = TypeList<U...>;
};

#pragma endregion

#pragma region Length
template<typename T>

struct Length;

template<>
struct Length<TypeList<>>
{
	enum { value = 0 };
};

template<typename T, typename... U>
struct Length<TypeList<T, U...>>
{
	enum { value = 1 + Length<TypeList<U...>>::value };
};

#pragma endregion

#pragma region TypeAt
template<typename TL, int32 index>
struct TypeAt;

template<typename Head, typename... Tail>
struct TypeAt<TypeList<Head, Tail...>, 0>
{
	using Result = Head;
};

template <typename Head, typename... Tail, int32 index>
struct TypeAt<TypeList<Head, Tail...>, index>
{
	using Result = typename TypeAt<TypeList<Tail...>, index - 1>::Result;
};

#pragma endregion

#pragma region IndexOf
template<typename TL, typename T>
struct IndexOf;

template<typename Tail, typename T>
struct IndexOf<TypeList<T, Tail>, T>
{
	enum { value = 0 };
};

template<typename T>
struct IndexOf<TypeList<>, T>
{
	enum { value = -1 };
};

template <typename Head, typename... Tail, typename T>
struct IndexOf<TypeList<Head, Tail...>, T>
{
private:
	enum { temp = IndexOf<TypeList<Tail...>, T>::value };
public:
	enum { value = (temp == -1 ? -1 : 1 + temp) };
};
#pragma endregion

#pragma region Conversion
template<typename From, typename To>
class Conversion
{
private:
	using Small = __int8;
	using Big = __int32;
	static Small Test(const To&) { return 0; }
	static Big Test(...) { return 0; }
	static From MakeFrom() { return 0; }
public:
	enum
	{
		exists = sizeof(Test(MakeFrom())) == sizeof(Small)
	};
};


#pragma endregion

#pragma region TypeCast
template<typename TL>
class TypeConversion
{
public:
	enum
	{
		length = Length<TL>::value
	};

	TypeConversion()
	{
		for (int i = 0; i < length; ++i)
		{
			for (int j = 0; j < length; ++j)
			{
				using FromType = typename TypeAt<TL, i>::Result;
				using ToType = typename TypeAt<TL, j>::Result;
				if (Conversion<const FromType*, const ToType*>::exists)
					s_convert[i][j] = true;
				else
					s_convert[i][j] = false;
			}
		}
	}

public:
	static bool s_convert[length][length];
};

template<typename TL>
bool TypeConversion<TL>::s_convert[length][length];

#pragma endregion