#pragma once
#include <typeinfo>
#include <string>

namespace ioc
{

    // Wrapping class for type inference for classes where RTTI type inference is difficult
	class IWrapper
	{
	public:
		virtual ~IWrapper() {}
	};

	template<typename T>
	class TypeWrapper : public IWrapper
	{

	};

	// Similar to std::type_index, but with a constexpr constructor and also storing the type size and alignment.
	// Also guaranteed to be aligned, to allow storing a TypeInfo and 1 bit together in the size of a void*.
	struct alignas(1) alignas(void*) TypeInfo {

		struct ConcreteTypeInfo {
			// These fields are allowed to have dummy values for abstract types.
			std::size_t type_size;
			std::size_t type_alignment;
			bool is_trivially_destructible;
		};

		TypeInfo(const std::type_info& info, ConcreteTypeInfo concrete_type_info)
			: info(&info), concrete_type_info(concrete_type_info) {}


		std::string name() const {
			if (info != nullptr)
				return std::string(info->name());
			else
				return "<unknown> (type name not accessible because RTTI is disabled)";
		};

		size_t size() const { return concrete_type_info.type_size; };

		size_t alignment() const { return concrete_type_info.type_size; };

		bool isTriviallyDestructible() const { return concrete_type_info.is_trivially_destructible; };

	private:
		// The std::type_info struct associated with the type, or nullptr if RTTI is disabled.
		// This is only used for the type name.
		const std::type_info* info;
		ConcreteTypeInfo concrete_type_info;
	};

	struct TypeId {
		const TypeInfo* type_info;

		explicit operator std::string() const { return type_info->name(); };

		bool operator==(TypeId x) const { return type_info == x.type_info; };
		bool operator!=(TypeId x) const { return type_info != x.type_info; };
		bool operator<(TypeId x) const { return type_info < x.type_info; };
	};

	template <typename T, bool is_abstract = std::is_abstract<T>::value>
	struct GetConcreteTypeInfo {
		TypeInfo::ConcreteTypeInfo operator()() const {
			return TypeInfo::ConcreteTypeInfo{
				sizeof(T), alignof(T), std::is_trivially_destructible<T>::value,
			};
		}
	};

	// For abstract types we don't need the real information.
	// Also, some compilers might report compile errors in this case, for example alignof(T) doesn't work in Visual Studio
	// when T is an abstract type.
	template <typename T>
	struct GetConcreteTypeInfo<T, true> {
		TypeInfo::ConcreteTypeInfo operator()() const {
			return TypeInfo::ConcreteTypeInfo{
				0 /* type_size */, 0 /* type_alignment */, false /* is_trivially_destructible */,
			};
		}
	};

	template <typename T>
	struct GetTypeInfoForType {
		TypeInfo operator()() const {
			return TypeInfo(typeid(T), GetConcreteTypeInfo<T>()());
		};
	};

	template <typename T>
	inline TypeId getTypeId() {
		static TypeInfo info = GetTypeInfoForType<TypeWrapper<T>>()();
		return TypeId{ &info };
	}; 

    
}; // namespace ioc
