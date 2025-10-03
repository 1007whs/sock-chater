#ifndef SOCK_HPP
#define SOCK_HPP

#include <bits/stdc++.h>
#include <winsock2.h>
#include <windows.h>
#include <thread>
#include <mutex>

// 仅在MSVC编译器下使用#pragma comment
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

inline void setConsoleUTF8()
{
    // 设置控制台输出编码为UTF-8
    SetConsoleOutputCP(CP_UTF8);
    // 设置控制台输入编码为UTF-8（支持中文输入）
    SetConsoleCP(CP_UTF8);
}

// 缓冲区大小可配置
const int DEFAULT_BUFFER_SIZE = 1048576; // 1MB

// 控制台颜色控制
namespace ConsoleColor
{
    inline void set(int color)
    {
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(handle, FOREGROUND_INTENSITY | color);
    }
    const int WHITE = 7;
    const int RED = 4;
    const int GREEN = 2;
    const int GRAY = 8;
    const int YELLOW = 6;
}

// 服务端类
class TCPServer
{
private:
    std::string ip;
    int port;
    SOCKET server_socket;
    bool is_running;
    int buffer_size;
    std::mutex console_mutex; // 控制台输出互斥锁

    // 处理单个客户端的线程函数
    void handle_client(SOCKET client_sock, const std::string &client_ip)
    {
        char *recv_buf = new char[buffer_size + 1];
        if (!recv_buf)
        {
            log_error("内存分配失败");
            closesocket(client_sock);
            return;
        }

        log_info("客户端 " + client_ip + " 连接成功");

        while (is_running)
        {
            int ret = recv(client_sock, recv_buf, buffer_size, 0);
            if (ret <= 0)
            {
                if (ret < 0)
                    log_error("接收数据失败 (" + client_ip + ")");
                else
                    log_info("客户端 " + client_ip + " 断开连接");
                break;
            }

            recv_buf[ret] = '\0';
            std::string data(recv_buf, ret);

            // 调用用户自定义处理函数
            if (!on_receive(client_sock, client_ip, data))
            {
                break; // 用户处理函数返回false时断开连接
            }
        }

        delete[] recv_buf;
        closesocket(client_sock);
        log_info("客户端 " + client_ip + " 连接已关闭");
    }

protected:
    // 日志输出（带线程安全）
    void log_info(const std::string &msg)
    {
        std::lock_guard<std::mutex> lock(console_mutex);
        ConsoleColor::set(ConsoleColor::GREEN);
        std::cout << "[INFO] " << msg << std::endl;
        ConsoleColor::set(ConsoleColor::WHITE);
    }

    void log_error(const std::string &msg)
    {
        std::lock_guard<std::mutex> lock(console_mutex);
        ConsoleColor::set(ConsoleColor::RED);
        std::cout << "[ERROR] " << msg << " (错误码: " << WSAGetLastError() << ")" << std::endl;
        ConsoleColor::set(ConsoleColor::WHITE);
    }

    void log_debug(const std::string &msg)
    {
        std::lock_guard<std::mutex> lock(console_mutex);
        ConsoleColor::set(ConsoleColor::GRAY);
        std::cout << "[DEBUG] " << msg << std::endl;
        ConsoleColor::set(ConsoleColor::WHITE);
    }

public:
    // 构造函数
    TCPServer(std::string ip = "0.0.0.0", int port = 8080, int buffer_size = DEFAULT_BUFFER_SIZE)
        : ip(ip), port(port), server_socket(INVALID_SOCKET), is_running(false), buffer_size(buffer_size) {}

    // 析构函数
    ~TCPServer()
    {
        stop();
    }

    // 初始化服务器
    bool init()
    {
        // 初始化Winsock
        WORD winsock_version = MAKEWORD(2, 2);
        WSADATA wsa_data;
        if (WSAStartup(winsock_version, &wsa_data) != 0)
        {
            log_error("Winsock初始化失败");
            return false;
        }

        // 创建套接字
        server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket == INVALID_SOCKET)
        {
            log_error("创建服务器套接字失败");
            WSACleanup();
            return false;
        }

