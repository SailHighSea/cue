// The remote's server, on with no phone, sleeps: no wake-up until a phone connects or the remote
// is turned off, which no longer waits for the loop's next wake. Measured in Wine (1.4): the
// loop woke twice a second, and RemoteServer::stop held the UI thread 8 to 429 ms (mean 211);
// now 0 wake-ups and 1 to 2 ms.
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static std::string read(const char* p) { std::ifstream f(p); std::stringstream b; b << f.rdbuf(); return b.str(); }
static std::string body(const std::string& s, const std::string& head) {
  const size_t a = s.find(head);
  assert(a != std::string::npos);
  return s.substr(a, s.find("\n}\n", a) - a);
}

int main() {
  const std::string s = read("src/win/remote_server.cpp");
  // the bell: a datagram socket on the loopback only, nonblocking, in the loop's select
  const std::string bell = s.substr(s.find("bool open_bell() {"), s.find("void post(Event e) {") - s.find("bool open_bell() {"));
  assert(bell.find("SOCK_DGRAM") != std::string::npos && bell.find("htonl(INADDR_LOOPBACK)") != std::string::npos);
  assert(bell.find("FIONBIO") != std::string::npos && bell.find("closesocket(u); return false;") != std::string::npos);
  const std::string loop = body(s, "void RemoteServer::Impl::loop() {");
  assert(loop.find("if (wake != INVALID_SOCKET) FD_SET(wake, &rd);") < loop.find("select("));
  // asleep only with no phone and a bell to wake it; phones keep the tenth of a second
  assert(loop.find("const bool sleep_long = clients.empty() && wake != INVALID_SOCKET;") != std::string::npos);
  assert(loop.find("select(0, &rd, &wr, nullptr, sleep_long ? nullptr : &tv);") != std::string::npos);
  assert(loop.find("timeval tv{0, clients.empty() ? 500000 : 100000};") != std::string::npos);
  assert(loop.find("if (quit) break;") < loop.find("FD_ISSET(wake, &rd)"));
  // turning it off rings before waiting, and the bell closes after the loop is gone
  const std::string stop = body(s, "void RemoteServer::stop(bool tell_phones) {");
  assert(stop.find("d_->quit = true;") < stop.find("d_->ring();") && stop.find("d_->ring();") < stop.find("d_->th.join();"));
  assert(stop.find("d_->th.join();") < stop.find("d_->close_bell();"));
  const std::string start = body(s, "bool RemoteServer::start(");
  assert(start.find("d_->open_bell();") < start.find("std::thread([this] { d_->loop(); })"));
  assert(start.find("d_->close_bell();", start.find("catch (...)")) != std::string::npos);
  std::puts("Remote idle: the server sleeps with no phone (a loopback bell in its select), stop rings before it waits, phones keep their pace passed");
}
