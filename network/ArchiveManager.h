// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Messenger.h>
#include <Message.h>
#include <gloox/client.h>
#include <gloox/iqhandler.h>
#include <gloox/message.h>
#include <memory>
#include <mutex>
#include <deque>
#include <map>
namespace Chat {
class ArchiveManager : public gloox::IqHandler {
public:
    explicit ArchiveManager(gloox::Client& client);
    bool Queue(const std::string& peer,bool group,const BMessenger& target,int64 since=0);
    void Tick();
    void Disconnected();
    bool Handle(const gloox::Message& message);
    bool handleIq(const gloox::IQ&) override { return false; }
    void handleIqID(const gloox::IQ& iq,int context) override;
private:
    struct Request { std::string peer,id,before;bool group=false;BMessenger target;BMessage result{'hDon'};int pages=0,count=0;bigtime_t deadline=0;int64 since=0; };
    void Send(const std::shared_ptr<Request>& request);
    void Finish(std::shared_ptr<Request> request,const char* status);
    gloox::Client& fClient;
    std::mutex fLock;
    bool fStopped=false;
    std::deque<std::shared_ptr<Request>> fQueue;
    std::map<std::string,std::shared_ptr<Request>> fActive;
};
}
