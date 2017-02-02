//-------------------------------------------------------------------
//  MetaInfo Framework (MIF)
//  https://github.com/tdv/mif
//  Created:     09.2016
//  Copyright (C) 2016 tdv
//-------------------------------------------------------------------

#ifndef __MIF_REMOTE_DETAIL_PS_H__
#define __MIF_REMOTE_DETAIL_PS_H__

// STD
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

// MIF
#include "mif/common/detail/hierarchy.h"
#include "mif/common/types.h"
#include "mif/common/uuid_generator.h"
#include "mif/service/inherited_list.h"
#include "mif/service/iservice.h"

namespace Mif
{
    namespace Remote
    {
        namespace Detail
        {

            template <typename TResult>
            struct FunctionWrap
            {
                template <typename TFunc, typename TStorage>
                static void Call(TFunc func, TStorage &storage)
                {
                    auto res = func();
                    storage.PutParams(std::move(res));
                }
            };

            template <>
            struct FunctionWrap<void>
            {
                template <typename TFunc, typename TStorage>
                static void Call(TFunc func, TStorage &storage)
                {
                    func();
                }
            };

            template <typename TResult>
            struct ResultExtractor
            {
                template <typename TStorage>
                static TResult Extract(TStorage &storage)
                {
                    return std::get<0>(storage.template GetParams<TResult>());
                }
            };

            template <>
            struct ResultExtractor<void>
            {
                template <typename TStorage>
                static void Extract(TStorage &)
                {
                }
            };

            class ProxyStubException final
                : public std::runtime_error
            {
            public:
                using std::runtime_error::runtime_error;
            };

            template <typename TSerializer>
            class Proxy
            {
            public:
                using Serializer = typename TSerializer::Serializer;
                using Deserializer = typename TSerializer::Deserializer;
                using DeserializerPtr = std::unique_ptr<Deserializer>;

                using Sender = std::function<DeserializerPtr (std::string const &, Serializer &)>;

                Proxy(std::string const &instance, Sender && sender)
                    : m_instance(instance)
                    , m_sender(std::move(sender))
                {
                }

                virtual ~Proxy() = default;

                template <typename TResult, typename ... TParams>
                TResult RemoteCall(std::string const &interface, std::string const &method, TParams && ... params)
                {
                    try
                    {
                        auto const requestId = m_generator.Generate();
                        Serializer serializer(true, requestId, m_instance, interface, method, std::forward<TParams>(params) ... );
                        auto deserializer = m_sender(requestId, serializer);
                        if (!deserializer->IsResponse())
                            throw ProxyStubException{"[Mif::Remote::Proxy::RemoteCall] Bad response type \"" + deserializer->GetType() + "\""};
                        auto const &instance = deserializer->GetInstance();
                        if (instance != m_instance)
                        {
                            throw ProxyStubException{"[Mif::Remote::Proxy::RemoteCall] Bad instance id \"" + instance + "\" "
                                "Needed instance id \"" + m_instance + "\""};
                        }
                        auto const &interfaceId = deserializer->GetInterface();
                        if (interface != interfaceId)
                        {
                            throw ProxyStubException{"[Mif::Remote::Proxy::RemoteCall] Bad interface for instance whith id \"" +
                                interfaceId + "\" Needed \"" + interface + "\""};
                        }
                        auto const &methodId = deserializer->GetMethod();
                        if (method != methodId)
                        {
                            throw ProxyStubException{"[Mif::Remote::Proxy::RemoteCall] Method \"" + methodId + "\" "
                                "of interface \"" + interface + "\" for instance with id \"" + m_instance + "\" "
                                "not found. Needed method \"" + method + "\""};
                        }

                        if (deserializer->HasException())
                            std::rethrow_exception(deserializer->GetException());

                        return ResultExtractor<TResult>::Extract(*deserializer);
                    }
                    catch (std::exception const &e)
                    {
                        throw ProxyStubException{"[Mif::Remote::Proxy::RemoteCall] Failed to call remote method \"" +
                            interface + "::" + method + "\" for instance with id \"" + m_instance + "\". Error: " +
                            std::string{e.what()}};
                    }
                }

            private:
                Common::UuidGenerator m_generator;
                std::string m_instance;
                Sender m_sender;
            };

            template <typename TSerializer>
            struct IStub
            {
                using Serializer = typename TSerializer::Serializer;
                using Deserializer = typename TSerializer::Deserializer;

                virtual ~IStub() = default;
                virtual void Call(void *instance, Deserializer &request, Serializer &response) = 0;
            };

