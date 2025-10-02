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

// 聊天服务器类
class ChatServer : public TCPServer
{
private:
    std::set<SOCKET> clients;                       // 所有客户端套接字
    std::map<SOCKET, std::string> client_nicknames; // 套接字到昵称的映射
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
        // 处理新客户端的昵称设置
        if (data.substr(0, 9) == "NICKNAME ")
        {
            // 提取“NICKNAME ”后的内容，并去除前后空白
            std::string nickname = trim(data.substr(9));
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.insert(client_sock);              // 将客户端加入广播列表
                client_nicknames[client_sock] = nickname; // 关联“套接字-正确昵称”
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
            }
            log_info("用户 " + nickname + " 离开聊天");
            broadcast(client_sock, "系统消息: " + nickname + " 离开了聊天");
            return false; // 返回false表示断开连接
        }

        // 处理普通消息
        std::string nickname;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            // 修复：若未找到昵称，用客户端IP作为默认名
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
