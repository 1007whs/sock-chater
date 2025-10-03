#include "sock.hpp"
#include <set>
#include <map>
#include <functional>

// 客户端状态枚举
enum class ClientState
{
    CONNECTED,    // 已连接但未设置昵称
    NICKNAME_SET, // 已设置昵称
    DISCONNECTED  // 已断开连接
};

// 全局变量
std::set<SOCKET> clients;                       // 所有客户端套接字
std::map<SOCKET, std::string> client_nicknames; // 套接字到昵称的映射
std::map<SOCKET, ClientState> client_states;    // 套接字到状态的映射
std::mutex clients_mutex;                       // 保护客户端集合的互斥锁

// 工具函数
std::string trim(const std::string &s)
{
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start))
    {
        ++start;
    }
    auto end = s.end();
    do
    {
        --end;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    return std::string(start, end + 1);
}

// 广播消息给所有客户端（除了发送者）
void broadcast(SOCKET sender, const std::string &msg)
{
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (SOCKET client : clients)
    {
        if (client != sender)
        {
            // 直接发送数据，不通过服务器实例
            // 这里需要访问服务器实例来发送数据
        }
    }
}

// 获取客户端昵称
std::string getNickname(SOCKET client_sock, const std::string &client_ip)
{
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = client_nicknames.find(client_sock);
    if (it != client_nicknames.end())
    {
        return it->second;
    }
    return client_ip;
}

// 自定义服务器类，重写on_receive方法
class ChatTCPServer : public TCPServer
{
public:
    ChatTCPServer(std::string ip = "0.0.0.0", int port = 8888) : TCPServer(ip, port) {}

    // 重写接收数据处理函数
    bool on_receive(SOCKET client_sock, const std::string &client_ip, const std::string &data) override
    {
        // 当新客户端连接时，初始化其状态
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            if (client_states.find(client_sock) == client_states.end())
            {
                client_states[client_sock] = ClientState::CONNECTED;
            }
        }

        // 处理新客户端的昵称设置
        if (data.substr(0, 9) == "NICKNAME ")
        {
            std::string nickname = trim(data.substr(9));
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.insert(client_sock);
                client_nicknames[client_sock] = nickname;
                client_states[client_sock] = ClientState::NICKNAME_SET;
            }

            log_info("用户 " + nickname + " 加入聊天");
            // 广播消息
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (SOCKET client : clients)
            {
                if (client != client_sock)
                {
                    send_data(client, "系统消息: " + nickname + " 加入了聊天");
                }
            }
            send_data(client_sock, "昵称已设置为: " + nickname);
            return true;
        }

        // 处理退出命令
        if (data == "exit")
        {
            std::string nickname = getNickname(client_sock, client_ip);
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.erase(client_sock);
                client_nicknames.erase(client_sock);
                client_states[client_sock] = ClientState::DISCONNECTED;
            }

            log_info("用户 " + nickname + " 离开聊天");
            // 广播消息
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (SOCKET client : clients)
            {
                if (client != client_sock)
                {
                    send_data(client, "系统消息: " + nickname + " 离开了聊天");
                }
            }
            return false;
        }

        // 检查客户端是否已设置昵称
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            auto state_it = client_states.find(client_sock);
            if (state_it == client_states.end() || state_it->second != ClientState::NICKNAME_SET)
            {
                return true;
            }
        }

        // 处理普通消息
        std::string nickname = getNickname(client_sock, client_ip);
        std::string message = "[" + nickname + "]: " + data;
        log_debug("转发消息: " + message);
        // 广播消息
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (SOCKET client : clients)
        {
            if (client != client_sock)
            {
                send_data(client, message);
            }
        }

        return true;
    }
};

ChatTCPServer *server = nullptr; // 服务器实例

int main()
{
    setConsoleUTF8();
    ConsoleColor::set(ConsoleColor::YELLOW);
    std::cout << "=== 多人聊天服务器 ===" << std::endl;
    ConsoleColor::set(ConsoleColor::WHITE);

    // 创建并启动服务器
    server = new ChatTCPServer("0.0.0.0", 8888);

    if (!server->init())
    {
        std::cerr << "服务器初始化失败" << std::endl;
        delete server;
        return 1;
    }

    if (!server->start())
    {
        std::cerr << "服务器启动失败" << std::endl;
        delete server;
        return 1;
    }

    std::cout << "服务器运行中，按Ctrl+C退出..." << std::endl;

    // 保持服务器运行
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    delete server;
    return 0;
}