            template <typename TSerializer>
            class Stub
                : public IStub<TSerializer>
            {
            public:
                using BaseType = IStub<TSerializer>;
                using Serializer = typename BaseType::Serializer;
                using Deserializer = typename BaseType::Deserializer;

                virtual void Call(void *instance, Deserializer &request, Serializer &response) override final
                {
                    try
                    {
                        auto const &interfaceId = request.GetInterface();
                        if (!ContainInterfaceId(interfaceId))
                        {
                            throw ProxyStubException{"[Mif::Remote::Stub::Call] Interface with id \"" +
                                interfaceId + "\" not supported for this object."};
                        }
                        auto const &method = request.GetMethod();
                        InvokeMethod(instance, method, &request, &response);
                    }
                    catch (...)
                    {
                        response.PutException(std::current_exception());
                    }
                }

            protected:
                virtual void InvokeMethod(void *, std::string const &method, void *, void *)
                {
                    throw ProxyStubException{"[Mif::Remote::Stub::InvokeMethod] Method \"" + method + "\" not found."};
                }

                template <typename TResult, typename TInterface, typename ... TParams>
                void InvokeRealMethod(TResult (*method)(TInterface &, std::tuple<TParams ... > && ),
                                      void *instance, void *deserializer, void *serializer)
                {
                    auto &inst = dynamic_cast<TInterface &>(*reinterpret_cast<Service::IService *>(instance));
                    auto params = reinterpret_cast<Deserializer *>(deserializer)->template GetParams<TParams ... >();
                    FunctionWrap<TResult>::Call(
                            [&method, &inst, &params] () { return method(inst, std::move(params)); },
                            *reinterpret_cast<Serializer *>(serializer)
                        );
                }

                virtual bool ContainInterfaceId(std::string const &id) const
                {
                    return false;
                }
            };

            using FakeHierarchy = Common::Detail::MakeHierarchy<100>;

            namespace Registry

            {
                namespace Counter
                {
                    inline constexpr std::size_t GetLast(...)
                    {
                        return 0;
                    }

                }   // namespace Counter

                template <typename TInterface>
                struct Registry;

                template <std::size_t I>
                struct Item;

            }   // namespace Registry

            template <typename, typename, typename>
            class BaseProxies;

            template <typename TSerializer, typename TInterface, typename TBase, typename ... TBases>
            class BaseProxies<TSerializer, TInterface, std::tuple<TBase, TBases ... >>
                : public Registry::Registry<TBase>::template Type<TSerializer>::template ProxyItem
                    <
                        BaseProxies<TSerializer, TInterface, std::tuple<TBases ... >>
                    >
            {
            public:
                using Registry::Registry<TBase>::template Type<TSerializer>::template ProxyItem
                        <
                            BaseProxies<TSerializer, TInterface, std::tuple<TBases ... >>
                        >::ProxyItem;
            };

            template <typename TSerializer, typename TInterface>
            class BaseProxies<TSerializer, TInterface, std::tuple<>>
                : public TInterface
            {
            public:
                template <typename ... TParams>
                BaseProxies(TParams && ... params)
                    : m_proxy(std::forward<TParams>(params) ... )
                {
                }

            private:
                mutable Proxy<TSerializer> m_proxy;

            protected:
                template <typename TResult, typename ... TParams>
                TResult _Mif_Remote_Call_Method(std::string const &interfaceId, std::string const &method, TParams && ... params) const
                {
                    return m_proxy.template RemoteCall<TResult>(interfaceId, method, std::forward<TParams>(params) ... );
                }
            };

            template <typename TSerializer, typename T>
            using InheritProxy = BaseProxies<TSerializer, T, Service::MakeInheritedIist<T>>;

            template <typename, typename>
            class BaseStubs;

            template <typename TSerializer, typename TBase, typename ... TBases>
            class BaseStubs<TSerializer, std::tuple<TBase, TBases ... >>
                : public Registry::Registry<TBase>::template Type<TSerializer>::template StubItem
                    <
                        BaseStubs<TSerializer, std::tuple<TBases ... >>
                    >
            {
            public:
                using Registry::Registry<TBase>::template Type<TSerializer>::template StubItem
                        <
                            BaseStubs<TSerializer, std::tuple<TBases ... >>
                        >::StubItem;
            };

            template <typename TSerializer>
            class BaseStubs<TSerializer, std::tuple<>>
                : public ::Mif::Remote::Detail::Stub<TSerializer>
            {
            public:
                using ::Mif::Remote::Detail::Stub<TSerializer>::Stub;
            };

            template <typename TSerializer, typename T>
            class InheritStub
                : public BaseStubs<TSerializer, Service::MakeInheritedIist<T>>
            {
            public:
                using BaseStubs<TSerializer, Service::MakeInheritedIist<T>>::BaseStubs;
            };

        }  // namespace Detail
    }   //  namespace Remote
}   // namespace Mif


#endif  // !__MIF_REMOTE_DETAIL_PS_H__
