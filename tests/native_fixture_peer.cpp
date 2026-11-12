// Synthetic local-only peer for native Haiku acceptance. Never contacts production.
#include <gloox/client.h>
#include <gloox/connectionlistener.h>
#include <gloox/message.h>
#include <gloox/messagehandler.h>
#include <gloox/oob.h>
#include <gloox/rostermanager.h>
#include <gloox/tlsdefault.h>
#include <stdexcept>
#include <csignal>
#include <iostream>

static volatile std::sig_atomic_t stop = 0;
static void Stop(int) { stop = 1; }

class Peer final : public gloox::ConnectionListener, public gloox::MessageHandler {
public:
	gloox::Client client{gloox::JID("bob@fixture.test/synthetic-peer"), "fixture-only-bob"};
	bool connected = false;
	int received = 0;
	explicit Peer(const char* ca) {
		client.setServer("127.0.0.1");
		client.setPort(15422);
		client.setTls(gloox::TLSRequired);
		// Gloox 1.0.28 SCRAM fails against this Prosody 13 fixture. TLS is required.
		client.setSASLMechanisms(gloox::SaslMechPlain);
		auto* tls = new gloox::TLSDefault(&client, "fixture.test");
		if (!tls->init()) { delete tls; throw std::runtime_error("TLS initialization failed"); }
		// GnuTLS in gloox 1.0.28 ignores init's cacerts argument; install it after init.
		tls->setCACerts({ca});
		client.setEncryptionImpl(tls);
		client.registerConnectionListener(this);
		client.registerMessageHandler(this);
		client.registerStanzaExtension(new gloox::OOB(nullptr));
	}
	void onConnect() override {
		connected = true;
		std::cout << "Synthetic peer connected with certificate verification\n" << std::flush;
		client.rosterManager()->subscribe(gloox::JID("alice@fixture.test"), "Synthetic Alice");
		gloox::Message hello(gloox::Message::Chat, gloox::JID("alice@fixture.test"),
			"Synthetic peer ready. Send a test image or file and I will echo its OOB attachment.");
		client.send(hello);
	}
	void onDisconnect(gloox::ConnectionError error) override {
		connected = false;
		std::cout << "Synthetic peer disconnected: " << error << " auth=" << client.authError() << '\n' << std::flush;
	}
	bool onTLSConnect(const gloox::CertInfo& info) override {
		std::cout << "Synthetic peer certificate status: " << info.status << '\n' << std::flush;
		return info.status == gloox::CertOk;
	}
	void handleMessage(const gloox::Message& message, gloox::MessageSession*) override {
		if (message.from().bare() != "alice@fixture.test" || message.body().empty()) return;
		const auto* attachment = message.findExtension<gloox::OOB>(gloox::ExtOOB);
		++received;
		std::cout << "Received synthetic direct message " << received
			<< " with OOB=" << (attachment != nullptr) << '\n' << std::flush;
		gloox::Message echo(gloox::Message::Chat, message.from(),
			attachment ? "Synthetic attachment echo" : "Synthetic text echo: " + message.body());
		if (attachment) echo.addExtension(new gloox::OOB(attachment->url(), attachment->desc(), false));
		client.send(echo);
	}
};

int main(int argc, char** argv)
{
	if (argc != 2) { std::cerr << "Usage: fixture-peer /path/to/local-fixture/ca.pem\n"; return 2; }
	std::signal(SIGINT, Stop); std::signal(SIGTERM, Stop);
	Peer peer(argv[1]);
	if (!peer.client.connect(false)) return 1;
	while (!stop && peer.client.recv(100000) == gloox::ConnNoError) {}
	peer.client.disconnect();
	return 0;
}
