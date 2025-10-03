#include "sock.hpp"
#include <set>
#include <map>

std::string trim(const std::string &s)
{
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start))
    {
        ++start; // 跳过开头空白
    }
    auto end = s.end();
    do
    {
        --end;
    } while (std::distance(start, end) > 0 && std::isspace(*end)); // 跳过结尾空白
    return std::string(start, end + 1);
}

// 客户端状态枚举
enum ClientState {
    CONNECTED,     // 已连接但未设置昵称
    NICKNAME_SET,  // 已设置昵称
    DISCONNECTED   // 已断开连接
};

// 聊天服务器类
class ChatServer : public TCPServer
{
private:
    std::set<SOCKET> clients;                       // 所有客户端套接字
    std::map<SOCKET, std::string> client_nicknames; // 套接字到昵称的映射
    std::map<SOCKET, ClientState> client_states;    // 套接字到状态的映射
    std::mutex clients_mutex;                       // 保护客户端集合的互斥锁

    // 广播消息给所有客户端（除了发送者）
    void broadcast(SOCKET sender, const std::string &msg)
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (SOCKET client : clients)
        {
            if (client != sender)
            {
                send_data(client, msg);
            }
        }
    }

public:
    ChatServer(std::string ip = "0.0.0.0", int port = 8888) : TCPServer(ip, port) {}

    // 重写接收数据处理函数
    bool on_receive(SOCKET client_sock, const std::string &client_ip, const std::string &data) override
    {
        // 当新客户端连接时，初始化其状态
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            if (client_states.find(client_sock) == client_states.end()) {
                client_states[client_sock] = CONNECTED; // 初始化客户端状态
            }
        }
        // 处理新客户端的昵称设置
        if (data.substr(0, 9) == "NICKNAME ")
        {
            // 提取“NICKNAME ”后的内容，并去除前后空白
            std::string nickname = trim(data.substr(9));
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.insert(client_sock);              // 将客户端加入广播列表
                client_nicknames[client_sock] = nickname; // 关联"套接字-正确昵称"
                client_states[client_sock] = NICKNAME_SET; // 更新客户端状态
            }
            log_info("用户 " + nickname + " 加入聊天");
            broadcast(client_sock, "系统消息: " + nickname + " 加入了聊天");
            send_data(client_sock, "昵称已设置为: " + nickname); // 向客户端确认正确昵称
            return true;
        }

        // 处理退出命令
        if (data == "exit")
        {
            std::string nickname;
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                nickname = client_nicknames[client_sock];
                clients.erase(client_sock);
                client_nicknames.erase(client_sock);
                client_states[client_sock] = DISCONNECTED; // 更新客户端状态
            }
            log_info("用户 " + nickname + " 离开聊天");
            broadcast(client_sock, "系统消息: " + nickname + " 离开了聊天");
            return false; // 返回false表示断开连接
        }

        // 检查客户端是否已设置昵称
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            auto state_it = client_states.find(client_sock);
            if (state_it == client_states.end() || state_it->second != NICKNAME_SET) {
                // 客户端尚未设置昵称，忽略消息
                return true;
            }
        }

        // 处理普通消息
        std::string nickname;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            // 获取客户端昵称
            auto it = client_nicknames.find(client_sock);
            if (it != client_nicknames.end())
            {
                nickname = it->second;
            }
            else
            {
                nickname = client_ip; // 默认用客户端IP作为名字
            }
        }

        std::string message = "[" + nickname + "]: " + data;
        log_debug("转发消息: " + message);
        broadcast(client_sock, message);

        return true;
    }
};

int main()
{
    setConsoleUTF8();
    ConsoleColor::set(ConsoleColor::YELLOW);
    std::cout << "=== 多人聊天服务器 ===" << std::endl;
    ConsoleColor::set(ConsoleColor::WHITE);

    // 创建并启动服务器
    ChatServer server("0.0.0.0", 8888);
    if (!server.init())
    {
        std::cerr << "服务器初始化失败" << std::endl;
        return 1;
    }

    if (!server.start())
    {
        std::cerr << "服务器启动失败" << std::endl;
        return 1;
    }

    std::cout << "服务器运行中，按Ctrl+C退出..." << std::endl;

    // 保持服务器运行
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server.stop();
    return 0;
}