        // 设置地址重用
        int opt = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) == SOCKET_ERROR)
        {
            log_error("设置地址重用失败");
        }

        // 绑定地址和端口
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.S_un.S_addr = (ip == "0.0.0.0") ? INADDR_ANY : inet_addr(ip.c_str());

        if (bind(server_socket, (LPSOCKADDR)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
        {
            log_error("绑定端口 " + std::to_string(port) + " 失败");
            closesocket(server_socket);
            WSACleanup();
            return false;
        }

        log_info("服务器初始化成功，绑定地址: " + ip + ":" + std::to_string(port));
        return true;
    }

    // 开始监听（非阻塞，支持多客户端）
    bool start()
    {
        if (server_socket == INVALID_SOCKET)
        {
            log_error("请先初始化服务器");
            return false;
        }

        if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR)
        {
            log_error("监听失败");
            return false;
        }

        is_running = true;
        log_info("服务器开始监听，等待客户端连接...");

        // 启动监听线程
        std::thread([this]()
                    {
            while (is_running) {
                sockaddr_in client_addr;
                int client_addr_len = sizeof(client_addr);
                SOCKET client_sock = accept(server_socket, (SOCKADDR*)&client_addr, &client_addr_len);

                if (client_sock == INVALID_SOCKET) {
                    if (is_running) log_error("接受客户端连接失败");
                    continue;
                }

                // 启动新线程处理客户端
                std::string client_ip = inet_ntoa(client_addr.sin_addr);
                std::thread(&TCPServer::handle_client, this, client_sock, client_ip).detach();
            } })
            .detach();

        return true;
    }

    // 停止服务器
    void stop()
    {
        if (!is_running)
            return;

        is_running = false;
        log_info("正在关闭服务器...");

        if (server_socket != INVALID_SOCKET)
        {
            closesocket(server_socket);
            server_socket = INVALID_SOCKET;
        }

        WSACleanup();
        log_info("服务器已完全关闭");
    }

    // 发送数据
    bool send_data(SOCKET client_sock, const std::string &data)
    {
        if (client_sock == INVALID_SOCKET || !is_running)
        {
            log_error("发送失败：无效的套接字或服务器未运行");
            return false;
        }

        int ret = send(client_sock, data.c_str(), data.size(), 0);
        if (ret == SOCKET_ERROR)
        {
            log_error("发送数据失败");
            return false;
        }
        return true;
    }

    // 发送文件（支持二进制）
    bool send_file(SOCKET client_sock, const std::string &file_path)
    {
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            log_error("无法打开文件: " + file_path);
            return false;
        }

        // 获取文件大小
        std::streamsize file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        // 先发送文件名和大小
        std::string file_info = "FILE:" + std::string(file_path) + ":" + std::to_string(file_size) + "\n";
        if (!send_data(client_sock, file_info))
        {
            file.close();
            return false;
        }

        // 发送文件内容
        char *buffer = new char[buffer_size];
        while (file_size > 0)
        {
            std::streamsize bytes_read = file.read(buffer, buffer_size).gcount();
            if (bytes_read <= 0)
                break;

            if (send(client_sock, buffer, bytes_read, 0) != bytes_read)
            {
                log_error("文件发送失败");
                delete[] buffer;
                file.close();
                return false;
            }

            file_size -= bytes_read;
        }

        delete[] buffer;
        file.close();
        log_info("文件发送完成: " + file_path);
        return true;
    }

    // 接收数据处理回调（用户可重写）
    virtual bool on_receive(SOCKET client_sock, const std::string &client_ip, const std::string &data)
    {
        log_debug("收到来自 " + client_ip + " 的数据: " + data);
        // 默认回复确认信息
        return send_data(client_sock, "已收到: " + data);
    }
};

// 客户端类
class TCPClient
{
private:
    std::string server_ip;
    int server_port;
    SOCKET client_socket;
    bool is_connected; // 成员变量
    int buffer_size;

public:
    // 日志输出
    void log_info(const std::string &msg)
    {
        ConsoleColor::set(ConsoleColor::GREEN);
        std::cout << "[CLIENT INFO] " << msg << std::endl;
        ConsoleColor::set(ConsoleColor::WHITE);
    }

    void log_error(const std::string &msg)
    {
        ConsoleColor::set(ConsoleColor::RED);
        std::cout << "[CLIENT ERROR] " << msg << " (错误码: " << WSAGetLastError() << ")" << std::endl;
        ConsoleColor::set(ConsoleColor::WHITE);
    }

    void log_debug(const std::string &msg)
    {
        ConsoleColor::set(ConsoleColor::GRAY);
        std::cout << "[CLIENT DEBUG] " << msg << std::endl;
        ConsoleColor::set(ConsoleColor::WHITE);
    }

    // 构造函数
    TCPClient(std::string ip, int port, int buffer_size = DEFAULT_BUFFER_SIZE)
        : server_ip(ip), server_port(port), client_socket(INVALID_SOCKET), is_connected(false), buffer_size(buffer_size) {}

    // 析构函数
    ~TCPClient()
    {
        disconnect();
    }

