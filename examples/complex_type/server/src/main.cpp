//-------------------------------------------------------------------
//  MetaInfo Framework (MIF)
//  https://github.com/tdv/mif
//  Created:     10.2016
//  Copyright (C) 2016-2017 tdv
//-------------------------------------------------------------------

// MIF
#include <mif/application/tcp_service.h>

// COMMON
#include "common/client.h"
#include "common/id/service.h"
#include "common/ps/imy_company.h"

class Application
    : public Mif::Application::TcpService
{
public:
    Application(int argc, char const **argv)
        : TcpService{argc, argv, Service::Ipc::MakeClientFactory}
    {
    }

private:
    // Mif.Application.Application
    virtual void Init(Mif::Service::FactoryPtr factory) override final
    {
        factory->AddClass<::Service::Id::MyCompany>();
    }
};

int main(int argc, char const **argv)
{
    return Mif::Application::Run<Application>(argc, argv);
}
