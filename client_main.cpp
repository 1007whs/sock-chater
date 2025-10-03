#include "sock.hpp"
#include <thread>

// 全局变量
TCPClient *client = nullptr;
std::thread receiver_thread;
bool receiving = false;

// 接收消息线程函数
void receiveMessages()
{
    std::string msg;
    while (receiving && client->isConnected())
    {
        if (client->receive_data(msg))
        {
            ConsoleColor::set(ConsoleColor::YELLOW);
            std::cout << "\n"
                      << msg << std::endl;
            ConsoleColor::set(ConsoleColor::WHITE);
            std::cout << "请输入消息 (输入exit退出): ";
            std::cout.flush();
        }
        else
        {
            break;
        }
    }
}

// 启动接收消息
void startReceiving()
{
    if (!client->isConnected())
        return;

    receiving = true;
    receiver_thread = std::thread(receiveMessages);
}

// 停止接收消息
void stopReceiving()
{
    receiving = false;
    if (receiver_thread.joinable())
    {
        receiver_thread.join();
    }
}

// 连接到服务器并设置昵称
bool connectWithNickname(const std::string &nickname)
{
    if (!client->connect())
    {
        client->log_error("连接服务器失败");
        return false;
    }

    if (!client->send_data("NICKNAME " + nickname))
    {
        client->log_error("发送昵称失败");
        client->disconnect();
        return false;
    }

    return true;
}

// 发送消息
bool sendMessage(const std::string &message)
{
    if (!client->send_data(message))
    {
        client->log_error("发送消息失败");
        return false;
    }
    return true;
}

int main()
{
    setConsoleUTF8();
    ConsoleColor::set(ConsoleColor::YELLOW);
    std::cout << "=== 多人聊天客户端 ===" << std::endl;
    ConsoleColor::set(ConsoleColor::WHITE);

    std::string server_ip;
    int port;
    std::string nickname;

    // 获取用户输入
    std::cout << "请输入服务器IP: ";
    std::cin >> server_ip;
    std::cout << "请输入端口号: ";
    std::cin >> port;
    std::cin.ignore(); // 忽略换行符
    std::cout << "请输入你的昵称: ";
    std::getline(std::cin, nickname);

    // 创建客户端
    client = new TCPClient(server_ip, port);

    if (!connectWithNickname(nickname))
    {
        std::cerr << "连接服务器失败" << std::endl;
        delete client;
        return 1;
    }

    client->log_info("成功连接到服务器");

    // 启动接收消息
    startReceiving();

    // 主线程处理输入
    std::string input;
    std::cout << "连接成功！";
    while (client->isConnected())
    {
        std::getline(std::cin, input);

        if (sendMessage(input))
        {
            if (input == "exit")
            {
                break;
            }
            std::cout << "请输入消息 (输入exit退出): ";
        }
        else
        {
            break;
        }
    }

    stopReceiving();
    client->disconnect();
    client->log_info("已断开与服务器的连接");
    delete client;
    std::cout << "已退出聊天" << std::endl;
    return 0;
}