    // 连接到服务器
    bool connect()
    {
        // 初始化Winsock
        WORD winsock_version = MAKEWORD(2, 2);
        WSADATA wsa_data;
        if (WSAStartup(winsock_version, &wsa_data) != 0)
        {
            log_error("Winsock初始化失败");
            return false;
        }

        // 创建套接字
        client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (client_socket == INVALID_SOCKET)
        {
            log_error("创建客户端套接字失败");
            WSACleanup();
            return false;
        }

        // 连接服务器
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        server_addr.sin_addr.S_un.S_addr = inet_addr(server_ip.c_str());

        if (::connect(client_socket, (LPSOCKADDR)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
        {
            log_error("连接服务器 " + server_ip + ":" + std::to_string(server_port) + " 失败");
            closesocket(client_socket);
            WSACleanup();
            return false;
        }

        is_connected = true;
        log_info("成功连接到服务器: " + server_ip + ":" + std::to_string(server_port));
        return true;
    }

    // 断开连接
    void disconnect()
    {
        if (!is_connected)
            return;

        is_connected = false;
        log_info("正在断开与服务器的连接...");

        if (client_socket != INVALID_SOCKET)
        {
            closesocket(client_socket);
            client_socket = INVALID_SOCKET;
        }

        WSACleanup();
        log_info("已断开与服务器的连接");
    }

    // 发送数据
    bool send_data(const std::string &data)
    {
        if (!is_connected || client_socket == INVALID_SOCKET)
        {
            log_error("发送失败：未连接到服务器");
            return false;
        }

        int ret = send(client_socket, data.c_str(), data.size(), 0);
        if (ret == SOCKET_ERROR)
        {
            log_error("发送数据失败");
            return false;
        }
        return true;
    }

    // 接收数据（阻塞）
    bool receive_data(std::string &data)
    {
        if (!is_connected || client_socket == INVALID_SOCKET)
        {
            log_error("接收失败：未连接到服务器");
            return false;
        }

        char *recv_buf = new char[buffer_size + 1];
        int ret = recv(client_socket, recv_buf, buffer_size, 0);

        if (ret <= 0)
        {
            if (ret < 0)
                log_error("接收数据失败");
            else
                log_info("服务器已断开连接");

            delete[] recv_buf;
            is_connected = false;
            return false;
        }

        recv_buf[ret] = '\0';
        data = std::string(recv_buf, ret);
        delete[] recv_buf;
        return true;
    }

    // 发送文件（支持二进制）
    bool send_file(const std::string &file_path)
    {
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            log_error("无法打开文件: " + file_path);
            return false;
        }

        // 获取文件大小
        std::streamsize file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        // 先发送文件名和大小
        std::string file_info = "FILE:" + std::string(file_path) + ":" + std::to_string(file_size) + "\n";
        if (!send_data(file_info))
        {
            file.close();
            return false;
        }

        // 发送文件内容
        char *buffer = new char[buffer_size];
        while (file_size > 0)
        {
            std::streamsize bytes_read = file.read(buffer, buffer_size).gcount();
            if (bytes_read <= 0)
                break;

            if (send(client_socket, buffer, bytes_read, 0) != bytes_read)
            {
                log_error("文件发送失败");
                delete[] buffer;
                file.close();
                return false;
            }

            file_size -= bytes_read;
        }

        delete[] buffer;
        file.close();
        log_info("文件发送完成: " + file_path);
        return true;
    }

    // 接收文件
    bool receive_file(const std::string &save_dir = ".")
    {
        if (!is_connected || client_socket == INVALID_SOCKET)
        {
            log_error("接收文件失败：未连接到服务器");
            return false;
        }

        // 先接收文件信息（格式：FILE:文件名:大小）
        std::string file_info;
        if (!receive_data(file_info))
        {
            return false;
        }

        size_t pos1 = file_info.find(':');
        size_t pos2 = file_info.find(':', pos1 + 1);
        if (pos1 == std::string::npos || pos2 == std::string::npos || file_info.substr(0, pos1) != "FILE")
        {
            log_error("无效的文件信息格式");
            return false;
        }

        std::string filename = file_info.substr(pos1 + 1, pos2 - pos1 - 1);
        std::streamsize file_size = std::stoll(file_info.substr(pos2 + 1));

        // 构建保存路径
        std::string save_path = save_dir + "\\" + filename;
        std::ofstream file(save_path, std::ios::binary);
        if (!file.is_open())
        {
            log_error("无法创建文件: " + save_path);
            return false;
        }

        // 接收文件内容
        char *buffer = new char[buffer_size];
        std::streamsize total_received = 0;

        while (total_received < file_size)
        {
            std::streamsize bytes_to_read = std::min((std::streamsize)buffer_size, file_size - total_received);
            int ret = recv(client_socket, buffer, bytes_to_read, 0);

            if (ret <= 0)
            {
                log_error("文件接收失败");
                delete[] buffer;
                file.close();
                std::remove(save_path.c_str()); // 删除不完整文件
                return false;
            }

            file.write(buffer, ret);
            total_received += ret;
        }

        delete[] buffer;
        file.close();
        log_info("文件接收完成: " + save_path + " (" + std::to_string(total_received) + " bytes)");
        return true;
    }

    // 检查连接状态 - 重命名以避免冲突
    bool isConnected() const
    {
        return is_connected;
    }
};

#endif // SOCK_HPP
