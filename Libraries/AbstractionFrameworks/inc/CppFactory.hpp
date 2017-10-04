#pragma once

#include <map>
#include <functional>
#include <memory>

/// <summary>
/// Modern c++ object factory implementation in <200 lines
/// </summary>
/// <remarks>
/// Version 0.2.0 - from https://github.com/bengreenier/CppFactory/releases/tag/v0.2.0
/// </remarks>
namespace CppFactory
{
	/// <summary>
	/// Represents an <see cref="Object"/> that has a global lifetime, meaning
	/// it doesn't get destroyed when it leaves scope
	/// </summary>
	/// <param name="TObject">The type of object</param>
	template <class TObject>
	class GlobalObject
	{
	public:
		/// <summary>
		/// Gets (and allocates, if needed) a global object (optionally from a particular zone) for type <c>TObject</c>
		/// </summary>
		/// <param name="TZone">The zone to get from</param>
		/// <returns>The object</returns>
		/// <example>
		/// GlobalObject&lt;TObject&gt;::Get();
		/// </example>
		template <int TZone = 0>
		static std::shared_ptr<TObject> Get()
		{
			if (m_allocObjMap[TZone].get() == nullptr)
			{
				m_allocObjMap[TZone] = Object<TObject>::Get<TZone>();
			}

			return m_allocObjMap[TZone];
		}

		/// <summary>
		/// Resets the global object cache for all zones for type <c>TObject</c>
		/// </summary>
		/// <param name="TZone">The zone to reset</param>
		/// <example>
		/// GlobalObject&lt;TObject&gt;::Reset<1>();
		/// </example>
		template <int TZone>
		static void Reset()
		{
			m_allocObjMap[TZone].reset();
		}

		/// <summary>
		/// Resets the global object cache for all zones for type <c>TObject</c>
		/// </summary>
		/// <example>
		/// GlobalObject&lt;TObject&gt;::Reset();
		/// </example>
		static void Reset()
		{
			m_allocObjMap.clear();
		}
	private:
		/// <summary>
		/// The type of the allocated object map
		/// </summary>
		typedef std::map<int, std::shared_ptr<TObject>> AllocObjMapType;

		/// <summary>
		/// The allocated object map
		/// </summary>
		static AllocObjMapType m_allocObjMap;
	};

	template <class TObject>
	typename GlobalObject<TObject>::AllocObjMapType GlobalObject<TObject>::m_allocObjMap = GlobalObject<TObject>::AllocObjMapType();

	/// <summary>
	/// Represents an object that is created via a "factory"
	/// </summary>
	/// <param name="TObject">The type of object</param>
	template <class TObject>
	class Object
	{
	public:
		/// <summary>
		/// Registers logic capable of allocating (and optionally deallocating) an object of type <c>TObject</c>
		/// </summary>
		/// <param name="TZone">The zone to register for</param>
		/// <param name="alloc">Function that allocates an object</param>
		/// <example>
		/// Object&lt;TObject&gt;::RegisterAllocator([] { return std::make_shared<TObject>(); });
		/// </example>
		template <int TZone = 0>
		static void RegisterAllocator(const std::function<std::shared_ptr<TObject>()>& alloc)
		{
			m_allocFunc[TZone] = alloc;
		}

		/// <summary>
		/// Unregisters all allocators for all zones for type <c>TObject</c>
		/// </summary>
		/// <example>
		/// Object&lt;TObject&gt;::UnregisterAllocator();
		/// </example>
		static void UnregisterAllocator()
		{
			m_allocFunc.clear();
		}
		
		/// <summary>
		/// Unregisters all allocators for a particular zone for type <c>TObject</c>
		/// </summary>
		/// <param name="TZone">The zone to unregister</param>
		/// <example>
		/// Object&lt;TObject&gt;::UnregisterAllocator(10);
		/// </example>
		template<int TZone>
		static void UnregisterAllocator()
		{
			m_allocFunc[TZone].erase(zone);
		}

		/// <summary>
		/// Gets (and allocates, if needed) an object (optionally from a particular zone) for type <c>TObject</c>
		/// </summary>
		/// <param name="TZone">The zone to get from</param>
		/// <returns>The object</returns>
		/// <example>
		/// Object<TObject>::Get();
		/// </example>
		template <int TZone = 0>
		static std::shared_ptr<TObject> Get()
		{
			std::shared_ptr<TObject> obj;

			// if we have a custom allocator use it
			if (m_allocFunc.find(TZone) == m_allocFunc.end())
			{
				// TODO(bengreenier): support not default ctors
				//
				// If compilation is failing here, you may have a ctor with parameters or a non-public ctor
				// Non-public: add `friend Object<TObject>;`
				// Parameters: not supported yet
				obj = std::shared_ptr<TObject>(new TObject());
			}
			else
			{
				obj = m_allocFunc[TZone]();
			}

			return obj;
		}

	private:
		/// <summary>
		/// The type of the allocator function map
		/// </summary>
		typedef std::map<int, std::function<std::shared_ptr<TObject>()>> AllocFuncMapType;

		/// <summary>
		/// The allocator function map
		/// </summary>
		static AllocFuncMapType m_allocFunc;
	};

	template <class TObject>
	typename Object<TObject>::AllocFuncMapType Object<TObject>::m_allocFunc = Object<TObject>::AllocFuncMapType();
}