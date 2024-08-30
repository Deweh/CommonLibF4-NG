#pragma once

#include "RE/Havok/hkBaseObject.h"

namespace RE
{
	class hkClass;

	class __declspec(novtable) hkReferencedObject :
		public hkBaseObject  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::hkReferencedObject };
		static constexpr auto VTABLE{ VTABLE::hkReferencedObject };

		inline std::int64_t addReference()
		{
			using func_t = decltype(&hkReferencedObject::addReference);
			static REL::Relocation<func_t> func{ REL::ID(866015) };
			return func(this);
		}

		inline std::int64_t removeReference()
		{
			using func_t = decltype(&hkReferencedObject::addReference);
			static REL::Relocation<func_t> func{ REL::ID(1379897) };
			return func(this);
		}

		inline std::uint32_t getReferenceCount()
		{
			return memSizeAndRefCount & 0xFFFF;
		}

		// add
		virtual const hkClass* GetClassType() const { return nullptr; }             // 02
		virtual void           DeleteThisReferencedObject() const { delete this; }  // 03

		// members
		std::uint32_t memSizeAndRefCount;  // 08
	};
	static_assert(sizeof(hkReferencedObject) == 0x10);
}
