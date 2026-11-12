#include "support/MessageDelivery.h"
#include <cassert>
#include <iostream>
int main() {
    Chat::Deliveries reply;
    assert(reply.Add("reply","> Original 🌞\n\nMy answer","original","bob@fixture.test","> Original 🌞\n\n","Reply to Bob"));
    assert(reply.Update("reply",Chat::Delivery::Failed));
    const auto* failed=reply.Find("reply");
    assert(failed && failed->replyId=="original" && failed->replyTo=="bob@fixture.test");
    assert(failed->text.substr(failed->replyPrefix.size())=="My answer");
    Chat::Deliveries d;
    assert(d.Add("a", "first"));
    assert(d.Add("b", "second"));
    assert(!d.Update("", Chat::Delivery::Failed));
    assert(!d.Update("foreign", Chat::Delivery::Delivered));
    assert(d.Update("a", Chat::Delivery::Failed));
    assert(d.Find("b")->state == Chat::Delivery::Queued);
    assert(d.Find("a")->text == "first");
    assert(!d.Update("a", Chat::Delivery::Unconfirmed));
    assert(d.Update("b", Chat::Delivery::Delivered));
    assert(!d.Update("b", Chat::Delivery::Failed));
    assert(!d.Update("b", Chat::Delivery::Delivered));
    assert(!d.Add("b", "duplicate"));
    assert(d.Update("a", Chat::Delivery::Delivered));
    for (int i = 2; i < 256; ++i) assert(d.Add(std::to_string(i), "bounded"));
    assert(d.Add("overflow", "bounded recent correlation"));
    assert(d.Find("a") == nullptr);
    assert(!d.Update("a", Chat::Delivery::Failed));
    std::cout << "Delivery correlation, reordered receipts, retained text, duplicate and capacity checks passed\n";
}
