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