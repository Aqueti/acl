#include <iostream>
#include <thread>
#include <vector>
#include <CoreSocket.hpp>

using namespace acl::CoreSocket;

/// @brief How many socket connections to try
static size_t g_numSockets = 100;
static int g_packetSize = 100;

/// @brief Function to read and verify the specified number of bytes from a socket.
///
/// This will ensure that it can read the requested number of bytes from the socket
/// and that the bytes contain modulo-128 numbers, 0 through 127 and then repeating.
/// @param [in] s Socket to read from
/// @param [in] bytes Total number of bytes to read
/// @param [in] chunkSize Size of chunks to read from the socket.
/// @param [out] result number of bytes read, -1 if there is a mismatch in the
///           data compared to what was expected.
void TestReadFromSocket(int &result, SOCKET s, int bytes, int chunkSize)
{
  int sofar = 0;
  int remaining = bytes;
  std::vector<char> buf(bytes);

  // Get all the bytes
  while (remaining > 0) {
    int nextChunk = chunkSize;
    if (nextChunk > remaining) {
      nextChunk = remaining;
    }

    if (nextChunk != noint_block_read(s, &buf[sofar], nextChunk)) {
      result = sofar;
      return;
    }
    sofar += nextChunk;
    remaining -= nextChunk;
  }
  
  // Check the values
  for (int i = 0; i < bytes; i++) {
    if (buf[i] != (i % 128)) {
      result = -1;
      return;
    }
  }

  result = sofar;
  return;
}

