#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

using namespace std;

void get_URL( const string& host, const string& path )
{
  // 预约一个 end-point (file-descriptor)
  TCPSocket tcpSocket;

  // 将域名解析为 IP 地址；将服务名解析为端口号
  Address address(host, "http");

  // tcp 触发"三次握手"
  tcpSocket.connect(address);

  // 请求文本
  std::string require_str = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";

  // write 把请求文本写入内核的发送缓存区，交给 tcp 处理后续工作
  tcpSocket.write(require_str);

  // read 读取后立刻打印
  std::string buffer;
  while (!tcpSocket.eof()) {
    tcpSocket.read(buffer);
    cout << buffer;
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