/// @brief Function to write the specified number of modulo-128 bytes to a socket.
///
/// This will ensure that it can write the requested number of bytes to the socket.
/// @param [in] s Socket to write to
/// @param [in] bytes Total number of bytes to write
/// @param [in] chunkSize Size of chunks to write to the socket.
/// @param [out] result Number of bytes successfully written.
void TestWriteToSocket(int& result, SOCKET s, int bytes, int chunkSize)
{
  int sofar = 0;
  int remaining = bytes;

  // Fill in the values
  std::vector<char> buf(bytes);
  for (int i = 0; i < bytes; i++) {
    buf[i] = i % 128;
  }

  // Send all the bytes
  while (remaining > 0) {
    int nextChunk = chunkSize;
    if (nextChunk > remaining) {
      nextChunk = remaining;
    }

    if (nextChunk != noint_block_write(s, &buf[sofar], nextChunk)) {
      result = sofar;
      return;
    }
    sofar += nextChunk;
    remaining -= nextChunk;

    // Sleep very briefly to keep from flooding the receiver in UDP tests.
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  result = sofar;
  return;
}

/// @brief Function to run the client side of a suite of client-server tests.
/// @param [in] host Host to connect to
/// @param [in] port Port to connect to
/// @param [out] result 0 on success, unique error code on failure.
void TestClientSide(int &result, std::string host, int port)
{
  //=======================================================================================
  // Test opening g_numSockets simultaneous connections and then writing a single
  // g_packetSize-byte packet to each connection.
  std::vector<acl::CoreSocket::SOCKET> socks;
  for (size_t i = 0; i < g_numSockets; i++) {
    acl::CoreSocket::SOCKET sock;
    if (!connect_tcp_to(host.c_str(), port, nullptr, &sock)) {
      std::cerr << "TestClientSide: Error Opening read socket " << i << std::endl;
      result = 1;
      return;
    }
    if (!set_tcp_socket_options(sock)) {
      std::cerr << "TestClientSide: Error setting TCP socket options on socket " << i << std::endl;
      result = 2;
      return;
    }
    socks.push_back(sock);
  }
  int ret;
  for (size_t i = 0; i < g_numSockets; i++) {
    TestWriteToSocket(ret, socks[i], g_packetSize, g_packetSize);
    if (ret != g_packetSize) {
      std::cerr << "TestClientSide: Error writing to socket " << i << std::endl;
      result = 3;
    }
  }
  for (size_t i = 0; i < g_numSockets; i++) {
    if (0 != acl::CoreSocket::close_socket(socks[i])) {
      std::cerr << "TestClientSide: Error closing socket " << i << std::endl;
      result = 4;
    }
  }

  /// @todo

  result = 0;
  return;
}

/// @brief Function to run the server side of a suite of client-server tests.
/// @param [in] port Port to listen on
/// @param [out] result 0 on success, unique error code on failure.
void TestServerSide(int &result, int port)
{
  //=======================================================================================
  // Test accepting g_numSockets simultaneous connection requests and reading a single
  // g_packetSize-byte packet from each connection.
  int myPort = port;
  SOCKET lSock = get_a_TCP_socket(&myPort);
  if (lSock == BAD_SOCKET) {
    std::cerr << "TestServerSide: Error Opening listening socket on arbitrary port" << std::endl;
    result = 1;
    return;
  }
  std::vector<acl::CoreSocket::SOCKET> socks;
  for (size_t i = 0; i < g_numSockets; i++) {
    SOCKET rSock;
    if (1 != poll_for_accept(lSock, &rSock, 10.0)) {
      std::cerr << "TestServerSide: Error Opening accept socket " << i << std::endl;
      result = 2;
      return;
    }
    if (!set_tcp_socket_options(rSock)) {
      std::cerr << "TestServerSide: Error setting TCP socket options on accept socket " << i << std::endl;
      result = 3;
      return;
    }
    socks.push_back(rSock);
  }
  int ret;
  for (size_t i = 0; i < g_numSockets; i++) {
    TestReadFromSocket(ret, socks[i], g_packetSize, g_packetSize);
    if (ret != g_packetSize) {
      std::cerr << "TestServerSide: Error reading from socket " << i << std::endl;
      result = 4;
    }
  }
  for (size_t i = 0; i < g_numSockets; i++) {
    if (0 != acl::CoreSocket::close_socket(socks[i])) {
      std::cerr << "TestServerSide: Error closing socket " << i << std::endl;
      result = 5;
    }
  }

  /// @todo

  result = 0;
  return;
}


void Usage(std::string name)
{
  std::cerr << "Usage: " << name << " [[--server PORT] | [--client HOST PORT]]" << std::endl;
  std::cerr << "       --server: Run only the server tests on the specified port on all NICs" << std::endl;
  std::cerr << "       --client: Run only the client tests and connect to  the specified port on the specified host name" << std::endl;
  exit(1);
}

int main(int argc, const char* argv[])
{
  size_t realParams = 0;
  bool doServer = true, doClient = true;
  std::string hostName = "localhost";
  int port = 12345;
  for (int i = 1; i < argc; i++) {
    if (std::string("--client").compare(argv[i]) == 0) {
      doClient = false;
      if (++i >= argc) { Usage(argv[0]); }
      hostName = argv[i];
      if (++i >= argc) { Usage(argv[0]); }
      port = atoi(argv[i]);
    } else if (std::string("--server").compare(argv[i]) == 0) {
      doServer = false;
      if (++i >= argc) { Usage(argv[0]); }
      port = atoi(argv[i]);
    } else if (argv[i][0] == '-') {
      Usage(argv[0]);
    } else switch (++realParams) {
      case 1:
        Usage(argv[0]);
        break;
      default:
        Usage(argv[0]);
    }
  }

  // Test closing a bad socket.
  {
    SOCKET s = BAD_SOCKET;
    if (-100 != close_socket(s)) {
      std::cerr << "Error closing BAD_SOCKET" << std::endl;
      return 1;
    }
  }

  // Test creating and destroying both types of server sockets
  // using the most-basic open command.
  {
    SOCKET s;
    s = open_socket(SOCK_STREAM, nullptr, nullptr);
    if (s == BAD_SOCKET) {
      std::cerr << "Error opening stream socket on any port and interface" << std::endl;
      return 101;
    }
    if (!set_tcp_socket_options(s)) {
      std::cerr << "Error setting stream socket options on any port and interface" << std::endl;
      return 102;
    }
    if (0 != close_socket(s)) {
      std::cerr << "Error closing stream socket on any port and interface" << std::endl;
      return 103;
    }

    s = open_socket(SOCK_DGRAM, nullptr, nullptr);
    if (s == BAD_SOCKET) {
      std::cerr << "Error opening datagram socket on any port and interface" << std::endl;
      return 104;
    }
    if (0 != close_socket(s)) {
      std::cerr << "Error closing datagram socket on any port and interface" << std::endl;
      return 105;
    }
  }

  // Test creating and destroying both types of server sockets
  // using the type-specific open commands.
  {
    SOCKET s;
    s = open_tcp_socket(nullptr, nullptr);
    if (s == BAD_SOCKET) {
      std::cerr << "Error opening TCP socket on any port and interface" << std::endl;
      return 201;
    }
    if (!set_tcp_socket_options(s)) {
      std::cerr << "Error setting TCP socket options on any port and interface" << std::endl;
      return 202;
    }
    if (0 != close_socket(s)) {
      std::cerr << "Error closing TCP socket on any port and interface" << std::endl;
      return 203;
    }

    s = open_udp_socket(nullptr, nullptr);
    if (s == BAD_SOCKET) {
      std::cerr << "Error opening UDP socket on any port and interface" << std::endl;
      return 204;
    }
    if (0 != close_socket(s)) {
      std::cerr << "Error closing UDP socket on any port and interface" << std::endl;
      return 205;
    }
  }

  // Test opening TCP server socket and a remote on different
  // threads and sending a bunch of data between them.  We use different threads to
  // avoid blocking when the network buffers get full.
  {
    // Construct and connect our writing and reading sockets.  First we make a listening socket
    // then connect a read to it and accept the write on it.
    int port = 0;
    SOCKET lSock = get_a_TCP_socket (&port);
    if (lSock == BAD_SOCKET) {
      std::cerr << "Error Opening listening socket on arbitrary port" << std::endl;
      return 301;
    }
    SOCKET rSock;
    if (!connect_tcp_to("localhost", port, nullptr, &rSock)) {
      std::cerr << "Error Opening read socket" << std::endl;
      return 302;
    }
    if (!set_tcp_socket_options(rSock)) {
      std::cerr << "Error setting TCP socket options on arbitrary port" << std::endl;
      return 303;
    }
    SOCKET wSock;
    if (1 != poll_for_accept(lSock, &wSock, 10.0)) {
      std::cerr << "Error Opening write socket" << std::endl;
      return 304;
    }
    if (!set_tcp_socket_options(wSock)) {
      std::cerr << "Error setting TCP socket options on write socket" << std::endl;
      return 305;
    }

    // Store the results of our threads, testing reading and writing.
    int NUM_BYTES = 1000000;
    int writeBytes = 0, readBytes = 0;
    std::thread wt(TestWriteToSocket, std::ref(writeBytes), wSock, NUM_BYTES, 65000);
    std::thread rt(TestReadFromSocket, std::ref(readBytes), rSock, NUM_BYTES, 65000);
    wt.join();
    rt.join();
    if (writeBytes != NUM_BYTES) {
      std::cerr << "Writing to socket failed" << std::endl;
      return 310;
    }
    if (readBytes != NUM_BYTES) {
      std::cerr << "Reading from socket failed" << std::endl;
      return 311;
    }

    // Done with the sockets
    if (0 != close_socket(wSock)) {
      std::cerr << "Error closing write socket on any port and interface" << std::endl;
      return 320;
    }
    if (0 != close_socket(lSock)) {
      std::cerr << "Error closing listening socket on any port and interface" << std::endl;
      return 321;
    }
    if (0 != close_socket(rSock)) {
      std::cerr << "Error closing read socket on any port and interface" << std::endl;
      return 322;
    }
  }

  // Test opening UDP server socket and a remote on different
  // threads and sending a bunch of data between them.  We use different threads to
  // avoid blocking when the network buffers get full.
  {
    // Construct and connect our writing and reading sockets.  First we make a server socket
    // then connect a client to it.
    // We set the port number to 0 to select "any port".
    unsigned short port = 0;
    SOCKET sSock = open_udp_socket(&port, "localhost");
    if (sSock == BAD_SOCKET) {
      std::cerr << "Error Opening UDP socket on arbitrary port" << std::endl;
      return 401;
    }
    SOCKET rSock = connect_udp_port("localhost", port, nullptr);
    if (rSock == BAD_SOCKET) {
      std::cerr << "Error Opening UDP remote socket" << std::endl;
      return 402;
    }

    // Store the results of our threads, testing reading and writing.
    int NUM_BYTES = 1000000;
    int writeBytes = 0, readBytes = 0;
    std::thread wt(TestWriteToSocket, std::ref(writeBytes), rSock, NUM_BYTES, 65000);
    std::thread rt(TestReadFromSocket, std::ref(readBytes), sSock, NUM_BYTES, 65000);
    wt.join();
    rt.join();
    if (writeBytes != NUM_BYTES) {
      std::cerr << "Writing to UDP socket failed" << std::endl;
      return 410;
    }
    if (readBytes != NUM_BYTES) {
      std::cerr << "Reading from UDP socket failed" << std::endl;
      return 411;
    }

    // Done with the sockets
    if (0 != close_socket(sSock)) {
      std::cerr << "Error closing UDP server socket on any port and interface" << std::endl;
      return 420;
    }
    if (0 != close_socket(rSock)) {
      std::cerr << "Error closing UDP remote socket on any port and interface" << std::endl;
      return 421;
    }
  }

  // Test opening TCP server socket and a remote and checking for partial reads
  // with timeouts in the case where the server just sends part of the data and then
  // leaves the connection open.
  {
    // Construct and connect our writing and reading sockets.  First we make a listening socket
    // then connect a read to it and accept the write on it.
    int port = 0;
    SOCKET lSock = get_a_TCP_socket (&port);
    if (lSock == BAD_SOCKET) {
      std::cerr << "Error Opening listening socket on arbitrary port" << std::endl;
      return 501;
    }
    SOCKET rSock;
    if (!connect_tcp_to("localhost", port, nullptr, &rSock)) {
      std::cerr << "Error Opening read socket" << std::endl;
      return 502;
    }
    if (!set_tcp_socket_options(rSock)) {
      std::cerr << "Error setting TCP socket options on arbitrary port" << std::endl;
      return 503;
    }
    SOCKET wSock;
    if (1 != poll_for_accept(lSock, &wSock, 10.0)) {
      std::cerr << "Error Opening write socket" << std::endl;
      return 504;
    }
    if (!set_tcp_socket_options(wSock)) {
      std::cerr << "Error setting TCP socket options on write socket" << std::endl;
      return 505;
    }

    // Send a smaller amount of data than we'd like to receive and then verify that
    // the read times out.
    std::vector<char> buffer(256);
    int halfSize = static_cast<int>(buffer.size()/2);
    if (halfSize != noint_block_write(wSock, buffer.data(), halfSize)) {
      std::cerr << "Error sending on write socket" << std::endl;
      return 506;
    }
    std::cout << "Testing blocking read with timeout..." << std::endl;
    struct timeval timeout = { 0, 100000 };
    int ret = noint_block_read_timeout(rSock, buffer.data(), buffer.size(), &timeout);
    if (ret != halfSize) {
      std::cerr << "Error with partial read with timeout: " << ret << std::endl;
      return 507;
    }
    std::cout << "... Completed" << std::endl;

    // Done with the sockets
    if (0 != close_socket(wSock)) {
      std::cerr << "Error closing write socket on any port and interface" << std::endl;
      return 520;
    }
    if (0 != close_socket(lSock)) {
      std::cerr << "Error closing listening socket on any port and interface" << std::endl;
      return 521;
    }
    if (0 != close_socket(rSock)) {
      std::cerr << "Error closing read socket on any port and interface" << std::endl;
      return 522;
    }
  }  

  // Test running separate server and client tests that talk to each other over the
  // network.  The default is to run both threads from this same process, but it can
  // also be specified on the command line to run them as separate processes on the
  // same or different computers.
  std::thread st, ct;
  int clientWorked = -1, serverWorked = -1;
  if (doServer) {
    std::cout << "Testing accepting " << g_numSockets << " sockets..." << std::endl;
    st = std::thread(TestServerSide, std::ref(serverWorked), port);
  }
  if (doClient) {
    std::cout << "Testing connecting " << g_numSockets << " sockets..." << std::endl;
    ct = std::thread(TestClientSide, std::ref(clientWorked), hostName, port);
  }
  if (doServer) {
    st.join();
    if (serverWorked != 0) {
      std::cerr << "Server code failed with code " << serverWorked << std::endl;
      return 311;
    }
    std::cout << "...Server success" << std::endl;
  }
  if (doClient) {
    ct.join();
    if (clientWorked != 0) {
      std::cerr << "Client code failed with code " << clientWorked << std::endl;
      return 310;
    }
    std::cout << "...Client success" << std::endl;
  }


  /// @todo Test reuseAddr parameter to open_socket() on both TCP and UDP.

  /// @todo More tests

  std::cout << "Success!" << std::endl;
  return 0;
}
